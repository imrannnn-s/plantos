#include <Wire.h>
#include <Adafruit_ADS1X15.h>
#include <DHT.h>
#include <WiFi.h>

// ======================================================
// WiFi & ThingSpeak
// ======================================================

const char* WIFI_SSID     = "STA-Training";
const char* WIFI_PASSWORD = "Madh@54321";

const char* TS_API_KEY    = "N702IBQAT82VAAJ8";
const char* TS_SERVER     = "api.thingspeak.com";

// ======================================================
// Pin Configuration
// ======================================================

#define RELAY_PIN 18

#define DHT_PIN   4
#define DHT_TYPE  DHT11

// ======================================================
// Soil Moisture Calibration
// Measure these values on YOUR sensor
// ======================================================

#define SOIL_DRY_VALUE 18500    // Sensor in air
#define SOIL_WET_VALUE 8500     // Sensor submerged in water

// ======================================================
// Watering Thresholds (Hysteresis)
// ======================================================

#define SOIL_MOISTURE_MIN 35    // Pump starts below this
#define SOIL_MOISTURE_MAX 45    // Pump allowed again above this

// ======================================================
// Timing
// ======================================================

#define PUMP_ON_DURATION_MS    3000
#define READ_INTERVAL_MS       2000
#define TS_UPLOAD_INTERVAL_MS  20000

// ======================================================
// Objects
// ======================================================

DHT dht(DHT_PIN, DHT_TYPE);
Adafruit_ADS1115 ads;

// ======================================================
// State Variables
// ======================================================

unsigned long lastReadTime = 0;
unsigned long lastTSUpload = 0;
unsigned long pumpStartTime = 0;

bool pumpRunning = false;
bool wateringLock = false;

// ======================================================
// Pump Control
// ======================================================

void startPump()
{
    digitalWrite(RELAY_PIN, LOW);   // Active LOW relay

    pumpRunning = true;
    pumpStartTime = millis();

    Serial.println("[PUMP] Started");
}

void stopPump()
{
    digitalWrite(RELAY_PIN, HIGH);

    pumpRunning = false;

    Serial.println("[PUMP] Stopped");
}

// ======================================================
// WiFi Reconnect
// ======================================================

void reconnectWiFi()
{
    if (WiFi.status() == WL_CONNECTED)
        return;

    Serial.println("[WiFi] Reconnecting...");

    WiFi.disconnect();
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    unsigned long start = millis();

    while (WiFi.status() != WL_CONNECTED &&
           millis() - start < 10000)
    {
        delay(500);
        Serial.print(".");
    }

    if (WiFi.status() == WL_CONNECTED)
    {
        Serial.println();
        Serial.print("[WiFi] Connected: ");
        Serial.println(WiFi.localIP());
    }
    else
    {
        Serial.println();
        Serial.println("[WiFi] Reconnect Failed");
    }
}

// ======================================================
// ThingSpeak Upload
// ======================================================

void uploadToThingSpeak(
    float temperature,
    float humidity,
    int soilPct,
    bool pumping)
{
    if (WiFi.status() != WL_CONNECTED)
    {
        Serial.println("[ThingSpeak] WiFi Offline");
        return;
    }

    WiFiClient client;

    Serial.println("[ThingSpeak] Connecting...");

    if (!client.connect(TS_SERVER, 80))
    {
        Serial.println("[ThingSpeak] Connection Failed");
        return;
    }

    String body =
        String("api_key=") + TS_API_KEY +
        "&field1=" + String(temperature, 1) +
        "&field2=" + String(humidity, 1) +
        "&field3=" + String(soilPct) +
        "&field4=" + String(pumping ? 1 : 0);

    client.println("POST /update HTTP/1.1");
    client.println("Host: api.thingspeak.com");
    client.println("Connection: close");
    client.println("Content-Type: application/x-www-form-urlencoded");

    client.print("Content-Length: ");
    client.println(body.length());

    client.println();
    client.print(body);

    // Force transmission
    client.println();
    client.flush();

    Serial.println();
    Serial.println("========== THINGSPEAK REQUEST ==========");
    Serial.println(body);
    Serial.println("========================================");

    // Wait for response
    unsigned long timeout = millis();

    while (client.connected() && millis() - timeout < 10000)
    {
        while (client.available())
        {
            String line = client.readStringUntil('\n');

            Serial.println(line);

            timeout = millis();
        }
    }

    client.stop();

    Serial.println("========== END RESPONSE ==========");
}

