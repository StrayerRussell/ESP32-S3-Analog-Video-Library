#include "cvbs.h"
#include "luts.h"
#include <string.h>
#include <math.h>
#include <driver/gpio.h>
#include <esp_rom_sys.h>
#include <esp_rom_gpio.h>
#include <esp_private/gdma.h>
#include <esp_private/periph_ctrl.h>
#include <hal/gpio_hal.h>
#include <soc/lcd_cam_struct.h>

//uint32_t lineMillis = 0;

/*
#ifndef min
#define min(a,b)((a)<(b)?(a):(b))
#endif
#ifndef max
#define max(a,b)((a)>(b)?(a):(b))
#endif
*/

//borrowed from esp code
#define HAL_FORCE_MODIFY_U32_REG_FIELD(base_reg, reg_field, field_val)		\
{																			\
		uint32_t temp_val = base_reg.val;								    \
		typeof(base_reg) temp_reg;											\
		temp_reg.val = temp_val;											\
		temp_reg.reg_field = (field_val);									\
		(base_reg).val = temp_reg.val;										\
}

#define MAXLVL 255.0f
#define MAXIRE 171.0f
#define IRESCALE (MAXLVL / MAXIRE)
#define SYNCLVL 0.0f
#define BLANKLVL (40.0f * IRESCALE)
#define BRSTAMP  ((40.0f * IRESCALE) / 2.0)
#define CHROMAAMP BRSTAMP * 2.57f

const struct cvbs CVBS_DEFAULT_CONFIG = {.bufferCount = 1, .doColorburst = true, .usePsram = true, .dmaChannel = 0};

struct cvbs cvbsDefaultConfig() {
    struct cvbs cvbs;
    cvbs.bufferCount = 1;
    cvbs.doColorburst = true;
    cvbs.usePsram = true;
    cvbs.dmaChannel = 0;
    cvbs.bits = 0;
    cvbs.backBuffer = 0;
    cvbs.dmaBuffer.valid = false;
    cvbs.dmaBuffer.descriptors = NULL;
    return cvbs;
}

void cvbsDeInit(struct cvbs *cvbs) {
        if (cvbs->dmaBuffer.valid) {
            deInitDmaBuff(&cvbs->dmaBuffer);
        }
		cvbs->bits = 0;
		cvbs->backBuffer = 0;
        return;
}

extern int Cache_WriteBack_Addr(uint32_t addr, uint32_t size);

void attachPinToSignal(int pin, int signal) {
	esp_rom_gpio_connect_out_signal(pin, signal, false, false);
	//gpio_hal_func_sel(GPIO_PIN_MUX_REG[pin], PIN_FUNC_GPIO);
	esp_rom_gpio_pad_select_gpio(pin);
	gpio_set_drive_capability((gpio_num_t)pin, (gpio_drive_cap_t)3);
}

