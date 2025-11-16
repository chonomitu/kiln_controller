/***************************************************************************************
 * FILE: src/control.cpp
 * LAST MODIFIED: 2025-11-14 09:20 (Europe/Warsaw)
 * PURPOSE: Pomiar MAX31855, PID, SSR + bufor próbek dla wykresu/CSV + profile
 ***************************************************************************************/
#include "control.h"
#include "config.h"

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_MAX31855.h>
#include <ArduinoJson.h>

#if USE_QUICKPID
  #include <QuickPID.h>
#else
  #include <PID_v1_bc.h>
#endif

#include <math.h>
#include <string.h>

// Zmienne globalne – typy MUSZĄ być zgodne z control.h (bez volatile)
// ────────────────────────────────────────────────────────────────────────────────

double   KILN_TEMP = 0.0;   // °C
double   CTRL_TEMP = NAN;   // °C – temperatura sterownika (złącze zimne MAX31855)
double   PID_SET   = 200.0; // °C
double   PID_OUT   = 0.0;   // 0..100 %


bool     RUN_ACTIVE  = false;
bool     SAFETY_TRIP = false;

bool     HEATER_ON   = false;
bool     SENSOR_OK   = false;

uint8_t  PROFILE_ACTIVE = 0;
uint8_t  PROFILE_LEN    = 0;
uint32_t RUN_REV        = 0;

// Profil – kroki
ProfileStep PROFILE[8];

// Czas etapu (sekundy)
uint32_t PROFILE_ELAPSED_SEC = 0;
uint32_t PROFILE_REMAIN_SEC  = 0;

// start aktualnego etapu (ms od startu MCU)
static uint32_t g_profileStepStartMs = 0;

// ────────────────────────────────────────────────────────────────────────────────
 // MAX31855 – obiekt tworzony dynamicznie, żeby można było go przepiąć po zmianie pinów
// ────────────────────────────────────────────────────────────────────────────────

static Adafruit_MAX31855* THERMO = nullptr;

// ────────────────────────────────────────────────────────────────────────────────
// PID (QuickPID lub klasyczny PID_v1_bc) – wejście: KILN_TEMP, wyjście: PID_OUT
// ────────────────────────────────────────────────────────────────────────────────

#if USE_QUICKPID
  static QuickPID PIDCTL(&KILN_TEMP, &PID_OUT, &PID_SET);
#else
  static PID PIDCTL(&KILN_TEMP, &PID_OUT, &PID_SET, 20.0, 0.8, 50.0, DIRECT);
#endif

// ────────────────────────────────────────────────────────────────────────────────
// Okno czasowe dla SSR (prosty time-proportioning)
// ────────────────────────────────────────────────────────────────────────────────

static unsigned long WINDOW_START = 0;
static const unsigned long WINDOW_MS = 2000UL; // 2 s – możesz zmienić jeśli chcesz


// ────────────────────────────────────────────────────────────────────────────────
// BUZZER – krótkie sygnały po etapach
// ────────────────────────────────────────────────────────────────────────────────

static bool g_buzzActive = false;
static unsigned long g_buzzEndMs = 0;

static void buzzStart(unsigned long durationMs) {
  if (CFG.pins.BUZZ < 0) return;      // brak pinu – nic nie rób
  if (durationMs == 0) return;

  g_buzzActive = true;
  g_buzzEndMs  = millis() + durationMs;
  digitalWrite(CFG.pins.BUZZ, HIGH);  // załącz buzzer
}

static void buzzLoop(unsigned long nowMs) {
  if (!g_buzzActive) return;
  if (CFG.pins.BUZZ < 0) {
    g_buzzActive = false;
    return;
  }
  // koniec sygnału?
  if ((long)(nowMs - g_buzzEndMs) >= 0) {
    g_buzzActive = false;
    digitalWrite(CFG.pins.BUZZ, LOW);
  }
}


// ────────────────────────────────────────────────────────────────────────────────
// Bufor próbek do wykresu / CSV
// ────────────────────────────────────────────────────────────────────────────────

struct Sample {
  uint32_t rev;   // RUN_REV przy próbkowaniu
  float    temp;  // KILN_TEMP
  float    out;   // PID_OUT
  uint16_t heat;  // HEATER_ON (0/1)
};

static const uint16_t S_CAP = 240;   // ~4 min przy kroku 1 s
static Sample   SAMPLES[S_CAP];
static uint16_t S_LEN = 0;

