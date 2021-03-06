#ifndef OV7690_REGS_H
#define OV7690_REGS_H

/*!
*	Helpful i2c defs
*	Terminating list entry for reg, val, len
*/
#define I2C_REG_TERM		0xFF
#define I2C_VAL_TERM		0xFF
#define I2C_LEN_TERM		0xFF


/*! 
*	Individual register settings
*/
#define REG_GAIN			0x00	/* Gain lower 8 bits */

#define REG_BLUE			0x01	/* blue gain */
#define REG_RED				0x02	/* red gain */
#define REG_GREEN			0x03	/* green gain */

#define REG_YAVE			0x04	/* Frame Average level */
#define REG_BAVE			0x05	/* B Pixel Average */
#define REG_RAVG			0x06	/* R Pixel Average */
#define REG_GAVG			0x07	/* G Pixel Average */

#define REG_PIDH			0x0a	/* Product ID MSB */
#define REG_PIDL			0x0b	/* Product ID LSB */

#define REG_COM0C			0x0c	/* Control 0C */
#define  COM0C_V_FLIP		  0x80	  /* Verti Flip */
#define  COM0C_BR_SWAP		  0x40	  /* Horiz Mirror */
#define  COM0C_CLK_OUTPUT	  0x04	  /* Reverse order of data bus */

#define REG_COM0D			0x0d	/* Control 0D */	
#define REG_COM0E			0x0e	/* Control 0E */
#define REG_AECH			0x0f	/* High bits of AEC Value */
#define REG_AECL			0x10	/* Low  bits of AEC value */

#define REG_CLKRC			0x11	/* Clock control */
#define   CLK_EXT			  0x40 	  /* Use external clock */
#define   CLK_SCALE_MASK	  0x3f	  /* internal clock pre-scale */

#define REG_COM12			0x12	/* Control 12 */
#define   COM12_RESET		  0x80    /* Register reset */
#define   COM12_SUBSAMPLE	  0x40	  /* Subsample (skip) mode */
#define   COM12_ITU656		  0x20	  /* ITU656 protocol */
#define	  COM12_RAW_OUTPUT	  0x10	  /* Select sensor raw data */
#define	  COM12_FMT_RGB565	  0x04	  /* RGB 565 format */
#define   COM12_YUV			  0x00	  /* YUV */
#define   COM12_BAYER		  0x01	  /* Bayer format */
#define	  COM12_RGB			  0x02	  /* RGB */

#define REG_COM13			0x13	/* Control 13 */
#define   COM13_FASTAEC		  0x80	  /* Enable fast AGC/AEC */
#define   COM13_AECSTEP		  0x40	  /* Unlimited AEC step size */
#define   COM13_BFILT		  0x20	  /* Band filter enable */
#define   COM13_LOW_BAND_AEC  0x10	  /* AEC below banding value */
#define	  COM13_EXP_SUB_LINE  0x08	  /* enable sub-line exposure */
#define   COM13_AGC			  0x04	  /* Auto gain enable */
#define   COM13_AWB			  0x02	  /* White balance enable */
#define	  COM13_AEC			  0x01	  /* Auto exposure control enable */
#define   COM13_DEFAULT		  0xf7	  /* Not datasheet, but actual */

#define REG_COM14			0x14	/* Control 14: gain ceiling */

#define REG_COM15			0x15	/* Control 15: frame rate ctrl */
#define	  COM15_REDUCE_FPS	  0x70	  /* Mask for fps reduce settings */
#define	  COM15_REDUCE__NONE	0x0 	/* Do not reduce fps */
#define	  COM15_REDUCE__1R2		0x1		/* Max reduce fps by 1/2 */
#define	  COM15_REDUCE__1R3		0x2		/* Max reduce fps by 1/3 */
#define	  COM15_REDUCE__1R4		0x3		/* Max reduce fps by 1/4 */
#define	  COM15_REDUCE__1R8		0x4		/* Max reduce fps by 1/8 */
#define	  COM15_AFR			  0x0C	  /* Mask for add frame settings */
#define	  COM15_AFR__1			0x0		/* Add frame when AGC=1*(1.9735) */
#define	  COM15_AFR__2			0x1		/* Add frame when AGC=2*(1.9735) */
#define	  COM15_AFR__4			0x2		/* Add frame when AGC=4*(1.9735) */
#define	  COM15_AFR__8			0x3		/* Add frame when AGC=8*(1.9735) */
#define	  COM15_DIG_GAIN	  0x03	  /* Mask for digital gain */
#define	  COM15_DIG_GAIN__NONE	0x0	/* No digital gain */
#define	  COM15_DIG_GAIN__2X	0x1	/* 2x digital gain */
#define	  COM15_DIF_GAIN__4X	0x3	/* 4x digital gain */

