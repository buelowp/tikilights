#define FASTLED_INTERNAL
#include <FastLED.h>
#include <sunset.h>
#include <MQTT.h>
#include "TikiCandle.h"
#include "Torch.h"

#define APP_VERSION			184

PRODUCT_ID(985);
PRODUCT_VERSION(APP_VERSION);

//SYSTEM_MODE(SEMI_AUTOMATIC);
//SYSTEM_THREAD(ENABLED);

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
int g_programColor;
bool g_running;
bool g_connected;
bool g_invalidJSON;
bool g_unblock;
uint16_t g_messageId;
String g_name = "tikilight-";
String g_mqttName = g_name + System.deviceID().substring(0, 8);
String g_checkin = "tiki/state/" + System.deviceID().substring(0, 8);
String g_version = System.version() + "." + String(APP_VERSION);
TikiCandle candle;
byte mqttServer[] = {172, 24, 1, 13};
MQTT client(mqttServer, 1883, mqttCallback);
char g_buffer[512];

STARTUP(WiFi.selectAntenna(ANT_INTERNAL));

SystemSleepConfiguration config;
SerialLogHandler logHandler;
Serial1LogHandler logHandler2(115200);
ApplicationWatchdog *wd;

int currentTimeZone()
{
    int timezone = CST_OFFSET;
    if (Time.month() > 3 && Time.month() < 11) {
        timezone = DST_OFFSET;
    }
    if (Time.month() == 3) {
        if ((Time.day() == _usDSTStart[Time.year() - TIME_BASE_YEAR]) && Time.hour() >= 2)
            timezone = DST_OFFSET;
        if (Time.day() > _usDSTStart[Time.year() - TIME_BASE_YEAR])
            timezone = DST_OFFSET;
    }
    if (Time.month() == 11) {
        if ((Time.day() == _usDSTEnd[Time.year() - TIME_BASE_YEAR]) && Time.hour() <=2)
            timezone = DST_OFFSET;
        if (Time.day() < _usDSTEnd[Time.year() - TIME_BASE_YEAR])
            timezone = DST_OFFSET;
    }
    Log.info("Returning %d as time zone offset", timezone);
    return timezone;
}

void netConnect()
{
    while (!client.isConnected()) {
        client.connect(g_mqttName.c_str());
        if (client.isConnected()) {
            Log.info("%s: MQTT Connected with name %s", __FUNCTION__, g_mqttName.c_str());
            client.subscribe("weather/conditions");
            g_connected = true;
        }
        Particle.process();
    }

    System.enableUpdates();
}

void netDisconnect()
{
    client.loop();
    client.disconnect();
    Particle.disconnect();
    Log.info("Disconnected");
}

void syncTime()
{
    Log.info("Syncing time...");
   	Particle.syncTime();
    waitUntil(Particle.syncTimeDone);
    g_timeZone = currentTimeZone();
    Time.zone(g_timeZone);
    sun.setCurrentDate(Time.year(), Time.month(), Time.day());
    sun.setTZOffset(g_timeZone);
    g_sunset = sun.calcSunset();
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
        
    g_programColor = 230 - map(constrain(g_temp, 10, 85), 10, 85, 5, 230);

    color = static_cast<NSFastLED::HSVHue>(g_programColor);

    candle.init(color, color - 5, color + 5, 25, 10);
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
    memset(g_buffer, '\0', 512);
    memcpy(g_buffer, payload, length);
    double temp;

    if (strcmp(topic, "weather/conditions") == 0) {
        JSONValue outerObj = JSONValue::parseCopy(g_buffer);
        JSONObjectIterator iter(outerObj);
        while(iter.next()) {
            if (iter.name() == "environment") {
                JSONObjectIterator env(iter.value());
                while (env.next()) {
                    if (env.name() == "farenheit") {
                        temp = env.value().toDouble() + .5;
                        g_temp = static_cast<int>(temp);
                        setProgram();
                        Log.info("New temperature is %d, RAM: %ld, Time: %s", g_temp, System.freeMemory(), Time.timeStr().c_str());
                        g_invalidJSON = false;
                    }
                }
            }
        }
    }
}

// QOS ack callback.
// if application use QOS1 or QOS2, MQTT server sendback ack message id.
void qoscallback(unsigned int messageid) 
{
    if (messageid == g_messageId) {
        Log.info("Got a matched message ID %d", g_messageId);
        g_unblock = true;
        return;
    }
    Log.info("Message ID mismatch: expected %d, got %d", g_messageId, messageid);
}

void mqttCheckin(int mpm, int sleep, int checkin)
{
    JSONBufferWriter writer(g_buffer, sizeof(g_buffer) - 1);
    if (!client.isConnected()) {
        netConnect();
    }
    if (client.isConnected()) {
        g_connected = true;
        writer.beginObject();
        writer.name("application");
        writer.beginObject();
            writer.name("mpm").value(mpm);
            writer.name("sunset").value(g_sunset);
            writer.name("program").value(g_programColor);
            writer.name("wakeup").value(sleep);
            if (checkin) {
                writer.name("lights").value("on");
            }
        writer.endObject();
        writer.name("photon");
        writer.beginObject();
            writer.name("deviceid").value(System.deviceID());
            writer.name("version").value(g_version);
        writer.endObject();
        writer.name("network");
        writer.beginObject();
            writer.name("ssid").value(WiFi.SSID());
            writer.name("signalquality").value(WiFi.RSSI());
        writer.endObject();
            
        writer.endObject();
        writer.buffer()[std::min(writer.bufferSize(), writer.dataSize())] = 0;
        Log.info("Publishing vitals: %s", writer.buffer());
        bool result = client.publish(g_checkin, writer.buffer(), MQTT::EMQTT_QOS::QOS1, &g_messageId);
        Log.info("Publish of message ID %d was %d", g_messageId, result);
    }
}

