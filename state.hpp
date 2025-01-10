#pragma once

#include <FastLED.h>

#include "consts.hpp"

enum class Mode
{
    Clock = 0,
    Stopwatch = 1,
};

enum class Op
{
    None = -1,
    End = 0,
    Get = 1,
    Set = 2,
};

enum class Variable
{
    HourColor = 1,
    MinuteColor,
    SecondColor,
    MsColor,
    Mode,
    StartTime,
    PrevTime,
    ShowMs,
    Brightness,
    On,
    TimeZoneOffset,
};

struct State
{
    CRGB hourColor = CRGB(255, 0, 0);
    CRGB minuteColor = CRGB(0, 255, 0);
    CRGB secondColor = CRGB(0, 0, 255);
    CRGB msColor = CRGB(0, 255, 255);

    uint8_t brightness = 200;
    int8_t timeZoneOffset = -4; // EDT

    Mode mode = Mode::Clock;

    unsigned long long stopwatchStartTime = 0;
    unsigned long long stopwatchPrevTime = 0;

    bool showMs = false;

    bool on = true;
};
