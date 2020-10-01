class XPT2046_Touchscreen {
public:
	constexpr XPT2046_Touchscreen(uint8_t cspin, uint8_t clk, uint8_t din, uint8_t dout)
		: csPin(cspin), clk(clk), din(din), dout(dout) { }
	bool begin();
	bool readData(int32_t &x, int32_t &y) { update(); if (zraw<50) return false; x= xraw, y= yraw; return true; }

private:
	uint32_t SPItransfer(uint32_t v, int nb);
	uint8_t SPItransfer(uint8_t v) { return SPItransfer(v, 8); }
	uint16_t SPItransfer16(uint16_t v)  { return SPItransfer(v, 16); }
	void update();
	uint8_t csPin, clk, din, dout;
	int32_t xraw=0, yraw=0, zraw=0;
	uint32_t msraw=0;
};
