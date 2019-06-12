#include <FastLED.h>
#include <SunSet.h>
#define ARDUINOJSON_ENABLE_PROGMEM 0
#include <ArduinoJson.h>
#include <HttpClient.h>
#include "TikiCandle.h"
#include "Torch.h"

#define APP_VERSION			94
#define API_KEY             "65e4c00704a8f00c6a1484b4f9eba63c"

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
double g_minsPastMidnight;
bool g_validRunTime;
int g_jsonError;
int g_temp;
int g_httpResponse;
TikiCandle candle;

HttpClient http;
http_header_t headers[] = {
    { "Accept" , "*/*"},
    { NULL, NULL } // NOTE: Always terminate headers will NULL
};
http_request_t request;
http_response_t response;

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

int remainingSleepTime()
{
    int remain = 60 * 60;
    g_minsPastMidnight = Time.hour() * 60 + Time.minute();
    int interval = (g_sunset - g_minsPastMidnight) * 60;
 
    /* Shouldn't get here */
    if (g_sunset < (g_minsPastMidnight - 60))
        return remain;

    if (interval > remain)
        return remain;

    remain = interval;

    return remain; 
}

void goToSleep()
{
    int rst = remainingSleepTime();
    FastLED.clear();
    FastLED.show();

    String time = String("Going to sleep for ") + String(rst) + String(" seconds, sunset at ") + String(g_sunset);
    Serial.println(time);
    Particle.publish("SleepTime", time, PRIVATE);
    Particle.process();
    delay(2000);
    System.sleep(SLEEP_MODE_DEEP, remainingSleepTime());
}

bool validRunTime()
{
    g_sunset = sun.calcSunset();
    g_minsPastMidnight = Time.hour() * 60 + Time.minute();
    g_validRunTime = true;

    if (g_minsPastMidnight >= g_sunset - 60)
        return g_validRunTime;
    
    g_validRunTime = false;
    return g_validRunTime;
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

    if (g_temp >= 70)
        candle.init(HUE_RED, 0, HUE_RED + 20, 25, 10);
    else if (g_temp >= 65)
        candle.init(HUE_YELLOW, HUE_YELLOW - 10, HUE_YELLOW + 10, 25, 10);
    else if (g_temp >= 60)
        candle.init(HUE_GREEN, HUE_GREEN - 10, HUE_GREEN + 10, 25, 10);
    else if (g_temp >= 55)
        candle.init(HUE_BLUE, HUE_BLUE - 10, HUE_BLUE + 10, 25, 10);
    else
        candle.init(HUE_PURPLE, HUE_PURPLE - 10, HUE_PURPLE + 10, 25, 10);
}

void setup()
{
    g_appId = APP_VERSION;
    g_timeZone = CST_OFFSET;
    g_jsonError = false;
    g_temp = 0;

    Particle.variable("appid", g_appId);
    Particle.variable("sunset", g_sunset);
    Particle.variable("temp", g_temp);
    Particle.variable("mpm", g_minsPastMidnight);
    Particle.variable("jsonerr", g_jsonError);

	Serial.begin(115200);
	delay(3000); // sanity delay
	FastLED.addLeds<NEOPIXEL, LED_PIN>(leds, NUM_LEDS);

	sun.setPosition(LATITUDE, LONGITUDE, CST_OFFSET);
	waitUntil(WiFi.ready);
	syncTime();
	setProgram();
}

void loop()
{
    static bool programSetDone = false;
    if (!validRunTime()) {
        goToSleep();
    }

	if ((Time.minute() == 1) && !programSetDone) {
		Serial.printf("Turning wifi on\n");
		WiFi.on();
        waitUntil(WiFi.ready);
        syncTime();
        setProgram();
		programSetDone = true;
	}

	if (Time.minute() != 1) {
		WiFi.off();
		programSetDone = false;
	}

    candle.run();
}