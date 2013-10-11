#ifndef OV7690_REGS_H

/*!
*	Helpful i2c defs
*	Terminating list entry for reg, val, len
*/
#define I2C_REG_TERM    0xFF
#define I2C_VAL_TERM    0xFF
#define I2C_LEN_TERM    0xFF


/*! 
*	Individual register settings
*/
#define REG_GAIN        	0x00    /* Gain lower 8 bits */

#define REG_BLUE        	0x01    /* blue gain */
#define REG_RED         	0x02    /* red gain */
#define REG_GREEN           0x03	/* green gain */

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
#define	  COM13_AEC_MASK	  0x01
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

#define REG_COM16		0x16
#define REG_HSTART		0x17    /* Horiz start point control */
#define REG_HSIZE		0x18	/* Horiz sensor size MSB (REG_COM16 for LSB)*/
#define REG_VSTART		0x19    /* Vert window start line */
#define REG_VSIZE		0x1A	/* Vert sensor size (2x) */
#define REG_PSHFT		0x1b    /* Pixel delay after HREF */

#define REG_MIDH		0x1c    /* Manuf. ID high */
#define REG_MIDL		0x1d    /* Manuf. ID low */

#define REG_COM22		0x22	/* Optical Black Output*/
#define   COM22_BLACK_ENABLE	  0x80

#define REG_AGC_UL		0x24    /* AGC upper limit */
#define REG_AGC_LL		0x25    /* AGC lower limit */
#define REG_VPT			0x26    /* AGC/AEC fast mode op region */

#define REG_PLL			0x29    /* PLL settings */
#define   PLL_DIV_1		  0x00
#define   PLL_DIV_2		  0x40
#define   PLL_DIV_3		  0x80
#define   PLL_DIV_4		  0xC0
#define	  PLL_DIV_MASK		  0xC0
#define   PLL_BYPASS		  0x00
#define   PLL_4X		  0x10
#define   PLL_6X		  0x20
#define   PLL_8X		  0x30
#define	  PLL_OUT_MASK		  0x30

#define REG_COMB4		0xB4
#define   COMB4_SHRP_AUTO	  0x20
#define   COMB4_NOIS_AUTO	  0x10
#define   COMB4_BEST_EDGE	  0x0F	/* Best edge we can get */
#define   COMB4_EDGE_MASK	  0x0F

#define REG_COMC8		0xC8	/* 2 MSBs of horz input size */
#define REG_COMC9		0xC9	/* 8 LSBs of horz input size */
#define REG_COMCA		0xCA	/* 2 MSBs of vert input size */
#define REG_COMCB		0xCB	/* 8 LSBs of vert input size */
#define REG_COMCC		0xCC	/* 2 MSBs of horz output size */
#define REG_COMCD		0xCD	/* 8 LSBs of horz output size */
#define REG_COMCE		0xCE	/* 2 MSBs of vert output size */
#define REG_COMCF		0xCF	/* 8 LSBs of vert output size */
#define   COMC8_CF_MSB_MASK	  0x03
#define   COMC8_CF_LSB_MASK	  0xFF

#define REG_YBRIGHT     0xD3    /* "YBRIGHT" */

/*! 
*	Data structure for OV7690 register and associated values
*/
struct ov7690_reg {
    u8 addr;
    u8 val;
	u8 mask;
};

/* No mask */
#define MASK_NONE 0x00
/* Predicate for testing mask */
#define IS_MASKED(r) (r).mask

#define OV7690_REG_TERM { I2C_REG_TERM, I2C_VAL_TERM, MASK_NONE }


/*!
*	Data structures for OV7690 modes of operation
*/
enum ov7690_mode {
        ov7690_mode_MIN = 0,
        ov7690_mode_VGA_640_480 = 0,
/*      ov7690_mode_QVGA_320_240 = 1,
        ov7690_mode_NTSC_720_480 = 2,
        ov7690_mode_PAL_720_576 = 3,
        ov7690_mode_720P_1280_720 = 4,
        ov7690_mode_1080P_1920_1080 = 5,
        ov7690_mode_QSXGA_2592_1944 = 6,
*/
        ov7690_mode_MAX = 0 /* TODO keep up to date! */
};

const char * ov7690_mode_names[] = {
    "VGA (640, 480)"
};

/* Helper mode check function */
#define IS_VALID_MODE(m) ((m) <= ov7690_mode_MAX && (m) >= ov7690_mode_MIN)

enum ov7690_frame_rate {
        ov7690_MIN_fps = 0,
        ov7690_30_fps  = 0,
        ov7690_60_fps  = 1,
        ov7690_MAX_fps = 1
};

const u8 ov7690_frame_rate_values[] = {
    30,
    60
};

struct ov7690_mode_info {
        enum ov7690_mode mode;
        u32 width;
        u32 height;
        struct ov7690_reg *init_data_ptr;
        u32 init_data_size;
};


/*!
*	Groups of register settings
*/

/* Our default register settings */
const struct ov7690_reg ov7690_default_regs[] = {
        { REG_COM12,    COM12_RESET,	MASK_NONE },	/* Reset registers */

        { REG_COMB4,    COMB4_BEST_EDGE,COMB4_EDGE_MASK },/* best edge! */
        { REG_COM13,    COM13_AEC,	COM13_AEC_MASK },/* manual exposure */

        /*END MARKER*/
	OV7690_REG_TERM
};


