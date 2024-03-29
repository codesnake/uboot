/*
 * AMLOGIC T13 LCD panel driver.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the named License,
 * or any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA
 *
 * Author:  Elvis Yu <elvis.yu@amlogic.com>
 *
 */

#include <common.h>
#include <asm/arch/io.h>
#include <asm/arch/pinmux.h>
#include <asm/arch/gpio.h>
#include <asm/arch/lcdoutc.h>
#include <asm/arch/osd.h>
#include <asm/arch/osd_hw.h>
#include <aml_i2c.h>
#include <amlogic/aml_lcd.h>
#include <amlogic/vinfo.h>
#include <sn7325.h>
#include <video_fb.h>

#ifdef CONFIG_AW_AXP20
#include <axp-gpio.h>
#endif

#define DEBUG

extern GraphicDevice aml_gdev;
vidinfo_t panel_info;

//Define backlight control method
#define BL_CTL_GPIO		0
#define BL_CTL_PWM		1
#define BL_CTL			BL_CTL_GPIO
#define DEFAULT_BL_LEVEL	128

#if(BL_CTL==BL_CTL_PWM)
#define PWM_MAX         60000   //PWM_MAX <= 65535
#define	PWM_PRE_DIV		0		//pwm_freq = 24M / (pre_div + 1) / PWM_MAX	
#endif

#define BL_MAX_LEVEL	255
#define BL_MIN_LEVEL	0

static unsigned bl_level = 0;

static void ttl_ports_ctrl(Bool_t status)
{ 
    debug("%s: %s\n", __FUNCTION__, (status ? "ON" : "OFF"));
	if (status) 
	{
        set_mio_mux(1, ((1<<14)|(1<<17)|(1<<18)|(1<<19)));
    	set_mio_mux(0,(3<<0)|(3<<2)|(3<<4));   //For 8bits
		//set_mio_mux(0,(1<<0)|(1<<2)|(1<<4));   //For 6bits
    }else {
        clear_mio_mux(1, ((1<<14)|(1<<17)|(1<<18)|(1<<19)));
    	clear_mio_mux(0,(3<<0)|(3<<2)|(3<<4));   //For 8bits
		//clear_mio_mux(0,(1<<0)|(1<<2)|(1<<4));   //For 6bits
    }
}

static void backlight_power_ctrl(Bool_t status)
{
	debug("%s: power %s\n", __FUNCTION__, (status ? "ON" : "OFF"));
    if( status == ON )
	{
	    mdelay(20);
	    ttl_ports_ctrl(ON);    
	    WRITE_CBUS_REG_BITS(LED_PWM_REG0, 1, 12, 2);
		mdelay(300);	
		//BL_EN: GPIOD_1(PWM_D)
#if (BL_CTL==BL_CTL_GPIO)	
	    set_gpio_val(GPIOD_bank_bit0_9(1), GPIOD_bit_bit0_9(1), 1);
	    set_gpio_mode(GPIOD_bank_bit0_9(1), GPIOD_bit_bit0_9(1), GPIO_OUTPUT_MODE);
#elif (BL_CTL==BL_CTL_PWM)
	    WRITE_CBUS_REG_BITS(PWM_PWM_D, 0, 0, 16);  		//pwm low
		WRITE_CBUS_REG_BITS(PWM_PWM_D, PWM_MAX, 16, 16);	//pwm high
		WRITE_MPEG_REG(PWM_MISC_REG_CD, (READ_MPEG_REG(PWM_MISC_REG_CD) & ~(0x7f<<16)) | ((1 << 23) | (PWM_PRE_DIV<<16) | (1<<1)));  //enable pwm clk & pwm output
		WRITE_MPEG_REG(PERIPHS_PIN_MUX_2, READ_MPEG_REG(PERIPHS_PIN_MUX_2) | (1<<3));  //enable pwm pinmux
#endif
    	mdelay(20);
	}
	else
	{
		mdelay(20);	
		//BL_EN -> GPIOD_1: 0	
	    WRITE_MPEG_REG(PWM_MISC_REG_CD, READ_MPEG_REG(PWM_MISC_REG_CD) & ~((1 << 23) | (1<<1)));  //disable pwm_clk & pwm port
		set_gpio_val(GPIOD_bank_bit0_9(1), GPIOD_bit_bit0_9(1), 0);
	    set_gpio_mode(GPIOD_bank_bit0_9(1), GPIOD_bit_bit0_9(1), GPIO_OUTPUT_MODE);
		
	    mdelay(20);
	    ttl_ports_ctrl(OFF);    
	    mdelay(20);
	}	
}

unsigned get_backlight_level(void)
{
    debug("%s :%d\n", __FUNCTION__,bl_level);
    return bl_level;
}

