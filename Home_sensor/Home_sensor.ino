#include <Wire.h>
#include <DHT.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ESP32 pins
#define DHT_PIN 27
#define DHT_TYPE DHT22
#define OLED_SDA 21
#define OLED_SCL 22

// OLED settings
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define OLED_ADDRESS 0x3C

DHT dht(DHT_PIN, DHT_TYPE);
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

void setup() {
  Serial.begin(115200);

  Wire.begin(OLED_SDA, OLED_SCL);
  dht.begin();

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
    Serial.println("OLED nije pronaden. Provjeri spojeve i adresu.");
    while (true) {
      delay(1000);
    }
  }

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("Pokretanje...");
  display.display();

  delay(2000);
}

void loop() {
  //use dht functions to read humidity and temperature
  float vlaga = dht.readHumidity();
  float temperatura = dht.readTemperature();

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  if (isnan(vlaga) || isnan(temperatura)) {
    Serial.println("Greska pri citanju DHT22.");

    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println("Greska senzora!");
    display.println("Provjeri DHT22.");
  } else {
    Serial.print("Temperatura: ");
    Serial.print(temperatura, 1);
    Serial.print(" C | Vlaga: ");
    Serial.print(vlaga, 1);
    Serial.println(" %");

    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println("Temperatura:");

    display.setTextSize(2);
    display.print(temperatura, 1);
    display.println(" C");

    display.setTextSize(1);
    display.println();
    display.println("Vlaga:");

    display.setTextSize(2);
    display.print(vlaga, 1);
    display.println(" %");
  }

  display.display();
  delay(3000);
}