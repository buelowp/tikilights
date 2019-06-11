#include <FastLED.h>
#include <SunSet.h>
#include "TikiCandle.h"
#include "Torch.h"

#define APP_VERSION			80

PRODUCT_ID(985);
PRODUCT_VERSION(APP_VERSION);

CRGB leds[NUM_LEDS];
const uint8_t _usDSTStart[22] = { 10, 8,14,13,12,10, 9, 8,14,12,11,10, 9,14,13,12,11, 9};
const uint8_t _usDSTEnd[22]   = { 3, 1, 7, 6, 5, 3, 2, 1, 7, 5, 4, 3, 2, 7, 6, 5, 4, 2};
SunSet sun;
int tikiCurrentProgram;
bool runProgramSet;
bool programSetDone;
bool gReverseDirection;
bool setupCandlesDone;
int g_appId;
int g_timeZone;
int g_oscillations;
int g_sleepTime;
double g_sunset;
double g_testSunset;
int g_convertSunset;
double g_minsPastMidnight;
double g_hour;
time_t g_lastSync;
bool g_validRunTime;
TikiCandle candle;

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
    g_lastSync = Particle.timeSyncedLast();
}

int remainingSleepTime()
{
    int mpm = Time.hour() * 60 + Time.minute();
    int remain = 3600;
    int interval = (g_sunset - mpm) * 60;
    g_convertSunset = (int)g_sunset;
    g_sleepTime = 3600;
    g_testSunset = g_sunset;
    g_hour = Time.hour();
    g_minsPastMidnight = Time.hour() * 60 + Time.minute();

    /* Shouldn't get here */
    if (g_sunset < mpm)
        return remain;

    if (interval < remain)
        return remain;

    remain = interval;
    g_sleepTime = remain;

    return remain; 
}

void goToSleep()
{
    FastLED.clear();
    FastLED.show();
//    System.sleep(SLEEP_MODE_DEEP, remainingSleepTime());
}

bool validRunTime()
{
    g_sunset = sun.calcSunset();
    g_minsPastMidnight = Time.hour() * 60 + Time.minute();
    g_validRunTime = true;
    g_hour = Time.hour();

    if (g_minsPastMidnight >= g_sunset)
        return true;
    
    g_validRunTime = false;
    return false;
}

void setProgram()
{
    String val("2");
		Serial.printf("Got temp value %d\n", val.toInt());
		switch (val.toInt()) {
		case 0:
			candle.init(HUE_PURPLE, HUE_PURPLE - 10, HUE_PURPLE + 10, 25, 10);
			break;
		case 1:
			candle.init(HUE_BLUE, HUE_BLUE - 10, HUE_BLUE + 10, 25, 10);
			break;
		case 2:
			candle.init(HUE_GREEN, HUE_GREEN - 10, HUE_GREEN + 10, 25, 10);
			break;
		case 3:
			candle.init(HUE_YELLOW, HUE_YELLOW - 10, HUE_YELLOW + 10, 25, 10);
			break;
		case 4:
			candle.init(HUE_RED, 0, HUE_RED + 20, 25, 10);
			break;
		}
}

int newProgram(String val)
{
		switch (val.toInt()) {
		case 0:
			candle.init(HUE_PURPLE, HUE_PURPLE - 10, HUE_PURPLE + 10, 25, 10);
			break;
		case 1:
			candle.init(HUE_BLUE, HUE_BLUE - 10, HUE_BLUE + 10, 25, 10);
			break;
		case 2:
			candle.init(HUE_GREEN, HUE_GREEN - 10, HUE_GREEN + 10, 25, 10);
			break;
		case 3:
			candle.init(HUE_YELLOW, HUE_YELLOW - 10, HUE_YELLOW + 10, 25, 10);
			break;
		case 4:
			candle.init(HUE_RED, 0, HUE_RED + 20, 25, 10);
			break;
		}
    return val.toInt();
}

void setup()
{
    g_appId = APP_VERSION;
    g_timeZone = CST_OFFSET;
    g_oscillations = 0;
    g_sleepTime = 0;
    g_testSunset = 0.0;
    g_convertSunset = 0;
    g_hour = 0;
    g_lastSync = 0;

    Particle.variable("appid", g_appId);
    Particle.variable("sunset", g_sunset);
    Particle.variable("sleep", g_sleepTime); 
    Particle.variable("testsun", g_testSunset);
    Particle.variable("castsun", g_convertSunset);
    Particle.variable("mpm", g_minsPastMidnight);
    Particle.variable("hour", g_hour);
    Particle.function("setprogram", newProgram);
    Particle.variable("lastsync", g_lastSync);
    Particle.variable("valid", g_validRunTime);

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
	if ((Time.minute() == 1) && !programSetDone) {
		Serial.printf("Turning wifi on\n");
//		WiFi.on();
        waitUntil(WiFi.ready);
        syncTime();
        setProgram();
		programSetDone = true;
	}

	if (Time.minute() != 1) {
//		WiFi.off();
		programSetDone = false;
	}

    if (!validRunTime()) {
        goToSleep();
    }
    else
	    candle.run();
}