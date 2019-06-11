/*
 * TikiCandle.cpp
 *
 *  Created on: Jun 13, 2016
 *      Author: pete
 */

#include "TikiCandle.h"

TikiCandle::TikiCandle()
{
	mIsFlickering = false;
	mBaseV = 0;
	mDir = CANDLE_UP;
	mLikely = 100;
	mHueTargetHigh = 0;
	mHueTargetLow = 0;
	mHueChange = 0;
	mHueUpdate = 0;
	m_toUpdateHue = 0;
}

TikiCandle::~TikiCandle()
{
}

void TikiCandle::seeTheRainbow()
{
	hsv2rgb_rainbow(candles, leds, NUM_LEDS);
	FastLED.show();
}

bool TikiCandle::init(HSVHue c, int tl, int th, int l, int h)
{
    for( int i = 0; i < NUM_LEDS; i++) {
        candles[i].h = c;
        candles[i].v = 100;
        candles[i].s = 255;
    }
    if (l < 4)
    	l = 4;
    mLikely = l;

    if (h < 1)
    	h = 1;
    mHueChange = h;

    mHueTargetHigh = th;
    mHueTargetLow = tl;

    return true;
}

void TikiCandle::switchDirection()
{
	int rval = random(0, mLikely);

//	Serial.printf("Random test value for direction is %d\n", rval);
	if (candles[0].v <= 30) {
//		Serial.println("Setting dir to CANDLE_UP as 0 is less than 30");
		mDir = CANDLE_UP;
		return;
	}

	if (candles[0].v >= 200) {
//		Serial.println("Setting dir to CANDLE_UP as 0 is less than 30");
		mDir = CANDLE_DOWN;
		return;
	}

	if (rval == 1) {
		mDir = CANDLE_DOWN;
	}
	if (rval == 2) {
		mDir = CANDLE_FLICKER;
	}
	if (rval == 3) {
		mDir = CANDLE_UP;
	}
//	Serial.printf("Setting dir to %d\n", mDir);
}

void TikiCandle::setHueDirection()
{
	int rval = random(0, mLikely);

//	Serial.printf("Random test value for hue is %d\n", rval);
	if (candles[0].h >= mHueTargetHigh) {
//		Serial.println("Setting hue update value to -1");
		mHueUpdate = -1;
		return;
	}

	if (candles[0].h <= mHueTargetLow) {
//		Serial.println("Setting hue update value to 1");
		mHueUpdate = 1;
		return;
	}

	switch (rval) {
	case 0:
		mHueUpdate = 0;
		break;
	case 1:
		mHueUpdate = -1;
		break;
	case 2:
		mHueUpdate = 1;
		break;
	}
//	Serial.printf("Set hue update value to %d\n", mHueUpdate);
}

void TikiCandle::flicker()
{
	int flickers = random(2, 5);
	int value = candles[0].v;

	for (int i = 0; i < flickers; i++) {
		for (int j = 0; j < NUM_LEDS; j++) {
			candles[j].v = candles[j].v / (random(1, 3));
		}
		seeTheRainbow();
		delay(random(250, 500));

		for (int j = 0; j < NUM_LEDS; j++) {
			candles[j].v = value;
		}
		seeTheRainbow();
		delay(random(100, 300));
	}
}

void TikiCandle::oscillate()
{
	for (int i = 0; i < NUM_LEDS; i++) {
		candles[i].h += mHueUpdate;
		candles[i].v += mDir;
	}
//	Serial.println("About to see the rainbow");
	seeTheRainbow();
	delay(20);
}

void TikiCandle::run()
{
	switchDirection();
	if (m_toUpdateHue++ == 5) {
		setHueDirection();
		m_toUpdateHue = 0;
	}
	else {
		mHueUpdate = 0;
	}
	oscillate();
}