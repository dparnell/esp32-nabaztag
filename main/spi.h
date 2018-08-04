#ifndef _SPI_H_
#define _SPI_H_

#include "common.h"

/*************/
/* Functions */
/*************/
void init_spi(void);

void WriteSPI(UBYTE data_out);
UBYTE ReadSPI(void);

void FastSPI();
void SlowSPI();

#endif
