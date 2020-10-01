#include <arduino.h>
#include "touch.h"

uint32_t XPT2046_Touchscreen::SPItransfer(uint32_t v, int nb)
{
	uint32_t r= 0;
	while (--nb>=0)
	{
		delayNanoseconds(500);
		digitalWrite(clk, HIGH);
		digitalWrite(dout, (v&(1<<nb))!=0 ? HIGH : LOW);
		delayNanoseconds(500);
		r= (r<<1) + (digitalRead(din)==HIGH ? 1 : 0);
		digitalWrite(clk, LOW);
	}
	return r;
}

bool XPT2046_Touchscreen::begin()
{
	pinMode(csPin, OUTPUT); digitalWrite(csPin, HIGH);
	pinMode(clk, OUTPUT); digitalWrite(clk, LOW);
	pinMode(dout, OUTPUT); digitalWrite(dout, LOW);
	pinMode(din, INPUT); 
	return true;
}

void XPT2046_Touchscreen::update()
{
	uint32_t now= millis();
	if (now-msraw<50) return;

	digitalWrite(csPin, LOW);
	SPItransfer(0xB1 /* Z1 */);
	int16_t z1 = SPItransfer16(0xC1 /* Z2 */) >> 3;
	/*int16_t z2 = */ SPItransfer16(0x91 /* X */);
	SPItransfer16(0x91 /* X */);  // dummy X measure, 1st is always noisy
	int32_t sx=0, sy=0, mx= 1<<16, Mx= 0, my= 1<<16, My= 0, t;
	t= SPItransfer16(0xD1 /* Y */) >> 3; if (t<mx) mx= t; if (t>Mx) Mx= t; sx+= t;
	t= SPItransfer16(0x91 /* X */) >> 3; if (t<my) my= t; if (t>My) My= t; sy+= t;
	t= SPItransfer16(0xD1 /* Y */) >> 3; if (t<mx) mx= t; if (t>Mx) Mx= t; sx+= t;
	t= SPItransfer16(0x91 /* X */) >> 3; if (t<my) my= t; if (t>My) My= t; sy+= t;
	t= SPItransfer16(0xD1 /* Y */) >> 3; if (t<mx) mx= t; if (t>Mx) Mx= t; sx+= t;
	t= SPItransfer16(0x91 /* X */) >> 3; if (t<my) my= t; if (t>My) My= t; sy+= t;
	t= SPItransfer16(0xD1 /* Y */) >> 3; if (t<mx) mx= t; if (t>Mx) Mx= t; sx+= t;
	t= SPItransfer16(0x91 /* X */) >> 3; if (t<my) my= t; if (t>My) My= t; sy+= t;
	t= SPItransfer16(0xD1 /* Y */) >> 3; if (t<mx) mx= t; if (t>Mx) Mx= t; sx+= t;
	t= SPItransfer16(0           ) >> 3; if (t<my) my= t; if (t>My) My= t; sy+= t;
	digitalWrite(csPin, HIGH);

	if (z1<40) { zraw= 0; return; }
	zraw= z1;
	msraw= now;	// good read completed, set wait
	xraw= (sx-mx-Mx)/4; yraw= (sy-my-My)/4;
	//char tt[100]; sprintf(tt, "raw x:%ld y:%ld", xraw, yraw); Serial.println(tt); 
	return;
}
