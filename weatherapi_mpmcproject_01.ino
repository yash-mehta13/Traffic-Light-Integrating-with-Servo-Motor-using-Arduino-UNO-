#include <WiFi.h>
#include <HTTPClient.h>
#include <Arduino_JSON.h>
#include <DHT.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// WiFi credentials
const char* ssid = "Net nahi hai";
const char* password = "Nahi Bataunga";

// OpenWeather API (Replace with your details)
const char* city = "Vellore"; 
const char* apiKey = "72143bcf87f6f6f1d3ecdb7313610efe";  
String weatherApiUrl = "http://api.openweathermap.org/data/2.5/weather?q=" + String(city) + "&appid=" + String(apiKey) + "&units=metric";

// Pin Definitions
#define RELAY_PIN 4
#define MOISTURE_SENSOR_PIN 34
#define DHT_PIN 18
#define DHT_TYPE DHT11

// Initialize components
DHT dht(DHT_PIN, DHT_TYPE);
LiquidCrystal_I2C lcd(0x27, 16, 2); // Default I2C Address 0x27, 16x2 LCD

void setup() {
    Serial.begin(115200);
    
    // I2C Scanner code to find the LCD I2C address
    Serial.println("\nI2C Scanner");
    Wire.begin();
    for (byte address = 8; address < 120; address++) {  // I2C addresses range from 8 to 119
        Wire.beginTransmission(address);
        byte error = Wire.endTransmission();
        if (error == 0) {
            Serial.print("I2C device found at address 0x");
            if (address < 16) {
                Serial.print("0");
            }
            Serial.println(address, HEX);
        }
    }

    // Initialize DHT and LCD
    dht.begin();
    lcd.begin();  
    lcd.backlight();
    
    pinMode(RELAY_PIN, OUTPUT);
    digitalWrite(RELAY_PIN, HIGH); // Initially OFF

    connectToWiFi();
}

void loop() {
    float localTemp = dht.readTemperature();
    float localHumidity = dht.readHumidity();
    
    if (isnan(localTemp) || isnan(localHumidity)) {
        Serial.println("DHT sensor error!");
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("DHT Error!");
        delay(3000);
        return;
    }

    Serial.print("Temp: "); Serial.print(localTemp); Serial.println("°C");
    Serial.print("Humidity: "); Serial.print(localHumidity); Serial.println("%");

    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("Wi-Fi Disconnected! Reconnecting...");
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Wi-Fi Error!");
        connectToWiFi();
        return;
    }

    float apiTemp, apiHumidity, rain = 0;
    if (!fetchWeatherData(apiTemp, apiHumidity, rain)) {
        Serial.println("Weather data fetch failed!");
        return;
    }

    int soilMoisture = analogRead(MOISTURE_SENSOR_PIN);
    Serial.print("Soil Moisture: "); Serial.println(soilMoisture);

    bool shouldIrrigate = false;
    if (rain > 0) {
        Serial.println("Rain detected. No irrigation.");
        digitalWrite(RELAY_PIN, HIGH);
        shouldIrrigate = false;
    } else if (soilMoisture < 2000 || localTemp > 30 || localHumidity < 40) {
        Serial.println("Dry conditions. Pump ON.");
        digitalWrite(RELAY_PIN, LOW);
        shouldIrrigate = true;
    } else {
        Serial.println("Soil OK. No irrigation.");
        digitalWrite(RELAY_PIN, HIGH);
        shouldIrrigate = false;
    }

    // Display different data every 3 seconds
    for (int i = 0; i < 6; i++) {
        lcd.clear();
        switch (i) {
            case 0:
                lcd.setCursor(0, 0);
                lcd.print("Soil Moisture:");
                lcd.setCursor(0, 1);
                lcd.print(soilMoisture);
                break;
            case 1:
                lcd.setCursor(0, 0);
                lcd.print("Temp (Local):");
                lcd.setCursor(0, 1);
                lcd.print(localTemp);
                lcd.print(" C");
                break;
            case 2:
                lcd.setCursor(0, 0);
                lcd.print("Humidity (Local):");
                lcd.setCursor(0, 1);
                lcd.print(localHumidity);
                lcd.print(" %");
                break;
            case 3:
                lcd.setCursor(0, 0);
                lcd.print("Temp (API):");
                lcd.setCursor(0, 1);
                lcd.print(apiTemp);
                lcd.print(" C");
                break;
            case 4:
                lcd.setCursor(0, 0);
                lcd.print("Humidity (API):");
                lcd.setCursor(0, 1);
                lcd.print(apiHumidity);
                lcd.print(" %");
                break;
            case 5:
                lcd.setCursor(0, 0);
                lcd.print("Pump Status:");
                lcd.setCursor(0, 1);
                lcd.print(shouldIrrigate ? "ON" : "OFF");
                break;
        }
        delay(3000); // Show each message for 3 seconds
    }

    delay(60000);
}

// Function to connect to Wi-Fi
void connectToWiFi() {
    WiFi.begin(ssid, password);
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 10) {
        delay(1000);
        Serial.print(".");
        attempts++;
    }
    Serial.println(WiFi.status() == WL_CONNECTED ? "\nConnected!" : "\nWi-Fi Failed!");
}

// Fetch weather data
bool fetchWeatherData(float &apiTemp, float &apiHumidity, float &rain) {
    HTTPClient http;
    http.begin(weatherApiUrl);
    int httpCode = http.GET();

    if (httpCode > 0) {
        String payload = http.getString();
        Serial.println("API Response: " + payload);  // PRINT API RESPONSE

        JSONVar weatherData = JSON.parse(payload);

        if (JSON.typeof(weatherData) == "undefined") {
            Serial.println("JSON Parsing Error!");  //  DEBUG JSON ERROR
            http.end();
            return false;
        }

        apiTemp = (double)weatherData["main"]["temp"];
        apiHumidity = (double)weatherData["main"]["humidity"];

        if (weatherData.hasOwnProperty("rain")) {
            rain = (double)weatherData["rain"]["1h"];
        } else {
            rain = 0;  //  Set rain to 0 if no rain data is available
        }

        Serial.print("API Temp: "); Serial.println(apiTemp);
        Serial.print("API Humidity: "); Serial.println(apiHumidity);
        Serial.print("Rain: "); Serial.println(rain);

        http.end();
        return true;
    } else {
        Serial.println("Weather API Error!");
        http.end();
        return false;
    }
}