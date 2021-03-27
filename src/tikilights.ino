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

#define APP_VERSION			155

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
int g_programColor;
bool g_running;
String g_name = "tikilight-";
String g_mqttName = g_name + System.deviceID().substring(0, 8);
String g_checkin = "tiki/state/" + System.deviceID().substring(0, 8);
TikiCandle candle;
byte mqttServer[] = {172, 24, 1, 13};
MQTT client(mqttServer, 1883, mqttCallback);
char g_buffer[512];

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

void netConnect(int mpm)
{
    WiFi.on();
    WiFi.connect();
    waitUntil(WiFi.ready);
    if (!client.isConnected()) {
        client.connect(g_mqttName.c_str());
        if (client.isConnected()) {
            Log.info("MQTT Connected");
            client.subscribe("weather/conditions");
        }
    }

    Particle.connect();
    Particle.publishVitals();
    mqttCheckin(mpm, false);
}

void netDisconnect()
{
    client.loop();
    delay(100);
    client.disconnect();
    WiFi.off();
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
        
    g_programColor = 230 - map(constrain(g_temp, 10, 85), 10, 85, 5, 230);

    color = static_cast<NSFastLED::HSVHue>(g_programColor);

    Log.info("New program value is %d", color);
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

void mqttCheckin(int mpm, bool checkin)
{
    JSONBufferWriter writer(g_buffer, sizeof(g_buffer) - 1);
    if (!client.isConnected()) {
        client.connect(g_mqttName.c_str());
        Log.printf("Client is connected now: %d\n", client.isConnected());
    }
    if (client.isConnected()) {
        writer.beginObject();
        writer.name("appid").value(g_appId);
        if (checkin) {
            writer.name("lights").value("on");
        }
        writer.name("time");
        writer.beginObject();
            writer.name("mpm").value(mpm);
            writer.name("sunset").value(sun.calcSunset());
        writer.endObject();
        writer.name("photon");
        writer.beginObject();
            writer.name("uptime").value(System.uptime());
            writer.name("deviceid").value(System.deviceID());
            writer.name("version").value(System.version());
        writer.endObject();
        writer.name("network");
        writer.beginObject();
            writer.name("ssid").value(WiFi.SSID());
            writer.name("signalquality").value(WiFi.RSSI());
        writer.endObject();
            
        writer.endObject();
        writer.buffer()[std::min(writer.bufferSize(), writer.dataSize())] = 0;
        Log.printf("Publishing vitals: %s\n", writer.buffer());
        client.publish(g_checkin, writer.buffer(), MQTT::EMQTT_QOS::QOS1, true);
        client.loop();
    }
}

void setup()
{
    g_appId = APP_VERSION;
    g_timeZone = CST_OFFSET;
    g_temp = 0;
    g_wakeOffset = 30;
    g_disabled = false;
    g_running = false;

	Serial.begin(115200);
	delay(2000); // sanity delay
	FastLED.addLeds<NEOPIXEL, LED_PIN>(leds, NUM_LEDS);

    Particle.variable("appid", g_appId);
    Particle.variable("sunset", g_sunset);
    Particle.variable("disabled", g_disabled);
    Particle.variable("temp", g_temp);
    Particle.variable("program", g_programColor);
    Particle.function("wakeoffset", setWakeOffset);
    Particle.function("shutdown", shutdownDevice);

	sun.setPosition(LATITUDE, LONGITUDE, CST_OFFSET);
	syncTime();
    
    int mpm = Time.minute() + (Time.hour() * 60);
    mqttCheckin(mpm, false);
    g_sunset = sun.calcSunset();
    FastLED.clear();
    FastLED.show();
    Log.info("Done with setup, app version %d", g_appId);
    netDisconnect();
}

void loop()
{
    int mpm = Time.minute() + (Time.hour() * 60);

    if (mpm >= g_sunset) {
        Log.info("We are past sunset: %d", mpm);
        g_running = true;
        if (!WiFi.ready()) {
            netConnect(mpm);
        }
        EVERY_N_MILLIS(ONE_SECOND) {
            if (!client.isConnected()) {
                if (client.connect(g_mqttName.c_str())) {
                    Log.info("MQTT Connected");
                    client.subscribe("weather/conditions");
                }
                else {
                    Log.info("Could not connect to MQTT server");
                }
            }
            else {
                client.loop();
            }
        }
    }
    else {
        if (g_running) {
            g_running = false;
            Log.info("We are not past sunset: %d (%d)", mpm, g_running);
            netDisconnect();
        }
    }
    
    EVERY_N_MILLIS(ONE_HOUR) {
        Log.info("Attempting to checkin in: %d", g_running);
        if (!g_running) {
            netConnect(mpm);
        }
        syncTime();
        if (!g_running) {
            netDisconnect();
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