//bool init(int* pins, CVBSMode mode, int bits) {
bool cvbsInit(struct cvbs *cvbs, int* pins, struct cvbsMode mode, int bits) {
    mode.TotalLineSamples = mode.HSyncSamples + mode.HBackSamples + mode.VisibleLineSamples + mode.HFrontSamples;
    cvbs->mode = mode;
    cvbs->bits = bits;
    cvbs->backBuffer = 0;

    // I am not using line doubling but I am keeping clones a variable just in case.
    int Clones = 1;

    initDmaBuff(&cvbs->dmaBuffer, cvbs->mode.TotalLines, cvbs->mode.TotalLineSamples * (cvbs->bits/8), Clones, true, cvbs->usePsram, cvbs->bufferCount);
    if (!cvbs->dmaBuffer.valid) {
        deInitDmaBuff(&cvbs->dmaBuffer);
        printf("Failed to create DMA Buffer\n");
        return false;
    }

    periph_module_enable(PERIPH_LCD_CAM_MODULE);
    periph_module_reset(PERIPH_LCD_CAM_MODULE);
    LCD_CAM.lcd_user.lcd_reset = 1;
    esp_rom_delay_us(100);


    //f=240000000/(n+1)
    //n=240000000/f-1;
    int N = round(240000000.0/(double)mode.Frequency);
    if(N < 2) N = 2;
    //clk = source / (N + b/a)
    LCD_CAM.lcd_clock.clk_en = 1;
    LCD_CAM.lcd_clock.lcd_clk_sel = 2;					// PLL240M
    // - For integer divider, LCD_CAM_LCD_CLKM_DIV_A and LCD_CAM_LCD_CLKM_DIV_B are cleared.
    // - For fractional divider, the value of LCD_CAM_LCD_CLKM_DIV_B should be less than the value of LCD_CAM_LCD_CLKM_DIV_A.
    LCD_CAM.lcd_clock.lcd_clkm_div_num = N;	 // 0 => 256; 1 => 2; 14 compfy
    LCD_CAM.lcd_clock.lcd_clkm_div_a = 0;
    LCD_CAM.lcd_clock.lcd_clkm_div_b = 0;
    LCD_CAM.lcd_clock.lcd_ck_out_edge = 0;			
    LCD_CAM.lcd_clock.lcd_ck_idle_edge = 0;
    LCD_CAM.lcd_clock.lcd_clk_equ_sysclk = 1;

    // puts LCD peripheral in i8080 mode which has every clock pulse output another value from the
    // buffer array withought interrupting data flow for H and V sync pulses. This allows for composite
    // signals such as CVBS video which require consistent data flow and does not require external sync
    LCD_CAM.lcd_ctrl.lcd_rgb_mode_en = 0; 
    LCD_CAM.lcd_user.lcd_2byte_en = (cvbs->bits == 8) ? 0 : 1;
    LCD_CAM.lcd_user.lcd_cmd = 0;
    LCD_CAM.lcd_user.lcd_dummy = 1;
    LCD_CAM.lcd_user.lcd_dout = 1;
    //LCD_CAM.lcd_user.lcd_cmd_2_cycle_en = 0;
    LCD_CAM.lcd_user.lcd_dummy_cyclelen = 0;	//-1;
    LCD_CAM.lcd_user.lcd_dout_cyclelen = 0;
    LCD_CAM.lcd_user.lcd_always_out_en = 1;
    //LCD_CAM.lcd_misc.lcd_bk_en = 1;
    //LCD_CAM.lcd_misc.lcd_vfk_cyclelen = 0;
    //LCD_CAM.lcd_misc.lcd_vbk_cyclelen = 0;
    //LCD_CAM.lcd_ctrl1.lcd_ha_width = mode.VisibleLineSamples;	//12 bit
    //LCD_CAM.lcd_ctrl1.lcd_ht_width = mode.TotalLineSamples;	//12 bit

    //HAL_FORCE_MODIFY_U32_REG_FIELD(LCD_CAM.lcd_ctrl1, lcd_vb_front, mode.BroadPulseSamples + (mode.TotalLineSamples - mode.BroadPulseSamples));	//8bit
    //LCD_CAM.lcd_ctrl.lcd_va_height = mode.VisibleLines * Clones;																																														 //10 bit
    //LCD_CAM.lcd_ctrl.lcd_vt_height = mode.TotalLines;																																																							 //10 bit
    //HAL_FORCE_MODIFY_U32_REG_FIELD(LCD_CAM.lcd_ctrl2, lcd_hsync_position, mode.HSyncSamples);

    LCD_CAM.lcd_misc.lcd_next_frame_en = 0;	//?? limitation

    //printf("LCD Initialized\n");
    //printf("Assigning pins\n");
    if (cvbs->bits == 8) {
        for (int i = 0; i < cvbs->bits; i++)
            if (pins[i] >= 0)
                attachPinToSignal(pins[i], LCD_DATA_OUT0_IDX + i);
    } else if (cvbs->bits == 16) {
        for (int i = 0; i < cvbs->bits; i++)
            if (pins[i] >= 0)
                attachPinToSignal(pins[i], LCD_DATA_OUT0_IDX + i);
    }

    gdma_channel_alloc_config_t dma_chan_config = {};

    gdma_channel_handle_t dmaCh = NULL;

    esp_err_t ret = gdma_new_ahb_channel(&dma_chan_config, &dmaCh, NULL);
    if(ret != ESP_OK)
    {
        printf("Failed to allocate GDMA channel, error: %d\n", ret);
        return false;
    }

    cvbs->dmaChannel = (int)dmaCh;
    ret = gdma_connect(dmaCh, GDMA_MAKE_TRIGGER(GDMA_TRIG_PERIPH_LCD, 0));
    if(ret != ESP_OK)
    {
        printf("Failed to connect GDMA to LCD Peripheral, error: %d\n", ret);
        return false;
    }

    gdma_transfer_config_t transfer_config = {
        .max_data_burst_size = 64,
        .access_ext_mem = true,
    };

    gdma_config_transfer(dmaCh, &transfer_config);

    //////////////////////////////////////////////////////////

    /* SYNC AND COLORBURST POPULATION SECTION */

    //////////////////////////////////////////////////////////

    int lineIndex = 0;	// keeps track of which line is currently being written to

    // Calculate the phase increment in fixed-point integer math
    cvbs->phaseIncPerSamp = ((2.0 * M_PI * mode.ClrBrstFreq) / mode.Frequency);
    printf("PhaseInc: %lf\n", cvbs->phaseIncPerSamp);

    for (int a = 0; a < cvbs->bufferCount; a++) {
    lineIndex = 0;
    printf("Running Sync Population for Buffer number: %d\n", a);
    switch (cvbs->mode.SyncType) {
        case 1:	//NTSC Sync
            printf("Starting NTSC Sync Population\n");
            //Populates entire framebuffer with Blank level for later sync overwriting
            for (int b = 0; b < cvbs->mode.TotalLines; b++) { fillbufferwithvalueforlength(cvbs, BLANKLVL, cvbs->mode.TotalLineSamples, 0, b, a); }
            // populates V sync for field one
            populateVsync(cvbs, false, 6, 6, 6, &lineIndex, a);
            // populates the H sync and colorburst of all field one lines including 13 blank lines
            populateHsync(cvbs, &lineIndex, a);

            if (cvbs->mode.Interlaced) {
                // populates V sync for field two, begins and ends with a 1/2 line offset to signify that it is field two
                populateVsync(cvbs, true, 6, 6, 6, &lineIndex, a);
                // populates the H sync and colorburst of all field two lines including 13 blank lines
                populateHsync(cvbs, &lineIndex, a);
            }
            break;
        case 2: //PAL Sync
            printf("Starting PAL Sync Population\n");
            //Populates entire framebuffer with Blank level for later sync overwriting
            for (int b = 0; b < cvbs->mode.TotalLines; b++) { fillbufferwithvalueforlength(cvbs, BLANKLVL, cvbs->mode.TotalLineSamples, 0, b, a); }
            // populates V sync for field one
            populateVsync(cvbs, false, 6, 5, 5, &lineIndex, a);
            // populates the H sync and colorburst of all field one lines including 13 blank lines
            populateHsync(cvbs, &lineIndex, a);

            if (cvbs->mode.Interlaced) {
                // populates V sync for field two, begins and ends with a 1/2 line offset to signify that it is field two
                populateVsync(cvbs, false, 5, 5, 4, &lineIndex, a);
                // populates the H sync and colorburst of all field two lines including 13 blank lines
                populateHsync(cvbs, &lineIndex, a);
            }
            break;
        case 3: //YPbPr Sync
            //IDK im not gonna implement the YPbPr sync yet. Im still mad about the negative voltages.
            break;
    }
    }
    //printf("Sync Completed\n");
    return true;
}

