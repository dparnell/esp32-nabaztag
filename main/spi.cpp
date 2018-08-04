#include "common.h"
#include "spi.h"

#include "esp_system.h"
#include "soc/spi_reg.h"
#include "soc/spi_struct.h"
#include "rom/gpio.h"
#include "soc/io_mux_reg.h"
#include "soc/gpio_sig_map.h"
#include "soc/dport_reg.h"

#define SPI_CLK_PIN GPIO_NUM_18
#define SPI_MISO_PIN GPIO_NUM_19
#define SPI_MOSI_PIN GPIO_NUM_23

#define SPI_CLOCK_DIV2    0x00101001 //8 MHz
#define SPI_CLOCK_DIV4    0x00241001 //4 MHz
#define SPI_CLOCK_DIV8    0x004c1001 //2 MHz
#define SPI_CLOCK_DIV16   0x009c1001 //1 MHz
#define SPI_CLOCK_DIV32   0x013c1001 //500 KHz
#define SPI_CLOCK_DIV64   0x027c1001 //250 KHz
#define SPI_CLOCK_DIV128  0x04fc1001 //125 KHz

static volatile spi_dev_t * dev = (volatile spi_dev_t *)(DR_REG_SPI3_BASE);

void FastSPI() {
  dev->clock.val = SPI_CLOCK_DIV4;
}

void SlowSPI() {
  dev->clock.val = SPI_CLOCK_DIV64;
}

static void stop_spi() {
  dev->slave.trans_done = 0;
  dev->slave.slave_mode = 0;
  dev->pin.val = 0;
  dev->user.val = 0;
  dev->user1.val = 0;
  dev->ctrl.val = 0;
  dev->ctrl1.val = 0;
  dev->ctrl2.val = 0;
  dev->clock.val = 0;
}

/****************************************************************************/
/*  Initialization of the SPI peripheral                                    */
/*  Function : init_spi                                                     */
/*      Parameters                                                          */
/*          Input   :   Nothing                                             */
/*          Output  :   Nothing                                             */
/****************************************************************************/
void init_spi(void)
{
  stop_spi();

  // we are using VSPI
  DPORT_SET_PERI_REG_MASK(DPORT_PERIP_CLK_EN_REG, DPORT_SPI_CLK_EN_2);
  DPORT_CLEAR_PERI_REG_MASK(DPORT_PERIP_RST_EN_REG, DPORT_SPI_RST_2);

  // SPI mode 0
  dev->pin.ck_idle_edge = 0;
  dev->user.ck_out_edge = 0;

  // MSB first
  dev->ctrl.wr_bit_order = 0;
  dev->ctrl.rd_bit_order = 0;

  FastSPI();

  dev->user.usr_mosi = 1;
  dev->user.usr_miso = 1;
  dev->user.doutdin = 1;

  for(int i=0; i<16; i++) {
    dev->data_buf[i] = 0x00000000;
  }

  // attach the GPIO pins
  gpio_set_direction(SPI_CLK_PIN, GPIO_MODE_OUTPUT);
  gpio_matrix_out(SPI_CLK_PIN, VSPICLK_OUT_IDX, FALSE, FALSE);

  gpio_set_direction(SPI_MISO_PIN, GPIO_MODE_INPUT);
  gpio_matrix_out(SPI_MISO_PIN, VSPIQ_OUT_IDX, FALSE, FALSE);

  gpio_set_direction(SPI_MOSI_PIN, GPIO_MODE_OUTPUT);
  gpio_matrix_out(SPI_MOSI_PIN, VSPID_IN_IDX, FALSE, FALSE);
}

/****************************************************************************/
/*  Write a byte to the SPI bus                                             */
/*  Function : WriteSPI                                                     */
/*      Parameters                                                          */
/*          Input   :   Byte to send                                        */
/*          Output  :   Nothing                                             */
/****************************************************************************/
void WriteSPI(UBYTE data_out)
{
  dev->mosi_dlen.usr_mosi_dbitlen = 7;
  dev->miso_dlen.usr_miso_dbitlen = 0;
  dev->data_buf[0] = data_out;
  dev->cmd.usr = 1;
  while(dev->cmd.usr);
}

/****************************************************************************/
/*  Read a byte from the SPI bus                                            */
/*  Function : ReadSPI                                                      */
/*      Parameters                                                          */
/*          Input   :   Nothing                                             */
/*          Output  :   Return of the byte received                         */
/****************************************************************************/
UBYTE ReadSPI(void)
{
  UBYTE read_char;
  dev->mosi_dlen.usr_mosi_dbitlen = 0;
  dev->miso_dlen.usr_miso_dbitlen = 7;
  dev->cmd.usr = 1;
  while(dev->cmd.usr);
  read_char = dev->data_buf[0] & 0xFF;

  //Return success
  return read_char;
}
