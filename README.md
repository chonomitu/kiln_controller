# KilnControllerESP8266

ESP8266 + MAX31855 (termopara K) + SSR (okno czasowe) + OLED SSD1306 + WebUI (LittleFS) + opcjonalny MQTT.

Sterownik pieca z prostym WebUI (`/`), możliwością pracy w trybie dynamicznym lub z profilem temperatury oraz OTA.

## Szybki start

1. Zainstaluj biblioteki z `library_deps.txt`.
2. Skompiluj i wgraj firmware (PlatformIO / Arduino IDE).
3. Wgraj pliki do LittleFS (Tools → ESP8266 LittleFS Data Upload).
4. Połącz się z siecią WiFi sterownika, np. `KilnCtrl-XXXX`.
5. Otwórz w przeglądarce: `http://192.168.4.1/`.

## Sprzęt (domyślna konfiguracja)

- **MCU:** ESP8266 (NodeMCU / Wemos)
- **Termopara:** MAX31855 (moduł SPI, typ K)
- **SSR:** sterowanie grzałką w oknie czasowym (OUT [%])
- **Wyświetlacz:** OLED SSD1306 128×64 (I²C)

Domyślne piny:

- OLED: `SDA = D5`, `SCL = D6`
- MAX31855: soft-SPI (domyślnie `MOSI = D7`, `MISO = D1`, `SCK = D4`, `CS = D8`) – patrz `config.h`

## Web UI i certyfikat

Na stronie głównej (`/`) wyświetlany jest panel sterowania (START/STOP, tryb, wykres) oraz baner informujący,
że **ten egzemplarz sterownika posiada certyfikat blockchain (NFT)**:

- Zrobione przez: `kyrson` dla `biroskop` (18.X 2025)
- Szczegóły certyfikatu: [kyrson.pl](https://kyrson.pl)

Kod frontendu (HTML/CSS/JS) jest trzymany w `src/webui/*.h` oraz w katalogu `data/` (LittleFS).