void watchdogHandler() 
{
  // Do as little as possible in this function, preferably just
  // calling System.reset().
  // Do not attempt to Particle.publish(), use Cellular.command()
  // or similar functions. You can save data to a retained variable
  // here safetly so you know the watchdog triggered when you 
  // restart.
  // In 2.0.0 and later, RESET_NO_WAIT prevents notifying the cloud of a pending reset
  System.reset(RESET_NO_WAIT);
}

void setup()
{
    g_appId = APP_VERSION;
    g_timeZone = CST_OFFSET;
    g_temp = 70;
    g_wakeOffset = 30;
    g_disabled = false;
    g_running = false;
    g_connected = false;
    g_invalidJSON = false;
    g_unblock = false;

    client.addQosCallback(qoscallback);

	delay(2000); // sanity delay
	FastLED.addLeds<NEOPIXEL, LED_PIN>(leds, NUM_LEDS);

    Particle.variable("appid", g_appId);
    Particle.variable("sunset", g_sunset);
    Particle.variable("disabled", g_disabled);
    Particle.variable("temp", g_temp);
    Particle.variable("program", g_programColor);
    Particle.function("wakeoffset", setWakeOffset);
    Particle.function("shutdown", shutdownDevice);
    Particle.variable("connected", g_connected);
    Particle.variable("invalid", g_invalidJSON);

    wd = new ApplicationWatchdog(60000, watchdogHandler, 1536);

    FastLED.clear();
    FastLED.show();

    waitFor(Particle.connected, 120000);

    setProgram();
    g_timeZone = currentTimeZone();
    Time.zone(g_timeZone);
    sun.setPosition(LATITUDE, LONGITUDE, -5);
    sun.setCurrentDate(Time.year(), Time.month(), Time.day());
    sun.setTZOffset(g_timeZone);
    g_sunset = sun.calcSunset();
    netConnect();
    wd->checkin();
    Log.info("Done with setup, app version %d", g_appId);
}

void loop()
{
    static int lastHour = 24;
    static int lastSecond = 60;
    static bool wasSleeping = false;

    if (wasSleeping) {
        Particle.connect();
        waitFor(Particle.connected, 120000);
        if (!Particle.connected()) {
            config.mode(SystemSleepMode::ULTRA_LOW_POWER).duration(1min);
            Log.info("Unable to connect to cloud, sleeping for 1min");
            System.sleep(config);
        }
        wd->checkin();

        Log.info("Particle cloud connected");
        netConnect();
        if (!client.isConnected()) {
            config.mode(SystemSleepMode::ULTRA_LOW_POWER).duration(1min);
            System.sleep(config);        
        }

        Log.info("MQTT client connected");
        wd->checkin();
        sun.setPosition(LATITUDE, LONGITUDE, CST_OFFSET);
        g_sunset = sun.calcSunset();
    }
    wasSleeping = false;

    int mpm = Time.minute() + (Time.hour() * 60);
    wd->checkin();

    if (mpm >= g_sunset) {
        candle.run(30);
        if (lastHour != Time.hour()) {
            Log.info("mpm = %d, sunset %f, time = %s, RAM: %ld", mpm, g_sunset, Time.timeStr().c_str(), System.freeMemory());
            syncTime();
            mqttCheckin(mpm, 0, true);
            lastHour = Time.hour();
        }
        if (lastSecond != Time.second()) {
            if (client.isConnected()) {
                client.loop();
            }
            else {
                netConnect();
            }
            lastSecond = Time.second();
            wd->checkin();
        }
    }
    else if (Time.hour() == 0 && Time.minute() == 0 && Time.second() == 0) {
        Log.info("Restarting after running program: %s", Time.timeStr().c_str());
        System.reset(RESET_NO_WAIT);
    }   
    else {
        syncTime();
        mpm = Time.minute() + (Time.hour() * 60);
        system_tick_t remaining = g_sunset - mpm;
        if (remaining > 60)
            remaining = 60;

        Log.info("MPM: %d, Remain: %ld, connected: %d, RAM: %ld", mpm, remaining, client.isConnected(), System.freeMemory());
        remaining = remaining * ONE_MINUTE;     // Convert to millis
        mqttCheckin(mpm, remaining, false);
        while (!g_unblock) {
            client.loop();
            delay(10);
        }
        g_unblock = false;
        FastLED.clear();
        FastLED.show();
        netDisconnect();
        wd->checkin();
        Log.info("sleeping for %ld millis at %s, RAM: %ld", remaining, Time.timeStr().c_str(), System.freeMemory());
        for (system_tick_t i = 0; i < remaining / 1000; i++) {
            delay(1000);
            wd->checkin();
        }
        Log.info("Ending sleep");
        wasSleeping = true;   // Uncomment when going back to sleep
    }
}
