#include <cstring>
#include <math.h>
#include <driver/gpio.h>
#include <esp_rom_sys.h>
#include <esp_rom_gpio.h>
#include <esp_private/gdma.h>
#include <esp_private/periph_ctrl.h>
#include <hal/gpio_hal.h>
#include <soc/lcd_cam_struct.h>
#include "CVBS.h"

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
{																													 \
		uint32_t temp_val = base_reg.val;											 \
		typeof(base_reg) temp_reg;															\
		temp_reg.val = temp_val;																\
		temp_reg.reg_field = (field_val);											 \
		(base_reg).val = temp_reg.val;													\
}

CVBS::CVBS() {
		bufferCount = 1;
		doColorburst = true;
		usePsram = true;
		dmaChannel = 0;
        dmaBuffer.valid = false;
        dmaBuffer.descriptors = nullptr;
}

CVBS::~CVBS() {
        if (dmaBuffer.valid) {
            deInitDmaBuff(&dmaBuffer);
        }
		bits = 0;
		backBuffer = 0;
}

extern int Cache_WriteBack_Addr(uint32_t addr, uint32_t size);

void CVBS::attachPinToSignal(int pin, int signal) {
	esp_rom_gpio_connect_out_signal(pin, signal, false, false);
	//gpio_hal_func_sel(GPIO_PIN_MUX_REG[pin], PIN_FUNC_GPIO);
	esp_rom_gpio_pad_select_gpio(pin);
	gpio_set_drive_capability((gpio_num_t)pin, (gpio_drive_cap_t)3);
}

