#ifndef DMA_VIDEO_BUFFER_H
#define DMA_VIDEO_BUFFER_H

#include "esp_psram.h"
#include <hal/dma_types.h>
#include <rom/cache.h>
#include <esp_heap_caps.h>
#include <stdio.h>
#include <stdbool.h>

#define MAX_BUFFERS 2
#define MAX_LINES 1024
#define MAX_DMA_BLOCK_SIZE 4029
#define ALIGNMENT_PSRAM 64
#define ALIGNMENT_SRAM 4

extern int Cache_WriteBack_Addr(uint32_t addr, uint32_t size);

struct dmaBuff {
	int	descriptorCount;
	dma_descriptor_t *descriptors;
	void *buffer[MAX_BUFFERS][MAX_LINES];
	int bufferCount;
	bool valid;
	int lines;
	int lineSize;
	bool psram;
	int clones;
};

void attachBuffer(struct dmaBuff *dma, int b);
uint8_t *getLineAddr8(struct dmaBuff *dma, int y ,int b);
uint16_t *getLineAddr16(struct dmaBuff *dma, int y ,int b);
void initDmaBuff(struct dmaBuff *dma, int lines, int lineSize, int clones, bool ring, bool psram, int bufferCount);
void deInitDmaBuff(struct dmaBuff *dma);
dma_descriptor_t *getDescriptor(struct dmaBuff *dma, int i);
void flush(struct dmaBuff *dma, int b);
#endif