#define REG_COM16			0x16
#define REG_HSTART			0x17	/* Horiz start point control */
#define REG_HSIZE			0x18	/* Horiz sensor size MSB (REG_COM16 for LSB)*/
#define REG_VSTART			0x19	/* Vert window start line */
#define REG_VSIZE			0x1A	/* Vert sensor size (2x) */
#define REG_PSHFT			0x1b	/* Pixel delay after HREF */

#define REG_MIDH			0x1c  /* Manuf. ID high */
#define REG_MIDL			0x1d  /* Manuf. ID low */

#define REG_COM22			0x22	/* Optical Black Output*/
#define   COM22_BLACK		  0x80

#define REG_AGC_UL			0x24	/* AGC upper limit */
#define REG_AGC_LL			0x25	/* AGC lower limit */
#define REG_VPT				0x26	/* AGC/AEC fast mode op region */

#define REG_PLL				0x29	/* PLL settings */
#define	  PLL_DIV			  0xC0	  /* PLL divider mask */
#define   PLL_DIV__1			0x0
#define   PLL_DIV__2			0x1
#define   PLL_DIV__3			0x2
#define   PLL_DIV__4			0x3
#define	  PLL_OUT			  0x30	  /* PLL output control mask */
#define   PLL_OUT__BYPASS		0x0
#define   PLL_OUT__4X			0x1
#define   PLL_OUT__6X			0x2
#define   PLL_OUT__8X			0x3

#define REG_COMB4			0xB4
#define   COMB4_SHRP_AUTO	  0x20
#define   COMB4_NOIS_AUTO	  0x10
#define   COMB4_BEST_EDGE	  0x0f	/* Best edge we can get */
#define   COMB4_EDGE_MASK	  0x0F

#define REG_COMC8			0xC8	/* 2 MSBs of horz input size */
#define REG_COMC9			0xC9	/* 8 LSBs of horz input size */
#define REG_COMCA			0xCA	/* 2 MSBs of vert input size */
#define REG_COMCB			0xCB	/* 8 LSBs of vert input size */
#define REG_COMCC			0xCC	/* 2 MSBs of horz output size */
#define REG_COMCD			0xCD	/* 8 LSBs of horz output size */
#define REG_COMCE			0xCE	/* 2 MSBs of vert output size */
#define REG_COMCF			0xCF	/* 8 LSBs of vert output size */
#define   COMC8_CF_MSB_MASK	  0x03
#define   COMC8_CF_LSB_MASK	  MASK_NONE /* for readability */

#define REG_YBRIGHT 		0xD3	/* "YBRIGHT" */
#define REG_YOFFSET			0xD5	/* "YOFFSET" */


/*! 
*	Data structure for OV7690 register and associated values
*/
struct ov7690_reg {
	u8 addr;
	u8 val;
	u8 mask;
};

/* A fake register representing the end of a list of register settings */
#define OV7690_REG_TERM { I2C_REG_TERM, I2C_VAL_TERM, MASK_NONE }

/*!
*	Data structure for aiding bitfield read/write
*/
struct ov7690_reg_control {
	u8 addr;
	u8 mask;
};

/* No mask */
#define MASK_NONE 0x00
/* Predicate for testing mask */
#define IS_MASKED(r) (r).mask


/*!
*	Data structures for OV7690 modes of operation
*/

/* Mode enum */
enum ov7690_mode {
		ov7690_mode_MIN = -1,
		ov7690_mode_VGA_640_480 = 0,
		ov7690_mode_QVGA_320_240,
		ov7690_mode_CIF_352_288,
		ov7690_mode_QCIF_176_144,

		ov7690_mode_MAX
};

/* Mode names (for debuging) */
/* TODO needs to be kept up to date with above enum! */
const char * ov7690_mode_names[ov7690_mode_MAX] = {
	"VGA (640, 480)",
	"QVGA (320, 240)",
	"CIF (352, 288)",
	"QCIF (176, 144)"
};

/* Helper mode check function */
#define IS_VALID_MODE(m) ((m) < ov7690_mode_MAX && ((s32)(m)) > ov7690_mode_MIN)

/* Frame rate enum */
enum ov7690_frame_rate {
		ov7690_MIN_fps = 0,
		ov7690_30_fps  = 0,
		ov7690_60_fps  = 1,
		ov7690_MAX_fps = 1
};

const u8 ov7690_frame_rate_values[2] = {
	30,
	60
};

/* Helper defs for frame rate values */
#define MAX_FPS ov7690_frame_rate_values[ov7690_MAX_fps]
#define MIN_FPS ov7690_frame_rate_values[ov7690_MIN_fps]
#define DEFAULT_FPS ov7690_frame_rate_values[ov7690_30_fps]

