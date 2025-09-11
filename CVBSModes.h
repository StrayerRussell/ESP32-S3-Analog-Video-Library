#ifndef CVBSMODE_H
#define CVBSMODE_H
#include <stdint.h>

class CVBSMode;
class CVBSMode 
{
  public:
  static const CVBSMode MODE_720x480x30NTSC;
  static const CVBSMode MODE_960x480x30NTSC;
  static const CVBSMode MODE_FractionalDivi;
  static const CVBSMode MODE_960x540x30NTSC;
  static const CVBSMode MODE_20MHZNTSC;
  static const CVBSMode MODE_21MHZNTSC;
  static const CVBSMode MODE_26MHZNTSC;
  static const CVBSMode MODE_30MHZNTSC;
  static const CVBSMode MODE_40MHZNTSC;
  static const CVBSMode MODE_60MHZNTSC;
  public:
  uint32_t EqPulseSamples, BroadPulseSamples;
  uint32_t HSyncSamples, HBackSamples, VisibleLineSamples, HFrontSamples;
  uint32_t ClrBrstHBackOffset, ClrBrstSamples, ClrBrstFreq;
  uint32_t TotalLines, VisibleLines;
  uint32_t SyncType;
  bool Interlaced;
  uint32_t Frequency;
  CVBSMode() 
  {
  }

  CVBSMode(const 
  
  CVBSMode &m) 
  {
    this->EqPulseSamples = m.EqPulseSamples;
    this->BroadPulseSamples = m.BroadPulseSamples;
    this->HSyncSamples = m.HSyncSamples;
    this->HBackSamples = m.HBackSamples;
    this->ClrBrstHBackOffset = m.ClrBrstHBackOffset;
    this->ClrBrstSamples = m.ClrBrstSamples;
    this->ClrBrstFreq = m.ClrBrstFreq;
    this->VisibleLineSamples = m.VisibleLineSamples;
    this->HFrontSamples = m.HFrontSamples;
    this->TotalLines = m.TotalLines;
    this->VisibleLines = m.VisibleLines;
    this->SyncType = m.SyncType;
    this->Interlaced = m.Interlaced;
    this->Frequency = m.Frequency;
  }

  CVBSMode(int EqPulseSamples, int BroadPulseSamples, int HSyncSamples, int HBackSamples, int ClrBrstHBackOffset, int ClrBrstSamples, 
  int ClrBrstFreq, int VisibleLineSamples, int HFrontSamples, int TotalLines, int VisibleLines, int SyncType, bool Interlaced, int Frequency)
  {
    this->EqPulseSamples = EqPulseSamples;
    this->BroadPulseSamples = BroadPulseSamples;
    this->HSyncSamples = HSyncSamples;
    this->HBackSamples = HBackSamples;
    this->ClrBrstHBackOffset = ClrBrstHBackOffset;
    this->ClrBrstSamples = ClrBrstSamples;
    this->ClrBrstFreq = ClrBrstFreq;
    this->VisibleLineSamples = VisibleLineSamples;
    this->HFrontSamples = HFrontSamples;
    this->TotalLines = TotalLines;
    this->VisibleLines = VisibleLines;
    this->SyncType = SyncType;
    this->Interlaced = Interlaced;
    this->Frequency = Frequency;
  }

  // functions to return various lengths and sums
  int TotalLineSamples() const {
    return HSyncSamples + HBackSamples + VisibleLineSamples + HFrontSamples;
  }

  int BurstPerSample() const {
    return ((2 * M_PI)/(Frequency/ClrBrstFreq));
  }
};

// I am choosing not to implement PAL. It should be just as achieveable as NTSC but I do not have access to PAL equipment to test
// as well as my applications being very NTSC/960h specific

// Sync Types
// 1 = NTSC
// 2 = PAL
// 3 = YPbPr (SMPTE 274M-2008 compliant 1920 x 1080 component video)

//Need to actually figure out timing stuff. standards compliant timing does not seem to work so IDK what to do
const CVBSMode CVBSMode::MODE_720x480x30NTSC(31, 361, 61, 63, 10, 35, 3579545, 720, 20, 525, 480, 1, 1, 13333333); // this is the one that flashes. I think it is almost syncing but IDK
const CVBSMode CVBSMode::MODE_960x480x30NTSC(43, 500, 84, 87, 13, 52, 3579545, 977, 28, 525, 480, 1, 1, 18461538); // technically not NTSC but 960H, designed for cheap aliexpress microdisplays and security monitors
const CVBSMode CVBSMode::MODE_FractionalDivi(43, 500, 84, 87, 13, 52, 3579545, 977, 28, 585, 540, 1, 1, 17897725); // using fractional divider for testing
const CVBSMode CVBSMode::MODE_960x540x30NTSC(43, 500, 84, 87, 13, 52, 3579545, 977, 28, 585, 540, 1, 1, 18461538); // same as above but with more vertical lines. IDK if it will work but its what the datasheet says
const CVBSMode CVBSMode::MODE_20MHZNTSC(47, 587, 94, 94, 12, 56, 3579545, 1060, 26, 585, 540, 1, 1, 20000000); // 20mhz version for testing and ease of use
const CVBSMode CVBSMode::MODE_21MHZNTSC(50, 591, 101, 103, 13, 62, 3579545, 1156, 30, 585, 540, 1, 1, 21818181); // 21mhz version for closest multiple of NTSC color carrier
const CVBSMode CVBSMode::MODE_26MHZNTSC(62, 722, 122, 126, 18, 75, 3579545, 1440, 40, 525, 480, 1, 1, 26666666);
const CVBSMode CVBSMode::MODE_30MHZNTSC(71, 881, 141, 141, 18, 85, 3579545, 1590, 45, 525, 480, 1, 1, 30000000); // 30mhz for ram access speed test
const CVBSMode CVBSMode::MODE_40MHZNTSC(94, 1174, 188, 188, 24, 120, 3579545, 2120, 60, 525, 480, 1, 1, 40000000); // 40mhz again for bandwidth testing
const CVBSMode CVBSMode::MODE_60MHZNTSC(138, 1626, 282, 282, 36, 180, 3579545, 3180, 90, 525, 480, 1, 1, 60000000); // 60mhz for transmission testing. highest theoretical
#endif //MODE_h