void set_backlight_level(unsigned level)
{
	debug("%s :%d\n", __FUNCTION__,level);
    level = level>BL_MAX_LEVEL ? BL_MAX_LEVEL:(level<BL_MIN_LEVEL ? BL_MIN_LEVEL:level);
	bl_level=level;

#if (BL_CTL==BL_CTL_GPIO)
	level = level * 15 / BL_MAX_LEVEL;
	level = 15 - level;
	WRITE_CBUS_REG_BITS(LED_PWM_REG0, level, 0, 4);
#elif (BL_CTL==BL_CTL_PWM)
	level = level * PWM_MAX / BL_MAX_LEVEL ;
	WRITE_CBUS_REG_BITS(PWM_PWM_D, (PWM_MAX - level), 0, 16);  //pwm low
    WRITE_CBUS_REG_BITS(PWM_PWM_D, level, 16, 16);  //pwm high
#endif
}

static void lcd_power_ctrl(Bool_t status)
{
	debug("%s: power %s\n", __FUNCTION__, (status ? "ON" : "OFF"));
    if (status) 
	{
		//GPIOA27 -> LCD_PWR_EN#: 0  lcd 3.3v
	    set_gpio_val(GPIOA_bank_bit0_27(27), GPIOA_bit_bit0_27(27), 0);
	    set_gpio_mode(GPIOA_bank_bit0_27(27), GPIOA_bit_bit0_27(27), GPIO_OUTPUT_MODE);
	    mdelay(30);	    
    
#ifdef CONFIG_AW_AXP20
		//AXP_GPIO3 -> VCCx3_EN#: 1
	    axp_gpio_set_io(3,1);
	    axp_gpio_set_value(3, 0);
#else
	    //GPIOD2 -> VCCx3_EN: 1
	    set_gpio_val(GPIOC_bank_bit0_15(2), GPIOC_bit_bit0_15(2), 1);
	    set_gpio_mode(GPIOC_bank_bit0_15(2), GPIOC_bit_bit0_15(2), GPIO_OUTPUT_MODE);
#endif    
    	mdelay(30); 
	}
	else
	{
		backlight_power_ctrl(OFF);
    mdelay(50);	
    
 #ifdef CONFIG_AW_AXP20
	//AXP_GPIO3 -> VCCx3_EN#: 0
    axp_gpio_set_io(3,0);
#else
    //GPIOD2 -> VCCx3_EN: 0
    set_gpio_val(GPIOC_bank_bit0_15(2), GPIOC_bit_bit0_15(2), 0);
    set_gpio_mode(GPIOC_bank_bit0_15(2), GPIOC_bit_bit0_15(2), GPIO_OUTPUT_MODE);
#endif    
    mdelay(30);
    
    //GPIOA27 -> LCD_PWR_EN#: 1  lcd 3.3v
    set_gpio_val(GPIOA_bank_bit0_27(27), GPIOA_bit_bit0_27(27), 1);
    set_gpio_mode(GPIOA_bank_bit0_27(27), GPIOA_bit_bit0_27(27), GPIO_OUTPUT_MODE);
    mdelay(10);
	}   
}

#define H_ACTIVE		800
#define V_ACTIVE		480
#define H_PERIOD		1056
#define V_PERIOD		525
#define VIDEO_ON_PIXEL	48
#define VIDEO_ON_LINE   22

int  video_dac_enable(unsigned char enable_mask)
{
	debug("%s\n", __FUNCTION__);
	CLEAR_CBUS_REG_MASK(VENC_VDAC_SETTING, enable_mask&0x1f);
	return 0;
}

int  video_dac_disable(void)
{
	debug("%s\n", __FUNCTION__);
	SET_CBUS_REG_MASK(VENC_VDAC_SETTING, 0x1f);
    return 0;    
}   

