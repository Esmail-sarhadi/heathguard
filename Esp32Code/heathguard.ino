#include <Wire.h>
#include "MAX30105.h"
#include "spo2_algorithm.h"
#include <Adafruit_NeoPixel.h>
#include <Adafruit_BME280.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <Arduino.h>

MAX30105 particleSensor;
Adafruit_BME280 bme;

#define LED_PIN     15
#define LED_COUNT   8
#define BRIGHTNESS  255

Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRBW + NEO_KHZ800);

#define I2C_SDA 21
#define I2C_SCL 22

const int mq135Pin = 34;  // Analog input pin for MQ135

uint32_t irBuffer[100];
uint32_t redBuffer[100];

int32_t bufferLength;
int32_t spo2;
int8_t validSPO2;
int32_t heartRate;
int8_t validHeartRate;

#define FINGER_ON_THRESHOLD 5000
bool fingerOn = false;
unsigned long lastChange = 0;
#define DEBOUNCE_DELAY 500

// Thresholds for environmental parameters
#define TEMP_THRESHOLD 30.0
#define HUMIDITY_THRESHOLD 60.0
#define CO2_THRESHOLD 500.0  // Updated CO2 threshold

// WiFi settings
const char* ssid = "your wifi name";
const char* password = "your pass ";
const char* serverName = "http://yourserver/index.php";
const char* deviceName = "ESP32_Device_2";

String encrypt(String data) {
  int shift = 3; // Caesar cipher shift
  String encrypted = "";
  for (int i = 0; i < data.length(); i++) {
    encrypted += char(int(data[i]) + shift);
  }
  return encrypted;
}

String decrypt(String data) {
  int shift = 3; // Caesar cipher shift
  String decrypted = "";
  for (int i = 0; i < data.length(); i++) {
    decrypted += char(int(data[i]) - shift);
  }
  return decrypted;
}

void setup() {
  Serial.begin(115200);
  Wire.begin(I2C_SDA, I2C_SCL);

  strip.begin();
  strip.show();
  strip.setBrightness(BRIGHTNESS);

  if (!particleSensor.begin(Wire, I2C_SPEED_FAST)) {
    Serial.println(F("MAX30105 was not found. Please check wiring/power."));
    while (1);
  }

  if (!bme.begin(0x76)) {
    Serial.println(F("Could not find a valid BME280 sensor, check wiring!"));
    while (1);
  }

  byte ledBrightness = 60;
  byte sampleAverage = 4;
  byte ledMode = 2;
  byte sampleRate = 100;
  int pulseWidth = 411;
  int adcRange = 4096;

  particleSensor.setup(ledBrightness, sampleAverage, ledMode, sampleRate, pulseWidth, adcRange);

  // Connect to WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi");
}