bool CVBS::init(int* pins, CVBSMode mode, int bits) {
    this->mode = mode;
    this->bits = bits;
    backBuffer = 0;

    // I am not using line doubling but I am keeping clones a variable just in case.
    int Clones = 1;

    //int lineSampleIndex = mode.totalLineSamples();

    this->sinLUT = new double[mode.TotalLineSamples()];
    this->cosLUT = new double[mode.TotalLineSamples()];

    initDmaBuff(&dmaBuffer, mode.TotalLines, mode.TotalLineSamples() * (bits/8), Clones, true, usePsram, bufferCount);
    if (!dmaBuffer.valid) {
        deInitDmaBuff(&dmaBuffer);
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
    LCD_CAM.lcd_user.lcd_2byte_en = (bits == 8) ? 0 : 1;
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
    //LCD_CAM.lcd_ctrl1.lcd_ht_width = mode.TotalLineSamples();	//12 bit

    //HAL_FORCE_MODIFY_U32_REG_FIELD(LCD_CAM.lcd_ctrl1, lcd_vb_front, mode.BroadPulseSamples + (mode.TotalLineSamples() - mode.BroadPulseSamples));	//8bit
    //LCD_CAM.lcd_ctrl.lcd_va_height = mode.VisibleLines * Clones;																																														 //10 bit
    //LCD_CAM.lcd_ctrl.lcd_vt_height = mode.TotalLines;																																																							 //10 bit
    //HAL_FORCE_MODIFY_U32_REG_FIELD(LCD_CAM.lcd_ctrl2, lcd_hsync_position, mode.HSyncSamples);

    LCD_CAM.lcd_misc.lcd_next_frame_en = 0;	//?? limitation

    //printf("LCD Initialized\n");
    //printf("Assigning pins\n");
    if (bits == 8) {
        for (int i = 0; i < bits; i++)
            if (pins[i] >= 0)
                attachPinToSignal(pins[i], LCD_DATA_OUT0_IDX + i);
    } else if (bits == 16) {
        for (int i = 0; i < bits; i++)
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

    dmaChannel = (int)dmaCh;
    ret = gdma_connect(dmaCh, GDMA_MAKE_TRIGGER(GDMA_TRIG_PERIPH_LCD, 0));
    if(ret != ESP_OK)
    {
        printf("Failed to connect GDMA to LCD Peripheral, error: %d\n", ret);
        return false;
    }

    gdma_transfer_config_t transfer_config {
        .max_data_burst_size = 64,
        .access_ext_mem = true,
    };

    gdma_config_transfer(dmaCh, &transfer_config);

    //////////////////////////////////////////////////////////

    /* SYNC AND COLORBURST POPULATION SECTION */

    //////////////////////////////////////////////////////////

    int lineIndex = 0;	// keeps track of which line is currently being written to

    int BlankLevel = 71;	// dac value which results in blank voltage (usually 0.3 volts) 
    int SyncLevel = 0;
    //int BlackLevel = 85;

    //int ClrbrstVerticalOffset = BlankLevel/2;
    //int ClrbrstPhaseSampleOffset = 0;
    //int ClrBrstMultiplier = 50;
    double ClrbrstSquarewaveWidth = mode.ClrBrstSamples/20.0;
    printf("SqrWaveWdth: %f\n", ClrbrstSquarewaveWidth);
    //ClrbrstSquarewaveWidth = round(ClrbrstSquarewaveWidth);
    //printf("SqrWaveWdth: %f\n", ClrbrstSquarewaveWidth);

    // Variables for best amplitude and phase shift
    double BestAmplitude = 1000000;		// Example amplitude, can be any value
    double BestPhaseShift = 0.0; // Example phase shift in radians (45 degrees)
    //BestPhaseShift = 4.2;

    // Calculate the phase increment in fixed-point integer math
    double phaseIncrementPerSample = ((2.0 * M_PI * mode.ClrBrstFreq) / mode.Frequency);

    // Populate Sin and Cos lookup tables for color modulation
    if (doColorburst) {
        printf("Populating Sin and Cos Lookup Tables for Color Modulation\n");
        for (int i = 0; i < mode.TotalLineSamples(); i++) {
                // Calculate phase with the applied phase shift
                double phase = i * phaseIncrementPerSample + BestPhaseShift;

                // Calculate the sine value with the amplitude applied
                double preSin = sin(phase) * BestAmplitude;

                // Populate the LUT with the scaled sine value
                this->sinLUT[i] = preSin;
        }
    }

    /*
    // Populate Sin and Cos lookup tables for color modulation
    if (doColorburst) {
    printf("Populating Sin and Cos Lookup Tables for Color Modulation\n");
    int sinIndex = 0;
    int togglingInverter = -1000000;
    for (int a = 0; a < mode.TotalLineSamples()/ClrbrstSquarewaveWidth; a++) {
        for(int b = 0; b < round((ClrbrstSquarewaveWidth * a)/a); b++) {
            //printf("");
            this->sinLUT[sinIndex++] = BlankLevel + (ClrbrstVerticalOffset * togglingInverter);
        }
        togglingInverter *= -1;
    }
    }
    */

    for (int a = 0; a < bufferCount; a++) {
    lineIndex = 0;
    printf("Running Sync Population for Buffer number: %d\n", a);
    switch (mode.SyncType) {
        case 1:	//NTSC Sync
            printf("Starting NTSC Sync Population\n");
            //Populates entire framebuffer with Blank level for later sync overwriting
            for (int b = 0; b < mode.TotalLines; b++) { fillbufferwithvalueforlength(BlankLevel, mode.TotalLineSamples(), 0, b, a); }
            // populates V sync for field one
            populateVsync(false, SyncLevel, 6, 6, 6, lineIndex, a);
            // populates the H sync and colorburst of all field one lines including 13 blank lines
            populateHsync(SyncLevel, BlankLevel, phaseIncrementPerSample, lineIndex, a);

            if (mode.Interlaced) {
                // populates V sync for field two, begins and ends with a 1/2 line offset to signify that it is field two
                populateVsync(true, SyncLevel, 6, 6, 6, lineIndex, a);
                // populates the H sync and colorburst of all field two lines including 13 blank lines
                populateHsync(SyncLevel, BlankLevel, phaseIncrementPerSample, lineIndex, a);
            }
            break;
        case 2: //PAL Sync
            printf("Starting PAL Sync Population\n");
            //Populates entire framebuffer with Blank level for later sync overwriting
            for (int b = 0; b < mode.TotalLines; b++) { fillbufferwithvalueforlength(BlankLevel, mode.TotalLineSamples(), 0, b, a); }
            // populates V sync for field one
            populateVsync(false, SyncLevel, 6, 5, 5, lineIndex, a);
            // populates the H sync and colorburst of all field one lines including 13 blank lines
            populateHsync(SyncLevel, BlankLevel, phaseIncrementPerSample, lineIndex, a);

            if (mode.Interlaced) {
                // populates V sync for field two, begins and ends with a 1/2 line offset to signify that it is field two
                populateVsync(false, SyncLevel, 5, 5, 4, lineIndex, a);
                // populates the H sync and colorburst of all field two lines including 13 blank lines
                populateHsync(SyncLevel, BlankLevel, phaseIncrementPerSample, lineIndex, a);
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

void CVBS::onebitdot(uint32_t x, uint32_t y, bool state) {
    if(ispositioninvalid(x, y)) return;
    calculateinterlace(y);
    switch(bits) {
        case 8:
            switch(state) {
                case true:
                    getLineAddr8(&dmaBuffer, y, backBuffer)[x] = 255;
                    break;
                case false:
                    getLineAddr8(&dmaBuffer, y, backBuffer)[x] = 71;
                    break;
            }
            break;
        case 16:
            switch(state) {
                case true:
                    getLineAddr16(&dmaBuffer, y, backBuffer)[x] = 65535;
                    break;
                case false:
                    getLineAddr16(&dmaBuffer, y, backBuffer)[x] = 65535;
                    break;
            }
            break;
    }
}

void CVBS::onebitimage(uint8_t* imagedata, int width, int height, int xoffset, int yoffset) {
	int byteIndex = 0, bytesPerLine = width/8;	

	// Calculate the maximum coordinates and check if within bounds
	uint32_t maxX = width + xoffset, maxY = height + yoffset;
	if(ispositioninvalid(maxX, maxY)) return;
	// Calculate where in the line the actual image data starts (offset from sync data)
	uint32_t totalxoffset = calculatetotalxoffset(xoffset);
	uint16_t max = 255, min = 71;
	void* linePtr = nullptr;

	// Precompute the bit expansion lookup table for 1bpp to 8bpp
	// This will create a lookup table where each byte maps to 8 pixels
	DRAM_ATTR static uint8_t bit_expand_lut[256][8];

	for(uint16_t byte = 0; byte < 256; byte++) {
		for(int i = 0; i < 8; i++) {
			bit_expand_lut[byte][i] = (byte & (1 << (7 - i))) ? max : min;
		}
	}
	
	// remember to test preformance impact of function pointers for combining 8 and 16 bit handling
	switch(bits) {
		case 8:
            for (int a = yoffset; a < maxY; a++) {
				uint32_t y = a;
				calculateinterlace(y);
				uint32_t x = totalxoffset;
				linePtr = getLineAddr8(&dmaBuffer, y, backBuffer);
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
				calculateinterlace(y);
				uint32_t x = totalxoffset;
				linePtr = getLineAddr16(&dmaBuffer, y, backBuffer);
				for (int b = 0; b < bytesPerLine; b++) {
					memcpy(&((uint16_t*)linePtr)[x], bit_expand_lut[imagedata[++byteIndex]], 8);
					x += 8;	// Advance by 8 pixels
				}
			}
			break;
	}
}

void CVBS::onebitchangemask(uint8_t* imagedata, uint8_t* changemask, int width, int height, int xoffset, int yoffset) {
	int byteIndex = 0, bytesPerLine = width/8;	

	// Calculate the maximum coordinates and check if within bounds
	uint32_t maxX = width + xoffset, maxY = height + yoffset;
	if(ispositioninvalid(maxX, maxY)) return;
	// Calculate where in the line the actual image data starts (offset from sync data)
	uint32_t totalxoffset = calculatetotalxoffset(xoffset);
	uint16_t max = 255, min = 71;
	void* linePtr = nullptr;
	switch(bits) {
		case 8:
			for (int a = yoffset; a < maxY; a++) {
				uint32_t y = a;
				calculateinterlace(y);
				uint32_t x = totalxoffset;
				linePtr = getLineAddr8(&dmaBuffer, y, backBuffer);
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
				//memcpy(((uint8_t*)linePtr) + totalxoffset, lineDMAbuffer, DMAbuffersize);
			}
			break;
		case 16:
			max = 65535, min = 18241;
			for (int a = yoffset; a < maxY; a++) {
				uint32_t y = a;
				calculateinterlace(y);
				uint32_t x = totalxoffset;
				linePtr = getLineAddr16(&dmaBuffer, y, backBuffer);
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
				//memcpy(((uint16_t*)linePtr) + totalxoffset, lineDMAbuffer, DMAbuffersize);
			}
			break;
	}
}

bool CVBS::monodot(uint32_t x, uint32_t y, uint32_t value) {
	if(ispositioninvalid(x, y)) return false;
	switch(bits) {
		case 8:
			if(value > 255) value = 255;
			if(value < 71) value = 71;
			getLineAddr8(&dmaBuffer, y, backBuffer)[x] = value;
			break;
		case 16:
			if(value > 65535) value = 65535;
			if(value < 18241) value = 18241;
			getLineAddr16(&dmaBuffer, y, backBuffer)[x] = value;
			break;
		default:
			return false; // Unsupported bits
	}
	return false;
}

void CVBS::monoimage(uint8_t* imagedata, int width, int height, int xoffset, int yoffset) {
	int byteIndex = 0;
	//int bytesPerLine = width/8;	

	// Calculate the maximum coordinates
	uint32_t maxX = width + xoffset;
	uint32_t maxY = height + yoffset;
	if(ispositioninvalid(maxX, maxY)) return;
	uint32_t totalxoffset = calculatetotalxoffset(xoffset);

	int xbound = width + maxX + 1; // +1 as this is one past the last pixel

	//shabingus
	int DMAbuffersize = width * sizeof(uint8_t);

	uint16_t max = 255, min = 71;
	void* linePtr = nullptr;
	void* lineDMAbuffer = nullptr;
	switch(bits) {
		case 8:
			lineDMAbuffer = new uint8_t[width];
			break;
		case 16:
			lineDMAbuffer = new uint16_t[width];
			DMAbuffersize = width * sizeof(uint16_t);
			max = 65535, min = 18241;
			break;
	}
	switch(bits) {
		case 8:
			for (int a = yoffset; a < maxY; a++) {
				uint32_t y = a;
				calculateinterlace(y);
				linePtr = getLineAddr8(&dmaBuffer, y, backBuffer);
				//memcpy((uint8_t*)linePtr + totalxoffset, imagedata + byteIndex, width);
				//byteIndex += width;
				for (int b = totalxoffset; b < xbound; b++) {
					//printf("Y: %d, X: %d, Value: %d\n", y, b, imagedata[byteIndex]);
					((uint8_t*)linePtr)[b] = (imagedata[byteIndex] < min) ? min : imagedata[byteIndex];
					byteIndex++;
				}
				//memcpy(((uint8_t*)linePtr) + totalxoffset, lineDMAbuffer, DMAbuffersize);
			}
			break;
		case 16:
			for (int a = yoffset; a < maxY; a++) {
				uint32_t y = a;
				calculateinterlace(y);
				linePtr = getLineAddr16(&dmaBuffer, y, backBuffer);
				for (int b = totalxoffset; b < xbound; b++) {
					((uint16_t*)linePtr)[b] = (imagedata[byteIndex] < min) ? min : imagedata[byteIndex];
					byteIndex++;
				}
				//memcpy(((uint8_t*)linePtr) + totalxoffset, lineDMAbuffer, DMAbuffersize);
			}
			break;
	}
}

bool CVBS::colordot(uint32_t x, uint32_t y, uint8_t rgb332) {
	bool firstPixel = false;
	if(y == 0) firstPixel = true;
	if(ispositioninvalid(x, y)) return false;

	int r = ((rgb332 >> 5) * 0x49) >> 1;
	int g = (((rgb332 >> 2) & 0x07) * 0x49) >> 1;
	int b = ((rgb332 & 0x03) * 0x55);
	//float lum = 0.299 * r + 0.587 * g + 0.114 * b;
	//float i = (b - lum) * -0.2680f + (r - lum) * 0.7358f;
	//float q = (b - lum) *	0.4127f + (r - lum) * 0.4778f;
	float lum = 0.299 * r + 0.587 * g + 0.114 * b;
	float i = 0.596 * r - 0.274 * g - 0.322 * b;
	float q = 0.211 * r - 0.523 * g + 0.311 * b;	 //(-0.614777, 0.614777)s


	int colorburstOffset = mode.HBackSamples - mode.ClrBrstHBackOffset;

	// Calculate the phase increment per sample based on the desired frequency
	//double phaseIncrement = (2.0 * M_PI * mode.ClrBrstFreq) / mode.Frequency;
	//double phase = (x - colorburstOffset - 5) * phaseIncrement;

	int modulationIndex = x - colorburstOffset;

	//double maxDither = 5.0;
	//double minDither = 0.0;
	double dither = 0.0;

	// Calculate the modulated color value using the sine and cosine functions at the current phase
	//dither = ((double)(rand() % 100) / 100.0) * maxDither * 2 - minDither; // Random value between -maxDither and +maxDither
	//double chromaValue = (q * this->sinLUT[modulationIndex] + i * this->cosLUT[modulationIndex]) / 1000000;
	double chromaValue = (sqrt((q * q) + (i * i)) * this->sinLUT[modulationIndex]) / 1000000;

	double highPrecolorValue = 71 + lum + chromaValue + dither;
	int colorValue = round(highPrecolorValue);
	//int colorValue = (71 + 120 + sin(phase) * 25);

	if(firstPixel) {
		//printf("Color: %d, X: %d, Err: %f\n", rgb332, x, highPrecolorValue - colorValue);
	}

	colorValue = colorValue < 0 ? 0 : colorValue > 255 ? 255 : colorValue;

	//colorValue += 71;
	switch(bits) {
		case 8:
			if((colorValue > 255) || (colorValue < 0)) {
					printf("colorvalNotAllowed: %d\n", colorValue);
					return false;
			}
			getLineAddr8(&dmaBuffer, y, backBuffer)[x] = colorValue;
			return getLineAddr8(&dmaBuffer, y, backBuffer)[x] == colorValue;
		case 16:
			if(colorValue > 65535 || colorValue < 0) {
					printf("colorvalNotAllowed\n");
					return false;
			}
			getLineAddr16(&dmaBuffer, y, backBuffer)[x] = colorValue;
			return getLineAddr16(&dmaBuffer, y, backBuffer)[x] == colorValue;
	}

	return false; // Default return if bits is neither 8 nor 16
}

bool CVBS::colordot(uint32_t x, uint32_t y, double lum, double i, double q) {
	bool firstPixel = false;
	if(y == 0) firstPixel = true;
	if(ispositioninvalid(x, y)) return false;

	int colorburstOffset = mode.HBackSamples - mode.ClrBrstHBackOffset;

	// Calculate the phase increment per sample based on the desired frequency
	//double phaseIncrement = (2.0 * M_PI * mode.ClrBrstFreq) / mode.Frequency;
	//double phase = (x - colorburstOffset - 5) * phaseIncrement;

	int modulationIndex = x - colorburstOffset;

	//double maxDither = 5.0;
	//double minDither = 0.0;
	double dither = 0.0;

	// Calculate the modulated color value using the sine and cosine functions at the current phase
	//dither = ((double)(rand() % 100) / 100.0) * maxDither * 2 - minDither; // Random value between -maxDither and +maxDither
	//double chromaValue = (q * this->sinLUT[modulationIndex] + i * this->cosLUT[modulationIndex]) / 1000000;
	double chromaValue = (sqrt((q * q) + (i * i)) * this->sinLUT[modulationIndex]);

	double highPrecolorValue = 71 + lum + chromaValue + dither;
	int colorValue = round(highPrecolorValue);
	//int colorValue = (71 + 120 + sin(phase) * 25);

	if(firstPixel) {
		//printf("Color: %d, X: %d, Err: %f\n", rgb332, x, highPrecolorValue - colorValue);
	}

	colorValue = colorValue < 0 ? 0 : colorValue > 255 ? 255 : colorValue;

	//colorValue += 71;
	switch(bits) {
		case 8:
			if((colorValue > 255) || (colorValue < 0)) {
					printf("colorvalNotAllowed: %d\n", colorValue);
					return false;
			}
			getLineAddr8(&dmaBuffer, y, backBuffer)[x] = colorValue;
			return getLineAddr8(&dmaBuffer, y, backBuffer)[x] == colorValue;
		case 16:
			if(colorValue > 65535 || colorValue < 0) {
					printf("colorvalNotAllowed\n");
					return false;
			}
			getLineAddr16(&dmaBuffer, y, backBuffer)[x] = colorValue;
			return getLineAddr16(&dmaBuffer, y, backBuffer)[x] == colorValue;
	}

	return false; // Default return if bits is neither 8 nor 16
}

void CVBS::modulatebuffer(uint32_t carrier, int buffernumber) { // modulate buffer by a carrier intended for transmission of NTSC via VHF/UHF
	//float colorburstOffset = mode.HSyncSamples - mode.ClrBrstHBackOffset;

	int SinMultiplier = 50;

	// Calculate the phase increment per sample based on the desired frequency
	double phaseIncrement = (2.0 * M_PI * carrier) / mode.Frequency;
	float phase = 0.0;	// Initial phase

	for (int y = 0; y < dmaBuffer.lines; y++) {
		phase = 0.0;
		for (int x = 0; x < dmaBuffer.lineSize; x++) {
			int value = (bits == 8) ? getLineAddr8(&dmaBuffer, y, backBuffer)[x]
															: getLineAddr16(&dmaBuffer, y, backBuffer)[x];

			// Only modulate if the value is non-zero
			int modulation = round(SinMultiplier + (sin(phase) * SinMultiplier));
			value += modulation;

			// Store the modulated value back
			switch(bits) {
				case 8:
					getLineAddr8(&dmaBuffer, y, buffernumber)[x] = value;
					break;
				case 16:
					getLineAddr16(&dmaBuffer, y, buffernumber)[x] = value;
					break;
			}

			phase += phaseIncrement;
		}
	}
}

void CVBS::clear(int value) {
	for(int y = 0; y < mode.VisibleLines; y++)
			for(int x = 0; x < mode.VisibleLineSamples; x++)
					CVBS::monodot(x,y,value);
}

void CVBS::fillbufferwithvalueforlength(int value, int len, int offset, int line, int bufferNumber) {
	for (int i = 0; i < len; i++) {
		switch(bits) {
			case 8:
				getLineAddr8(&dmaBuffer, line, bufferNumber)[i + offset] = value;
				break;
			case 16:
				getLineAddr16(&dmaBuffer, line, bufferNumber)[i + offset] = value;
				break;
		}
	}
}

void CVBS::fillbufferwithvalue(uint32_t value) {
	for (int y = 0; y < mode.TotalLines; y++) {
		for (int x = 0; x < mode.TotalLineSamples(); x++) {
			switch(bits) {
				case 8:
					getLineAddr8(&dmaBuffer, y, backBuffer)[x] = value;
					break;
				case 16:
					getLineAddr16(&dmaBuffer, y, backBuffer)[x] = value;
					break;
			}
		}
	}
}

int CVBS::accessbuffervalue(int x, int y, int buffernumber) {
	if (buffernumber >= dmaBuffer.bufferCount || y >= dmaBuffer.lines || x >= dmaBuffer.lineSize) return -1;
	switch(bits) {
		case 8:
			return getLineAddr8(&dmaBuffer, y, buffernumber)[x];
		case 16:
			return getLineAddr16(&dmaBuffer, y, buffernumber)[x];
	}
	return -1;
}

void CVBS::dumpbuffer(int buffernumber) { // kept as diagnostic tool
	for (int y = 0; y < dmaBuffer.lines; y++) {
		for (int x = 0; x < dmaBuffer.lineSize; x++) {
			switch(bits) {
				case 8:
					printf("%d ", getLineAddr8(&dmaBuffer, y, buffernumber)[x]);
					break;
				case 16:
					printf("%d ", getLineAddr16(&dmaBuffer, y, buffernumber)[x]);
					break;
			}
		}
	}
}

void CVBS::dumpbufferline(int y, int buffernumber) {
	for (int x = 0; x < dmaBuffer.lineSize; x++) {
		if (bits == 8) {
			int value = getLineAddr8(&dmaBuffer, y, buffernumber)[x];
			printf("%d", value);
			printf("\n");
		} else if (bits == 16) {
			int value = getLineAddr16(&dmaBuffer, y, buffernumber)[x];
			printf("%d", value);
			printf("\n");
		}
	}
}

bool CVBS::ispositioninvalid(uint32_t x, uint32_t y) {
	if (backBuffer >= dmaBuffer.bufferCount || y >= dmaBuffer.lines || x >= dmaBuffer.lineSize) return true; // position is invalid
	// Check if the coordinates are within the visible area
	if (x > mode.VisibleLineSamples || y > mode.VisibleLines) return true; // position is invalid

	return false; // position is not invalid
}

void CVBS::calculateinterlace(uint32_t &y) {
	// Adjust y for interlacing
	int interlacingoffset = 42 + (mode.VisibleLines / 2);
	y = (mode.Interlaced && y % 2) ? (y >> 1) + interlacingoffset : (y >> 1) + 19;
	
	return; // Valid position
}

uint32_t CVBS::calculatetotalxoffset(uint32_t xoffset) {
	// Calculate total x offset based on the mode
	uint32_t totalxoffset = xoffset + mode.HSyncSamples + mode.HBackSamples + 1;
	return totalxoffset; // Return the calculated total x offset
}

// 8-bit version
void CVBS::writePixel8(void* linePtr, uint32_t x, uint32_t value) {
	((uint8_t*)linePtr)[x] = (uint8_t)value;
}

// 16-bit version
void CVBS::writePixel16(void* linePtr, uint32_t x, uint32_t value) {
	((uint16_t*)linePtr)[x] = (uint16_t)value;
}

bool CVBS::start() {
    //TODO check start
    //very delicate... dma might be late for peripheral
    gdma_reset((gdma_channel_handle_t)dmaChannel);
    esp_rom_delay_us(1);		
    LCD_CAM.lcd_user.lcd_start = 0;
    LCD_CAM.lcd_user.lcd_update = 1;
    esp_rom_delay_us(1);
    LCD_CAM.lcd_misc.lcd_afifo_reset = 1;
    LCD_CAM.lcd_user.lcd_update = 1;
    gdma_start((gdma_channel_handle_t)dmaChannel, (intptr_t)&dmaBuffer.descriptors[0]);
    esp_rom_delay_us(1);
    LCD_CAM.lcd_user.lcd_update = 1;
    LCD_CAM.lcd_user.lcd_start = 1;
    //TODO check end
    return true;
}

bool CVBS::show() {
    //TODO check start
    flush(&dmaBuffer, backBuffer);
    if(bufferCount <= 1) 
        return true;
    attachBuffer(&dmaBuffer, backBuffer);
    backBuffer = (backBuffer + 1) % bufferCount;
    //TODO check end
    return true;
}

void CVBS::populateVsync(bool halfOffset, int syncLevel, int numPre, int numSync, int numPost, int &lineIndex, int bufferNumber) {
	// Handles 6 NTSC Pre Equalisation Pulses. 2 Per Line Repeats 3 times for a total of 6
	int sampleIndex = 0;
	int lineSamples = mode.TotalLineSamples();
	int halfLineSamples = mode.TotalLineSamples() / 2;
	if(halfOffset) sampleIndex = halfLineSamples;
	for (int b = 0; b < numPre; b++) {
		fillbufferwithvalueforlength(syncLevel, mode.EqPulseSamples, sampleIndex, lineIndex, bufferNumber);
		sampleIndex += halfLineSamples;
		if(sampleIndex >= lineSamples) {lineIndex++; sampleIndex = 0;}
	}
	// Handles 6 NTSC V Sync Broad Pulses. 2 Per Line Repeats 3 times for a total of 6
	for (int b = 0; b < numSync; b++) {
		fillbufferwithvalueforlength(syncLevel, mode.BroadPulseSamples, sampleIndex, lineIndex, bufferNumber);
		sampleIndex += halfLineSamples;
		if(sampleIndex >= lineSamples) {lineIndex++; sampleIndex = 0;}
	}
	// Handles 6 NTSC Post Equalisation Pulses. 2 Per Line Repeats 3 times for a total of 6
	for (int b = 0; b < numPost; b++) {
		fillbufferwithvalueforlength(syncLevel, mode.EqPulseSamples, sampleIndex, lineIndex, bufferNumber);
		sampleIndex += halfLineSamples;
		if(sampleIndex >= lineSamples) {lineIndex++; sampleIndex = 0;}
	}
	if(halfOffset) lineIndex++;
	return;
}

void CVBS::populateHsync(int syncLevel, int blankLevel, double phaseIncrementPerSample, int &lineIndex, int bufferNumber) {
	//int ClrbrstVerticalOffset = blankLevel/2;
	int ClrbrstPhaseSampleOffset = 0;
	int ClrBrstMultiplier = 50;
	//int ClrbrstSquarewaveWidth = mode.ClrBrstSamples/20;
	// Handles H sync of 13 blank lines and first field of active video lines. Active video lines are basically the same as
	// blank lines until actual picure data is written to them so the same funciton is used
	int pictureLines = mode.Interlaced ? mode.VisibleLines/2 : mode.VisibleLines;
	for (int b = 0; b < pictureLines + 13; b++) {
		// Only populates H sync because both porches are already covered by existing blank level.
		// Active video area offset still requires knowing how long the porches are so the variables
		// are kept in the mode declaration
		fillbufferwithvalueforlength(syncLevel, mode.HSyncSamples, 0, lineIndex, bufferNumber);
		// colorburst generation logic
		if(doColorburst) {
			//int colorburstOffset = mode.HSyncSamples + mode.ClrBrstHBackOffset;
			/*
			for(int c = 0; c < 10; c++) {
				fillbufferwithvalueforlength(blankLevel + ClrbrstVerticalOffset, ClrbrstSquarewaveWidth, colorburstOffset, lineIndex, bufferNumber);
				colorburstOffset += ClrbrstSquarewaveWidth;
				fillbufferwithvalueforlength(blankLevel - ClrbrstVerticalOffset, ClrbrstSquarewaveWidth, colorburstOffset, lineIndex, bufferNumber);
				colorburstOffset += ClrbrstSquarewaveWidth;
			}
			*/
			
			for (int c = ClrbrstPhaseSampleOffset; c < mode.ClrBrstSamples + ClrbrstPhaseSampleOffset; c++) {

				uint32_t colorburstOffset = mode.HSyncSamples + mode.ClrBrstHBackOffset + c;
				uint32_t colorValue = blankLevel + round(sin(c * phaseIncrementPerSample) * ClrBrstMultiplier);

				if (bits == 8)
					getLineAddr8(&dmaBuffer, lineIndex, bufferNumber)[colorburstOffset] = colorValue;
				else if (bits == 16)
					getLineAddr16(&dmaBuffer, lineIndex, bufferNumber)[colorburstOffset] = colorValue;

				// Increment the phase for the next sample
				//phase += phaseIncrementPerSample;
			}
			
			//phase = 0.0;
		}
		lineIndex++;
    }
}