// ────────────────────────────────────────────────────────────────────────────────
// Pomocnicze
// ────────────────────────────────────────────────────────────────────────────────

static inline void ssrWrite(bool on) {
  HEATER_ON = on;
  if (CFG.pins.SSR >= 0) {
    digitalWrite(CFG.pins.SSR, on ? HIGH : LOW);
  }
  if (CFG.pins.LED >= 0) {
    // jeśli używasz wbudowanej diody z odwrotną logiką, możesz to tu odwrócić
    digitalWrite(CFG.pins.LED, on ? LOW : HIGH);
  }
}

static inline double readTempC() {
  if (!THERMO) return NAN;
  double v = THERMO->readCelsius();
  if (!isfinite(v)) return NAN;
  return v;
}

// pojedynczy krok algorytmu okna SSR
static void windowDrive(unsigned long nowMs) {
  if (nowMs - WINDOW_START >= WINDOW_MS) {
    WINDOW_START = nowMs;
  }

  double duty = PID_OUT;
  if (!isfinite(duty)) duty = 0.0;
  if (duty < 0.0) duty = 0.0;
  if (duty > 100.0) duty = 100.0;

  unsigned long onMs = (unsigned long)(duty * (double)WINDOW_MS / 100.0);
  bool on = RUN_ACTIVE && !SAFETY_TRIP && (nowMs - WINDOW_START < onMs);
  ssrWrite(on);
}

// próbki do bufora – max ~1 próbkowanie na sekundę
static void updateSamples(unsigned long nowMs) {
  static unsigned long lastSampleMs = 0;

  if (nowMs - lastSampleMs < 1000UL) {
    return;  // próbkujemy co ~1 s
  }
  lastSampleMs = nowMs;

  uint16_t heat = (uint16_t)(HEATER_ON ? 1u : 0u);

  if (S_LEN < S_CAP) {
    SAMPLES[S_LEN++] = { RUN_REV, (float)KILN_TEMP, (float)PID_OUT, heat };
  } else {
    // przesuwamy bufor o 1 w lewo
    memmove(&SAMPLES[0], &SAMPLES[1], sizeof(Sample) * (S_CAP - 1));
    SAMPLES[S_CAP - 1] = { RUN_REV, (float)KILN_TEMP, (float)PID_OUT, heat };
  }
}

// ────────────────────────────────────────────────────────────────────────────────
// PROFIL – wewnętrzna pomocnicza: skok na etap
// ────────────────────────────────────────────────────────────────────────────────
static void profileGotoStep(uint8_t idx) {
  if (idx >= PROFILE_LEN) return;
  PROFILE_ACTIVE = idx;
  g_profileStepStartMs = millis();
  PROFILE_ELAPSED_SEC  = 0;
  PROFILE_REMAIN_SEC   = PROFILE[PROFILE_ACTIVE].holdSec;
  PID_SET = PROFILE[PROFILE_ACTIVE].targetC;
  Serial.print(F("[CTRL] Profile step="));
  Serial.print(PROFILE_ACTIVE + 1);
  Serial.print('/');
  Serial.println(PROFILE_LEN);
}

// ────────────────────────────────────────────────────────────────────────────────
// API z control.h
// ────────────────────────────────────────────────────────────────────────────────

// inicjalizacja pinów + magistrali I2C + MAX31855
void pinsAndBusesInit() {
  // GPIO
  if (CFG.pins.SSR  >= 0) { pinMode(CFG.pins.SSR,  OUTPUT); digitalWrite(CFG.pins.SSR, LOW); }
  if (CFG.pins.LED  >= 0) { pinMode(CFG.pins.LED,  OUTPUT); digitalWrite(CFG.pins.LED, HIGH); }
  if (CFG.pins.BUZZ >= 0) { pinMode(CFG.pins.BUZZ, OUTPUT); digitalWrite(CFG.pins.BUZZ, LOW); }

  // I2C – piny z konfiguracji (OLED itp.)
  Wire.begin(CFG.pins.I2C_SDA, CFG.pins.I2C_SCL);
  delay(50); // mała przerwa po starcie I2C

  // MAX31855 – SoftSPI na pinach z CFG
  if (THERMO) {
    delete THERMO;
    THERMO = nullptr;
  }
  THERMO = new Adafruit_MAX31855(CFG.pins.SPI_SCK,
                                 CFG.pins.SPI_CS,
                                 CFG.pins.SPI_MISO);
}

