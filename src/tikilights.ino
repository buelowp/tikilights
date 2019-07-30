#include <FastLED.h>
#include <SunSet.h>
#define ARDUINOJSON_ENABLE_PROGMEM 0
#include <ArduinoJson.h>
#include <HttpClient.h>
#include "TikiCandle.h"
#include "Torch.h"

#define APP_VERSION			107
#define API_KEY             "1f87fe9d8a437f713c617f962df4f0a9"

PRODUCT_ID(985);
PRODUCT_VERSION(APP_VERSION);

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
bool g_validRunTime;
int g_jsonError;
int g_temp;
int g_httpResponse;
int g_wakeOffset;
e_runAnyway g_runAnyway;
TikiCandle candle;

HttpClient http;
http_header_t headers[] = {
    { "Accept" , "*/*"},
    { NULL, NULL } // NOTE: Always terminate headers with NULL
};
http_request_t request;
http_response_t response;

STARTUP(WiFi.selectAntenna(ANT_INTERNAL));

void turnOffWifi()
{
    #ifdef ON_BATTERY
        WiFi.off();
    #endif
}

void turnOnWifi()
{
    #ifdef ON_BATTERY
        WiFi.on();
    #endif
}

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

int remainingSleepTime()
{
    int remain = 60 * 60;
    int interval = 0;
    g_minsPastMidnight = Time.hour() * 60 + Time.minute();
    interval = (g_awakeTime - g_minsPastMidnight) * 60;
 
    /* Shouldn't get here */
    if (g_awakeTime < (g_minsPastMidnight - 60))
        return remain;

    if (interval > remain)
        return remain;

    remain = interval;

    return remain; 
}

void goToSleep()
{
    FastLED.clear();
    FastLED.show();
#ifdef ON_BATTERY
    int rst = remainingSleepTime();
    String time = String("Going to sleep for ") + String(rst) + String(" seconds, turning lights on at ") + String(g_awakeTime);
    Serial.println(time);
    Particle.publish("SleepTime", time, PRIVATE);
    Particle.process();
    delay(2000);
    System.sleep(SLEEP_MODE_DEEP, remainingSleepTime());
#endif
}

bool validRunTime()
{
    g_sunset = sun.calcSunset();
    g_minsPastMidnight = Time.hour() * 60 + Time.minute();
    g_awakeTime = g_sunset - g_wakeOffset;

    if (g_runAnyway == OOB_ON) {
        return true;
    }

    if (g_runAnyway == OOB_OFF) {
        return false;
    }

    if (g_minsPastMidnight == 0)
        g_runAnyway = NORMAL_OPERATION;

    if (g_minsPastMidnight >= g_awakeTime) {
        return true;
    }
    
    return false;
}

void setProgram()
{
    String path = String("/data/2.5/weather?units=imperial&zip=60005&appid=");

    g_temp = 199;    // default if we fail
    path.concat(API_KEY);
    request.hostname = "api.openweathermap.org";
    request.port = 80;
    request.path = path.c_str();

    http.get(request, response, headers);
    if (response.status == 200) {
        DynamicJsonDocument doc(1024);
        auto error = deserializeJson(doc, response.body.c_str());
        if (error) {
            Serial.print(F("deserializeJson() failed with code "));
            Serial.println(error.c_str());
            g_jsonError = static_cast<int>(error.code());
        }
        g_temp = doc["main"]["temp"];
        String temp = String("Received temperature ") + String(g_temp) + String(" from api.openweathermap.org");
        Serial.println(temp);
        Particle.publish("Temperature", temp, PRIVATE);
        Particle.process();
    }
    else {
        String failed = String("Connection to ") + String(request.hostname) + String(" failed: reason code ") + String(response.status);
        Serial.println(failed);
        Particle.publish("Connection", failed, PRIVATE);
        Particle.process();
        g_httpResponse = response.status;
    }

/* Possible colors to use
    HUE_RED
    HUE_ORANGE
    HUE_YELLOW
    HUE_GREEN
    HUE_AQUA
    HUE_BLUE
    HUE_PURPLE
    HUE_PINK
 */
    if (g_temp >= 75)
        candle.init(HUE_RED, 0, HUE_RED + 10, 25, 10);
    else if (g_temp >= 70)
        candle.init(HUE_ORANGE, HUE_ORANGE - 10, HUE_ORANGE + 10, 25, 10);
    else if (g_temp >= 65)
        candle.init(HUE_YELLOW, HUE_YELLOW - 10, HUE_YELLOW + 10, 25, 10);
    else if (g_temp >= 60)
        candle.init(HUE_GREEN, HUE_GREEN - 10, HUE_GREEN + 10, 25, 10);
    else if (g_temp >= 55)
        candle.init(HUE_AQUA, HUE_AQUA - 10, HUE_GREEN + 10, 25, 10);
    else if (g_temp >= 50)
        candle.init(HUE_BLUE, HUE_BLUE - 10, HUE_BLUE + 10, 25, 10);
    else
        candle.init(HUE_PURPLE, HUE_PURPLE - 10, HUE_PURPLE + 10, 25, 10);
}

