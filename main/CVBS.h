#ifndef CVBS_H
#define CVBS_H

#include "DMAVideoBuffer.h"
#include "CVBSModes.h"

class CVBS
{
	public:
	CVBSMode mode;
	int bufferCount;
	bool doColorburst;
	int bits;
	void* interlacingLUT;
	double* sinLUT;
	double* cosLUT;
	int backBuffer;
	DMAVideoBuffer *dmaBuffer;
	bool usePsram;
	int dmaChannel;
	
	public:
	CVBS();
	~CVBS();
	void fillbufferwithvalueforlength(int value, int len, int offset, int line, int bufferNumber);
	void populateVsync(bool halfOffset, int syncLevel, int numPre, int numSync, int numPost, int &lineIndex, int bufferNumber);
	void populateHsync(int syncLevel, int blankLevel, double phaseIncrementPerSample, int &lineIndex, int bufferNumber);
	bool init(int* pins, const CVBSMode mode, int bits);
	bool start();
	bool show();
	void onebitdot(uint32_t x, uint32_t y, bool state);
	void onebitimage(uint8_t* imagedata, int width, int height, int xoffset, int yoffset);
	void onebitchangemask(uint8_t* imagedata, uint8_t* changemask, int width, int height, int xoffset, int yoffset);
	bool monodot(uint32_t x, uint32_t y, uint32_t value);
	void monoimage(uint8_t* imagedata, int width, int height, int xoffset, int yoffset);
	bool colordot(uint32_t x, uint32_t y, uint8_t rgb332);
	bool colordot(uint32_t x, uint32_t y, double lum, double i, double q);
	void fillbufferwithvalue(uint32_t value);
	void dotdit(uint32_t x, uint32_t y, uint8_t r, uint8_t g, uint8_t b);
	int accessbuffervalue(int x, int y, int buffervalue);
	void dumpbuffer(int buffernumber);
	void dumpbufferline(int y, int buffernumber);
	void modulatebuffer(uint32_t carrier, int buffernumber);
	void clear(int value);
	bool ispositioninvalid(uint32_t x, uint32_t y);
	void calculateinterlace(uint32_t &y);
	void writePixel8(void* linePtr, uint32_t x, uint32_t value);
	void writePixel16(void* linePtr, uint32_t x, uint32_t value);
	uint32_t calculatetotalxoffset(uint32_t xoffset);
	protected:
	void attachPinToSignal(int pin, int signal);
};

#endif //VGA_h
