#include <FastLED.h>

// clang-format off
#include <SPI.h>
#include <WiFiNINA.h>
#include <WiFiUdp.h>
// clang-format on
#include <ArduinoOTA.h>
#include <NTPClient.h>
#include <colorutils.h>

#include "Arduino.h"
#include "consts.hpp"
#include "state.hpp"

WiFiServer server(30003);

WiFiUDP ntpUdp;
NTPClient timeClient(ntpUdp);

State state;

CRGB leds[NUM_LEDS];

int status = WL_IDLE_STATUS;

int range(int start, int end, int num) {
    if (num > end) {
        return end;
    } else if (num < start) {
        return start;
    } else {
        return num;
    }
}

void connect() {
    digitalWrite(LED_BUILTIN, HIGH);
    delay(1000);

    if (WiFi.status() == WL_NO_MODULE) {
        digitalWrite(LED_BUILTIN, HIGH);
        delay(1000);
        digitalWrite(LED_BUILTIN, LOW);
        delay(1000);
        digitalWrite(LED_BUILTIN, HIGH);
        while (1)
            ;
    }

    arduino::String fv = WiFi.firmwareVersion();

    if (fv < WIFI_FIRMWARE_LATEST_VERSION) {
        digitalWrite(LED_BUILTIN, HIGH);
        delay(1000);
        digitalWrite(LED_BUILTIN, LOW);
        delay(1000);
        digitalWrite(LED_BUILTIN, HIGH);
    }

    int attempts = 0;

    while (status != WL_CONNECTED) {
        digitalWrite(LED_BUILTIN, LOW);

        status = WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

        if (status == WL_CONNECTED) {
            break;
        }

        delay(1000);
        // Watchdog.reset();
        attempts++;
        if (attempts > 10) {
            NVIC_SystemReset();
        }
    }
}

void setup() {
    pinMode(LED_BUILTIN, OUTPUT);
    FastLED.addLeds<WS2812B, DATA_PIN, GRB>(leds, NUM_LEDS);

    connect();

    ArduinoOTA.begin(WiFi.localIP(), "arduino", "password", InternalStorage);
    timeClient.begin();
    server.begin();
}

WiFiClient currentConnection;

int opNum = 0;
uint8_t data[9];
int read = 0;

void handleServer() {
    static Op op = Op::None;

    if (!currentConnection.connected()) {
        currentConnection.stop();
        currentConnection = server.available();
        opNum = 0;
        op = Op::None;
    } else {
        while (currentConnection.available()) {
            uint8_t byte = currentConnection.read();
            if (op == Op::None) {
                op = Op(byte);
                opNum++;
                read = 0;
                if (op == Op::End) {
                    currentConnection.stop();
                    op = Op::None;
                }
            } else {
                if (op == Op::Get) {
                    handleGet(Variable(byte));
                    op = Op::None;
                } else if (op == Op::Set) {
                    if (read < 9) {
                        data[read] = byte;
                        read++;
                        if (read == 9) {
                            handleSet();
                            op = Op::None;
                        }
                    }
                }
            }
        }
    }
}

void handleGet(Variable var) {
    currentConnection.write(opNum);
    switch (var) {
        case Variable::HourColor:
            sendColor(state.hourColor);
            break;
        case Variable::MinuteColor:
            sendColor(state.minuteColor);
            break;
        case Variable::SecondColor:
            sendColor(state.secondColor);
            break;
        case Variable::MsColor:
            sendColor(state.msColor);
            break;
        case Variable::Mode:
            currentConnection.write(char(state.mode));
            break;
        case Variable::ShowMs:
            currentConnection.write(char(state.showMs));
            break;
        case Variable::StartTime:
            currentConnection.write(char(state.stopwatchStartTime));
            break;
        case Variable::PrevTime:
            currentConnection.write(char(state.stopwatchPrevTime));
            break;
        default:
            break;
    }
}

