#define FASTLED_INTERNAL
#define CLK_DBL             0
#define CLEANUP_R1_AVRASM   0
#include <FastLED.h>
#include <sunset.h>
#define ARDUINOJSON_ENABLE_PROGMEM 0
#include <ArduinoJson.h>
#include <MQTT.h>
#include "TikiCandle.h"
#include "Torch.h"

#define APP_VERSION			126

PRODUCT_ID(985);
PRODUCT_VERSION(APP_VERSION);

SerialLogHandler logHandler;

void mqttCallback(char* topic, byte* payload, unsigned int length);

CRGB leds[NUM_LEDS];
const uint8_t _usDSTStart[22] = { 10, 8,14,13,12,10, 9, 8,14,12,11,10, 9,14,13,12,11, 9};
const uint8_t _usDSTEnd[22]   = { 3, 1, 7, 6, 5, 3, 2, 1, 7, 5, 4, 3, 2, 7, 6, 5, 4, 2};
SunSet sun;
int tikiCurrentProgram;
int g_appId;
int g_timeZone;
double g_sunset;
double g_awakeTime;
double g_minsPastMidnight;
bool g_disabled;
int g_jsonError;
int g_temp;
int g_httpResponse;
int g_wakeOffset;
bool g_running;
String g_name = "tikilight-";
String g_mqttName = g_name + System.deviceID().substring(0, 8);
TikiCandle candle;
byte mqttServer[] = {172, 24, 1, 13};
MQTT client(mqttServer, 1883, mqttCallback);

STARTUP(WiFi.selectAntenna(ANT_INTERNAL));

int currentTimeZone()
{
    g_timeZone = DST_OFFSET;
    if (Time.month() > 3 && Time.month() < 11) {
        return DST_OFFSET;
    }
    if (Time.month() == 3) {
        if ((Time.day() == _usDSTStart[Time.year() - TIME_BASE_YEAR]) && Time.hour() >= 2)
            return DST_OFFSET;
        if (Time.day() > _usDSTStart[Time.year() - TIME_BASE_YEAR])
            return DST_OFFSET;
    }
    if (Time.month() == 11) {
        if ((Time.day() == _usDSTEnd[Time.year() - TIME_BASE_YEAR]) && Time.hour() <=2)
            return DST_OFFSET;
        if (Time.day() < _usDSTEnd[Time.year() - TIME_BASE_YEAR])
            return DST_OFFSET;
    }
    g_timeZone = CST_OFFSET;
    return CST_OFFSET;
}

void syncTime()
{
   	Particle.syncTime();
    waitUntil(Particle.syncTimeDone);
    Time.zone(currentTimeZone());
    sun.setCurrentDate(Time.year(), Time.month(), Time.day());
    sun.setTZOffset(currentTimeZone());
}

int shutdownDevice(String)
{
    g_disabled = !g_disabled;
    
    if (g_disabled)
        return 0;

    return 1;
}

void setProgram()
{
    NSFastLED::HSVHue color;
    int c = 255 - map(g_temp, 0, 100, 0, 255);

    color = static_cast<NSFastLED::HSVHue>(c);

    Log.info("New program value is %d", color);
    candle.init(color, color - 10, color + 10, 25, 10);
}

int setWakeOffset(String p)
{
    g_wakeOffset = p.toInt();
    return g_wakeOffset;
}

int wakeup(String p)
{
    g_disabled = false;
    setProgram();
    return g_temp;
}

void restartDevice()
{
    System.reset();
}

void mqttCallback(char* topic, byte* payload, unsigned int length) 
{
    StaticJsonDocument<200> json;
    auto err = deserializeJson(json, static_cast<unsigned char*>(payload), static_cast<size_t>(length));

    if (err != DeserializationError::Ok) {
        Log.error("Unable to deserialize JSON");
        Log.info("%s", payload);
        return;
    }

    if (strcmp(topic, "weather/conditions") == 0) {
        g_temp = json["environment"]["farenheit"];
        setProgram();
        Log.info("New temperature is %d", g_temp);
    }
    if (strcmp(topic, "tiki/device/restart") == 0) {
        restartDevice();
    }
    if (strcmp(topic, "tiki/device/disable") == 0) {
        g_disabled = true;
    }
}

void setup()
{
    g_appId = APP_VERSION;
    g_timeZone = CST_OFFSET;
    g_temp = 0;
    g_wakeOffset = 30;
    g_disabled = false;

	Serial.begin(115200);
	delay(2000); // sanity delay
	FastLED.addLeds<NEOPIXEL, LED_PIN>(leds, NUM_LEDS);

    Particle.variable("appid", g_appId);
    Particle.variable("sunset", g_sunset);
    Particle.variable("disabled", g_disabled);
    Particle.variable("temp", g_temp);
    Particle.function("wakeoffset", setWakeOffset);
    Particle.function("shutdown", shutdownDevice);

	sun.setPosition(LATITUDE, LONGITUDE, CST_OFFSET);
	waitUntil(WiFi.ready);
	syncTime();
    
    g_sunset = sun.calcSunset();
    FastLED.clear();
    FastLED.show();
    Log.info("Done with setup, app version %d", g_appId);
    WiFi.off();
}

void loop()
{
    int mpm = Time.minute() + (Time.hour() * 60);

    if (mpm >= g_sunset) {
        if (!WiFi.ready()) {
            WiFi.on();
            WiFi.connect();
            waitUntil(WiFi.ready);
            Particle.connect();
            Particle.publishVitals();
        }
        g_running = true;
        if (!client.isConnected()) {
            client.connect(g_mqttName.c_str());
            if (client.isConnected()) {
                Log.info("MQTT Connected");
                client.subscribe("weather/conditions");
            }
        }
    }
    else {
        g_running = false;
        WiFi.off();
    }
    
    EVERY_N_MILLIS(ONE_HOUR) {
        if (!g_running) {
            WiFi.on();
            WiFi.connect();
            waitUntil(WiFi.ready);
            Particle.connect();
            Particle.publishVitals();
        }
        syncTime();
        g_sunset = sun.calcSunset();
        if (!g_running) {
            WiFi.off();
        }
    }

    EVERY_N_MILLIS(FIVE_SECONDS) {
        if (g_running) {
            if (WiFi.ready() && client.isConnected()) {
                client.loop();
            }   
            else {
                Log.error("We're not looping");
                client.connect(g_mqttName.c_str());
                if (client.isConnected()) {
                    client.subscribe("weather/event");
                }
            }
        }
    }
    if (!g_disabled && g_running) {
        candle.run(25);        
    }
    else {
        FastLED.clear();
        FastLED.show();
    }
}
