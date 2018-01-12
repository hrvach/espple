/******************************************************************************
 * Copyright 2013-2015 Espressif Systems
 *           2015 <>< Charles Lohr
 *           2017 Hrvoje Cavrak
 */

#include "slc_register.h"
#include <c_types.h>
#include "user_interface.h"
#include "pin_mux_register.h"
#include "signetics_video_rom.h"
#include <dmastuff.h>

/* Change this to #define NTSC if you like */
#define PAL 

#ifdef PAL
  #define LINE_BUFFER_LENGTH 160
  #define VIDEO_LINES 625
  #define LINETYPES 6

  /* PAL signals */
  #define SHORT_SYNC_INTERVAL    5
  #define LONG_SYNC_INTERVAL    75
  #define NORMAL_SYNC_INTERVAL  10
  #define LINE_SIGNAL_INTERVAL 150
  
  #define PIXEL_LINE_RESET_EVEN 55
  #define PIXEL_LINE_RESET_ODD 367

#else

  #define LINE_BUFFER_LENGTH 159
  #define VIDEO_LINES 525
  #define LINETYPES 6

  /* NTSC signals */
  #define SHORT_SYNC_INTERVAL    6
  #define LONG_SYNC_INTERVAL    73
  #define SERRATION_PULSE_INT   67
  #define NORMAL_SYNC_INTERVAL  12
  #define LINE_SIGNAL_INTERVAL 147

  #define PIXEL_LINE_RESET_EVEN 38
  #define PIXEL_LINE_RESET_ODD 299
#endif


#define SYNC_LEVEL        0x99999999
#define WHITE_LEVEL       0xffffffff
#define BLACK_LEVEL       0xbbbbbbbb

#define WS_I2S_BCK 1
#define WS_I2S_DIV 2

extern uint8_t terminal_ram[40 * 24];

uint32_t i2s_dma_buffer[LINE_BUFFER_LENGTH*LINETYPES];
static struct sdio_queue i2sBufDesc[VIDEO_LINES];

int current_pixel_line;

LOCAL void slc_isr(void) {
        struct sdio_queue *finishedDesc;
        uint32 slc_intr_status;
        uint8_t x;

        //Grab int status
        slc_intr_status = READ_PERI_REG(SLC_INT_STATUS);

        //clear all intr flags
        WRITE_PERI_REG(SLC_INT_CLR, 0xffffffff);

        if (slc_intr_status & SLC_RX_EOF_INT_ST) {
                //The DMA subsystem is done with this block: Push it on the queue so it can be re-used.
                finishedDesc=(struct sdio_queue*)READ_PERI_REG(SLC_RX_EOF_DES_ADDR);

                struct sdio_queue * next = (struct sdio_queue *)finishedDesc->next_link_ptr;
                uint32_t * buffer_pointer = (uint32_t*)next->buf_ptr;

                if( next->unused > 1)
                {
                        current_pixel_line = 0;
                }

                else if( next->unused )
                {
                        uint8_t pixel_column = 30;

                        /* Determines which terminal line we are currently rendering, 192 pixel lines >> 3 = 24 text lines. Each is 40 bytes long. */
                        uint8_t *terminal_line = &terminal_ram[40 * (current_pixel_line >> 3)];

                        /* 8 banks (one for each char line) of 192 bytes contain character definitions. Each character is pre-encoded to optimize performance. */
                        uint32_t *character_row = &Signetics_2513_Modulated_Video_ROM[(current_pixel_line & 0b111) * 192];

                        /* For each character in the line */
                        for( x = 0; x < 40; x++ )
                        {

                                /* Get pre-encoded character definition from array */
                                uint32_t *character = &character_row[3*(terminal_line[x] & 0x3F)];

                                /* Use loop unrolling to improve performance */
                                buffer_pointer[pixel_column++] = *character++;
                                buffer_pointer[pixel_column++] = *character++;
                                buffer_pointer[pixel_column++] = *character++;
                        }

                        current_pixel_line++;
                }
        }
}

#define SHORT_SYNC      0
#define LONG_SYNC       1
#define BLACK_SIGNAL    2
#define SHORT_TO_LONG   3
#define LONG_TO_SHORT   4
#define LINE_SIGNAL     5

