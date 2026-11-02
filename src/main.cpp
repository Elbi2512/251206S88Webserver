// 240505: Ontvangen van 1 client (S88:0). Nog geen versturen van wisselcommando's.
//

#include <Arduino.h>
#include <spi.h>
#include <Wire.h>        // Only needed for Arduino 1.6.5 and earlier
#include "SSD1306Wire.h" // legacy: #include "SSD1306.h"
#define CONFIG_ARDUINO_USB_CDC_ON_BOOT 1

#if defined(ARDUINO_ARCH_ESP8266)
#include <ESP8266WiFi.h>
#elif defined(ARDUINO_ARCH_ESP32)
#include <WiFi.h>

#include <ESPmDNS.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#endif

HardwareSerial SerialUART1(1); // gebruik UART1

const int RX2_PIN = 16;
const int TX2_PIN = 17;
#define SDA 12
#define SCL 11
#define SCREEN_I2C_ADDRESS 0x3C
#define SCREEN_HEIGHT 64
#define SCREEN_WIDTH 128
#define SCREEN_VERTICAL_FLIP true
SSD1306Wire display(0x3c, SDA, SCL);
// ==================================================
// 1) Zet hieronder uit als je klaar bent met debuggen
#define DEBUG_PRINT
// ==================================================

#if defined DEBUG_PRINT
#define baud 115200
#define debug_begin(baud) Serial.begin(baud)
#define debug(x) Serial.print(x)
#define debugln(x) Serial.println(x)
#else
#define debug_begin(baud)
#define debug(x)
#define debugln(x)
#endif