void onebitdot(struct cvbs *cvbs, uint32_t x, uint32_t y, bool state) {
	if(ispositioninvalid(cvbs, x, y)) return;
	calculateinterlace(cvbs, &y);
	x = calculatetotalxoffset(cvbs, x);
    switch(cvbs->bits) {
        case 8:
            switch(state) {
                case true:
                    getLineAddr8(&cvbs->dmaBuffer, y, cvbs->backBuffer)[x] = 255;
                    break;
                case false:
                    getLineAddr8(&cvbs->dmaBuffer, y, cvbs->backBuffer)[x] = 71;
                    break;
            }
            break;
        case 16:
            switch(state) {
                case true:
                    getLineAddr16(&cvbs->dmaBuffer, y, cvbs->backBuffer)[x] = 65535;
                    break;
                case false:
                    getLineAddr16(&cvbs->dmaBuffer, y, cvbs->backBuffer)[x] = 65535;
                    break;
            }
            break;
    }
}

void onebitimage(struct cvbs *cvbs, uint8_t* imagedata, int width, int height, int xoffset, int yoffset) {
	int byteIndex = 0, bytesPerLine = width/8;	

	// Calculate the maximum coordinates and check if within bounds
	uint32_t maxX = width + xoffset, maxY = height + yoffset;
	if(ispositioninvalid(cvbs, maxX, maxY)) return;
	// Calculate where in the line the actual image data starts (offset from sync data)
	uint32_t totalxoffset = calculatetotalxoffset(cvbs, xoffset);
	uint16_t max = 255, min = 71;
	void* linePtr = NULL;

	// Precompute the bit expansion lookup table for 1bpp to 8bpp
	// This will create a lookup table where each byte maps to 8 pixels
	DRAM_ATTR static uint8_t bit_expand_lut[256][8];

	for(uint16_t byte = 0; byte < 256; byte++) {
		for(int i = 0; i < 8; i++) {
			bit_expand_lut[byte][i] = (byte & (1 << (7 - i))) ? max : min;
		}
	}
	
	// remember to test preformance impact of function pointers for combining 8 and 16 bit handling
	switch(cvbs->bits) {
		case 8:
            for (int a = yoffset; a < maxY; a++) {
				uint32_t y = a;
				calculateinterlace(cvbs, &y);
				uint32_t x = totalxoffset;
				linePtr = getLineAddr8(&cvbs->dmaBuffer, y, cvbs->backBuffer);
				for (int b = 0; b < bytesPerLine; b++) {
					uint8_t byte = imagedata[byteIndex++];	// Input byte (1bpp)
					// Copy 8 precomputed pixels in one shot (for some reason memcpy is slower than manual copy)
					
					((uint8_t*)linePtr)[x++] = bit_expand_lut[byte][0];
					((uint8_t*)linePtr)[x++] = bit_expand_lut[byte][1];
					((uint8_t*)linePtr)[x++] = bit_expand_lut[byte][2];
					((uint8_t*)linePtr)[x++] = bit_expand_lut[byte][3];
					((uint8_t*)linePtr)[x++] = bit_expand_lut[byte][4];
					((uint8_t*)linePtr)[x++] = bit_expand_lut[byte][5];
					((uint8_t*)linePtr)[x++] = bit_expand_lut[byte][6];
					((uint8_t*)linePtr)[x++] = bit_expand_lut[byte][7];
					
					//memcpy(&((uint8_t*)linePtr)[x], bit_expand_lut[byte], 8);
					//x += 8;	// Advance by 8 pixels
				}
			}
			break;
		case 16:
			max = 65535, min = 18241;
			for (int a = yoffset; a < maxY; a++) {
				uint32_t y = a;
				calculateinterlace(cvbs, &y);
				uint32_t x = totalxoffset;
				linePtr = getLineAddr16(&cvbs->dmaBuffer, y, cvbs->backBuffer);
				for (int b = 0; b < bytesPerLine; b++) {
					memcpy(&((uint16_t*)linePtr)[x], bit_expand_lut[imagedata[++byteIndex]], 8);
					x += 8;	// Advance by 8 pixels
				}
			}
			break;
	}
}

