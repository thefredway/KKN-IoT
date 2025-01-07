#include "esp_camera.h"
#include <WiFi.h>
#include <Wire.h>
#include <DHT.h>
#include <math.h>

// ===================
// Select camera model
// ===================
#define CAMERA_MODEL_WROVER_KIT // Has PSRAM
#include "camera_pins.h"

// ===========================
// Enter your WiFi credentials
// ===========================
const char *ssid = "g00pall";
const char *password = "122140026.1";

// Pin Definitions
#define DHTPIN 33
#define MQ135PIN 32
#define DHTTYPE DHT22
#define RL 10.0
#define AIR_R0 3.7

// Sensor Initialization
DHT dht(DHTPIN, DHTTYPE);

// Function to convert RS/R0 ratio to ppm for ammonia
float getPPM(float ratio) {
  float m = -0.47;
  float b = 1.58;
  return pow(10, (m * log10(ratio) + b));
}

void startCameraServer();
void setupLedFlash(int pin);

void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println();

  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.frame_size = FRAMESIZE_UXGA;
  config.pixel_format = PIXFORMAT_JPEG;  // for streaming
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.jpeg_quality = 12;
  config.fb_count = 1;

  if (config.pixel_format == PIXFORMAT_JPEG) {
    if (psramFound()) {
      config.jpeg_quality = 10;
      config.fb_count = 2;
      config.grab_mode = CAMERA_GRAB_LATEST;
    } else {
      config.frame_size = FRAMESIZE_SVGA;
      config.fb_location = CAMERA_FB_IN_DRAM;
    }
  } else {
    config.frame_size = FRAMESIZE_240X240;
#if CONFIG_IDF_TARGET_ESP32S3
    config.fb_count = 2;
#endif
  }

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }

  Serial.println("Camera initialized.");

  WiFi.begin(ssid, password);
  WiFi.setSleep(false);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");

  startCameraServer();

  Serial.print("Camera Ready! Use 'http://");
  Serial.print(WiFi.localIP());
  Serial.println("' to connect");

  // Initialize DHT22
  dht.begin();
  Serial.println("DHT22 initialized.");

  // Initialize MQ-135
  pinMode(MQ135PIN, INPUT);
  Serial.println("MQ-135 initialized.");
}

void loop() {
  static unsigned long lastMillis = 0;

  if (millis() - lastMillis >= 5000) {
    lastMillis = millis();

    // Read DHT22 data
    float temperature = dht.readTemperature();
    float humidity = dht.readHumidity();
    if (isnan(temperature) || isnan(humidity)) {
      Serial.println("Failed to read from DHT sensor!");
    } else {
      Serial.println("=====================================");
      Serial.printf("Temperature: %.2f °C\n", temperature);
      Serial.printf("Humidity: %.2f%%\n", humidity);
    }

    // Read MQ-135 data
    int sensorValue = analogRead(MQ135PIN);
    float voltage = (sensorValue / 4095.0) * 3.3;
    if (voltage > 0 && voltage <= 3.3) {
      float RS = (3.3 - voltage) * RL / voltage;
      float ratio = RS / AIR_R0;
      float ammoniaPPM = getPPM(ratio);
      Serial.printf("Ammonia Concentration (ppm): %.2f\n", ammoniaPPM);
    } else {
      Serial.println("Invalid voltage. Check sensor and connections!");
    }
  }
}