#define debugf(fmt, ...)               \
  {                                    \
    Serial.print(__FILE__);            \
    Serial.print(' : ');               \
    Serial.print(__LINE__);            \
    Serial.print(': ');                \
    Serial.printf(fmt, ##__VA_ARGS__); \
  }
const char *ssid = "YOUR_WIFI_SSID";
const char *password = "YOUR_WIFI_PASSWORD";

void func(uint16_t module, uint16_t port, bool occupied);
void reset();

uint16_t S88BitTouch[512];
uint8_t lastPayload[64] = {};
portMUX_TYPE stateMux = portMUX_INITIALIZER_UNLOCKED;

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="nl">
<head>
  <meta charset="utf-8" />
  <title>ESP S88 Monitor</title>
  <meta name="viewport" content="width=device-width, initial-scale=1" />
  <style>
    :root {
      --bg: #0f172a; --panel: #111827; --panel-2: #1f2937; --text: #e5e7eb; --muted: #94a3b8;
      --accent: #22d3ee; --ok: #34d399; --bad: #f87171;
    }
    html, body { margin: 0; background: radial-gradient(1200px circle at 20% 0%, #0b1220, var(--bg)); color: var(--text); font-family: system-ui, sans-serif; }
    .container { max-width: 1000px; margin: 24px auto; padding: 0 16px; }
    header { display: flex; justify-content: space-between; align-items: baseline; border-bottom: 1px solid #1e293b; margin-bottom: 16px; padding-bottom: 12px; }
    header h1 { margin: 0; font-size: 24px; }
    .badge { display: inline-flex; align-items: center; gap: 6px; padding: 4px 10px; background: var(--panel-2); border: 1px solid #1e293b; border-radius: 999px; font-size: 14px; color: var(--muted); }
    .dot { width: 8px; height: 8px; border-radius: 999px; } .dot.connected { background: var(--accent); } .dot.disconnected { background: var(--bad); }
    .module-bar { display: grid; grid-template-columns: repeat(4, 1fr); gap: 8px; margin: 12px 0 16px; }
    .module-pill { padding: 10px 12px; background: var(--panel-2); border: 1px solid #1e293b; border-radius: 10px; display: flex; align-items: center; justify-content: space-between; font-variant-numeric: tabular-nums; }
    .module-pill .title { color: var(--muted); font-size: 13px; } .module-pill .value { font-weight: 600; }
    .module-block { margin-bottom: 12px; border: 1px solid #1e293b; border-radius: 8px; background: var(--panel); overflow: hidden; }
    .module-header { background: var(--panel-2); padding: 8px 12px; font-weight: 600; color: var(--text); display: flex; justify-content: space-between; align-items: center; cursor: pointer; }
    .module-header:hover { background: #374151; }
    .module-content { display: block; padding: 12px; }
    
   .grid {
  display: flex;
  flex-wrap: wrap;
  gap: 16px; /* ruimte tussen blokken */
}

.block {
  display: grid;
  grid-template-columns: repeat(4, 1fr);
  gap: 8px; /* ruimte tussen cellen binnen een blok */
  flex: 1 1 calc(25% - 16px); /* 4 blokken naast elkaar */
}

/* Responsief: op smalle schermen blokken onder elkaar */
@media (max-width: 600px) {
  .block {
    flex: 1 1 100%; /* blok neemt volledige breedte */
  }
}

    .cell { background: var(--panel-2); border: 1px solid #374151; border-radius: 6px; padding: 6px; text-align: center; font-size: 12px; color: #cbd5e1; transition: background 0.3s; white-space: pre-line; }
    .cell.status-active { background: #065f46; color: #d1fae5; }
    .cell.status-idle { background: #7f1d1d; color: #fee2e2; }
  </style>
</head>
<body>
  <div class="container">
    <header>
      <h1>ESP IOT S88 Monitor</h1>
      <div class="badge" id="connBadge">
        <span class="dot connected" id="connDot"></span>
        <span id="connText">Verbonden</span>
      </div>
    </header>

    <div class="module-bar" id="moduleBar"></div>

    <div id="modules"></div>
  </div>

<script>
  const NUM_MODULES = 32;
  const PORTS_PER_MODULE = 16;
  const modulesContainer = document.getElementById('modules');
  const moduleBar = document.getElementById('moduleBar');
  const portCells = [];

  // Bouw modules en poorten
  for (let m = 0; m < NUM_MODULES; m++) {
  const moduleBlock = document.createElement('div');
  moduleBlock.className = 'module-block';

  const header = document.createElement('div');
  header.className = 'module-header';
  header.innerHTML = `<span>Module: ${m+1}</span>`;
  moduleBlock.appendChild(header);

  const content = document.createElement('div');
  content.className = 'module-content';
 // content.style.display = 'block';
  moduleBlock.appendChild(content);

  const grid = document.createElement('div');
  grid.className = 'grid';
  content.appendChild(grid);

  header.addEventListener('click', () => {
    content.style.display = content.style.display === 'block' ? 'none' : 'block';
  });

  // 16 poorten in 4 blokken van 4
  for (let b = 0; b < 4; b++) {
    const block = document.createElement('div');
    block.className = 'block';
    for (let p = 0; p < 4; p++) {
      const idx = m * PORTS_PER_MODULE + b * 4 + p;
      const cell = document.createElement('div');
      cell.className = 'cell status-idle';
      cell.dataset.index = idx;
      cell.textContent = `00:00\n0`;
      block.appendChild(cell);
      portCells[idx] = cell;
    }
    grid.appendChild(block);
  }

  modulesContainer.appendChild(moduleBlock);
}
  // Bovenbalk bijwerken
  function updateModuleBar(module, port, timestamp) {
    moduleBar.innerHTML = `
      <div class="module-pill"><span class="title">Module</span><span class="value">${module}</span></div>
      <div class="module-pill"><span class="title">Poort</span><span class="value">${port}</span></div>
      <div class="module-pill"><span class="title">Index</span><span class="value">${(module - 1) * PORTS_PER_MODULE + port}</span></div>
      <div class="module-pill"><span class="title">Laatste update</span><span class="value">${timestamp}</span></div>
    `;
  }
//  updateModuleBar('-', '-', '-');

  // Verbinding badge
  const connDot = document.getElementById('connDot');
  const connText = document.getElementById('connText');
  function setConnected(state) {
    if (state) { connDot.classList.add('connected'); connDot.classList.remove('disconnected'); connText.textContent = 'Verbonden'; }
    else { connDot.classList.add('disconnected'); connDot.classList.remove('connected'); connText.textContent = 'Verbinding verbroken'; }
  }

  setConnected(false);

  // SSE
  if (!!window.EventSource) {
    const source = new EventSource('/events');
    source.addEventListener('open', () => setConnected(true), false);
    source.addEventListener('error', e => { if (e.target.readyState !== EventSource.OPEN) setConnected(false); }, false);
    source.addEventListener('s88', e => setS88Value(e.data), false);
    source.addEventListener('snapshot', e => setS88Snapshot(e.data), false);
  } else {
    setConnected(false);
  }

  function updateCell(idx, status, count) {
    if (!Number.isInteger(idx) || idx < 0 || idx >= portCells.length) return;
    if ((status !== 0 && status !== 1) || !Number.isInteger(count) || count < 0) return;
    const module = Math.floor(idx / PORTS_PER_MODULE) + 1;
    const port = (idx % PORTS_PER_MODULE) + 1; // consequent 1..16
    const ts = new Date().toLocaleTimeString([], { hour: '2-digit', minute: '2-digit', second: '2-digit' });
    const cell = portCells[idx];
    const active = status === 1;
    cell.classList.toggle('status-active', active);
    cell.classList.toggle('status-idle', !active);
    const now = new Date();
    const minutes = String(now.getMinutes()).padStart(2, '0');
    const seconds = String(now.getSeconds()).padStart(2, '0');
    cell.textContent = `${minutes}:${seconds}\n${count}`;
    return { module, port, ts };
  }

  function setS88Value(s88) {
    try {
      const obj = JSON.parse(s88);
      const updated = updateCell(Number(obj.value), Number(obj.status), Number(obj.aantal));
      if (updated) updateModuleBar(updated.module, updated.port, updated.ts);
    } catch (error) {
      console.error('Ongeldig S88-event:', error);
    }
  }

  function setS88Snapshot(snapshot) {
    try {
      const obj = JSON.parse(snapshot);
      const start = Number(obj.start);
      if (!Number.isInteger(start) || !Array.isArray(obj.items)) return;
      obj.items.forEach((item, offset) => {
        if (Array.isArray(item) && item.length === 2) {
          updateCell(start + offset, Number(item[0]), Number(item[1]));
        }
      });
    } catch (error) {
      console.error('Ongeldige S88-snapshot:', error);
    }
  }
</script>
</body>
</html>
)rawliteral";
// Web server running on port 80
AsyncWebServer server(80);
// Async Events
AsyncEventSource events("/events");

void errorMsg(String error, bool restart = true)
{
  debugln(error);
  if (restart)
  {
    debugln("Rebooting now...");
    delay(2000);
    ESP.restart();
    delay(2000);
  }
}

bool formatSnapshotModule(uint8_t module, char *snapshot, size_t snapshotSize)
{
  uint16_t counts[16];
  uint8_t moduleBytes[2];
  const uint16_t moduleStart = static_cast<uint16_t>(module) * 16;
  portENTER_CRITICAL(&stateMux);
  memcpy(counts, &S88BitTouch[moduleStart], sizeof(counts));
  moduleBytes[0] = lastPayload[module * 2];
  moduleBytes[1] = lastPayload[module * 2 + 1];
  portEXIT_CRITICAL(&stateMux);

  int used = snprintf(snapshot, snapshotSize, "{\"start\":%u,\"items\":[", moduleStart);
  for (uint8_t displayPort = 0; displayPort < 16 && used > 0 && static_cast<size_t>(used) < snapshotSize; displayPort++)
  {
    const uint8_t wirePort = 15 - displayPort;
    const uint8_t byteValue = moduleBytes[wirePort / 8];
    const bool occupied = (byteValue >> (7 - (wirePort % 8))) & 0x01;
    const uint8_t status = !occupied;
    used += snprintf(snapshot + used, snapshotSize - used, "%s[%u,%u]",
                     displayPort == 0 ? "" : ",", status, counts[displayPort]);
  }
  if (used <= 0 || static_cast<size_t>(used) >= snapshotSize)
  {
    return false;
  }
  const int suffixLength = snprintf(snapshot + used, snapshotSize - used, "]}");
  return suffixLength > 0 && static_cast<size_t>(used + suffixLength) < snapshotSize;
}

void sendSnapshot(AsyncEventSourceClient *client)
{
  char snapshot[256];
  for (uint8_t module = 0; module < 32; module++)
  {
    if (formatSnapshotModule(module, snapshot, sizeof(snapshot)))
    {
      client->send(snapshot, "snapshot", millis());
    }
    else
    {
      debugln("Failed to format S88 snapshot");
      return;
    }
  }
}

void broadcastSnapshot()
{
  char snapshot[256];
  for (uint8_t module = 0; module < 32; module++)
  {
    if (formatSnapshotModule(module, snapshot, sizeof(snapshot)))
    {
      events.send(snapshot, "snapshot", millis());
    }
    else
    {
      debugln("Failed to format S88 snapshot");
      return;
    }
  }
}

void configureEvents()
{
  events.onConnect([](AsyncEventSourceClient *client)
                   {
    if(client->lastId()){
      char tekst[80];
      sprintf(tekst, "Client connections. Id: %u\n", client->lastId());
      debug(tekst);
    }
    client->send("hello from ESP32", nullptr, millis(), 1000);
    sendSnapshot(client); });
  server.addHandler(&events);
}

void func(uint16_t mod, uint16_t i, bool bBezet)
{
  char tekst[80];
  // mod is de module waar het om gaat;
  // bBezet is de  aan uit waarde (telt delta's)
  // i is de bit positie binnen het module (1-16)
  // BitTouch[j] is de tabel met hoevaak deze is geraakt (klapper detectie)
  if (mod < 32 && i < 16)
  {
    const uint16_t j = mod * 16 + (15 - i);
    uint16_t aantal;
    portENTER_CRITICAL(&stateMux);
    if (bBezet)
    {
      if (S88BitTouch[j] < UINT16_MAX)
      {
        S88BitTouch[j]++;
      }
    }
    aantal = S88BitTouch[j];
    portEXIT_CRITICAL(&stateMux);
    snprintf(tekst, sizeof(tekst), "{\"value\": %u, \"status\": %u, \"aantal\": %u}", j, !bBezet, aantal);
    debugln(tekst);
    events.send(tekst, "s88", millis());
  }
  else
  {
    debugln("Foutieve S88 adressering");
    debugln(mod);
    debugln(i);
  }
}

constexpr size_t SERIAL_PAYLOAD_SIZE = 64;

uint16_t crc16(const uint8_t *data, size_t length)
{
  uint16_t crc = 0xFFFF;
  for (size_t i = 0; i < length; i++)
  {
    crc ^= data[i];
    for (uint8_t bit = 0; bit < 8; bit++)
    {
      crc = (crc & 1U) ? (crc >> 1) ^ 0xA001U : crc >> 1;
    }
  }
  return crc;
}

void processS88Payload(const uint8_t *payload)
{
  static uint8_t previousPayload[SERIAL_PAYLOAD_SIZE] = {};
  for (size_t byteIndex = 0; byteIndex < SERIAL_PAYLOAD_SIZE; byteIndex++)
  {
    if (payload[byteIndex] == previousPayload[byteIndex])
    {
      continue;
    }
    for (uint8_t bit = 0; bit < 8; bit++)
    {
      const bool previousBit = (previousPayload[byteIndex] >> (7 - bit)) & 0x01;
      const bool currentBit = (payload[byteIndex] >> (7 - bit)) & 0x01;
      if (previousBit != currentBit)
      {
        const uint16_t bitPosition = byteIndex * 8 + bit;
        func(bitPosition / 16, bitPosition % 16, currentBit);
      }
    }
    previousPayload[byteIndex] = payload[byteIndex];
  }
  portENTER_CRITICAL(&stateMux);
  memcpy(lastPayload, payload, SERIAL_PAYLOAD_SIZE);
  portEXIT_CRITICAL(&stateMux);
}

void processSerialByte(uint8_t value)
{
  enum class FrameState : uint8_t
  {
    WaitForSync1,
    WaitForSync2,
    WaitForLength,
    ReadPayload,
    ReadCrcHigh,
    ReadCrcLow
  };
  static FrameState state = FrameState::WaitForSync1;
  static uint8_t payload[SERIAL_PAYLOAD_SIZE];
  static size_t payloadIndex = 0;
  static uint16_t receivedCrc = 0;

  switch (state)
  {
  case FrameState::WaitForSync1:
    if (value == 0x55)
    {
      state = FrameState::WaitForSync2;
    }
    break;
  case FrameState::WaitForSync2:
    if (value == 0xAA)
    {
      state = FrameState::WaitForLength;
    }
    else
    {
      state = value == 0x55 ? FrameState::WaitForSync2 : FrameState::WaitForSync1;
    }
    break;
  case FrameState::WaitForLength:
    if (value == SERIAL_PAYLOAD_SIZE)
    {
      payloadIndex = 0;
      state = FrameState::ReadPayload;
    }
    else
    {
      state = value == 0x55 ? FrameState::WaitForSync2 : FrameState::WaitForSync1;
    }
    break;
  case FrameState::ReadPayload:
    payload[payloadIndex++] = value;
    if (payloadIndex == SERIAL_PAYLOAD_SIZE)
    {
      state = FrameState::ReadCrcHigh;
    }
    break;
  case FrameState::ReadCrcHigh:
    receivedCrc = static_cast<uint16_t>(value) << 8;
    state = FrameState::ReadCrcLow;
    break;
  case FrameState::ReadCrcLow:
    receivedCrc |= value;
    if (receivedCrc == crc16(payload, SERIAL_PAYLOAD_SIZE))
    {
      processS88Payload(payload);
    }
    else
    {
      debugln("Serial CRC error");
    }
    state = FrameState::WaitForSync1;
    break;
  }
}

void reset()
{
  portENTER_CRITICAL(&stateMux);
  for (uint16_t i = 0; i < 512; i++)
  {
    S88BitTouch[i] = 0;
  }
  portEXIT_CRITICAL(&stateMux);
}

void setup()
{
  char tekst[80];
  debug_begin(115200);
  debug("\r\nReboot\r\n");
  display.init();
  display.flipScreenVertically();
  display.clear(); // Note the format for setting a serial port is as follows: Serial2.begin(baud-rate, protocol, RX pin, TX pin);
  display.setFont(ArialMT_Plain_10);
  display.println("BOOTING... ");
  display.display();
  SerialUART1.begin(115200, SERIAL_8N1, RX2_PIN, TX2_PIN);
  pinMode(0, INPUT_PULLUP);
  WiFi.mode(WIFI_MODE_STA);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    debug(".");
    delay(500);
    // we can even make the ESP32 to sleep
  }
  debugln(WiFi.macAddress());
  WiFi.setAutoReconnect(true);
  WiFi.persistent(true);
  debugln();
  debug("Connected. IP: ");
  debugln(WiFi.localIP());
  display.clear();
  const String ipAddress = WiFi.localIP().toString();
  snprintf(tekst, sizeof(tekst), "Booted: %s", ipAddress.c_str());
  display.drawString(1, 0, tekst);
  display.display();
  debug("Kanaal: ");
  debugln(WiFi.channel());
  int channel = WiFi.channel();

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(200, "text/html", index_html); });
  configureEvents();
  reset();
  server.begin();

  debugln("Init Done");
}

void loop()
{
  while (SerialUART1.available() > 0)
  {
    processSerialByte(static_cast<uint8_t>(SerialUART1.read()));
  }

  static bool previousButtonReading = HIGH;
  static bool stableButtonState = HIGH;
  static unsigned long lastDebounceTime = 0;
  const bool buttonReading = digitalRead(0);
  if (buttonReading != previousButtonReading)
  {
    lastDebounceTime = millis();
    previousButtonReading = buttonReading;
  }
  if (millis() - lastDebounceTime >= 30 && buttonReading != stableButtonState)
  {
    stableButtonState = buttonReading;
    if (stableButtonState == LOW)
    {
      debugln("Reset Serverwaarden");
      reset();
      broadcastSnapshot();
    }
  }
  yield();
}