void onebitchangemask(struct cvbs *cvbs, uint8_t* imagedata, uint8_t* changemask, int width, int height, int xoffset, int yoffset) {
	int byteIndex = 0, bytesPerLine = width/8;	

	// Calculate the maximum coordinates and check if within bounds
	uint32_t maxX = width + xoffset, maxY = height + yoffset;
	if(ispositioninvalid(cvbs, maxX, maxY)) return;
	// Calculate where in the line the actual image data starts (offset from sync data)
	uint32_t totalxoffset = calculatetotalxoffset(cvbs, xoffset);
	uint16_t max = 255, min = 71;
	void* linePtr = nullptr;
	switch(cvbs->bits) {
		case 8:
			for (int a = yoffset; a < maxY; a++) {
				uint32_t y = a;
				calculateinterlace(cvbs, &y);
				uint32_t x = totalxoffset;
				linePtr = getLineAddr8(&cvbs->dmaBuffer, y, cvbs->backBuffer);
				for (int b = 0; b < bytesPerLine; b++) {
					//printf("8 bits, y: %d, x: %d\n", y, x);
					switch(changemask[byteIndex]) {
						case 0:
							x += 8;
							break;
						default:
							imagedata[byteIndex] = (imagedata[byteIndex] ^ changemask[byteIndex]);
							((uint8_t*)linePtr)[x++] = (imagedata[byteIndex] & 128) ? max : min;
							((uint8_t*)linePtr)[x++] = (imagedata[byteIndex] & 64) ? max : min;
							((uint8_t*)linePtr)[x++] = (imagedata[byteIndex] & 32) ? max : min;
							((uint8_t*)linePtr)[x++] = (imagedata[byteIndex] & 16) ? max : min;
							((uint8_t*)linePtr)[x++] = (imagedata[byteIndex] & 8) ? max : min;
							((uint8_t*)linePtr)[x++] = (imagedata[byteIndex] & 4) ? max : min;
							((uint8_t*)linePtr)[x++] = (imagedata[byteIndex] & 2) ? max : min;
							((uint8_t*)linePtr)[x++] = (imagedata[byteIndex] & 1) ? max : min;
							break;
					}
					byteIndex++;
				}
			}
			break;
		case 16:
			max = 65535, min = 18241;
			for (int a = yoffset; a < maxY; a++) {
				uint32_t y = a;
				calculateinterlace(cvbs, &y);
				uint32_t x = totalxoffset;
				linePtr = getLineAddr16(&cvbs->dmaBuffer, y, cvbs->backBuffer);
				for (int b = 0; b < bytesPerLine; b++) {
					//printf("8 bits, y: %d, x: %d\n", y, x);
					switch(changemask[byteIndex]) {
						case 0:
							x += 8;
							break;
						default:
							imagedata[byteIndex] = (imagedata[byteIndex] ^ changemask[byteIndex]);
							((uint16_t*)linePtr)[x++] = (imagedata[byteIndex] & 128) ? max : min;
							((uint16_t*)linePtr)[x++] = (imagedata[byteIndex] & 64) ? max : min;
							((uint16_t*)linePtr)[x++] = (imagedata[byteIndex] & 32) ? max : min;
							((uint16_t*)linePtr)[x++] = (imagedata[byteIndex] & 16) ? max : min;
							((uint16_t*)linePtr)[x++] = (imagedata[byteIndex] & 8) ? max : min;
							((uint16_t*)linePtr)[x++] = (imagedata[byteIndex] & 4) ? max : min;
							((uint16_t*)linePtr)[x++] = (imagedata[byteIndex] & 2) ? max : min;
							((uint16_t*)linePtr)[x++] = (imagedata[byteIndex] & 1) ? max : min;
							break;
					}
					byteIndex++;
				}
			}
			break;
	}
}

