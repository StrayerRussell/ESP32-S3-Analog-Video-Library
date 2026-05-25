#include "DMAVideoBuffer.h"

void attachBuffer(struct dmaBuff *dma, int b)
{
   for(int i = 0; i < dma->lines; i++) 
        for(int j = 0; j < dma->clones; j++)
            dma->descriptors[i * dma->clones + j].buffer = dma->buffer[b][i];
}

uint8_t *getLineAddr8(struct dmaBuff *dma, int y ,int b)
{
    return (uint8_t*)dma->buffer[b][y];
}


uint16_t *getLineAddr16(struct dmaBuff *dma, int y ,int b)
{
    return (uint16_t*)dma->buffer[b][y];
}

void initDmaBuff(struct dmaBuff *dma, int lines, int lineSize, int clones, bool ring, bool psram, int bufferCount)
{
    dma->lineSize = lineSize;
    dma->psram = psram;
    dma->bufferCount = bufferCount;
    dma->lines = lines;
    dma->clones = clones;
    dma->valid = false;
    dma->descriptorCount = lines * clones; //assume we dont need more than 4095 bytes per line

    dma->descriptors = (dma_descriptor_t *)heap_caps_aligned_calloc(ALIGNMENT_SRAM, 1, dma->descriptorCount * sizeof(dma_descriptor_t), MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    if (!dma->descriptors) 
    {
        printf("Failed to allocate descriptors\n");
        return;
    }
    for(int i = 0; i < bufferCount; i++)		
    {
        for(int j = 0; j < lines; j++)
        {
            dma->buffer[i][j] = 0;
        }
    }
    for(int i = 0; i < bufferCount; i++)
    {
        for(int y = 0; y < lines; y++)
        {
            if(psram)
                dma->buffer[i][y] = heap_caps_aligned_calloc(ALIGNMENT_PSRAM, 1, lineSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
            else
                dma->buffer[i][y] = heap_caps_aligned_calloc(ALIGNMENT_SRAM, 1, lineSize, MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA | MALLOC_CAP_8BIT);
            if (!dma->buffer[i][y]) 
            {
                //TODO f this
                /*heap_caps_free(descriptors);
                descriptors = 0;
                for(int j = 0; j < i * lines + y; j++)
                {
                    if(buffer[j])
                        heap_caps_free(buffer[j]);
                    buffer[j] = 0;
                }*/
                printf("Failed to allocate buffer\n");
                return;
            }
        }
    }
    for (int i = 0; i < dma->descriptorCount; i++) 
    {
        dma->descriptors[i].dw0.owner = DMA_DESCRIPTOR_BUFFER_OWNER_CPU;
        dma->descriptors[i].dw0.suc_eof = 0;
        dma->descriptors[i].next = &dma->descriptors[i + 1];
        dma->descriptors[i].dw0.size = lineSize;
        dma->descriptors[i].dw0.length = dma->descriptors[i].dw0.size;
    }
    attachBuffer(dma, 0);
    if(ring)
    {
        dma->descriptors[dma->descriptorCount - 1].next = dma->descriptors;
    }

    else
    {
        dma->descriptors[dma->descriptorCount - 1].dw0.suc_eof = 1;
        dma->descriptors[dma->descriptorCount - 1].next = 0;
    }
    dma->valid = true;
}

void deInitDmaBuff(struct dmaBuff *dma)
{
    if(dma->descriptors)
        heap_caps_free(dma->descriptors);
    for(int i = 0; i < dma->bufferCount; i++)		
        for(int j = 0; j < dma->lines; j++)
            if(dma->buffer[i][j])
                heap_caps_free(dma->buffer[i][j]);
}

void flush(struct dmaBuff *dma, int b)
{
    if(!dma->psram) return;
    for(int y = 0; y < dma->lines; y++)
        Cache_WriteBack_Addr((uint32_t)dma->buffer[b][y], dma->lineSize);
}
