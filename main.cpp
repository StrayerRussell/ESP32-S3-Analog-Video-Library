#include "PhazeCVBS.h"
#include "USB.h"
#include "testimages.h"
#include "CVBSGfxWrapper.h"
#include <Fonts/FreeMonoBoldOblique24pt7b.h>
#include <Fonts/FreeSerif24pt7b.h>
#include <zlib_turbo.h>
#define DISPLAY_WIDTH 720
#define DISPLAY_HEIGHT 480

zlib_turbo zt;

const size_t MAX_DECODED_DATA_SIZE = 43208; // Adjust the size based on your image resolution and color depth
uint8_t decodedData[MAX_DECODED_DATA_SIZE];

bool decompressZlib(uint8_t* inputBuffer, int inputSize, uint8_t* outputBuffer) {
  //ws.textAll("DecompressionBegin");
  zt.inflate_init((uint8_t *)outputBuffer, MAX_DECODED_DATA_SIZE);
  int rc = zt.inflate(inputBuffer, inputSize);
  if (rc == ZT_SUCCESS) {
    //ws.textAll("DecompressionEnd");
    return 1;
  } else {
    printf("decompressionFail\n");
    return 0;
  }
  //ws.textAll("sendNextFrame");
}

const PinConfig pins(11,12,13,14,15,16,17,18,-1,-1,-1,-1,-1,-1,-1,-1);
//const PinConfig pins(1,2,3,4,5,6,7,8,-1,-1,-1,-1,-1,-1,-1,-1);
//const PinConfig pins(18,17,16,15,14,13,12,11,-1,-1,-1,-1,-1,-1,-1,-1);
//const PinConfig pins(8,7,6,5,4,3,2,1,-1,-1,-1,-1,-1,-1,-1,-1);

CVBS cvbs;
//CVBSMode mode = CVBSMode::MODE_720x480x30NTSC;
CVBSMode mode = CVBSMode::MODE_960x480x30NTSC;
//CVBSMode mode = CVBSMode::MODE_FractionalDivi;
//CVBSMode mode = CVBSMode::MODE_960x540x30NTSC;
//CVBSMode mode = CVBSMode::MODE_20MHZNTSC;
//CVBSMode mode = CVBSMode::MODE_21MHZNTSC;
//CVBSMode mode = CVBSMode::MODE_26MHZNTSC;
//CVBSMode mode = CVBSMode::MODE_30MHZNTSC;
//CVBSMode mode = CVBSMode::MODE_40MHZNTSC; //does not work
//CVBSMode mode = CVBSMode::MODE_60MHZNTSC; // does not work

GfxWrapper<CVBS> gfx(cvbs, mode.VisibleLineSamples, mode.VisibleLines);

uint8_t linearityCompensation[257];

void setup() {
  Serial.begin(115200);

  // controlls whether or not the CVBS library double buffers
  cvbs.bufferCount = 2;

  // experimental color feature
  cvbs.doColorburst = false;
  //cvbs.doColorburst = true;

	if(!cvbs.init(pins, mode, 8)) while(1) delay(1);
  cvbs.start();
  // White value = 255
  // Black/Sync value = 71

  int offset = 0;
  for(int x = 0; x < 257; x++) {
    //cvbs.colordot(x, y, color);
    switch(x) {
      case 55:
        offset += 5;
        break;
      case 115:
        offset += 3;
        break;
    }
    linearityCompensation[x] = (x + offset);
  }
  for(int i = 0; i < 257; i++) {
    printf("linearityCompensation index: %d, value: %d\n", i, linearityCompensation);
  }

  printf("CPU Clock Speed: %d Mhz\n",ESP.getCpuFreqMHz());
  printf("Flash Speed: %d Mhz\n",ESP.getFlashChipSpeed());
  printf("Flash Size: %d \n",ESP.getFlashChipSize());
  printf("Free Psram: %d \n",ESP.getFreePsram());
  printf("Psram Size: %d \n",ESP.getPsramSize());

  decompressZlib((uint8_t*)static_zlib, (int)sizeof(static_zlib), decodedData);
  cvbs.onebitimage(decodedData, 720, 480, 120, 0);
  cvbs.show();
}

int color = 0;

int curentMillis = 0;

void loop() {
  /*
  //printf("Color Number: %d\n", color);
  for (int y = 0; y < 540; y++) {
    int color = 0; // Reset color at the start of each line
    for (int x = 0; x < 960; x += 5) { // Increment x by 5
      // Set 5 pixels to the same color
      for (int z = 0; z < 5; z++) {
        if (x + z < 960) { // Ensure we don't write out of bounds
          cvbs.monodot(x + z, y, linearityCompensation[color] + 71);
        }
      }
      color = (color + 1) % 256; // Cycle color between 0 and 255
    }
  }
  cvbs.show();
  */
  //color++;
  //delay(500);
  //color = color > 255 ? 0 : color;
  //printf("DispMs: %d\n", millis() - displayMillis);

  sleep(1);
  curentMillis = millis();
  cvbs.onebitimage(decodedData, 720, 480, 120, 0);
  cvbs.show();
  printf("Drawing Time: %d ms\n", millis() - curentMillis);
}