bool monodot(struct cvbs *cvbs, uint32_t x, uint32_t y, uint32_t value) {
	if(ispositioninvalid(cvbs, x, y)) return false;
	calculateinterlace(cvbs, &y);
	x = calculatetotalxoffset(cvbs, x);
	switch(cvbs->bits) {
		case 8:
			if(value > 255) value = 255;
			if(value < 71) value = 71;
			getLineAddr8(&cvbs->dmaBuffer, y, cvbs->backBuffer)[x] = value;
			break;
		case 16:
			if(value > 65535) value = 65535;
			if(value < 18241) value = 18241;
			getLineAddr16(&cvbs->dmaBuffer, y, cvbs->backBuffer)[x] = value;
			break;
		default:
			return false; // Unsupported bits
	}
	return false;
}

void monoimage(struct cvbs *cvbs, uint8_t* imagedata, int width, int height, int xoffset, int yoffset) {
	int byteIndex = 0;
	//int bytesPerLine = width/8;	

	// Calculate the maximum coordinates
	uint32_t maxX = width + xoffset;
	uint32_t maxY = height + yoffset;
	if(ispositioninvalid(cvbs, maxX, maxY)) return;
	uint32_t totalxoffset = calculatetotalxoffset(cvbs, xoffset);

	int xbound = width + maxX + 1; // +1 as this is one past the last pixel

	//shabingus
	int DMAbuffersize = width * sizeof(uint8_t);

	uint16_t max = 255, min = 71;
	void* linePtr = nullptr;
	switch(cvbs->bits) {
		case 8:
			break;
		case 16:
			max = 65535, min = 18241;
			break;
	}
	switch(cvbs->bits) {
		case 8:
			for (int a = yoffset; a < maxY; a++) {
				uint32_t y = a;
				calculateinterlace(cvbs, &y);
				linePtr = getLineAddr8(&cvbs->dmaBuffer, y, cvbs->backBuffer);
				//memcpy((uint8_t*)linePtr + totalxoffset, imagedata + byteIndex, width);
				//byteIndex += width;
				for (int b = totalxoffset; b < xbound; b++) {
					//printf("Y: %d, X: %d, Value: %d\n", y, b, imagedata[byteIndex]);
					((uint8_t*)linePtr)[b] = (imagedata[byteIndex] < min) ? min : imagedata[byteIndex];
					byteIndex++;
				}
			}
			break;
		case 16:
			for (int a = yoffset; a < maxY; a++) {
				uint32_t y = a;
				calculateinterlace(cvbs, &y);
				linePtr = getLineAddr16(&cvbs->dmaBuffer, y, cvbs->backBuffer);
				for (int b = totalxoffset; b < xbound; b++) {
					((uint16_t*)linePtr)[b] = (imagedata[byteIndex] < min) ? min : imagedata[byteIndex];
					byteIndex++;
				}
			}
			break;
	}
}

bool iredot(struct cvbs *cvbs, uint32_t x, uint32_t y, float lum, float mult, float shft) {
	if(ispositioninvalid(cvbs, x, y)) return false;
	calculateinterlace(cvbs, &y);
	x = calculatetotalxoffset(cvbs, x);
    //int modIndex = (y * cvbs->mode.TotalLineSamples) + x;
    int modIndex = x;

    lum = (lum * IRESCALE);
    mult = ((mult / 2.0) * IRESCALE);

	int chromaValue = round(mult * sin((cvbs->phaseIncPerSamp * modIndex) + ((M_PI/180) * (shft - 33))));
	int colorValue = BLANKLVL + lum + chromaValue;

	switch(cvbs->bits) {
		case 8:
			getLineAddr8(&cvbs->dmaBuffer, y, cvbs->backBuffer)[x] = r2r_lut[colorValue];
            break;
		case 16:
			getLineAddr16(&cvbs->dmaBuffer, y, cvbs->backBuffer)[x] = r2r_lut[colorValue];
            break;
	}

	return false; // Default return if bits is neither 8 nor 16
}