// konfiguracja PID – używamy pól Kp, Ki, Kd z CFG.pid
void controlSetup() {
  // najpierw piny / I2C / MAX
  pinsAndBusesInit();
  
#if USE_QUICKPID
  PIDCTL.SetTunings(CFG.pid.Kp, CFG.pid.Ki, CFG.pid.Kd);
  PIDCTL.SetOutputLimits(0, 100);
#else
  PIDCTL.SetTunings(CFG.pid.Kp, CFG.pid.Ki, CFG.pid.Kd);
  PIDCTL.SetOutputLimits(0, 100);
  PIDCTL.SetMode(AUTOMATIC);
#endif

  PID_SET = CFG.pid.setpointC;
  WINDOW_START = millis();

  PROFILE_ACTIVE = 0;
  PROFILE_LEN    = 0;
  PROFILE_ELAPSED_SEC = 0;
  PROFILE_REMAIN_SEC  = 0;
  g_profileStepStartMs = millis();
}

void runStart() {
  if (SAFETY_TRIP) {
    // nie startujemy jeśli zadziałał trip
    return;
  }
  RUN_ACTIVE = true;
  RUN_REV++;

  // przy starcie profilu zresetuj licznik etapu
  if (CFG.mode == MODE_PROFILE && PROFILE_LEN > 0) {
    g_profileStepStartMs = millis();
    PROFILE_ELAPSED_SEC  = 0;
    PROFILE_REMAIN_SEC   = PROFILE[PROFILE_ACTIVE].holdSec;
  }
}

void runStop() {
  RUN_ACTIVE = false;
  ssrWrite(false);
}

/***************************************************************************************
 * FILE: src/control.cpp
 * LAST MODIFIED: 2025-11-15 04:10 (Europe/Warsaw)
 * PURPOSE: Pomiar MAX31855, PID, SSR + bufor próbek dla wykresu/CSV + profile
 ***************************************************************************************/

// ...

// główna pętla sterowania – wywoływana z loop()
void controlLoop() {
  const unsigned long now = millis();

  // Odczyt MAX31855 z throttlingiem
  static unsigned long lastThermoMs = 0;
  static double lastThermoC  = NAN;  // temp. pieca
  static double filteredC = NAN;   // EMA filtrowana temp
  static double lastBoardC   = NAN;  // temp. sterownika (złącze zimne)

  if (now - lastThermoMs >= 250UL) {      // max 4 odczyty na sekundę
    lastThermoMs = now;

    // temperatura pieca (termopara)
    double raw = readTempC();

    // temperatura złącza zimnego / układu – traktujemy jako "temp. sterownika"
    double board = NAN;
    if (THERMO) {
      board = THERMO->readInternal();   // funkcja z Adafruit_MAX31855
    }

    if (isfinite(raw)) {
      lastThermoC = raw;
    } else {
      lastThermoC = NAN;
    }

    // wygładzanie wyników odczytu temperatury (filtr)
    // if (isfinite(raw)) {
    //   lastThermoC = raw;

    //   if (!isfinite(filteredC)) filteredC = raw;        // pierwszy pomiar bez filtru
    //   else filteredC = filteredC * 0.85 + raw * 0.15;  // EMA
    //   }



    if (isfinite(board)) {
      lastBoardC = board;
    } else {
      lastBoardC = NAN;
    }
  }

  // BUZZER – obsługa czasu trwania sygnału
  buzzLoop(now);

  // ──────────────────────────────────────────────────────────────────────
  // Aktualizacja globalnych temperatur
  // ──────────────────────────────────────────────────────────────────────
  KILN_TEMP = lastThermoC;   // temp. pieca
  // KILN_TEMP = filteredC;
  CTRL_TEMP = lastBoardC;    // temp. sterownika (złącze zimne MAX31855)
  SENSOR_OK = isfinite(KILN_TEMP);

  // ──────────────────────────────────────────────────────────────────────
  // LOGIKA PROFILU
  // ──────────────────────────────────────────────────────────────────────
  if (CFG.mode == MODE_PROFILE && PROFILE_LEN > 0) {
    if (RUN_ACTIVE && SENSOR_OK) {
      ProfileStep &step = PROFILE[PROFILE_ACTIVE];
      uint32_t hold = step.holdSec;           // sekundy
      uint32_t elapsedMs = now - g_profileStepStartMs;

      PROFILE_ELAPSED_SEC = elapsedMs / 1000UL;

      if (hold > 0) {
        if (PROFILE_ELAPSED_SEC >= hold) {
          // koniec etapu → kolejny
          if (PROFILE_ACTIVE + 1 < PROFILE_LEN) {
            profileGotoStep(PROFILE_ACTIVE + 1);
          } else {
            // ostatni etap zakończony – stop grzania
            RUN_ACTIVE = false;
            ssrWrite(false);
            PROFILE_REMAIN_SEC = 0;
            Serial.println(F("[CTRL] Profile finished – RUN stopped"));
          }
        } else {
          PROFILE_REMAIN_SEC = hold - PROFILE_ELAPSED_SEC;
        }
      } else {
        // holdSec == 0 → nieskończony etap
        PROFILE_REMAIN_SEC = 0;
      }

      // zadana temperatura z aktualnego kroku
      PID_SET = step.targetC;
    } else {
      // nie biegniemy (RUN_STOP / brak czujnika) – nie przesuwamy etapów
      // ale PID_SET zostaje na ostatnim kroku
    }
  } else {
    // tryb dynamiczny albo brak profilu – licznik etapu zerowy
    PROFILE_ELAPSED_SEC = 0;
    PROFILE_REMAIN_SEC  = 0;
  }

  // PID – pracuje tylko gdy mamy sensowny pomiar
  if (!SENSOR_OK) {
    PID_OUT = 0.0;
  } else {
#if USE_QUICKPID
    PIDCTL.Compute();
#else
    PIDCTL.Compute();
#endif
  }

  // sterowanie SSR
  windowDrive(now);

  // próbkowanie (do CSV/wykresu web)
  updateSamples(now);
}