Lcd_Config_t lcd_config = 
{
    .lcd_basic = {
        .h_active = H_ACTIVE,
        .v_active = V_ACTIVE,
        .h_period = H_PERIOD,
        .v_period = V_PERIOD,
    	.screen_ratio_width = 16,
     	.screen_ratio_height = 9,
        .lcd_type = LCD_DIGITAL_TTL,    //LCD_DIGITAL_TTL  //LCD_DIGITAL_LVDS  //LCD_DIGITAL_MINILVDS
        .lcd_bits = 8,  //8  //6
    },
	
	.lcd_timing = {
        .pll_ctrl = 0x10225, //clk=27.8MHz, 50.1Hz
        .div_ctrl = 0x18813,  
        .clk_ctrl = 0x1118, //[19:16]ss_ctrl, [12]pll_sel, [8]div_sel, [4]vclk_sel, [3:0]xd
        //.sync_duration_num = 501,
        //.sync_duration_den = 10,
  
		.video_on_pixel = VIDEO_ON_PIXEL,
		.video_on_line = VIDEO_ON_LINE,
		
		.sth1_hs_addr = 20,
        .sth1_he_addr = 10,
        .sth1_vs_addr = 0,
        .sth1_ve_addr = V_PERIOD - 1,
        .oeh_hs_addr = 67,
        .oeh_he_addr = 67+H_ACTIVE,
        .oeh_vs_addr = VIDEO_ON_LINE,
        .oeh_ve_addr = VIDEO_ON_LINE+V_ACTIVE-1,
        .vcom_hswitch_addr = 0,
        .vcom_vs_addr = 0,
        .vcom_ve_addr = 0,
        .cpv1_hs_addr = 0,
        .cpv1_he_addr = 0,
        .cpv1_vs_addr = 0,
        .cpv1_ve_addr = 0,
        .stv1_hs_addr = 0,
        .stv1_he_addr = H_PERIOD-1,
        .stv1_vs_addr = 5,
        .stv1_ve_addr = 2,
        .oev1_hs_addr = 0,
        .oev1_he_addr = 0,
        .oev1_vs_addr = 0,
        .oev1_ve_addr = 0,
		
		.pol_cntl_addr = (0x0 << LCD_CPH1_POL) |(0x1 << LCD_HS_POL) | (0x1 << LCD_VS_POL),
		.inv_cnt_addr = (0<<LCD_INV_EN) | (0<<LCD_INV_CNT),
		.tcon_misc_sel_addr = (1<<LCD_STV1_SEL) | (1<<LCD_STV2_SEL),
		.dual_port_cntl_addr = (1<<LCD_TTL_SEL) | (1<<LCD_ANALOG_SEL_CPH3) | (1<<LCD_ANALOG_3PHI_CLK_SEL) | (0<<LCD_RGB_SWP) | (0<<LCD_BIT_SWP),
    },
	
	.lcd_effect = {
        .gamma_cntl_port = (1 << LCD_GAMMA_EN) | (0 << LCD_GAMMA_RVS_OUT) | (1 << LCD_GAMMA_VCOM_POL),
        .gamma_vcom_hswitch_addr = 0,
        .rgb_base_addr = 0xf0,
        .rgb_coeff_addr = 0x74a,        
    },
	
    // .power_on=lcd_power_on,
    // .power_off=lcd_power_off,
    // .backlight_on = power_on_backlight,
    // .backlight_off = power_off_backlight,
	// .get_bl_level = get_backlight_level,
    // .set_bl_level = set_backlight_level,
};

static void lcd_setup_gamma_table(Lcd_Config_t *pConf)
{
    int i;
	debug("%s\n", __FUNCTION__);
	const unsigned short gamma_adjust[256] = {
        0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,
        32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,52,53,54,55,56,57,58,59,60,61,62,63,
        64,65,66,67,68,69,70,71,72,73,74,75,76,77,78,79,80,81,82,83,84,85,86,87,88,89,90,91,92,93,94,95,
        96,97,98,99,100,101,102,103,104,105,106,107,108,109,110,111,112,113,114,115,116,117,118,119,120,121,122,123,124,125,126,127,
        128,129,130,131,132,133,134,135,136,137,138,139,140,141,142,143,144,145,146,147,148,149,150,151,152,153,154,155,156,157,158,159,
        160,161,162,163,164,165,166,167,168,169,170,171,172,173,174,175,176,177,178,179,180,181,182,183,184,185,186,187,188,189,190,191,
        192,193,194,195,196,197,198,199,200,201,202,203,204,205,206,207,208,209,210,211,212,213,214,215,216,217,218,219,220,221,222,223,
        224,225,226,227,228,229,230,231,232,233,234,235,236,237,238,239,240,241,242,243,244,245,246,247,248,249,250,251,252,253,254,255
    };

    for (i=0; i<256; i++) {
        pConf->lcd_effect.GammaTableR[i] = gamma_adjust[i] << 2;
        pConf->lcd_effect.GammaTableG[i] = gamma_adjust[i] << 2;
        pConf->lcd_effect.GammaTableB[i] = gamma_adjust[i] << 2;
    }
}

static void lcd_video_adjust(Lcd_Config_t *pConf)
{
	int i;
	
	const signed short video_adjust[33] = { -999, -937, -875, -812, -750, -687, -625, -562, -500, -437, -375, -312, -250, -187, -125, -62, 0, 62, 125, 187, 250, 312, 375, 437, 500, 562, 625, 687, 750, 812, 875, 937, 1000};
	
	for (i=0; i<33; i++)
	{
		pConf->lcd_effect.brightness[i] = video_adjust[i];
		pConf->lcd_effect.contrast[i]   = video_adjust[i];
		pConf->lcd_effect.saturation[i] = video_adjust[i];
		pConf->lcd_effect.hue[i]        = video_adjust[i];
	}
}