void handleSet() {
    Variable var = Variable(data[0]);
    switch (var) {
        case Variable::HourColor:
            setColor(state.hourColor);
            break;
        case Variable::MinuteColor:
            setColor(state.minuteColor);
            break;
        case Variable::SecondColor:
            setColor(state.secondColor);
            break;
        case Variable::MsColor:
            setColor(state.msColor);
            break;
        case Variable::Mode:
            state.mode = Mode(data[1]);
            break;
        case Variable::ShowMs:
            state.showMs = bool(data[1]);
            break;
        case Variable::StartTime:
            setLong(state.stopwatchStartTime);
            break;
        case Variable::PrevTime:
            setLong(state.stopwatchPrevTime);
            break;
        default:
            break;
    }
}

void setLong(unsigned long long &var) {
    var = (data[1] << 56) + (data[2] << 48) + (data[3] << 40) + (data[4] << 32) + (data[5] << 24) + (data[6] << 16) + (data[7] << 8) + data[8];
}

void sendLong(unsigned long long var) {
    currentConnection.write((byte)(var >> (8 * 7)));
    currentConnection.write((byte)(var >> (8 * 6)));
    currentConnection.write((byte)(var >> (8 * 5)));
    currentConnection.write((byte)(var >> (8 * 4)));
    currentConnection.write((byte)(var >> (8 * 3)));
    currentConnection.write((byte)(var >> (8 * 2)));
    currentConnection.write((byte)(var >> (8 * 1)));
    currentConnection.write((byte)(var >> (8 * 0)));
}

void sendColor(CRGB color) {
    currentConnection.write(color.r);
    currentConnection.write(color.g);
    currentConnection.write(color.b);
}

void setColor(CRGB &color) {
    color.r = data[1];
    color.g = data[2];
    color.b = data[3];
}

int lastSecond = 0;
int startMs = 0;

void loop() {
    if (WiFi.status() != WL_CONNECTED) {
        NVIC_SystemReset();
    }

    timeClient.update();

    handleServer();

    ArduinoOTA.poll();

    if (timeClient.getSeconds() != lastSecond) {
        startMs = millis();
        lastSecond = timeClient.getSeconds();
    }

    int hours = 0;
    int minutes = 0;
    int seconds = 0;
    int ms = 0;

    if (state.mode == Mode::Clock) {
        hours = (timeClient.getHours() + state.timeZoneOffset) % 12;

        minutes = timeClient.getMinutes();
        seconds = timeClient.getSeconds();

        ms = (millis() - startMs) % 1000;
    } else if (state.mode == Mode::Stopwatch) {
        long long msPassed = (millis() - startMs) % 1000;
        long long totalMs = (long long)(timeClient.getEpochTime()) * 1000LL + msPassed;
        long long stopwatchMs = (totalMs - state.stopwatchStartTime) + state.stopwatchPrevTime;

        hours = int(stopwatchMs / 1000.0 / 60.0 / 60.0) % 12;
        minutes = int(stopwatchMs / 1000.0 / 60.0) % 60;
        seconds = int(stopwatchMs / 1000.0) % 60;
        ms = int(stopwatchMs % 1000);
    }

    while (hours < 0) {
        hours += 12;
    }

    int msInd = (ms / 1000.0) * 60.0;

    int hoursInd = int(hours * 5 + floor(minutes / 12.0)) % 60;

    while (hoursInd < 0) {
        hoursInd += 60;
    }

    for (int x = 0; x < NUM_LEDS; x++) {
        leds[x] = CRGB::Black;
    }

    hoursInd = range(0, NUM_LEDS, hoursInd);
    minutes = range(0, NUM_LEDS, minutes);
    seconds = range(0, NUM_LEDS, seconds);
    msInd = range(0, NUM_LEDS, msInd);

    if (state.showMs) {
        leds[msInd] = state.msColor;
    }

    leds[seconds] = state.secondColor;
    leds[minutes] = state.minuteColor;
    leds[hoursInd] = state.hourColor;

    if (hoursInd == minutes) {
        leds[hoursInd] = blend(state.hourColor, state.minuteColor, 128);
    }
    if (hoursInd == seconds) {
        leds[hoursInd] = blend(state.hourColor, state.secondColor, 128);
    }
    if (minutes == seconds) {
        leds[minutes] = blend(state.minuteColor, state.secondColor, 128);
    }

    FastLED.setBrightness(state.brightness);

    FastLED.show();
}