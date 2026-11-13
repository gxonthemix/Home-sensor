#include <Wire.h>
#include <DHT.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <math.h>
#include <WiFi.h>
#include <WebServer.h>
#include "wifi_secrets.h"

// Pins and I2C addresses: each OLED has its own bus.
#define DHT_PIN 27
#define DHT_TYPE DHT22
#define OLED_SDA 21
#define OLED_SCL 22
#define SECOND_OLED_SDA 18
#define SECOND_OLED_SCL 19
#define OLED_ADDRESS 0x3C
#define SECOND_OLED_ADDRESS 0x3C
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1

// Independent intervals keep sensor reads, page changes and animation responsive.
const unsigned long SENSOR_INTERVAL = 3000;
const unsigned long PAGE_INTERVAL = 15000;
const unsigned long FRAME_INTERVAL = 200;

DHT dht(DHT_PIN, DHT_TYPE);
WebServer server(80);
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
Adafruit_SSD1306 dewDisplay(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire1, OLED_RESET);

bool secondDisplayReady = false;
bool readingValid = false;
bool hasTemperatureHistory = false;
float temperature = NAN;
float humidity = NAN;
float dewPoint = NAN;
float minTemperature = NAN;
float maxTemperature = NAN;
unsigned long lastSensorRead = 0;
unsigned long lastPageChange = 0;
unsigned long lastFrame = 0;
bool showHistory = false;
uint8_t animationStep = 0;
unsigned long lastWifiAttempt = 0;

// The page runs locally on the ESP32 and requests fresh readings every 3 seconds.
const char WEB_PAGE[] PROGMEM = R"html(
<!doctype html><html lang="hr"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Home Sensor</title><style>
body{font-family:system-ui,sans-serif;background:#101d25;color:#effaf8;margin:0;padding:32px 20px}
main{max-width:680px;margin:auto}h1{font-size:2.3rem;margin-bottom:4px}p{color:#a6c5c2}
.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(180px,1fr));gap:14px;margin-top:32px}
.card{background:#1c3039;border:1px solid #35515a;border-radius:18px;padding:22px}
.label{color:#a6c5c2}.value{font-size:2rem;font-weight:650;margin-top:12px;color:#9eead6}
</style></head><body><main><p>KUĆNA METEOROLOŠKA STANICA</p><h1>Home Sensor</h1>
<p id="status" role="status">Povezivanje sa senzorom...</p><div class="grid">
<div class="card"><div class="label">Temperatura</div><div class="value" id="temperature">—</div></div>
<div class="card"><div class="label">Vlažnost</div><div class="value" id="humidity">—</div></div>
<div class="card"><div class="label">Rosište</div><div class="value" id="dewPoint">—</div></div>
</div></main><script>
async function refresh(){try{const response=await fetch('/api/readings',{cache:'no-store'});
if(!response.ok)throw Error();const data=await response.json();
for(const [key,unit] of [['temperature',' °C'],['humidity',' %'],['dewPoint',' °C']])
document.getElementById(key).textContent=data[key]===null?'—':data[key].toFixed(1)+unit;
document.getElementById('status').textContent=data.valid?'Ažurirano: '+new Date().toLocaleTimeString('hr-HR'):'Očitavanje senzora nije dostupno.';
}catch(e){document.getElementById('status').textContent='Veza sa senzorom je prekinuta.';
for(const id of ['temperature','humidity','dewPoint'])document.getElementById(id).textContent='—';}}
refresh();setInterval(refresh,3000);
</script></body></html>)html";

String jsonNumber(float value) {
  return isfinite(value) ? String(value, 1) : "null";
}

void handleReadings() {
  String json = "{\"valid\":" + String(readingValid ? "true" : "false");
  json += ",\"temperature\":" + jsonNumber(readingValid ? temperature : NAN);
  json += ",\"humidity\":" + jsonNumber(readingValid ? humidity : NAN);
  json += ",\"dewPoint\":" + jsonNumber(readingValid ? dewPoint : NAN) + "}";
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", json);
}

// Check for an I2C response before initializing a display.
bool i2cDeviceFound(TwoWire &bus, uint8_t address) {
  bus.beginTransmission(address);
  return bus.endTransmission() == 0;
}

// Estimate dew point in Celsius with the Magnus formula for liquid water.
float calculateDewPoint(float airTemperature, float relativeHumidity) {
  const float a = 17.62;
  const float b = 243.12;
  float gamma = log(relativeHumidity / 100.0f) +
                (a * airTemperature) / (b + airTemperature);
  return (b * gamma) / (a - gamma);
}

// Center a single line of text within the specified horizontal region.
void centeredText(Adafruit_SSD1306 &screen, const char *text, int centerX,
                  int y, uint8_t size) {
  int16_t x1, y1;
  uint16_t width, height;
  screen.setTextSize(size);
  screen.getTextBounds(text, 0, 0, &x1, &y1, &width, &height);
  screen.setCursor(centerX - width / 2 - x1, y);
  screen.print(text);
}

// Draw a thin circular gauge and a slowly moving highlight around the edge.
void drawTemperatureScreen() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextWrap(false);
  const int cx = 64, cy = 32, radius = 30;
  display.drawCircle(cx, cy, radius, SSD1306_WHITE);
  for (int i = 0; i < 9; ++i) {
    float angle = (animationStep * 4 + i) * PI / 90.0f;
    int x = cx + (int)roundf((radius - 2) * cosf(angle));
    int y = cy + (int)roundf((radius - 2) * sinf(angle));
    display.drawPixel(x, y, SSD1306_WHITE);
  }

  centeredText(display, "TEMP", cx, 17, 1);
  if (readingValid) {
    char value[16];
    snprintf(value, sizeof(value), "%.1f", temperature);
    // Long negative or three-digit values use a smaller font to fit the circle.
    centeredText(display, value, cx, 28, strlen(value) <= 4 ? 2 : 1);
    centeredText(display, "C", cx, 46, 1);
  } else {
    centeredText(display, "--.-", cx, 28, 2);
    centeredText(display, "NO DATA", cx, 46, 1);
  }
  display.display();
}

