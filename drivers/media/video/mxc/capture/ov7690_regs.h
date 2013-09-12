#ifndef OV7690_REGS_H

/* Registers */
#define REG_GAIN        	0x00    /* Gain lower 8 bits */

#define REG_BLUE        	0x01    /* blue gain */
#define REG_RED         	0x02    /* red gain */
#define REG_GREEN		0x03	/* green gain */

#define REG_YAVE		0x04	/* Frame Average level */
#define REG_BAVE        	0x05    /* B Pixel Average */
#define REG_RAVG		0x06	/* R Pixel Average */
#define REG_GAVG		0x07	/* G Pixel Average */

#define REG_PIDH		0x0a    /* Product ID MSB */
#define REG_PIDL		0x0b    /* Product ID LSB */

#define REG_COM0C	 	0x0c    /* Control 0C */
#define  COM0C_V_FLIP		  0x80	  /* Verti Flip */
#define  COM0C_BR_SWAP		  0x40    /* Horiz Mirror */
#define  COM0C_CLK_OUTPUT	  0x04    /* Reverse order of data bus */

#define REG_COM0D		0x0d    /* Control 0D */
#define REG_COM0E		0x0e    /* Control 0E */
#define REG_AECH		0x0f    /* High bits of AEC Value */
#define REG_AECL		0x10    /* Low  bits of AEC value */

#define REG_CLKRC		0x11    /* Clock control */
#define   CLK_EXT		  0x40    /* Use external clock */
#define   CLK_SCALE_MASK	  0x3f    /* internal clock pre-scale */

#define REG_COM12		0x12    /* Control 12 */
#define   COM12_RESET		  0x80    /* Register reset */
#define   COM12_SUBSAMPLE	  0x40    /* Subsample (skip) mode */
#define   COM12_ITU656		  0x20    /* ITU656 protocol */
#define	  COM12_RAW_OUTPUT	  0x10	  /* Select sensor raw data */
#define	  COM12_FMT_RGB565	  0x04	  /* RGB 565 format */
#define   COM12_YUV		  0x00    /* YUV */
#define   COM12_BAYER		  0x01    /* Bayer format */
#define	  COM12_RGB		  0x02	  /* RGB */

#define REG_COM13		0x13    /* Control 13 */
#define   COM13_FASTAEC		  0x80    /* Enable fast AGC/AEC */
#define   COM13_AECSTEP		  0x40    /* Unlimited AEC step size */
#define   COM13_BFILT		  0x20    /* Band filter enable */
#define   COM13_LOW_BAND_AEC	  0x10	  /* AEC below banding value */
#define	  COM13_EXP_SUB_LINE	  0x08	  /* enable sub-line exposure */
#define   COM13_AGC		  0x04    /* Auto gain enable */
#define   COM13_AWB		  0x02    /* White balance enable */
#define   COM13_AEC		  0x01    /* Auto exposure enable */
#define   COM13_DEFAULT		  0xf7	  /* Not datasheet, but actual */

#define REG_COM14		0x14    /* Control 14: gain ceiling */

#define REG_COM15		0x15    /* Control 15: frame rate ctrl */
#define	  COM15_REDUCE_NONE	  0x00	  /* Do not reduce fps */
#define	  COM15_REDUCE_1R2	  0x10	  /* Max reduce fps by 1/2 */
#define	  COM15_REDUCE_1R3	  0x20	  /* Max reduce fps by 1/3 */
#define	  COM15_REDUCE_1R4	  0x30	  /* Max reduce fps by 1/4 */
#define	  COM15_REDUCE_1R8	  0x40	  /* Max reduce fps by 1/8 */
#define	  COM15_AFR_1		  0x00	  /* Add frame when AGC=1*(1.9735) */
#define	  COM15_AFR_2		  0x04	  /* Add frame when AGC=2*(1.9735) */
#define	  COM15_AFR_4		  0x08	  /* Add frame when AGC=4*(1.9735) */
#define	  COM15_AFR_8		  0x0C	  /* Add frame when AGC=8*(1.9735) */
#define	  COM15_DIG_GAIN_NONE	  0x00	  /* No digital gain */
#define	  COM15_DIG_GAIN_2X	  0x01	  /* 2x digital gain */
#define	  COM15_DIF_GAIN_4X	  0x03	  /* 4x digital gain */

#define REG_COM16	0x16
#define REG_HSTART      0x17    /* Horiz start point control */
#define REG_HSIZE	0x18	/* Horiz sensor size MSB (REG_COM16 for LSB)*/
#define REG_VSTART      0x19    /* Vert window start line */
#define REG_VSIZE	0x1A	/* Vert sensor size (2x) */
#define REG_PSHFT       0x1b    /* Pixel delay after HREF */

#define REG_MIDH        0x1c    /* Manuf. ID high */
#define REG_MIDL        0x1d    /* Manuf. ID low */

#define REG_AGC_UL         0x24    /* AGC upper limit */
#define REG_AGC_LL         0x25    /* AGC lower limit */
#define REG_VPT		0x26    /* AGC/AEC fast mode op region */

#define REG_PLL         0x29    /* PLL settings */
#define   PLL_DIV_1       0x00
#define   PLL_DIV_2       0x40
#define   PLL_DIV_3       0x80
#define   PLL_DIV_4       0xC0
#define   PLL_BYPASS      0x00
#define   PLL_4X          0x10
#define   PLL_6X          0x20
#define   PLL_8X          0x30

#define REG_COMB4       0xB4
#define   COMB4_SHRP_AUTO 0x20
#define   COMB4_NOIS_AUTO 0x10
#define   COMB4_BEST_EDGE 0x0F /* Best edge we can get */

#endif /* OV7690_REGS_H */
