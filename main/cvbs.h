#ifndef CVBS_H
#define CVBS_H

#include "dmaVideoBuffer.h"
#include "cvbsModes.h"

struct cvbs {
    struct cvbsMode mode;
	int bufferCount;
	bool doColorburst;
	int bits;
	void* interlacingLUT;
	int backBuffer;
    struct dmaBuff dmaBuffer;
	bool usePsram;
	int dmaChannel;
};

extern const struct cvbs CVBS_DEFAULT_CONFIG;

struct cvbs cvbsDefaultConfig();
void cvbsDeInit(struct cvbs *cvbs);
void fillbufferwithvalueforlength(struct cvbs *cvbs, int value, int len, int offset, int line, int bufferNumber);
//line index might be a problem
void populateVsync(struct cvbs *cvbs, bool halfOffset, int syncLevel, int numPre, int numSync, int numPost, int *lineIndex, int bufferNumber);
void populateHsync(struct cvbs *cvbs, int syncLevel, int blankLevel, double phaseIncrementPerSample, int *lineIndex, int bufferNumber);
bool cvbsInit(struct cvbs *cvbs, int* pins, struct cvbsMode mode, int bits);
bool cvbsStart(struct cvbs *cvbs);
bool cvbsShow(struct cvbs *cvbs);
void onebitdot(struct cvbs *cvbs, uint32_t x, uint32_t y, bool state);
void onebitimage(struct cvbs *cvbs, uint8_t* imagedata, int width, int height, int xoffset, int yoffset);
void onebitchangemask(struct cvbs *cvbs, uint8_t* imagedata, uint8_t* changemask, int width, int height, int xoffset, int yoffset);
bool monodot(struct cvbs *cvbs, uint32_t x, uint32_t y, uint32_t value);
void monoimage(struct cvbs *cvbs, uint8_t* imagedata, int width, int height, int xoffset, int yoffset);
bool colordot(struct cvbs *cvbs, uint32_t x, uint32_t y, uint8_t rgb332);
void fillbufferwithvalue(struct cvbs *cvbs, uint32_t value);
void dotdit(struct cvbs *cvbs, uint32_t x, uint32_t y, uint8_t r, uint8_t g, uint8_t b);
int accessbuffervalue(struct cvbs *cvbs, int x, int y, int buffervalue);
void dumpbuffer(struct cvbs *cvbs, int buffernumber);
void dumpbufferline(struct cvbs *cvbs, int y, int buffernumber);
void modulatebuffer(struct cvbs *cvbs, uint32_t carrier, int buffernumber);
void clear(struct cvbs *cvbs, int value);
bool ispositioninvalid(struct cvbs *cvbs, uint32_t x, uint32_t y);
void calculateinterlace(struct cvbs *cvbs, uint32_t *y);
void writePixel8(void* linePtr, uint32_t x, uint32_t value);
void writePixel16(void* linePtr, uint32_t x, uint32_t value);
uint32_t calculatetotalxoffset(struct cvbs *cvbs, uint32_t xoffset);
void attachPinToSignal(int pin, int signal);
#endif //VGA_h