int setWakeOffset(String p)
{
    g_wakeOffset = p.toInt();
    return g_wakeOffset;
}

int wakeup(String p)
{
    g_runAnyway = OOB_ON;

    if (p == "yellow")
        candle.init(HUE_YELLOW, HUE_YELLOW - 10, HUE_YELLOW + 10, 25, 10);
    else if (p == "orange")
        candle.init(HUE_ORANGE, HUE_ORANGE - 10, HUE_ORANGE + 10, 25, 10);
    else if (p == "green")
        candle.init(HUE_GREEN, HUE_GREEN - 10, HUE_GREEN + 10, 25, 10);
    else if (p == "aqua")
        candle.init(HUE_AQUA, HUE_AQUA - 10, HUE_GREEN + 10, 25, 10);
    else if (p == "blue")
        candle.init(HUE_BLUE, HUE_BLUE - 10, HUE_BLUE + 10, 25, 10);
    else if (p == "purple")
        candle.init(HUE_PURPLE, HUE_PURPLE - 10, HUE_PURPLE + 10, 25, 10);
    else if (p == "pink")
        candle.init(HUE_PINK, HUE_PINK - 10, HUE_PINK + 10, 25, 10);
    else
        candle.init(HUE_RED, 0, HUE_RED + 10, 25, 10);

    return 0;
}

int shutdown(String)
{
    g_runAnyway = OOB_OFF;
    return 2;
}

int runOperation(String p)
{
    if (p == "reset") {
        g_runAnyway = NORMAL_OPERATION;
        return 0;
    }

    return -1;
}

void setup()
{
    g_appId = APP_VERSION;
    g_timeZone = CST_OFFSET;
    g_jsonError = false;
    g_temp = 0;
    g_runAnyway = NORMAL_OPERATION;
    g_validRunTime = false;
    g_wakeOffset = 60;
    g_awakeTime = 0;

	Serial.begin(115200);
	delay(3000); // sanity delay
	FastLED.addLeds<NEOPIXEL, LED_PIN>(leds, NUM_LEDS);

    Particle.variable("appid", g_appId);
    Particle.variable("sunset", g_sunset);
    Particle.variable("wakeup", g_awakeTime);
    Particle.variable("temp", g_temp);
    Particle.variable("mpm", g_minsPastMidnight);
    Particle.variable("jsonerr", g_jsonError);
    Particle.variable("awake", g_validRunTime);
    Particle.function("wakeoffset", setWakeOffset);
    Particle.function("wakeup", wakeup);
    Particle.function("shutdown", shutdown);
    Particle.function("operation", runOperation);

	sun.setPosition(LATITUDE, LONGITUDE, CST_OFFSET);
	waitUntil(WiFi.ready);
	syncTime();
	setProgram();
    
    String ident = String("App Version: ") + String(g_appId);
    Particle.publish("Identity", ident, PRIVATE);
}

void loop()
{
    EVERY_N_MILLIS(ONE_HOUR) {
		turnOnWifi();
        waitUntil(WiFi.ready);
        syncTime();
        setProgram();
        turnOffWifi();
    }

    if ((g_validRunTime = validRunTime()) == true)
        candle.run();
    else
        goToSleep();
}