bool yiqdot(struct cvbs *cvbs, uint32_t x, uint32_t y, float y_lum, float i_val, float q_val) {
    if(ispositioninvalid(cvbs, x, y)) return false;
    calculateinterlace(cvbs, &y);
    x = calculatetotalxoffset(cvbs, x);
    //int modIndex = (y * cvbs->mode.TotalLineSamples) + x;
    int modIndex = x;

    float lumaDac = (y_lum * 100.0) * IRESCALE;
    float iDac = i_val * 114.0 * IRESCALE;
    float qDac = q_val * 114.0 * IRESCALE;

    float trigOffset = cvbs->phaseIncPerSamp * modIndex;
    
    int chromaValue = round(iDac * cos(trigOffset) + qDac * sin(trigOffset));
    int colorValue = BLANKLVL + lumaDac + chromaValue;


    switch(cvbs->bits) {
        case 8:
            getLineAddr8(&cvbs->dmaBuffer, y, cvbs->backBuffer)[x] = r2r_lut[colorValue];
            break;
        case 16:
            getLineAddr16(&cvbs->dmaBuffer, y, cvbs->backBuffer)[x] = r2r_lut[colorValue];
            break;
    }

    return false;
}

bool rgbdot(struct cvbs *cvbs, uint32_t x, uint32_t y, int r, int g, int b) {
    if(ispositioninvalid(cvbs, x, y)) return false;
    calculateinterlace(cvbs, &y);
    x = calculatetotalxoffset(cvbs, x);
    //int modIndex = (y * cvbs->mode.TotalLineSamples) + x;
    int modIndex = x;

    // FCC NTSC 1953 Matrix Equations
    float lumaDac = ((0.108461 * r) + (0.212931 * g) + (0.041353 * b)) * IRESCALE;
    float iDac = ((0.199801 * r) - (0.092071 * g) - (0.107730 * b)) * IRESCALE;
    float qDac = ((0.070914 * r) - (0.175258 * g) + (0.104343 * b)) * IRESCALE; 

    float trigOffset = cvbs->phaseIncPerSamp * modIndex;
    
    int chromaValue = round(iDac * cos(trigOffset) + qDac * sin(trigOffset));
    int colorValue = BLANKLVL + lumaDac + chromaValue;


    switch(cvbs->bits) {
        case 8:
            getLineAddr8(&cvbs->dmaBuffer, y, cvbs->backBuffer)[x] = r2r_lut[colorValue];
            break;
        case 16:
            getLineAddr16(&cvbs->dmaBuffer, y, cvbs->backBuffer)[x] = r2r_lut[colorValue];
            break;
    }

    return false;
}

void modulatebuffer(struct cvbs *cvbs, uint32_t carrier, int bufferNumber) { // modulate buffer by a carrier intended for transmission of NTSC via VHF/UHF
	//float colorburstOffset = mode.HSyncSamples - mode.ClrBrstHBackOffset;

	int SinMultiplier = 50;

	// Calculate the phase increment per sample based on the desired frequency
	double phaseIncrement = (2.0 * M_PI * carrier) / cvbs->mode.Frequency;
	float phase = 0.0;	// Initial phase

	for (int y = 0; y < cvbs->dmaBuffer.lines; y++) {
		phase = 0.0;
		for (int x = 0; x < cvbs->dmaBuffer.lineSize; x++) {
			int value = (cvbs->bits == 8) 
                ? getLineAddr8(&cvbs->dmaBuffer, y, cvbs->backBuffer)[x]
			    : getLineAddr16(&cvbs->dmaBuffer, y, cvbs->backBuffer)[x];

			// Only modulate if the value is non-zero
			int modulation = round(SinMultiplier + (sin(phase) * SinMultiplier));
			value += modulation;

			// Store the modulated value back
			switch(cvbs->bits) {
				case 8:
					getLineAddr8(&cvbs->dmaBuffer, y, bufferNumber)[x] = value;
					break;
				case 16:
					getLineAddr16(&cvbs->dmaBuffer, y, bufferNumber)[x] = value;
					break;
			}

			phase += phaseIncrement;
		}
	}
}


void clear(struct cvbs *cvbs, int value) {
	for(int y = 0; y < cvbs->mode.VisibleLines; y++)
			for(int x = 0; x < cvbs->mode.VisibleLineSamples; x++)
					monodot(cvbs, x, y, value);
}

void fillbufferwithvalueforlength(struct cvbs *cvbs, int value, int len, int offset, int line, int bufferNumber) {
	for (int i = 0; i < len; i++) {
		switch(cvbs->bits) {
			case 8:
				getLineAddr8(&cvbs->dmaBuffer, line, bufferNumber)[i + offset] = r2r_lut[value];
				break;
			case 16:
				getLineAddr16(&cvbs->dmaBuffer, line, bufferNumber)[i + offset] = r2r_lut[value];
				break;
		}
	}
}