// Show humidity and dew point, or the minimum and maximum since startup.
void drawDetailsScreen() {
  if (!secondDisplayReady) return;
  dewDisplay.clearDisplay();
  dewDisplay.setTextColor(SSD1306_WHITE);
  dewDisplay.setTextWrap(false);
  centeredText(dewDisplay, showHistory ? "TEMPERATURE" : "AIR CONDITIONS", 64, 0, 1);
  dewDisplay.drawFastHLine(8, 11, 112, SSD1306_WHITE);
  dewDisplay.drawFastVLine(63, 17, 31, SSD1306_WHITE);
  char left[16], right[16];
  if (showHistory) {
    centeredText(dewDisplay, "MIN C", 31, 18, 1);
    centeredText(dewDisplay, "MAX C", 96, 18, 1);
    if (hasTemperatureHistory) {
      snprintf(left, sizeof(left), "%.1f", minTemperature);
      snprintf(right, sizeof(right), "%.1f", maxTemperature);
    } else {
      snprintf(left, sizeof(left), "--.-");
      snprintf(right, sizeof(right), "--.-");
    }
  } else {
    centeredText(dewDisplay, "HUM %", 31, 18, 1);
    centeredText(dewDisplay, "DEW C", 96, 18, 1);
    if (readingValid) snprintf(left, sizeof(left), "%.1f", humidity);
    else snprintf(left, sizeof(left), "--.-");
    if (readingValid && isfinite(dewPoint)) snprintf(right, sizeof(right), "%.1f", dewPoint);
    else snprintf(right, sizeof(right), "--.-");
  }
  centeredText(dewDisplay, left, 31, 31, strlen(left) <= 4 ? 2 : 1);
  centeredText(dewDisplay, right, 96, 31, strlen(right) <= 4 ? 2 : 1);
  centeredText(dewDisplay, showHistory ? "SINCE START" : "HUMIDITY / DEW POINT", 64, 56, 1);
  dewDisplay.display();
}