static void lcd_sync_duration(Lcd_Config_t *pConf)
{
	unsigned m, n, od, div, xd;
	unsigned pre_div;
	unsigned sync_duration;
	
	m = ((pConf->lcd_timing.pll_ctrl) >> 0) & 0x1ff;
	n = ((pConf->lcd_timing.pll_ctrl) >> 9) & 0x1f;
	od = ((pConf->lcd_timing.pll_ctrl) >> 16) & 0x3;
	div = ((pConf->lcd_timing.div_ctrl) >> 4) & 0x7;
	xd = ((pConf->lcd_timing.clk_ctrl) >> 0) & 0xf;
	
	od = (od == 0) ? 1:((od == 1) ? 2:4);
	switch(pConf->lcd_basic.lcd_type)
	{
		case LCD_DIGITAL_TTL:
			pre_div = 1;
			break;
		case LCD_DIGITAL_LVDS:
			pre_div = 7;
			break;
		default:
			pre_div = 1;
			break;
	}
	
	sync_duration = m*24*100/(n*od*(div+1)*xd*pre_div);	
	sync_duration = ((sync_duration * 100000 / H_PERIOD) * 10) / V_PERIOD;
	sync_duration = (sync_duration + 5) / 10;	
	
	pConf->lcd_timing.sync_duration_num = sync_duration;
	pConf->lcd_timing.sync_duration_den = 10;
}

static void power_on_backlight(void)
{
	//debug("%s\n", __FUNCTION__);
	backlight_power_ctrl(ON);
}

static void power_off_backlight(void)
{
	//debug("%s\n", __FUNCTION__);
	backlight_power_ctrl(OFF);
}

static void lcd_power_on(void)
{
	debug("%s\n", __FUNCTION__);
	video_dac_disable();    
    //power_on_lcd(); 
	lcd_power_ctrl(ON);       
}
static void lcd_power_off(void)
{
	debug("%s\n", __FUNCTION__);
	//power_off_backlight();
	backlight_power_ctrl(OFF);
    //power_off_lcd();
	lcd_power_ctrl(OFF);
}

static void lcd_io_init(void)
{
    debug("%s\n", __FUNCTION__);    
    //power_on_lcd();
	lcd_power_ctrl(ON);     
    //set_backlight_level(DEFAULT_BL_LEVEL);
}

static int lcd_enable(void)
{
	debug("%s\n", __FUNCTION__);

	panel_info.vd_base = simple_strtoul(getenv("fb_addr"), NULL, NULL);
	panel_info.vl_col = simple_strtoul(getenv("display_width"), NULL, NULL);
	panel_info.vl_row = simple_strtoul(getenv("display_height"), NULL, NULL);
	panel_info.vl_bpix = simple_strtoul(getenv("display_bpp"), NULL, NULL);
	panel_info.vd_color_fg = simple_strtoul(getenv("display_color_fg"), NULL, NULL);
	panel_info.vd_color_bg = simple_strtoul(getenv("display_color_bg"), NULL, NULL);
	
	lcd_sync_duration(&lcd_config);
	lcd_setup_gamma_table(&lcd_config);
	lcd_video_adjust(&lcd_config);
    lcd_io_init();
    lcd_probe();

    return 0;
}

void lcd_disable(void)
{
	debug("%s\n", __FUNCTION__);
	//power_off_backlight();
	backlight_power_ctrl(OFF);
    //power_off_lcd();
	lcd_power_ctrl(OFF);
    lcd_remove();
}

vidinfo_t panel_info = 
{
	.vl_col	=	0,		/* Number of columns (i.e. 160) */
	.vl_row	=	0,		/* Number of rows (i.e. 100) */

	.vl_bpix	=	0,				/* Bits per pixel */

	.vd_console_address	=	NULL,	/* Start of console buffer	*/
	.console_col	=	0,
	.console_row	=	0,
	
	.vd_color_fg	=	0,
	.vd_color_bg	=	0,
	.max_bl_level	=	255,

	.cmap	=	NULL,		/* Pointer to the colormap */

	.priv		=	NULL,			/* Pointer to driver-specific data */
};

struct panel_operations panel_oper =
{
	.enable	=	lcd_enable,
	.disable	=	lcd_disable,
	.bl_on	=	power_on_backlight,
	.bl_off	=	power_off_backlight,
	.set_bl_level	=	set_backlight_level,
	.get_bl_level = get_backlight_level,	
	.power_on=lcd_power_on,
    .power_off=lcd_power_off,
};