void fillbufferwithvalue(struct cvbs *cvbs, uint32_t value) {
	for (int y = 0; y < cvbs->mode.TotalLines; y++) {
		for (int x = 0; x < cvbs->mode.TotalLineSamples; x++) {
			switch(cvbs->bits) {
				case 8:
					getLineAddr8(&cvbs->dmaBuffer, y, cvbs->backBuffer)[x] = r2r_lut[value];
					break;
				case 16:
					getLineAddr16(&cvbs->dmaBuffer, y, cvbs->backBuffer)[x] = r2r_lut[value];
					break;
			}
		}
	}
}

int accessbuffervalue(struct cvbs *cvbs, int x, int y, int bufferNumber) {
	if (bufferNumber >= cvbs->dmaBuffer.bufferCount || y >= cvbs->dmaBuffer.lines || x >= cvbs->dmaBuffer.lineSize) return -1;
	switch(cvbs->bits) {
		case 8:
			return getLineAddr8(&cvbs->dmaBuffer, y, bufferNumber)[x];
		case 16:
			return getLineAddr16(&cvbs->dmaBuffer, y, bufferNumber)[x];
	}
	return -1;
}

void dumpbuffer(struct cvbs *cvbs, int bufferNumber) { // kept as diagnostic tool
	for (int y = 0; y < cvbs->dmaBuffer.lines; y++) {
		for (int x = 0; x < cvbs->dmaBuffer.lineSize; x++) {
			switch(cvbs->bits) {
				case 8:
					printf("%d ", getLineAddr8(&cvbs->dmaBuffer, y, bufferNumber)[x]);
					break;
				case 16:
					printf("%d ", getLineAddr16(&cvbs->dmaBuffer, y, bufferNumber)[x]);
					break;
			}
		}
	}
}

void dumpbufferline(struct cvbs *cvbs, int y, int bufferNumber) {
	for (int x = 0; x < cvbs->dmaBuffer.lineSize; x++) {
		if (cvbs->bits == 8) {
			int value = getLineAddr8(&cvbs->dmaBuffer, y, bufferNumber)[x];
			printf("%d\n", value);
		} else if (cvbs->bits == 16) {
			int value = getLineAddr16(&cvbs->dmaBuffer, y, bufferNumber)[x];
			printf("%d\n", value);
		}
	}
}

bool ispositioninvalid(struct cvbs *cvbs, uint32_t x, uint32_t y) {
	if (cvbs->backBuffer >= cvbs->dmaBuffer.bufferCount || y >= cvbs->dmaBuffer.lines || x >= cvbs->dmaBuffer.lineSize) return true; // position is invalid
	// Check if the coordinates are within the visible area
	if (x > cvbs->mode.VisibleLineSamples || y > cvbs->mode.VisibleLines) return true; // position is invalid

	return false; // position is not invalid
}

void calculateinterlace(struct cvbs *cvbs, uint32_t *y) {
	// Adjust y for interlacing
	int interlacingoffset = 42 + (cvbs->mode.VisibleLines / 2);
	*y = (cvbs->mode.Interlaced && *y % 2) ? (*y >> 1) + interlacingoffset : (*y >> 1) + 19;
	
	return; // Valid position
}

uint32_t calculatetotalxoffset(struct cvbs *cvbs, uint32_t xoffset) {
	// Calculate total x offset based on the mode
	uint32_t totalxoffset = xoffset + cvbs->mode.HSyncSamples + cvbs->mode.HBackSamples;
	return totalxoffset; // Return the calculated total x offset
}

// 8-bit version
void writePixel8(void* linePtr, uint32_t x, uint32_t value) {
	((uint8_t*)linePtr)[x] = (uint8_t)value;
}

// 16-bit version
void writePixel16(void* linePtr, uint32_t x, uint32_t value) {
	((uint16_t*)linePtr)[x] = (uint16_t)value;
}

bool cvbsStart(struct cvbs *cvbs) {
    //TODO check start
    //very delicate... dma might be late for peripheral
    gdma_reset((gdma_channel_handle_t)cvbs->dmaChannel);
    esp_rom_delay_us(1);		
    LCD_CAM.lcd_user.lcd_start = 0;
    LCD_CAM.lcd_user.lcd_update = 1;
    esp_rom_delay_us(1);
    LCD_CAM.lcd_misc.lcd_afifo_reset = 1;
    LCD_CAM.lcd_user.lcd_update = 1;
    gdma_start((gdma_channel_handle_t)cvbs->dmaChannel, (intptr_t)&cvbs->dmaBuffer.descriptors[0]);
    esp_rom_delay_us(1);
    LCD_CAM.lcd_user.lcd_update = 1;
    LCD_CAM.lcd_user.lcd_start = 1;
    //TODO check end
    return true;
}

