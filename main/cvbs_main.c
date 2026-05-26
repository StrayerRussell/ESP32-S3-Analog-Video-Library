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

    // controls whether or not the CVBS library double buffers
    cvbs.bufferCount = 2;

    // experimental color feature
    cvbs.doColorburst = false;
    //cvbs.doColorburst = true;

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

   onebitimage(&cvbs, bitonal_image, 720, 480, 120, 0);

    while(1) {
        /*
        //printf("Color Number: %d\n", color);
        for (int y = 0; y < 540; y++)
        {
            int color = 0; // Reset color at the start of each line
            for (int x = 0; x < 960; x += 5)
            {
                for (int z = 0; z < 5; z++)
                {
                    if (x + z < 960)
                    {
                        //cvbs.monodot(x + z, y, linearityCompensation[color] + 71);
                        cvbs.monodot(x + z, y, color + 71);
                    }
                }
                color = (color + 1) % 256; // Cycle color between 0 and 255
            }
        }
        cvbs.show();
        */
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