// Read every three seconds; failed readings never replace recorded min/max.
void readSensor() {
  float newHumidity = dht.readHumidity();
  float newTemperature = dht.readTemperature();
  readingValid = isfinite(newHumidity) && isfinite(newTemperature) &&
                 newHumidity >= 0.0f && newHumidity <= 100.0f;
  if (!readingValid) {
    dewPoint = NAN;
    Serial.println("DHT22 read failed or returned an invalid value.");
    return;
  }

  humidity = newHumidity;
  temperature = newTemperature;
  dewPoint = humidity > 0.0f ? calculateDewPoint(temperature, humidity) : NAN;
  if (!hasTemperatureHistory) {
    minTemperature = maxTemperature = temperature;
    hasTemperatureHistory = true;
  } else {
    if (temperature < minTemperature) minTemperature = temperature;
    if (temperature > maxTemperature) maxTemperature = temperature;
  }
  Serial.printf("Temperature: %.1f C | Humidity: %.1f %%", temperature, humidity);
  if (isfinite(dewPoint)) Serial.printf(" | Dew point: %.1f C\n", dewPoint);
  else Serial.println(" | Dew point unavailable");
}

void setup() {
  // Initialize both buses explicitly so the display library preserves their pins.
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  server.on("/", []() { server.send_P(200, "text/html; charset=utf-8", WEB_PAGE); });
  server.on("/api/readings", handleReadings);
  server.begin();
  Wire.begin(OLED_SDA, OLED_SCL);
  bool secondBusReady = Wire1.begin(SECOND_OLED_SDA, SECOND_OLED_SCL);
  dht.begin();

  if (!i2cDeviceFound(Wire, OLED_ADDRESS) ||
      !display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS, true, false)) {
    Serial.println("Main OLED unavailable. Check GPIO21/22, power and address.");
    while (true) delay(1000);
  }
  if (secondBusReady && i2cDeviceFound(Wire1, SECOND_OLED_ADDRESS)) {
    secondDisplayReady = dewDisplay.begin(SSD1306_SWITCHCAPVCC, SECOND_OLED_ADDRESS, true, false);
  }
  if (!secondDisplayReady) Serial.println("Second OLED unavailable. Check GPIO18/19, power and address.");

  // Display placeholders until the first sensor read after its startup interval.
  drawTemperatureScreen();
  drawDetailsScreen();
}

void loop() {
  unsigned long now = millis();
  server.handleClient();
  static bool wifiWasConnected = false;
  bool wifiConnected = WiFi.status() == WL_CONNECTED;
  if (wifiConnected && !wifiWasConnected) {
    Serial.print("Home Sensor web page: http://");
    Serial.println(WiFi.localIP());
  }
  if (!wifiConnected && now - lastWifiAttempt >= 30000) {
    lastWifiAttempt = now;
    WiFi.reconnect();
  }
  wifiWasConnected = wifiConnected;
  bool detailsNeedUpdate = false;
  if (now - lastSensorRead >= SENSOR_INTERVAL) {
    lastSensorRead = now;
    readSensor();
    detailsNeedUpdate = true;
  }
  if (now - lastPageChange >= PAGE_INTERVAL) {
    lastPageChange = now;
    showHistory = !showHistory;
    detailsNeedUpdate = true;
  }
  if (now - lastFrame >= FRAME_INTERVAL) {
    lastFrame = now;
    animationStep = (animationStep + 1) % 45;
    drawTemperatureScreen();
  }
  if (detailsNeedUpdate) drawDetailsScreen();
}