/* Solutions to high chroma noise */
/* 1) low frame rate, high exposure time*/
const struct ov7690_reg low_fr_high_exp[] = {
        /* decrease frame rate */
        { REG_CLKRC,    0x03,		CLK_SCALE_MASK },
	{ REG_PLL,	PLL_DIV_4,	PLL_DIV_MASK },
	{ REG_PLL,	PLL_4X,		PLL_OUT_MASK },
	
        /* increase exposure */
        { REG_AECH,     0x02,		MASK_NONE },
        { REG_AECL,     0xFF,		MASK_NONE },

        /*END MARKER*/
	OV7690_REG_TERM
};

/* 2) compromise: mid frame rate, mid exposure time */
/* TODO Exactly the same as VGA mode */
const struct ov7690_reg mid_fr_mid_exp[] = 
{
        /* decrease frame rate */
        { REG_CLKRC,    0x03,		CLK_SCALE_MASK },
        { REG_PLL,      PLL_DIV_1,	PLL_DIV_MASK },
	{ REG_PLL,	PLL_4X,		PLL_OUT_MASK },

        /* increase exposure */
        { REG_AECH,     0x02,		MASK_NONE },
        { REG_AECL,     0xFF,		MASK_NONE },

        /*END MARKER*/
	OV7690_REG_TERM
};

/* Manual white balance modes
 To use these, you must first enable manual mode for AWB
 i.e. bit 0 in REG_COM8 must be 0

 Taken and modified from:
 https://github.com/omegamoon/rockchip-rk30xx-mk808/blob/master/drivers/media/video/ov7690.c
*/
/* Cloudy Colour Temperature : 6500K - 8000K */
const struct ov7690_reg awb_cloudy[] = 
{
        { REG_BLUE,     0x40,		MASK_NONE },
        { REG_RED,      0x5d,		MASK_NONE },
        { REG_GREEN,    0x40,		MASK_NONE },

	/*END MARKER*/
	OV7690_REG_TERM
};

/* Clear Day Colour Temperature : 5000K - 6500K */
const struct ov7690_reg awb_clear[] =
{
        { REG_BLUE,     0x44,		MASK_NONE },
        { REG_RED,      0x55,		MASK_NONE },
        { REG_GREEN,    0x40,		MASK_NONE },

	/*END MARKER*/
	OV7690_REG_TERM
};

/* Office Colour Temperature : 3500K - 5000K */
const struct ov7690_reg awb_tungsten[]=
{
        { REG_BLUE,     0x5b,		MASK_NONE },
        { REG_RED,      0x4c,		MASK_NONE },
        { REG_GREEN,    0x40,		MASK_NONE }, 

	/*END MARKER*/
	OV7690_REG_TERM
};

const struct ov7690_reg *awb_manual_mode_list[] = {
        awb_tungsten, awb_clear, awb_cloudy, NULL
};


/*!
*	Supported OV7690 Modes
*/
const struct ov7690_reg ov7690_mode_VGA_data[] = 
{
        /* decrease frame rate */
        { REG_CLKRC,    0x03,           CLK_SCALE_MASK },
        { REG_PLL,      PLL_DIV_1,      PLL_DIV_MASK },
        { REG_PLL,      PLL_4X,         PLL_OUT_MASK },

        /* increase exposure */
        { REG_AECH,     0x02,           MASK_NONE },
        { REG_AECL,     0xFF,           MASK_NONE },

        /*END MARKER*/
        OV7690_REG_TERM
};

/* Taken from https://gitorious.org/flow-g1-5/kernel_flow */
/*      TODO Output is weird - 4 screens, first column duplicated QVGA, 
second two green areas 
*/
#if 0 /* TODO enable */
const struct ov7690_reg ov7690_qvga_regs[] = {
        { REG_COM16,	0x03 },
        { REG_HSTART,	0x69 },
        { REG_HSIZE,	0xa4 },
        { REG_VSTART,	0x06 },
        { REG_VSIZE,	0xf6 },
        { REG_COM22,	0x10 },
        { REG_COMC8,	0x02 },
        { REG_COMC9,	0x80 },
        { REG_COMCA,	0x00 },
        { REG_COMCB,	0xf0 },
        { REG_COMCC,	0x01 },
        { REG_COMCD,	0x40 },
        { REG_COMCE,	0x00 },
        { REG_COMCF,	0xf0 },

        /*END MARKER*/
        { I2C_REG_TERM, I2C_VAL_TERM }
};
#endif

/* Array of operational modes 
* First dimension is fps, second is output mode
*/
/* TODO using arrays for this sort of thing is a terrible hack! */
#define DECLARE_MODE_FPS_NOT_SUPPORTED(mode) {(mode), 0, 0, NULL, 0}
const struct ov7690_mode_info 
ov7690_mode_info_data[ov7690_MAX_fps+1][ov7690_mode_MAX+1] =
{
        /* 30 fps */
        {
                {ov7690_mode_VGA_640_480,       640,      480,
                 ov7690_mode_VGA_data, ARRAY_SIZE(ov7690_mode_VGA_data)}
        },

        /* 60 fps */
        {
                DECLARE_MODE_FPS_NOT_SUPPORTED(ov7690_mode_VGA_640_480)
        }
};

#undef DECLARE_MODE_FPS_NOT_SUPPORTED
#define MODE_FPS_NOT_SUPPORTED(m) ((m).width == 0 || (m).height == 0 ||\
	(m).init_data_ptr == NULL || (m).init_data_size == 0)


#endif /* OV7690_REGS_H */
