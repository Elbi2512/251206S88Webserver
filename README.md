# 251206 S88-webserver

## Doel

ESP32-S2-webserver die S88-statusframes via UART ontvangt en actuele ingangstoestanden en tellers in een browser toont.

## Werking volgens `src/main.cpp`

- Levert een ingebedde webpagina met 32 modules van elk 16 poorten.
- Ontvangt seriële bytes op UART1 (`SerialUART1`) met 115200 baud.
- Herkent non-blocking frames van de vorm `0x55 0xAA`, payloadlengte 64, 64 statusbytes en een CRC16 van twee bytes (high byte eerst).
- Leest de payload MSB-first en publiceert gewijzigde poortstatussen als Server-Sent Events (SSE).
- Stuurt bij een nieuwe browserverbinding een status-snapshot per module.
- Houdt per poort een teller bij die oploopt wanneer een ingang actief wordt; de bootknop wist de tellers en publiceert een nieuwe snapshot.

## Hardware en bouwen

- PlatformIO-target: `esp32-s2-mini` (`esp32-s2-saola-1`)
- Seriële verbinding met de bridge: RX GPIO 16, TX GPIO 17, 115200 baud
- OLED I2C: SDA GPIO 12, SCL GPIO 11
- Stel `YOUR_WIFI_SSID` en `YOUR_WIFI_PASSWORD` in `src/main.cpp` in voordat je verbinding met wifi verwacht.

Bouwen vanuit deze projectmap:

```sh
pio run
```

## Koppeling en configuratie

Deze webserver verwacht het volledige frame dat `251111HostESPNowWEB` via UART verstuurt. Gebruik dezelfde baudrate, payloadlengte, CRC16-variant en bytevolgorde aan beide kanten. De wifi-credentials zijn voor publicatie vervangen door placeholders.