/* Struct containing important mode information */
struct ov7690_mode_info {
		enum ov7690_mode mode;

		/* Pixel format information */
		u32 pixel_format;
		__u8 description[32];

		/* Supported dimensions */
		u32 width;
		u32 height;

		/* Register settings for mode */
		struct ov7690_reg *reg_list;
		u32 reg_list_sz;
};


/*!
*	Groups of register settings
*/

/* Our default register settings. 
	Camera must be set to these *every* time we change modes */
const struct ov7690_reg ov7690_default_regs[] = {
	/*
	 * Reset registers
	 */
	{ REG_COM12,	COM12_RESET,	MASK_NONE },

	/*
	 * Improve edge definition
	 */
	{ REG_COMB4,	COMB4_BEST_EDGE,COMB4_EDGE_MASK },

	/* 
	 * Lower chroma gain through increased exposure time
	 */
	{ REG_CLKRC,	0x01,			CLK_SCALE_MASK },	/* lower clockspeed so more exposure happens */
	{ REG_COM13,	0x00,			COM13_AEC },		/* disable automatic exposure control */
	{ REG_AECH,		0x02,			MASK_NONE },		/* increase exposure time to "too high"  			*/
	{ REG_AECL,		0xFF,			MASK_NONE },		/* (will automatically reduce to frame time if so)	*/

	/*END MARKER*/
	OV7690_REG_TERM
};

/* Solutions to high chroma noise */
/* 1) low frame rate, high exposure time*/
const struct ov7690_reg low_fr_high_exp[] = {
	/* decrease frame rate */
	{ REG_CLKRC,	0x03,		CLK_SCALE_MASK },
	{ REG_PLL,		PLL_DIV__4,	PLL_DIV },
	{ REG_PLL,		PLL_OUT__4X,PLL_OUT },
	
	/* increase exposure */
	{ REG_AECH, 	0x02,		MASK_NONE },
	{ REG_AECL, 	0xFF,		MASK_NONE },

	/*END MARKER*/
	OV7690_REG_TERM
};

/* 2) compromise: mid frame rate, mid exposure time */
/* TODO Exactly the same as VGA mode */
const struct ov7690_reg mid_fr_mid_exp[] = 
{
	/* decrease frame rate */
	{ REG_CLKRC,	0x03,		CLK_SCALE_MASK },
	{ REG_PLL,		PLL_DIV__1,	PLL_DIV },
	{ REG_PLL,		PLL_OUT__4X,PLL_OUT },

	/* increase exposure */
	{ REG_AECH,		0x02,		MASK_NONE },
	{ REG_AECL,		0xFF,		MASK_NONE },

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
	{ REG_BLUE,		0x40,	MASK_NONE },
	{ REG_RED,		0x5d,	MASK_NONE },
	{ REG_GREEN,	0x40,	MASK_NONE },

	/*END MARKER*/
	OV7690_REG_TERM
};

/* Clear Day Colour Temperature : 5000K - 6500K */
const struct ov7690_reg awb_clear[] =
{
	{ REG_BLUE,		0x44,	MASK_NONE },
	{ REG_RED,		0x55,	MASK_NONE },
	{ REG_GREEN,	0x40,	MASK_NONE },

	/*END MARKER*/
	OV7690_REG_TERM
};

/* Office Colour Temperature : 3500K - 5000K */
const struct ov7690_reg awb_tungsten[]=
{
	{ REG_BLUE,		0x5b,	MASK_NONE },
	{ REG_RED,		0x4c,	MASK_NONE },
	{ REG_GREEN,	0x40,	MASK_NONE }, 

	/*END MARKER*/
	OV7690_REG_TERM
};

const struct ov7690_reg *awb_manual_mode_list[] = {
	awb_tungsten, awb_clear, awb_cloudy, NULL
};


/*!
 *	Supported OV7690 Modes
 *	These all assume that the registers have been reset to default before hand
 */

/* Custom VGA data 
 * Currently no different from default register values
 */
const struct ov7690_reg ov7690_mode_VGA_data[] = { OV7690_REG_TERM };

/* Custom QVGA data */
const struct ov7690_reg ov7690_mode_QVGA_data[] = 
{

	{ REG_COM12,	0x01,	COM12_SUBSAMPLE },	/* Enable subsampling */
	{ REG_VSTART,	0x00,	MASK_NONE },		/* TODO you can't explain that */

	/* Our horizontal input size is 640 (subsampled) but output is 320 */
	{ REG_COMCC,	0x01,	COMC8_CF_MSB_MASK },
	{ REG_COMCD,	0x40,	COMC8_CF_LSB_MASK },

	/* Sub-sampling already halves the vertical output, 
		so no need to modify it here unless we change that */

	/*END MARKER*/
	OV7690_REG_TERM
};

