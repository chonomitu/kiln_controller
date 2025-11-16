
# KilnControllerESP8266
ESP8266 + MAX31865 (PT100) + SSR (okno czasowe) + OLED SSD1306 + WebUI (LittleFS) + opcjonalny MQTT.

## Szybki start
1. Zainstaluj biblioteki z `library_deps.txt`.
2. Wgraj firmware.
3. Wgraj pliki do LittleFS (Tools → ESP8266 LittleFS Data Upload).
4. Połącz się do `KilnCtrl-AP`
5. Otwórz `http://192.168.4.1/`.

Domyślne OLED: SDA=D5, SCL=D6. MAX31865: soft‑SPI (MOSI=D7, MISO=D1, SCK=D4, CS=D8).
