#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "cvbs.h"
#include "testimages.h"
#define DISPLAY_WIDTH 720
#define DISPLAY_HEIGHT 480

//const size_t MAX_DECODED_DATA_SIZE = 43208; // Adjust the size based on your image resolution and color depth
//uint8_t decodedData[MAX_DECODED_DATA_SIZE];

int pins[16] = {11,12,13,14,15,16,17,18,-1,-1,-1,-1,-1,-1,-1,-1};
//const PinConfig pins(1,2,3,4,5,6,7,8,-1,-1,-1,-1,-1,-1,-1,-1);
//const PinConfig pins(18,17,16,15,14,13,12,11,-1,-1,-1,-1,-1,-1,-1,-1);
//const PinConfig pins(8,7,6,5,4,3,2,1,-1,-1,-1,-1,-1,-1,-1,-1);


struct cvbs cvbs;
struct cvbsMode mode;

void app_main(void) 
{
    cvbs = CVBS_DEFAULT_CONFIG;
    mode = MODE_960x480x30NTSC;
    //mode = MODE_21MHZNTSC;

    // controls whether or not the CVBS library double buffers
    cvbs.bufferCount = 1;

    // experimental color feature
    //cvbs.doColorburst = false;
    cvbs.doColorburst = true;

    if(!cvbsInit(&cvbs, pins, mode, 8)) {
        printf("Failed to Initialize, Halting\n");
        while(1) {
            vTaskDelay(pdMS_TO_TICKS(1000));
            //delay(1);
        }
    }
    cvbsStart(&cvbs);
    // White value = 255
    // Black/Sync value = 71

    //onebitimage(&cvbs, bitonal_image, 720, 480, 120, 0);

    int color = 0; // Reset color at the start of each line
    
    float colorbars[8][3] = {
        {100, 0.0, 0},
        {89.5, 82.70, 167.1},
        {72.3, 116.9, 283.5},
        {61.8, 109.2, 240.7},
        {45.7, 109.2, 60.70},
        {35.2, 116.9, 103.5},
        {18.0, 82.70, 347.1},
        {7.50, 0.000, 0.000}
    };

    float colorbars_yiq[8][3] = {
        { 1.0000,  0.0000, -0.0000 },
        { 0.8946,  0.2410, -0.2334 },
        { 0.7234, -0.4469, -0.1586 },
        { 0.6180, -0.2060, -0.3920 },
        { 0.4570,  0.2060,  0.3920 },
        { 0.3516,  0.4469,  0.1586 },
        { 0.1805, -0.2410,  0.2334 },
        { 0.0750,  0.0000,  0.0000 },
    };

    int rgb [8][3] = 
    {
        {255, 255, 255}, // 100% White
        {255, 255,   0}, // 100% Yellow
        {  0, 255, 255}, // 100% Cyan
        {  0, 255,   0}, // 100% Green
        {255,   0, 255}, // 100% Magenta
        {255,   0,   0}, // 100% Red
        {  0,   0, 255}, // 100% Blue
        {  0,   0,   0}  // Black
    };

    while(1) 
    {
        printf("Clr: %d\n", color);
        for (int y = 0; y < 160; y++)
        {
            for (int x = 0; x < 960; x++)
            {
                //onebitdot(&cvbs, x + z, y, true);
                //monodot(&cvbs, x + z, y, 100);
                iredot(&cvbs, x, y, colorbars[x/120][0], colorbars[x/120][1], colorbars[x/120][2]);
            }
            vTaskDelay(pdMS_TO_TICKS(10));
        }
        for (int y = 161; y < 320; y++)
        {
            for (int x = 0; x < 960; x++)
            {
                //onebitdot(&cvbs, x + z, y, true);
                //monodot(&cvbs, x + z, y, 100);
                yiqdot(&cvbs, x, y, colorbars_yiq[x/120][0], colorbars_yiq[x/120][1], colorbars_yiq[x/120][2]);
            }
            vTaskDelay(pdMS_TO_TICKS(10));
        }
        for (int y = 321; y < 480; y++)
        {
            for (int x = 0; x < 960; x++)
            {
                //onebitdot(&cvbs, x + z, y, true);
                //monodot(&cvbs, x + z, y, 100);
                rgbdot(&cvbs, x, y, rgb[x/120][0], rgb[x/120][1], rgb[x/120][2]);
            }
            vTaskDelay(pdMS_TO_TICKS(10));
        }
        color = (color >= 50) ? 0 : (color + 1);
        cvbsShow(&cvbs);
        vTaskDelay(pdMS_TO_TICKS(10));

    }
}
