#ifndef USER_APPLICATIONS_TIKITORCH_TIKICANDLE_H_
#define USER_APPLICATIONS_TIKITORCH_TIKICANDLE_H_

#include <FastLED.h>
#include "Torch.h"

#define CANDLE_DOWN		-1
#define CANDLE_FLICKER	0
#define CANDLE_UP		1

using namespace NSFastLED;

extern CRGB leds[NUM_LEDS];

class TikiCandle {
public:
	TikiCandle();
	virtual ~TikiCandle();

	bool init(HSVHue, int, int, int, int);
	void run(int);
	void switchDirection();
	void setHueDirection();
	void flicker();
	void oscillate();
	void seeTheRainbow();

private:
	CHSV candles[NUM_LEDS];
	int mDir;
	bool mIsFlickering;
	int mBaseV;
	int mLikely;
	int mHueTargetHigh;
	int mHueTargetLow;
	int mHueChange;
	int mHueUpdate;
	int m_toUpdateHue;
};

#endif