//Initialize I2S subsystem for DMA circular buffer use
void ICACHE_FLASH_ATTR testi2s_init() {
        int x, y;

        uint32_t * line = i2s_dma_buffer;


#ifdef PAL
	/* PAL timings and definitions */
        uint8_t single_line_timings[20] = {
                SHORT_SYNC_INTERVAL,  LONG_SYNC_INTERVAL,   SHORT_SYNC_INTERVAL,  LONG_SYNC_INTERVAL,
                LONG_SYNC_INTERVAL,   SHORT_SYNC_INTERVAL,  LONG_SYNC_INTERVAL, SHORT_SYNC_INTERVAL,
                NORMAL_SYNC_INTERVAL, LINE_SIGNAL_INTERVAL,
                SHORT_SYNC_INTERVAL,  LONG_SYNC_INTERVAL,   SHORT_SYNC_INTERVAL,  LONG_SYNC_INTERVAL,
                LONG_SYNC_INTERVAL,   SHORT_SYNC_INTERVAL,  LONG_SYNC_INTERVAL, SHORT_SYNC_INTERVAL,
                NORMAL_SYNC_INTERVAL, LINE_SIGNAL_INTERVAL
        };

        /* Reference: http://martin.hinner.info/vga/pal_tv_diagram_interlace.jpg */

        uint16_t video_lines[42] = {
                3,   LONG_SYNC,     0,
                4,   LONG_TO_SHORT, 0,
                6,   SHORT_SYNC,    0,

                57,  BLACK_SIGNAL,  0,
                250, LINE_SIGNAL,   1,
                311, BLACK_SIGNAL,  0,

                313, SHORT_SYNC,    0,
                314, SHORT_TO_LONG, 0,
                316, LONG_SYNC,     0,
                318, SHORT_SYNC,    0,

                370, BLACK_SIGNAL,  0,
                562, LINE_SIGNAL,   1,
                623, BLACK_SIGNAL,  0,

                626, SHORT_SYNC,    0,
        };


#else
	/* NTSC timings and definitions */
        uint8_t single_line_timings[20] = {
                SHORT_SYNC_INTERVAL,  LONG_SYNC_INTERVAL,   SHORT_SYNC_INTERVAL,  LONG_SYNC_INTERVAL + 1,
                SERRATION_PULSE_INT,  NORMAL_SYNC_INTERVAL, SERRATION_PULSE_INT,  NORMAL_SYNC_INTERVAL + 1,
                NORMAL_SYNC_INTERVAL, LINE_SIGNAL_INTERVAL,
                SHORT_SYNC_INTERVAL,  LONG_SYNC_INTERVAL,   SERRATION_PULSE_INT, NORMAL_SYNC_INTERVAL + 1,
                SERRATION_PULSE_INT,  NORMAL_SYNC_INTERVAL, SHORT_SYNC_INTERVAL, LONG_SYNC_INTERVAL + 1,
                NORMAL_SYNC_INTERVAL, LINE_SIGNAL_INTERVAL,
        };
        
	/* Reference: http://www.avrfreaks.net/sites/default/files/ntsctime.pdf */ 
	uint16_t video_lines[48] = {
		/* Even Field */

		3,   SHORT_SYNC,    0,
		6,   LONG_SYNC,     0,
		9,   SHORT_SYNC,    0,

		40,  BLACK_SIGNAL,  0,
		233, LINE_SIGNAL,   1,
		263, BLACK_SIGNAL,  0,
		
		/* Odd Field */

		265, SHORT_SYNC,    0,
		266, SHORT_TO_LONG, 0,
		268, LONG_SYNC,     0,
		269, LONG_TO_SHORT, 0,
		271, SHORT_SYNC,    0,

		302, BLACK_SIGNAL,  0,
		495, LINE_SIGNAL,   1,
		525, BLACK_SIGNAL,  0,
	};

#endif

        uint32_t single_line_levels[20] = {
                SYNC_LEVEL, BLACK_LEVEL, SYNC_LEVEL,  BLACK_LEVEL,
                SYNC_LEVEL, BLACK_LEVEL, SYNC_LEVEL,  BLACK_LEVEL,
                SYNC_LEVEL, BLACK_LEVEL,
                SYNC_LEVEL, BLACK_LEVEL, SYNC_LEVEL,  BLACK_LEVEL,
                SYNC_LEVEL, BLACK_LEVEL, SYNC_LEVEL,  BLACK_LEVEL,
                SYNC_LEVEL, BLACK_LEVEL,
        };

        uint8_t i, signal;

        for (signal = 0; signal < 20; signal++)
                for (i=0; i < single_line_timings[signal]; i++, *line++ = single_line_levels[signal]) ;


        uint16_t *video_line = video_lines;

        //Initialize DMA buffer descriptors in such a way that they will form a circular
        //buffer.
        for (x=0; x<VIDEO_LINES; x++) {
                i2sBufDesc[x].owner=1;
                i2sBufDesc[x].eof=1;
                i2sBufDesc[x].sub_sof=0;
                i2sBufDesc[x].datalen=LINE_BUFFER_LENGTH*4;
                i2sBufDesc[x].blocksize=LINE_BUFFER_LENGTH*4;
                i2sBufDesc[x].next_link_ptr=(int)((x<(VIDEO_LINES-1)) ? (&i2sBufDesc[x+1]) : (&i2sBufDesc[0]));

                if (*video_line <= x + 1)
                        video_line += 3;

                i2sBufDesc[x].buf_ptr = (uint32_t)&i2s_dma_buffer[(*(video_line + 1))*LINE_BUFFER_LENGTH];
                i2sBufDesc[x].unused = *(video_line + 2);

        }

        i2sBufDesc[PIXEL_LINE_RESET_EVEN].unused = 2;
        i2sBufDesc[PIXEL_LINE_RESET_ODD].unused = 3;


        /* I2S DMA initialization code */

        //Reset DMA
        SET_PERI_REG_MASK(SLC_CONF0, SLC_RXLINK_RST|SLC_TXLINK_RST);
        CLEAR_PERI_REG_MASK(SLC_CONF0, SLC_RXLINK_RST|SLC_TXLINK_RST);

        //Clear DMA int flags
        SET_PERI_REG_MASK(SLC_INT_CLR,  0xffffffff);
        CLEAR_PERI_REG_MASK(SLC_INT_CLR,  0xffffffff);

        //Enable and configure DMA
        CLEAR_PERI_REG_MASK(SLC_CONF0, (SLC_MODE<<SLC_MODE_S));
        SET_PERI_REG_MASK(SLC_CONF0,(1<<SLC_MODE_S));
        SET_PERI_REG_MASK(SLC_RX_DSCR_CONF,SLC_INFOR_NO_REPLACE|SLC_TOKEN_NO_REPLACE);
        CLEAR_PERI_REG_MASK(SLC_RX_DSCR_CONF, SLC_RX_FILL_EN|SLC_RX_EOF_MODE | SLC_RX_FILL_MODE);

        CLEAR_PERI_REG_MASK(SLC_TX_LINK,SLC_TXLINK_DESCADDR_MASK);
        SET_PERI_REG_MASK(SLC_TX_LINK, ((uint32)&i2sBufDesc[1]) & SLC_TXLINK_DESCADDR_MASK); //any random desc is OK, we don't use TX but it needs something valid

        CLEAR_PERI_REG_MASK(SLC_RX_LINK,SLC_RXLINK_DESCADDR_MASK);
        SET_PERI_REG_MASK(SLC_RX_LINK, ((uint32)&i2sBufDesc[0]) & SLC_RXLINK_DESCADDR_MASK);

        //Attach the DMA interrupt
        ets_isr_attach(ETS_SLC_INUM, slc_isr);
        //Enable DMA operation intr
        WRITE_PERI_REG(SLC_INT_ENA,  SLC_RX_EOF_INT_ENA);
        //clear any interrupt flags that are set
        WRITE_PERI_REG(SLC_INT_CLR, 0xffffffff);
        ///enable DMA intr in cpu
        ets_isr_unmask(1<<ETS_SLC_INUM);

        //Start transmission
        SET_PERI_REG_MASK(SLC_TX_LINK, SLC_TXLINK_START);
        SET_PERI_REG_MASK(SLC_RX_LINK, SLC_RXLINK_START);

        //Init pins to i2s functions
        PIN_FUNC_SELECT(PERIPHS_IO_MUX_U0RXD_U, FUNC_I2SO_DATA);

        //Enable clock to i2s subsystem
        i2c_writeReg_Mask_def(i2c_bbpll, i2c_bbpll_en_audio_clock_out, 1);

        //Reset I2S subsystem
        CLEAR_PERI_REG_MASK(I2SCONF,I2S_I2S_RESET_MASK);
        SET_PERI_REG_MASK(I2SCONF,I2S_I2S_RESET_MASK);
        CLEAR_PERI_REG_MASK(I2SCONF,I2S_I2S_RESET_MASK);

        //Select 16bits per channel (FIFO_MOD=0), no DMA access (FIFO only)
        CLEAR_PERI_REG_MASK(I2S_FIFO_CONF, I2S_I2S_DSCR_EN|(I2S_I2S_RX_FIFO_MOD<<I2S_I2S_RX_FIFO_MOD_S)|(I2S_I2S_TX_FIFO_MOD<<I2S_I2S_TX_FIFO_MOD_S));

        //Enable DMA in i2s subsystem
        SET_PERI_REG_MASK(I2S_FIFO_CONF, I2S_I2S_DSCR_EN);

        //tx/rx binaureal
        CLEAR_PERI_REG_MASK(I2SCONF_CHAN, (I2S_TX_CHAN_MOD<<I2S_TX_CHAN_MOD_S)|(I2S_RX_CHAN_MOD<<I2S_RX_CHAN_MOD_S));

        //Clear int
        SET_PERI_REG_MASK(I2SINT_CLR,   I2S_I2S_TX_REMPTY_INT_CLR|I2S_I2S_TX_WFULL_INT_CLR|
                          I2S_I2S_RX_WFULL_INT_CLR|I2S_I2S_PUT_DATA_INT_CLR|I2S_I2S_TAKE_DATA_INT_CLR);
        CLEAR_PERI_REG_MASK(I2SINT_CLR, I2S_I2S_TX_REMPTY_INT_CLR|I2S_I2S_TX_WFULL_INT_CLR|
                            I2S_I2S_RX_WFULL_INT_CLR|I2S_I2S_PUT_DATA_INT_CLR|I2S_I2S_TAKE_DATA_INT_CLR);

        //trans master&rece slave,MSB shift,right_first,msb right
        CLEAR_PERI_REG_MASK(I2SCONF, I2S_TRANS_SLAVE_MOD|(I2S_BITS_MOD<<I2S_BITS_MOD_S)|(I2S_BCK_DIV_NUM <<I2S_BCK_DIV_NUM_S)|
                            (I2S_CLKM_DIV_NUM<<I2S_CLKM_DIV_NUM_S));

        SET_PERI_REG_MASK(I2SCONF, I2S_RIGHT_FIRST|I2S_MSB_RIGHT|I2S_RECE_SLAVE_MOD|I2S_RECE_MSB_SHIFT|I2S_TRANS_MSB_SHIFT|
                          ((WS_I2S_BCK&I2S_BCK_DIV_NUM )<<I2S_BCK_DIV_NUM_S)|((WS_I2S_DIV&I2S_CLKM_DIV_NUM)<<I2S_CLKM_DIV_NUM_S));

        //No idea if ints are needed...
        SET_PERI_REG_MASK(I2SINT_CLR,   I2S_I2S_TX_REMPTY_INT_CLR|I2S_I2S_TX_WFULL_INT_CLR|
                          I2S_I2S_RX_WFULL_INT_CLR|I2S_I2S_PUT_DATA_INT_CLR|I2S_I2S_TAKE_DATA_INT_CLR);
        CLEAR_PERI_REG_MASK(I2SINT_CLR,   I2S_I2S_TX_REMPTY_INT_CLR|I2S_I2S_TX_WFULL_INT_CLR|
                            I2S_I2S_RX_WFULL_INT_CLR|I2S_I2S_PUT_DATA_INT_CLR|I2S_I2S_TAKE_DATA_INT_CLR);
        //enable int
        SET_PERI_REG_MASK(I2SINT_ENA,   I2S_I2S_TX_REMPTY_INT_ENA|I2S_I2S_TX_WFULL_INT_ENA|
                          I2S_I2S_RX_REMPTY_INT_ENA|I2S_I2S_TX_PUT_DATA_INT_ENA|I2S_I2S_RX_TAKE_DATA_INT_ENA);

        //Start transmission
        SET_PERI_REG_MASK(I2SCONF,I2S_I2S_TX_START);
}


#define BASEFREQ (160000000L)