// ======================================================
// Setup
// ======================================================

void setup()
{
    Serial.begin(115200);

    Serial.println();
    Serial.println("====================================");
    Serial.println(" Smart Plant Watering System");
    Serial.println("====================================");

    // Relay OFF immediately

    pinMode(RELAY_PIN, OUTPUT);
    digitalWrite(RELAY_PIN, HIGH);

    // I2C

    Wire.begin(21, 22);

    if (!ads.begin(0x48))
    {
        Serial.println("[ADS1115] Initialization Failed");

        while (true)
        {
            delay(1000);
        }
    }

    // Better operating range

    ads.setGain(GAIN_TWOTHIRDS);

    Serial.println("[ADS1115] Ready");

    // DHT11

    dht.begin();

    // WiFi

    Serial.print("[WiFi] Connecting");

    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    int attempts = 0;

    while (WiFi.status() != WL_CONNECTED &&
           attempts < 20)
    {
        delay(500);
        Serial.print(".");
        attempts++;
    }

    Serial.println();

    if (WiFi.status() == WL_CONNECTED)
    {
        Serial.print("[WiFi] Connected: ");
        Serial.println(WiFi.localIP());
    }
    else
    {
        Serial.println("[WiFi] Offline Mode");
    }

    delay(1000);

    Serial.println("[SYSTEM] Ready");
}

// ======================================================
// Main Loop
// ======================================================

void loop()
{
    unsigned long now = millis();

    // ------------------------------------------
    // Keep WiFi alive
    // ------------------------------------------

    static unsigned long wifiCheck = 0;

    if (now - wifiCheck > 30000)
    {
        wifiCheck = now;
        reconnectWiFi();
    }

    // ------------------------------------------
    // Pump timeout
    // ------------------------------------------

    if (pumpRunning &&
        (now - pumpStartTime >= PUMP_ON_DURATION_MS))
    {
        stopPump();
    }

    // ------------------------------------------
    // Sensor Read
    // ------------------------------------------

    if (now - lastReadTime >= READ_INTERVAL_MS)
    {
        lastReadTime = now;

        // DHT11

        float humidity = dht.readHumidity();
        float temperature = dht.readTemperature();

        if (isnan(humidity) || isnan(temperature))
        {
            Serial.println("[DHT11] Read Failed");

            humidity = -1;
            temperature = -1;
        }

        // ADS1115 A0

        int16_t soilRaw =
            ads.readADC_SingleEnded(0);

        int soilPct =
            map(
                soilRaw,
                SOIL_DRY_VALUE,
                SOIL_WET_VALUE,
                0,
                100);

        soilPct = constrain(soilPct, 0, 100);

    // --------------------------------------
    // Hysteresis Logic
    // --------------------------------------

    if (soilPct >= SOIL_MOISTURE_MAX)
    {
        wateringLock = false;
    }

    bool soilDry =
        (soilPct < SOIL_MOISTURE_MIN);

    // --------------------------------------
    // Serial Output
    // --------------------------------------

    Serial.println();
    Serial.println("--------------------------------");

    Serial.printf(
        "[SOIL] Raw: %d | Moisture: %d%%\n",
        soilRaw,
        soilPct);

    if (temperature >= 0)
    {
        Serial.printf(
            "[DHT11] Temp: %.1f C | Humidity: %.1f%%\n",
            temperature,
            humidity);
    }

    Serial.printf(
        "[PUMP] %s\n",
        pumpRunning ? "RUNNING" : "OFF");

    // --------------------------------------
    // Watering Decision
    // --------------------------------------

    if (!pumpRunning)
    {
        if (soilDry && !wateringLock)
        {
            Serial.printf(
                "[DECISION] Moisture %d%% < %d%% -> Pump ON\n",
                soilPct,
                SOIL_MOISTURE_MIN);

            startPump();

            wateringLock = true;
        }
        else
        {
            Serial.println(
                "[DECISION] Soil moisture acceptable");
        }
    }

        // --------------------------------------
        // ThingSpeak Upload
        // --------------------------------------

        if (now - lastTSUpload >= TS_UPLOAD_INTERVAL_MS)
        {
            lastTSUpload = now;

            uploadToThingSpeak(
                temperature,
                humidity,
                soilPct,
                pumpRunning);
        }
    }
}