// eksport bufora próbek jako CSV (dla /export)
void buildSamplesCSV(String& out) {
  out.reserve(64 * (S_LEN + 4));
  out  = F("rev,tempC,out,heat\n");
  for (uint16_t i = 0; i < S_LEN; i++) {
    const Sample& s = SAMPLES[i];
    out += String((uint32_t)s.rev); out += ',';
    if (isfinite(s.temp)) out += String(s.temp, 2);
    else                  out += F("NaN");
    out += ',';
    out += String(s.out, 1);
    out += ',';
    out += String((uint16_t)s.heat);
    out += '\n';
  }
}

// ────────────────────────────────────────────────────────────────────────────────
// PROFIL – implementacja applyProfileFromJson (web_config.cpp)
// ────────────────────────────────────────────────────────────────────────────────
bool applyProfileFromJson(const JsonArray& arr) {
  uint8_t n = 0;
  for (JsonObject s : arr) {
    if (n >= 8) break;
    double   t    = s["targetC"] | NAN;
    uint32_t hold = s["holdSec"] | 0UL;
    if (!isfinite(t)) continue;

    PROFILE[n].targetC = t;
    PROFILE[n].holdSec = hold;
    n++;
  }

  PROFILE_LEN    = n;
  PROFILE_ACTIVE = 0;
  g_profileStepStartMs = millis();
  PROFILE_ELAPSED_SEC = 0;
  PROFILE_REMAIN_SEC  = (PROFILE_LEN>0) ? PROFILE[0].holdSec : 0;

  if (PROFILE_LEN > 0) {
    CFG.mode = MODE_PROFILE;   // po wczytaniu profilu przełącz na tryb PROFIL
    PID_SET  = PROFILE[0].targetC;
    Serial.print(F("[CTRL] Profile loaded, steps="));
    Serial.println(PROFILE_LEN);
    return true;
  } else {
    CFG.mode = MODE_DYNAMIC;
    Serial.println(F("[CTRL] Profile cleared (0 steps)"));
    return false;
  }
}

// ────────────────────────────────────────────────────────────────────────────────
// PROFIL – ręczne przełączanie kroków (przyciski / API)
// ────────────────────────────────────────────────────────────────────────────────
void profileNextStep() {
  if (PROFILE_LEN == 0) return;
  if (PROFILE_ACTIVE + 1 < PROFILE_LEN) {
    profileGotoStep(PROFILE_ACTIVE + 1);
  }
}

void profilePrevStep() {
  if (PROFILE_LEN == 0) return;
  if (PROFILE_ACTIVE > 0) {
    profileGotoStep(PROFILE_ACTIVE - 1);
  }
}