void loop() {
  bufferLength = 100;

  for (byte i = 0; i < bufferLength; i++) {
    while (particleSensor.available() == false) particleSensor.check();

    redBuffer[i] = particleSensor.getRed();
    irBuffer[i] = particleSensor.getIR();

    if (redBuffer[i] < FINGER_ON_THRESHOLD) redBuffer[i] = 0;
    if (irBuffer[i] < FINGER_ON_THRESHOLD) irBuffer[i] = 0;

    particleSensor.nextSample();
  }

  maxim_heart_rate_and_oxygen_saturation(irBuffer, bufferLength, redBuffer, &spo2, &validSPO2, &heartRate, &validHeartRate);

  for (byte i = 25; i < 100; i++) {
    redBuffer[i - 25] = redBuffer[i];
    irBuffer[i - 25] = irBuffer[i];
  }

  for (byte i = 75; i < 100; i++) {
    while (particleSensor.available() == false) particleSensor.check();

    redBuffer[i] = particleSensor.getRed();
    irBuffer[i] = particleSensor.getIR();

    if (redBuffer[i] < FINGER_ON_THRESHOLD) redBuffer[i] = 0;
    if (irBuffer[i] < FINGER_ON_THRESHOLD) irBuffer[i] = 0;

    particleSensor.nextSample();

    float temperature = bme.readTemperature();
    float pressure = bme.readPressure() / 100.0F;
    float humidity = bme.readHumidity();

    int sensorValue = analogRead(mq135Pin);
    float voltage = sensorValue * (5.0 / 4095.0);
    float ppm = 100 * pow(1.75 * (voltage / 5.0), 3);
    String airQuality;
    if (ppm < 100) airQuality = "Good";
    else if (ppm < 250) airQuality = "Fair";
    else if (ppm < 500) airQuality = "Poor";
    else airQuality = "Very Poor";

    Serial.print(F("red="));
    Serial.print(redBuffer[i], DEC);
    Serial.print(F(", ir="));
    Serial.print(irBuffer[i], DEC);
    Serial.print(F(", HR="));
    Serial.print(heartRate, DEC);
    Serial.print(F(", HRvalid="));
    Serial.print(validHeartRate, DEC);
    Serial.print(F(", SPO2="));
    Serial.print(spo2, DEC);
    Serial.print(F(", SPO2Valid="));
    Serial.print(validSPO2, DEC);
    Serial.print(F(", Temp="));
    Serial.print(temperature);
    Serial.print(F(", Pressure="));
    Serial.print(pressure);
    Serial.print(F(", Humidity="));
    Serial.print(humidity);
    Serial.print(F(", CO2 PPM="));
    Serial.print(ppm);
    Serial.print(F(" | Air Quality="));
    Serial.println(airQuality);

    bool newFingerState = (redBuffer[i] > FINGER_ON_THRESHOLD && irBuffer[i] > FINGER_ON_THRESHOLD);

    if (newFingerState != fingerOn || millis() - lastChange > DEBOUNCE_DELAY) {
      fingerOn = newFingerState;
      lastChange = millis();

      if (fingerOn && validHeartRate && heartRate > 0) {
        pulseHeartRateLEDs(strip.Color(0, 255, 0), heartRate);
      } else {
        setAllLEDs(strip.Color(255, 0, 0));
      }
      clearAllLEDs();
    }

    if (temperature > TEMP_THRESHOLD || humidity > HUMIDITY_THRESHOLD || ppm > CO2_THRESHOLD) {
      blinkRedLEDs();
      clearAllLEDs();
    }

    if(WiFi.status() == WL_CONNECTED) {
      HTTPClient http;
      http.begin(serverName);
      http.addHeader("Content-Type", "application/x-www-form-urlencoded");

      String data = "device=" + String(deviceName) + 
                    "&temperature=" + String(temperature) + 
                    "&humidity=" + String(humidity) + 
                    "&co2=" + String(ppm) + 
                    "&heart_rate=" + String(heartRate) + 
                    "&oxygen=" + String(spo2) + 
                    "&pressure=" + String(pressure) + 
                    "&air_quality=" + String(airQuality);

      String encryptedData = encrypt(data);
      Serial.println("Encrypted Data: " + encryptedData);

      // Sending the encrypted data as POST
      int httpResponseCode = http.POST(encryptedData);

      if (httpResponseCode > 0) {
        String response = http.getString();
        String decryptedResponse = decrypt(response);
        Serial.println("HTTP Response Code: " + String(httpResponseCode));
        Serial.println("Decrypted Response: " + decryptedResponse);
      } else {
        Serial.print("Error on sending POST: ");
        Serial.println(httpResponseCode);
      }
      http.end();
    }
    delay(100); // Delay between readings and HTTP POST requests
  }

  maxim_heart_rate_and_oxygen_saturation(irBuffer, bufferLength, redBuffer, &spo2, &validSPO2, &heartRate, &validHeartRate);
}

void setAllLEDs(uint32_t color) {
  for (int i = 0; i < strip.numPixels(); i++) {
    strip.setPixelColor(i, color);
  }
  strip.show();
}

void clearAllLEDs() {
  for (int i = 0; i < strip.numPixels(); i++) {
    strip.setPixelColor(i, 0);
  }
  strip.show();
}

void pulseHeartRateLEDs(uint32_t color, int rate) {
  int delayTime = 60000 / rate / 2;

  for (int i = 0; i < strip.numPixels(); i++) {
    strip.setPixelColor(i, color);
    strip.show();
    delay(50);
    strip.setPixelColor(i, 0);
    strip.show();
    delay(delayTime - 50);
  }
}

void blinkRedLEDs() {
  for (int j = 0; j < 3; j++) {  // Blink 3 times
    for (int i = 0; i < strip.numPixels(); i++) {
      strip.setPixelColor(i, strip.Color(255, 0, 0));
    }
    strip.show();
    delay(500);
    clearAllLEDs();
    delay(500);
  }
}
