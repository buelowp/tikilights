#include <FastLED.h>
#include <SunSet.h>
#define ARDUINOJSON_ENABLE_PROGMEM 0
#include <ArduinoJson.h>
#include <MQTT.h>
#include "TikiCandle.h"
#include "Torch.h"

#define APP_VERSION			111

PRODUCT_ID(985);
PRODUCT_VERSION(APP_VERSION);

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
    NSFastLED::HSVHue color = static_cast<NSFastLED::HSVHue>(map(g_temp, 0, 100, 0, 224));

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
    NSFastLED::HSVHue color = static_cast<NSFastLED::HSVHue>(map(g_temp, 0, 100, 0, 224));

    candle.init(color, color - 10, color + 10, 25, 10);

    return 0;
}

void restartDevice()
{
    System.reset();
}

void mqttCallback(char* topic, byte* payload, unsigned int length) 
{
    StaticJsonDocument<200> json;
    auto err = deserializeJson(json, static_cast<unsigned char*>(payload), static_cast<size_t>(length));

    if (err != DeserializationError::Ok)
        return;

    if (strcmp(topic, "weather/conditions") == 0) {
        g_temp = json["environment"]["farenheit"];
        setProgram();
        Serial.print("New temperature is ");
        Serial.println(g_temp);
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
    Particle.function("wakeoffset", setWakeOffset);
    Particle.function("shutdown", shutdownDevice);

	sun.setPosition(LATITUDE, LONGITUDE, CST_OFFSET);
	waitUntil(WiFi.ready);
	syncTime();
    
    client.connect(g_mqttName.c_str());
    if (client.isConnected()) {
        Serial.println("Connected");
        client.subscribe("weather/event");
        client.subscribe("device/operation/restart");
        client.subscribe("device/operation/wakeup");
    }
    Serial.println("Done with setup");
    g_sunset = sun.calcSunset();
}

void loop()
{
    int mpm = Time.minute() + (Time.hour() * 60);

    EVERY_N_MILLIS(ONE_HOUR) {
        syncTime();
        g_sunset = sun.calcSunset();
    }

    EVERY_N_MILLIS(FIVE_SECONDS) {
        if (client.isConnected()) {
            client.loop();
        }   
        else {
            Serial.println("We're not looping");
            client.connect(g_mqttName.c_str());
            if (client.isConnected()) {
                client.subscribe("weather/event");
                client.subscribe("device/operation/restart");
                client.subscribe("device/operation/wakeup");
            }
        }
    }
    if (!g_disabled) {
        if (g_sunset >= mpm)
            candle.run();
    }
    else {
        FastLED.clear();
        FastLED.show();
    }
}