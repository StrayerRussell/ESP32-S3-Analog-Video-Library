#ifndef CVBSMODE_H
#define CVBSMODE_H
#include <stdint.h>

struct cvbsType {
    int width;
    int height;
    float burstFreq;
    float lineUs;
    float fPorchUs;
    float hSyncUs;
    float bPorchUs;
    float brstStartUs;
    float burstUs;
    float visibleUs;
    float eqPulseUs;
    float broadPulseUs;
};

extern const struct cvbsType NTSC;

struct cvbsMode {
    int EqPulseSamples;
    int BroadPulseSamples;
    int HSyncSamples;
    int HBackSamples;
    int ClrBrstHBackOffset;
    int ClrBrstSamples;
    int ClrBrstFreq;
    int VisibleLineSamples;
    int HFrontSamples;
    int TotalLines;
    int VisibleLines;
    int SyncType;
    bool Interlaced;
    int Frequency;
    int TotalLineSamples;
};

extern const struct cvbsMode MODE_720x480x30NTSC;
extern const struct cvbsMode MODE_960x480x30NTSC;
extern const struct cvbsMode MODE_FractionalDivi;
extern const struct cvbsMode MODE_960x540x30NTSC;
extern const struct cvbsMode MODE_21MHZNTSC;

#endif //MODE_h
