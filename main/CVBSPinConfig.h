#ifndef PINCONFIG_H
#define PINCONFIG_H
class PinConfig;
class PinConfig
{
  public:
    static const PinConfig CVBSPins;
  public:
    int pins[16];

  PinConfig() {};

  PinConfig(
    int p0, int p1, int p2, int p3,
    int p4, int p5, int p6, int p7,
    int p8 = -1, int p9 = -1, int p10 = -1, int p11 = -1,
    int p12 = -1, int p13 = -1, int p14 = -1, int p15 = -1) 
    {
      pins[0] = p0;
      pins[1] = p1;
      pins[2] = p2;
      pins[3] = p3;
      pins[4] = p4;
      pins[5] = p5;
      pins[6] = p6;
      pins[7] = p7;
      pins[8] = p8;
      pins[9] = p9;
      pins[10] = p10;
      pins[11] = p11;
      pins[12] = p12;
      pins[13] = p13;
      pins[14] = p14;
      pins[15] = p15;
    }
};

const PinConfig PinConfig::CVBSPins(11,12,13,14,15,16,17,18,-1,-1,-1,-1,-1,-1,-1,-1);

#endif //PINCONFIG_H