#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <BH1750.h>
#include <DHT.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <math.h>

// Pin Definitions
#define DHTPIN 33
#define MQ135PIN 32
#define DHTTYPE DHT22
#define RL 10.0  // Load resistance in kiloohms
#define AIR_R0 3.7 // R0 value in clean air

// Wi-Fi Credentials
const char* ssid = "SYIFA";
const char* password = "24041981";

// API Endpoint
const char* apiEndpoint = "https://sindanganomfarm.com/api/iot-sensors";

// Farm ID
const char* farm_id = "9e074c61-46e9-4a6e-8f56-a4563b671387";

// Sensor Initialization
DHT dht(DHTPIN, DHTTYPE);
BH1750 lightMeter;
LiquidCrystal_I2C lcd(0x27, 16, 2); // I2C LCD Address: 0x27 or 0x3F

// Function to convert RS/R0 ratio to ppm for ammonia
float getPPM(float ratio) {
  float m = -0.47; // Slope from datasheet curve
  float b = 1.58;  // Intercept from datasheet curve
  return pow(10, (m * log10(ratio) + b));
}

void setup() {
  Serial.begin(115200);

  // Initialize I2C for BH1750
  Wire.begin();

  // Initialize LCD
  lcd.begin(16, 2);
  lcd.backlight();

  // Display installation message
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Instalasi...");
  delay(3000);

  // Initialize DHT22
  dht.begin();
  Serial.println("DHT22 initialized.");

  // Initialize BH1750
  if (lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE)) {
    Serial.println("BH1750 ready!");
  } else {
    Serial.println("Failed to initialize BH1750. Check connections.");
    while (1);
  }

  // Initialize MQ-135
  pinMode(MQ135PIN, INPUT);
  Serial.println("MQ-135 initialized.");

  // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Connecting WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("WiFi Connected");
  delay(2000);
}

void loop() {
  static unsigned long lastLCDMillis = 0;  // Waktu terakhir pembaruan LCD
  static unsigned long lastSendMillis = 0; // Waktu terakhir pengiriman data
  static int cycle = 0;

  // Interval pembaruan LCD (10 detik)
  if (millis() - lastLCDMillis >= 10000) {
    lastLCDMillis = millis();
    cycle = (cycle + 1) % 4; // Cycle antara 0 hingga 3

    // Baca data dari sensor
    float temperature = dht.readTemperature();
    float humidity = dht.readHumidity();
    float lux = lightMeter.readLightLevel();

    int sensorValue = analogRead(MQ135PIN);
    float voltage = (sensorValue / 4095.0) * 3.3; // Konversi ke voltase
    float RS = (3.3 - voltage) * RL / voltage; // Hitung resistansi sensor
    float ratio = RS / AIR_R0; // Hitung rasio RS/R0
    float ammoniaPPM = getPPM(ratio); // Konversi ke ppm

    if (isnan(temperature) || isnan(humidity) || lux < 0 || voltage <= 0 || voltage > 3.3) {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Sensor Error");
      return;
    }

    // Tentukan kondisi
    String tempCondition = (temperature < 20) ? "Dingin" : 
                           (temperature > 35) ? "Panas" : "Optimal";
    String humCondition = (humidity < 30) ? "Kering" : 
                          (humidity > 80) ? "Lembab" : "Optimal";
    String lightCondition = (lux <= 6000) ? "Normal" : "Kritis";
    String ammoniaCondition = (ammoniaPPM <= 25) ? "Normal" : "Kritis";

    // Perbarui LCD
    
    lcd.clear();
    switch (cycle) {
      case 0:
        // Siklus 1: Suhu
        lcd.setCursor(0, 0);
        lcd.print("Suhu: ");
        lcd.print(String(temperature, 1) + " C");
        lcd.setCursor(0, 1);
        lcd.print("Kondisi: " + tempCondition);
        break;
      case 1:
        // Siklus 2: Kelembapan
        lcd.setCursor(0, 0);
        lcd.print("Kelembapan: ");
        lcd.print(String(humidity, 1) + " %");
        lcd.setCursor(0, 1);
        lcd.print("Kondisi: " + humCondition);
        break;
      case 2:
        // Siklus 3: Amonia
        lcd.setCursor(0, 0);
        lcd.print("Amonia: ");
        lcd.print(String(ammoniaPPM, 1) + " ppm");
        lcd.setCursor(0, 1);
        lcd.print("Kondisi: " + ammoniaCondition);
        break;
      case 3:
        // Siklus 4: Intensitas Cahaya
        lcd.setCursor(0, 0);
        lcd.print("Cahaya: ");
        lcd.print(String(lux, 1) + " lux");
        lcd.setCursor(0, 1);
        lcd.print("Kondisi: " + lightCondition);
        break;
    }
  }

  // Interval pengiriman data ke API (1 jam = 3600000 ms)
  if (millis() - lastSendMillis >= 3600000) {
    lastSendMillis = millis();

    // Baca data sensor untuk pengiriman
    float temperature = dht.readTemperature();
    float humidity = dht.readHumidity();
    float lux = lightMeter.readLightLevel();
    int sensorValue = analogRead(MQ135PIN);
    float voltage = (sensorValue / 4095.0) * 3.3; // Konversi ke voltase
    float RS = (3.3 - voltage) * RL / voltage; // Hitung resistansi sensor
    float ratio = RS / AIR_R0; // Hitung rasio RS/R0
    float ammoniaPPM = getPPM(ratio); // Konversi ke ppm

    if (WiFi.status() == WL_CONNECTED) {
      HTTPClient http;
      http.begin(apiEndpoint);
      http.addHeader("Content-Type", "application/json");

      // Siapkan payload JSON
      String payload = "{";
      payload += "\"farm_id\": \"" + String(farm_id) + "\",";
      payload += "\"temperature\": " + String(temperature, 1) + ",";
      payload += "\"humidity\": " + String(humidity, 1) + ",";
      payload += "\"ammonia\": " + String(ammoniaPPM, 1) + ",";
      payload += "\"light_intensity\": " + String(lux, 1);
      payload += "}";

      int httpResponseCode = http.POST(payload);

      if (httpResponseCode > 0) {
        Serial.print("Response code: ");
        Serial.println(httpResponseCode);
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Data Sent OK");
      } else {
        Serial.print("Error sending POST: ");
        Serial.println(httpResponseCode);
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Send Error");
      }
      http.end();
    } else {
      Serial.println("WiFi disconnected.");
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("WiFi Error");
    }
  }
}