/* Custom CIF data */
const struct ov7690_reg ov7690_mode_CIF_data[] = 
{

	/* Our horizontal input size is 640 but output is 352 */
	{ REG_COMCC,	0x01,	COMC8_CF_MSB_MASK },
	{ REG_COMCD,	0x60,	COMC8_CF_LSB_MASK },

	/* Our vertical input size is 480 but output is 288 */
	{ REG_COMCE,	0x01,	COMC8_CF_MSB_MASK },
	{ REG_COMCF,	0x20,	COMC8_CF_LSB_MASK },

	/*END MARKER*/
	OV7690_REG_TERM
};

/* Custom QCIF data */
const struct ov7690_reg ov7690_mode_QCIF_data[] =
{

	{ REG_COM12,	0x01,	COM12_SUBSAMPLE },	/* Enable subsampling */
	{ REG_VSTART,	0x00,	MASK_NONE },		/* TODO you can't explain that */

	/* Our horizontal input size is 640 but output is 176 */
	{ REG_COMCC,	0x00,	COMC8_CF_MSB_MASK },
	{ REG_COMCD,	0xb0,	COMC8_CF_LSB_MASK },

	/* Here's the exciting part: since subsampling halves vertical output,
	 * we need to ask the device for twice the vertical output of QCIF.
	 *
	 * So, our capture size is subsampled 480 (240) but we want subsampled 288 (144)
	 */
	{ REG_COMCE,	0x01,	COMC8_CF_MSB_MASK },
	{ REG_COMCF,	0x20,	COMC8_CF_LSB_MASK },

	/*END MARKER*/
	OV7690_REG_TERM
};

/* Taken from https://gitorious.org/flow-g1-5/kernel_flow */
/*	TODO Output is weird - 4 screens, first column duplicated QVGA, 
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
/* TODO update to match current definition of mode info */
#define DECLARE_MODE_FPS_NOT_SUPPORTED(mode) { \
	(mode), \
	0, {NULL}, \
	0, 0, \
	NULL, 0 \
}

const struct ov7690_mode_info 
ov7690_mode_info_data[ov7690_MAX_fps+1][ov7690_mode_MAX+1] =
{
		/* 30 fps */
		{
				/* VGA */
				{
					ov7690_mode_VGA_640_480, 
					V4L2_PIX_FMT_YUYV, "YUYV",
					640, 480,
					ov7690_mode_VGA_data, ARRAY_SIZE(ov7690_mode_VGA_data)
				}, 

				/* QVGA */
				{
					ov7690_mode_QVGA_320_240,
					V4L2_PIX_FMT_YUYV, "YUYV",
					320, 240,
					ov7690_mode_QVGA_data, ARRAY_SIZE(ov7690_mode_QVGA_data)
				},

				/* CIF */
				{
					ov7690_mode_CIF_352_288,
					V4L2_PIX_FMT_YUYV, "YUYV",
					352, 288,
					ov7690_mode_CIF_data, ARRAY_SIZE(ov7690_mode_CIF_data)
				},

				/* QCIF */
				{
					ov7690_mode_QCIF_176_144,
					V4L2_PIX_FMT_YUYV, "YUYV",
					176, 144,
					ov7690_mode_QCIF_data, ARRAY_SIZE(ov7690_mode_QCIF_data)
				}
		},

		/* 60 fps */
		{
				/* VGA */
				DECLARE_MODE_FPS_NOT_SUPPORTED(ov7690_mode_VGA_640_480),

				/* QVGA */
				{
					ov7690_mode_QVGA_320_240,
					V4L2_PIX_FMT_YUYV, "YUYV",
					320, 240,
					ov7690_mode_QVGA_data, ARRAY_SIZE(ov7690_mode_QVGA_data)
				},

				/* CIF */
				DECLARE_MODE_FPS_NOT_SUPPORTED(ov7690_mode_CIF_352_288),

				/* QCIF */
				{
					ov7690_mode_QCIF_176_144,
					V4L2_PIX_FMT_YUYV, "YUYV",
					176, 240,
					ov7690_mode_QCIF_data, ARRAY_SIZE(ov7690_mode_QCIF_data)
				}
		}
};
#undef DECLARE_MODE_FPS_NOT_SUPPORTED

#define MODE_FPS_NOT_SUPPORTED(m) ((m).width == 0 || (m).height == 0 ||\
	(m).reg_list == NULL || (m).reg_list_sz == 0)


#endif /* OV7690_REGS_H */