bool cvbsShow(struct cvbs *cvbs) {
    //TODO check start
    flush(&cvbs->dmaBuffer, cvbs->backBuffer);
    if(cvbs->bufferCount <= 1) 
        return true;
    attachBuffer(&cvbs->dmaBuffer, cvbs->backBuffer);
    cvbs->backBuffer = (cvbs->backBuffer + 1) % cvbs->bufferCount;
    //TODO check end
    return true;
}
void populateVsync(struct cvbs *cvbs, bool halfOffset, int numPre, int numSync, int numPost, int *lineIndex, int bufferNumber) {
	// Handles 6 NTSC Pre Equalisation Pulses. 2 Per Line Repeats 3 times for a total of 6
	int sampleIndex = 0;
	int lineSamples = cvbs->mode.TotalLineSamples;
	int halfLineSamples = cvbs->mode.TotalLineSamples / 2;
	if(halfOffset) sampleIndex = halfLineSamples;
	for (int b = 0; b < numPre; b++) {
		fillbufferwithvalueforlength(cvbs, SYNCLVL, cvbs->mode.EqPulseSamples, sampleIndex, *lineIndex, bufferNumber);
		sampleIndex += halfLineSamples;
		if(sampleIndex >= lineSamples) {(*lineIndex)++; sampleIndex = 0;}
	}
	// Handles 6 NTSC V Sync Broad Pulses. 2 Per Line Repeats 3 times for a total of 6
	for (int b = 0; b < numSync; b++) {
		fillbufferwithvalueforlength(cvbs, SYNCLVL, cvbs->mode.BroadPulseSamples, sampleIndex, *lineIndex, bufferNumber);
		sampleIndex += halfLineSamples;
		if(sampleIndex >= lineSamples) {(*lineIndex)++; sampleIndex = 0;}
	}
	// Handles 6 NTSC Post Equalisation Pulses. 2 Per Line Repeats 3 times for a total of 6
	for (int b = 0; b < numPost; b++) {
		fillbufferwithvalueforlength(cvbs, SYNCLVL, cvbs->mode.EqPulseSamples, sampleIndex, *lineIndex, bufferNumber);
		sampleIndex += halfLineSamples;
		if(sampleIndex >= lineSamples) {(*lineIndex)++; sampleIndex = 0;}
	}
	if(halfOffset) (*lineIndex)++;
	return;
}

void populateHsync(struct cvbs *cvbs, int *lineIndex, int bufferNumber) {
	// Handles H sync of 13 blank lines and first field of active video lines. Active video lines are basically the same as
	// blank lines until actual picure data is written to them so the same funciton is used
    int pictureLines = cvbs->mode.Interlaced ? cvbs->mode.VisibleLines/2 : cvbs->mode.VisibleLines;
	for (int b = 0; b < pictureLines + 13; b++) {
		// Only populates H sync because both porches are already covered by existing blank level.
		// Active video area offset still requires knowing how long the porches are so the variables
		// are kept in the mode declaration
		fillbufferwithvalueforlength(cvbs, SYNCLVL, cvbs->mode.HSyncSamples, 0, *lineIndex, bufferNumber);
        // colorburst generation logic
        if(cvbs->doColorburst) {
            for (int c = 0; c < cvbs->mode.ClrBrstSamples; c++) {

                uint32_t colorburstOffset = cvbs->mode.HSyncSamples + cvbs->mode.ClrBrstHBackOffset + c;
                //uint32_t sinOffset = ((*lineIndex) * cvbs->mode.TotalLineSamples) + colorburstOffset;
                uint32_t sinOffset = colorburstOffset;
                
                // FIX: Compute raw subcarrier accumulated angle, then add absolute M_PI phase offset
                float subcarrierAngle = cvbs->phaseIncPerSamp * sinOffset;
                float burstPhaseRad = M_PI - (33.0 * M_PI/180); // 180 degrees reference
                
                uint32_t colorValue = BLANKLVL + round(sin(subcarrierAngle + burstPhaseRad) * BRSTAMP);

                if (cvbs->bits == 8)
                    getLineAddr8(&cvbs->dmaBuffer, *lineIndex, bufferNumber)[colorburstOffset] = r2r_lut[colorValue];
                else if (cvbs->bits == 16)
                    getLineAddr16(&cvbs->dmaBuffer, *lineIndex, bufferNumber)[colorburstOffset] = r2r_lut[colorValue];
            }
        }
		(*lineIndex)++;
    }
}
