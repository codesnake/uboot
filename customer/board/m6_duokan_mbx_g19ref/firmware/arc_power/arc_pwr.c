#include <config.h>
#include <asm/arch/io.h>
#include <asm/arch/uart.h>
#include <asm/arch/reg_addr.h>
#include <asm/arch/pctl.h>
#include <asm/arch/dmc.h>
#include <asm/arch/ddr.h>
#include <asm/arch/memtest.h>
#include <asm/arch/pctl.h>
//#include <asm/arch/register.h>
#include "boot_code.dat"
#include <asm/arch/cec_tx_reg.h>


#define CONFIG_IR_REMOTE_WAKEUP 1//for M6 MBox
#define CONFIG_CEC_WAKEUP       0//for CEC function
#define CONFIG_RTC_WAKEUP       1//for RTC function

#ifdef CONFIG_IR_REMOTE_WAKEUP
#include "irremote2arc.c"
#endif

//----------------------------------------------------
unsigned UART_CONFIG_24M= (200000000/(115200*4)  );
unsigned UART_CONFIG= (32*1000/(300*4));
//#define EN_DEBUG
//----------------------------------------------------
//functions declare
void store_restore_plls(int flag);
#define clkoff_a9()

#define ISOLATE_RESET_N_TO_EE       clrbits_le32(P_AO_RTI_PWR_CNTL_REG0 ,(1 << 5))
#define ISOLATE_TEST_MODE_FROM_EE   clrbits_le32(P_AO_RTI_PWR_CNTL_REG0 ,(1 << 3))
#define ISOLATE_IRQ_FROM_EE         clrbits_le32(P_AO_RTI_PWR_CNTL_REG0 ,(1 << 2))
#define ISOLATE_RESET_N_FROM_EE     clrbits_le32(P_AO_RTI_PWR_CNTL_REG0 ,(1 << 1))
#define ISOLATE_AHB_BUS_FROM_EE     clrbits_le32(P_AO_RTI_PWR_CNTL_REG0 ,(1 << 0))

#define ENABLE_RESET_N_TO_EE        setbits_le32(P_AO_RTI_PWR_CNTL_REG0 ,  (1 << 5))
#define ENABLE_TEST_MODE_FROM_EE    setbits_le32(P_AO_RTI_PWR_CNTL_REG0 ,  (1 << 3))
#define ENABLE_IRQ_FROM_EE          setbits_le32(P_AO_RTI_PWR_CNTL_REG0 ,  (1 << 2))
#define ENABLE_RESET_N_FROM_EE      setbits_le32(P_AO_RTI_PWR_CNTL_REG0 ,  (1 << 1))
#define ENABLE_AHB_BUS_FROM_EE      setbits_le32(P_AO_RTI_PWR_CNTL_REG0 ,  (1 << 0))

#define TICK_OF_ONE_SECOND 32000

#define dbg_out(s,v) f_serial_puts(s);serial_put_hex(v,32);f_serial_puts('\n');wait_uart_empty();

void store_vid_pll(void);

static void timer_init()
{
	//100uS stick timer a mode : periodic, timer a enable, timer e enable
    setbits_le32(P_AO_TIMER_REG,0x1f);
}

unsigned  get_tick(unsigned base)
{
    return readl(P_AO_TIMERE_REG)-base;
}

unsigned t_delay_tick(unsigned count)
{
    unsigned base=get_tick(0);
    if(readl(P_AO_RTI_PWR_CNTL_REG0)&(1<<8)){
        while(get_tick(base)<count);
    }else{
        while(get_tick(base)<count*100);
    }
    return 0;
}

unsigned delay_tick(unsigned count)
{
    unsigned i,j;
    for(i=0;i<count;i++)
    {
        for(j=0;j<1000;j++)
        {
            asm("mov r0,r0");
            asm("mov r0,r0");
        }
    }
}

void delay_ms(int ms)
{
		while(ms > 0){
		delay_tick(32);
		ms--;
	}
}

#define delay_1s() delay_tick(TICK_OF_ONE_SECOND);

//volatile unsigned * arm_base=(volatile unsigned *)0x8000;
void copy_reboot_code()
{
	int i;
	int code_size;
	volatile unsigned char* pcode = (volatile unsigned char*)arm_reboot;
  volatile unsigned char * arm_base = (volatile unsigned char *)0x0000;

	code_size = sizeof(arm_reboot);
	//copy new code for ARM restart
	for(i = 0; i < code_size; i++)
	{
/*	 	f_serial_puts("[ ");
		serial_put_hex(*arm_base,8);
	 	f_serial_puts(" , ");
		serial_put_hex(*pcode,8);
	 	f_serial_puts(" ]  ");
*/	 	
		
		if(i != 32 && i != 33 && i != 34 && i != 35) //skip firmware's reboot entry address.
				*arm_base = *pcode;
		pcode++;
		arm_base++;
	}
}


/*
#define POWER_OFF_AVDD25
#define POWER_OFF_AVDD33
#define POWER_OFF_VCCK12
#define POWER_OFF_VDDIO
#define DCDC_SWITCH_PWM
*/
//#define POWER_DOWN_DDR15

//#define POWER_OFF_WIFI_VCC
//#define POWER_OFF_3GVCC

//for mbox
#define POWER_OFF_VCCK_VDDIO
//#define POWER_OFF_VCC5V


/***********************
**Power control domain**
************************/
#ifdef POWER_OFF_VCCK_VDDIO
static void power_off_vcck_vddio(void)
{
    clrbits_le32(P_AO_GPIO_O_EN_N, 1<<21);//GPIO_AO 5 L VDDIO
    clrbits_le32(P_AO_GPIO_O_EN_N, 1<<5); //GPIO_AO 5 output    
    clrbits_le32(P_AO_GPIO_O_EN_N, 1<<31);//TEST_N L VCCK
    udelay(100);	
}
static void power_on_vcck_vddio(void)
{
    setbits_le32(P_AO_GPIO_O_EN_N,1<<31);//TEST_N H VCCK
    setbits_le32(P_AO_GPIO_O_EN_N, 1<<21);//GPIO_AO 5 L VDDIO
    clrbits_le32(P_AO_GPIO_O_EN_N, 1<<5); //GPIO_AO 5 output
    udelay(100);
}
#endif

#ifdef POWER_OFF_VCC5V
static void power_off_vcc5v(void)
{
	udelay(100);
}
static void power_on_vcc5v(void)
{
	udelay(100);
}
#endif

/***********************/

static void enable_iso_ee()
{
	writel(readl(P_AO_RTI_PWR_CNTL_REG0)&(~(1<<4)),P_AO_RTI_PWR_CNTL_REG0);
}
static void disable_iso_ee()
{
	writel(readl(P_AO_RTI_PWR_CNTL_REG0)|(1<<4),P_AO_RTI_PWR_CNTL_REG0);
}

static void cpu_off()
{
	writel(readl(P_HHI_SYS_CPU_CLK_CNTL)|(1<<19),P_HHI_SYS_CPU_CLK_CNTL);
}
static void switch_to_rtc()
{
	 writel(readl(P_AO_RTI_PWR_CNTL_REG0)|(1<<8),P_AO_RTI_PWR_CNTL_REG0);
   udelay(100);
}
static void switch_to_81()
{
	 writel(readl(P_AO_RTI_PWR_CNTL_REG0)&(~(1<<8)),P_AO_RTI_PWR_CNTL_REG0);
   udelay(100);
}
static void enable_iso_ao()
{
	 writel(readl(P_AO_RTI_PWR_CNTL_REG0)&(~(0xF<<0)),P_AO_RTI_PWR_CNTL_REG0);
}
static void disable_iso_ao()
{
	 writel(readl(P_AO_RTI_PWR_CNTL_REG0)|(0xD<<0),P_AO_RTI_PWR_CNTL_REG0);
}
static void ee_off()
{
	 writel(readl(P_AO_RTI_PWR_CNTL_REG0)&(~(0x1<<9)),P_AO_RTI_PWR_CNTL_REG0);
}
static void ee_on()
{
	 writel(readl(P_AO_RTI_PWR_CNTL_REG0)|(0x1<<9),P_AO_RTI_PWR_CNTL_REG0);
}
#define P_A9_CFG2 CBUS_REG_ADDR(A9_CFG2) 
void restart_arm()
{
	//------------------------------------------------------------------------
        // Set up conditions on the ports of the A9 BEFORE releasing the
        // isolation clamps (bit[4] of AO_RTI_PWR_CNTL_REG0)
        // Start the A9 using the Crystal as the master clock
        writel(readl(P_HHI_SYS_CPU_CLK_CNTL) & ~(1<<7),P_HHI_SYS_CPU_CLK_CNTL);
	// restart arm
		//0. make sure a9 reset
	setbits_le32(P_HHI_SYS_CPU_CLK_CNTL,1<<19);

	// Set the soft resets of the APB, AXI and A9 Debug module
	// bits[18] AT soft reset
	// bits[17] APB soft reset
	// bits[16] AXI soft reset
	setbits_le32( P_A9_CFG2,7<<16);
	//1. write flag, Move it to before
//	writel(0x1234abcd,P_AO_RTI_STATUS_REG2);
	
	//2. remap AHB SRAM
//	writel(3,P_AO_REMAP_REG0);
	writel(1,P_AHB_ARBDEC_REG);
 
	//3. turn off romboot clock
	writel(readl(P_HHI_GCLK_MPEG1)&0x7fffffff,P_HHI_GCLK_MPEG1);
 
	//4. Release ISO for A9 domain.
	setbits_le32(P_AO_RTI_PWR_CNTL_REG0,1<<4);

	// Delay a little to let things stabilize
	udelay(100);
	// Reset the Clock dividers associated with the A9. 1 = reset
	// [15] general divider soft reset
	// [14] Soft reset for the AXI, APB, AT, and peripheral clock dividers
	setbits_le32( P_HHI_SYS_CPU_CLK_CNTL, (0x3 << 14) );
	udelay(10);
	clrbits_le32( P_HHI_SYS_CPU_CLK_CNTL, (0x3 << 14) );
	udelay(100);
	// Asynchronously reset the A9, APB, AT and AXI modules
	writel(0xF, P_RESET4_REGISTER);
	// Reset the Mali
	writel(1<<14,P_RESET2_REGISTER);
	// Release all soft resets except the A9
	clrbits_le32(P_A9_CFG2,7<<16);
	udelay(100);
	// Release the A9 to start the booting process
	clrbits_le32(P_HHI_SYS_CPU_CLK_CNTL,1<<19);
}
#define v_outs(s,v) {f_serial_puts(s);serial_put_hex(v,32);f_serial_puts("\n"); wait_uart_empty();}

#define pwr_ddr_off 
void enter_power_down()
{
	unsigned v;
	int i;
	unsigned addr;
	unsigned gate;
	unsigned power_key;
    //unsigned char cec_repeat = 0;
    //unsigned char power_key_num = 0x0;
    //unsigned long cec_key = 0;
    //unsigned long cec_status;
    unsigned long test_status_0;
    unsigned long test_status_1;
    //unsigned long test_reg_0;
    //unsigned long test_reg_1;
    //unsigned long poweronflag = 0;
    unsigned long cec_flag = 0;
    hdmi_cec_func_config = readl(P_AO_DEBUG_REG0); 
    f_serial_puts("CEC P_AO_DEBUG_REG0:\n");
    serial_put_hex(hdmi_cec_func_config,32);
    f_serial_puts("\n");        	
//	disp_pctl();
//	test_ddr(0);
	 // First, we disable all memory accesses.
	f_serial_puts("step 1\n");
	f_serial_puts("cec\n");
	//f_serial_puts("readl(P_AO_IR_DEC_REG0):\n");      
    //serial_put_hex(readl(P_AO_IR_DEC_REG0),32);
    //f_serial_puts("\n");
	//f_serial_puts("readl(P_AO_IR_DEC_REG1):\n");      
    //serial_put_hex(readl(P_AO_IR_DEC_REG1),32);
    //f_serial_puts("\n");
    //f_serial_puts("readl(P_AO_IR_DEC_STATUS):\n");      
    //serial_put_hex(readl(P_AO_IR_DEC_STATUS),32);
    //f_serial_puts("\n");
    //f_serial_puts("AO_IR_DEC_LDR_REPEAT:\n");      
    //serial_put_hex(readl(P_AO_IR_DEC_LDR_REPEAT),32);
    //f_serial_puts("\n");
#ifdef CONFIG_IR_REMOTE_WAKEUP
    //backup the remote config (on arm)
    backup_remote_register();
#endif

   asm(".long 0x003f236f"); //add sync instruction.

#ifdef pwr_ddr_off
   disable_mmc_req();

   serial_put_hex(APB_Rd(MMC_LP_CTRL1),32);
   f_serial_puts("  LP_CTRL1\n");
   wait_uart_empty();

   serial_put_hex(APB_Rd(UPCTL_MCFG_ADDR),32);
   f_serial_puts("  MCFG\n");
   wait_uart_empty();

   store_restore_plls(1);

   APB_Wr(UPCTL_SCTL_ADDR, 1);
   APB_Wr(UPCTL_MCFG_ADDR, 0x60021 );
   APB_Wr(UPCTL_SCTL_ADDR, 2);

   serial_put_hex(APB_Rd(UPCTL_MCFG_ADDR),32);
   f_serial_puts("  MCFG\n");
   wait_uart_empty();
#endif

    f_serial_puts("step 2\n");
    wait_uart_empty();
#ifdef pwr_ddr_off
    // Next, we sleep
    mmc_sleep();

#if 1
  //Clear PGCR CK
  APB_Wr(PUB_PGCR_ADDR,APB_Rd(PUB_PGCR_ADDR)&(~(3<<12)));
  APB_Wr(PUB_PGCR_ADDR,APB_Rd(PUB_PGCR_ADDR)&(~(7<<9)));
  //APB_Wr(PUB_PGCR_ADDR,APB_Rd(PUB_PGCR_ADDR)&(~(3<<9)));
#endif
    mmc_sleep();
    // enable retention
    //only necessory if you want to shut down the EE 1.1V and/or DDR I/O 1.5V power supply.
    //but we need to check if we enable this feature, we can save more power on DDR I/O 1.5V domain or not.
    enable_retention();

    // save ddr power
    // before shut down DDR PLL, keep the DDR PHY DLL in reset mode.
    // that will save the DLL analog power.
#ifndef POWER_DOWN_DDRPHY
	APB_Wr(MMC_SOFT_RST, 0x0);	 // keep all MMC submodules in reset mode
#else
	power_down_ddr_phy();
#endif

  // shut down DDR PLL. 
	writel(readl(P_HHI_DDR_PLL_CNTL)|(1<<30),P_HHI_DDR_PLL_CNTL);


 	f_serial_puts("step 3\n");
 	wait_uart_empty();

 	f_serial_puts("step 4\n");
 	wait_uart_empty();
  // turn off ee
//  enable_iso_ee();
//	writel(readl(P_HHI_MPEG_CLK_CNTL)|(1<<9),P_HHI_MPEG_CLK_CNTL);
#endif
  
 	f_serial_puts("step 5\n");
 	wait_uart_empty();
	cpu_off();
  
  
 	f_serial_puts("step 6\n");
 	wait_uart_empty();

#if CONFIG_RTC_WAKEUP
	//enable power_key int	
	writel(0x100,0xc1109860);//clear int
 	writel(readl(0xc1109868)|1<<8,0xc1109868);
	writel(readl(0xc8100080)|0x1,0xc8100080);
#endif


#ifdef POWER_OFF_3GVCC
	power_off_3gvcc();
#endif

#ifdef POWER_OFF_AVDD33
	power_off_avdd33();
#endif


	//set signal as 0 from EE for prepare power off core power
	writel(readl(P_AO_RTI_PWR_CNTL_REG0)&(~(1<<4)),P_AO_RTI_PWR_CNTL_REG0);

// ee use 32k, So interrup status can be accessed.
	writel(readl(P_HHI_MPEG_CLK_CNTL)|(1<<9),P_HHI_MPEG_CLK_CNTL);
	switch_to_rtc();
	udelay(1000);

#ifdef POWER_OFF_AVDD25
	power_off_avdd25();
#endif

#ifdef POWER_OFF_VDDIO
	power_off_vddio();
#endif

#ifdef POWER_OFF_VCCK_VDDIO
	power_off_vcck_vddio();
#endif

#ifdef POWER_OFF_VCC5V
  power_off_vcc5v();
#endif

#ifdef POWER_DOWN_DDR15
	power_down_ddr15();//1.5v -> 1.3v
#endif

#ifdef DCDC_SWITCH_PWM
	dc_dc_pwm_switch(0);
#endif


#ifdef CONFIG_IR_REMOTE_WAKEUP
    //set the ir_remote to 32k mode at ARC
    init_custom_trigger();
    //test_reg_0 =readl(P_AO_IR_DEC_REG0);
    //test_reg_1 =readl(P_AO_IR_DEC_REG1);
#if CONFIG_CEC_WAKEUP
    if(hdmi_cec_func_config & 0x1){
        cec_power_on();
        remote_cec_hw_reset();  
        cec_node_init();
    }  
#endif
    udelay(10000);
       
    //set the detect gpio
    setbits_le32(P_AO_GPIO_O_EN_N, 3<<2);
    while(1)
    {
        if(((test_status_0 = readl(P_AO_IR_DEC_STATUS))>>3) & 0x1){
        	power_key = readl(P_AO_IR_DEC_FRAME);
        	if(power_key == 0xd5800000){
#if CONFIG_CEC_WAKEUP	
    		    if(hdmi_cec_func_config & 0x1){
    		        cec_imageview_on();
    		    }
#endif
    		    break;
            }
        }
#if CONFIG_CEC_WAKEUP
        if(hdmi_cec_func_config & 0x1){
          cec_handler();	
          if(cec_msg.cec_power == 0x1){  //cec power key
                break;
            }
        }
#endif
#if CONFIG_RTC_WAKEUP
        //detect RTC Alarm
        if(readl(0xc1109860)&0x100){
            power_key=0x100;
            break;
            }
#endif
        //detect WiFi & BT Wake up
        power_key=readl(P_AO_GPIO_I); 
        power_key=power_key&(3<<2);
        if(power_key)
            break;
    }

#else
// gate off REMOTE, UART
  	writel(readl(P_AO_RTI_GEN_CNTL_REG0)&(~(0xF)),P_AO_RTI_GEN_CNTL_REG0);

#if 0
//	udelay(200000);//Drain power
	do{udelay(2000);}while(!(readl(0xc1109860)&0x100));
//	while(!(readl(0xc1109860)&0x100)){break;}
#else
	for(i=0;i<200;i++)
   {
        udelay(1000);
        //udelay(1000);
   }
#endif

// gate on REMOTE, UART
	writel(readl(P_AO_RTI_GEN_CNTL_REG0)|0xF,P_AO_RTI_GEN_CNTL_REG0);

 #endif//CONFIG_IR_REMOTE_WAKEUP
#if CONFIG_RTC_WAKEUP
	//disable power_key int
	writel(readl(0xc1109868)&(~(1<<8)),0xc1109868);
	writel(readl(0xc8100080)&(~0x1),0xc8100080);
	writel(0x100,0xc1109860);//clear int
#endif
//  ee_on();
 
//  disable_iso_ao();
#ifdef DCDC_SWITCH_PWM
	dc_dc_pwm_switch(1);
#endif

#ifdef POWER_DOWN_DDR15
	power_up_ddr15();//1.3v -> 1.5v
#endif

#ifdef POWER_OFF_VCCK_VDDIO
	power_on_vcck_vddio(); 
#endif

#ifdef POWER_OFF_VCC5V
    power_on_vcc5v();
#endif 
#ifdef POWER_OFF_VDDIO
	power_on_vddio();
#endif

#ifdef POWER_OFF_AVDD25
	power_on_avdd25();
#endif
        //  In 32k mode, we had better not print any log.
        store_restore_plls(0);//Before switch back to clk81, we need set PLL
	switch_to_81();
  // ee go back to clk81
	writel(readl(P_HHI_MPEG_CLK_CNTL)&(~(0x1<<9)),P_HHI_MPEG_CLK_CNTL);
	udelay(10000);


#ifdef POWER_OFF_AVDD33
	power_on_avdd33();
#endif

#ifdef POWER_OFF_3GVCC
	power_on_3gvcc();
#endif

//turn on ee
// 	writel(readl(P_HHI_MPEG_CLK_CNTL)&(~(0x1<<9)),P_HHI_MPEG_CLK_CNTL);
// 	writel(readl(P_HHI_GCLK_MPEG1)&(~(0x1<<31)),P_HHI_GCLK_MPEG1);
 	uart_reset();

	
    //f_serial_puts("step 7\n"); 
    //wait_uart_empty();
    //store_restore_plls(0);

#ifdef pwr_ddr_off    
    f_serial_puts("step 8\n");
    wait_uart_empty();  
    init_ddr_pll();

    //store_vid_pll();

    // Next, we reset all channels 
    reset_mmc();
    f_serial_puts("step 9\n");
    wait_uart_empty();

    // disable retention
    // disable retention before init_pctl is because init_pctl you need to data training stuff.
    disable_retention();    
    // initialize mmc and put it to sleep
    init_pctl();
    f_serial_puts("step 10\n");
    wait_uart_empty();
    
    //if(hdmi_cec_func_config & 0x1){
    //    f_serial_puts("CEC P_AO_DEBUG_REG0:\n");
    //    serial_put_hex(readl(P_AO_DEBUG_REG0),32);
    //    f_serial_puts("\n");   
    //    f_serial_puts("CEC P_AO_DEBUG_REG1:\n");
    //    serial_put_hex(readl(P_AO_DEBUG_REG1),32);          
    //    f_serial_puts("\n");       
    //    f_serial_puts("CEC CEC_LOGICAL_ADDR0:\n");      
    //    serial_put_hex(cec_rd_reg(CEC0_BASE_ADDR+CEC_LOGICAL_ADDR0),32);
    //    f_serial_puts("\n");

        f_serial_puts("power_key: ");      
        serial_put_hex(power_key,32);
        f_serial_puts("\n");
        //f_serial_puts("CEC readl(P_AO_DEBUG_REG0):\n");      
        //serial_put_hex(readl(P_AO_DEBUG_REG0),32);
        //f_serial_puts("\n");
        //f_serial_puts("CEC test_reg_0:\n");      
        //serial_put_hex(test_reg_0,32);
        //f_serial_puts("\n");
        //f_serial_puts("CEC test_reg_1:\n");      
        //serial_put_hex(test_reg_1,32);
        //f_serial_puts("\n");
        //f_serial_puts("CEC test_status_0:\n");      
        //serial_put_hex(test_status_0,32);
        //f_serial_puts("\n");
        //f_serial_puts("CEC test_status_1:\n");      
        //serial_put_hex(test_status_1,32);
        //f_serial_puts("\n");
        f_serial_puts("AO_IR_DEC_LDR_REPEAT:\n");      
        serial_put_hex(readl(P_AO_IR_DEC_LDR_REPEAT),32);
        f_serial_puts("\n");
        //f_serial_puts("cec_flag:\n");      
        //serial_put_hex(cec_flag,32);
        //f_serial_puts("\n");
        //f_serial_puts("poweronflag:\n");      
        //serial_put_hex(poweronflag,32);
        //f_serial_puts("\n");
    //}
    //print some useful information to help debug.
    serial_put_hex(APB_Rd(MMC_LP_CTRL1),32);
    f_serial_puts("  MMC_LP_CTRL1\n");
    wait_uart_empty();
    
    serial_put_hex(APB_Rd(UPCTL_MCFG_ADDR),32);
    f_serial_puts("  MCFG\n");
    wait_uart_empty();

#endif   //pwr_ddr_off
  // Moved the enable mmc req and SEC to ARM code.
  //enable_mmc_req();
	
//	disp_pctl();
	
//	test_ddr(1);
//	test_ddr(0);
//	test_ddr(1);
	
//	disp_code();	
        //1. write flag
        switch(power_key){
#if CONFIG_RTC_WAKEUP
            case 0x10 : //RTC
                writel(0x12345678,P_AO_RTI_STATUS_REG2);
                break;
#endif
            case 0x04 : //BT
            case 0x0C : //BT & WiFi
                writel(0x12344331,P_AO_RTI_STATUS_REG2);
                break;
            case 0x08 : //WiFi
                writel(0x12344330,P_AO_RTI_STATUS_REG2);
                break;
            default: //IR Remote
                writel(0x1234abcd,P_AO_RTI_STATUS_REG2);
                break;
            }
	f_serial_puts("restart arm\n");
	wait_uart_empty();
	restart_arm();

#ifdef CONFIG_IR_REMOTE_WAKEUP
	resume_remote_register();
#endif
}


//#define ART_CORE_TEST
#ifdef ART_CORE_TEST
void test_arc_core()
{
    int i;
    int j,k;
    unsigned int power_key=0;
    
    for(i=0;i<1000;i++)
    {
        asm("mov r0,r0");
        asm("mov r0,r0");
        //udelay(1000);
        //udelay(1000);
        
    }
    
    
	f_serial_puts("\n");
	wait_uart_empty();

    writel(0,P_AO_RTI_STATUS_REG1);    

 	// reset A9 clock
	//setbits_le32(P_HHI_SYS_CPU_CLK_CNTL,1<<19);

	// enable iso ee for A9
	//writel(readl(P_AO_RTI_PWR_CNTL_REG0)&(~(1<<4)),P_AO_RTI_PWR_CNTL_REG0);


	// wait key
    power_key = readl(0Xc8100744);
    
    f_serial_puts("get power_key\n");
    #if 0
    while (((power_key&4) != 0)&&((power_key&8) == 0))
   {
     	power_key = readl(0Xc8100744);
   }
   #else
    for(i=0;i<1000;i++)
    {
        for(j=0;j<1000;j++)
        {
            for(k=0;k<100;k++)
            {
                asm("mov r0,r0");
            }
        }
        //udelay(1000);
        //udelay(1000);
        
    }
   #endif

    f_serial_puts("delay 2s\n");

	//0. make sure a9 reset
//	setbits_le32(P_HHI_SYS_CPU_CLK_CNTL,1<<19);

#if 0
	//1. write flag
	if (power_key&8)
		writel(0xabcd1234,P_AO_RTI_STATUS_REG2);
	else
		writel(0x1234abcd,P_AO_RTI_STATUS_REG2);
#endif
	//2. remap AHB SRAM
	writel(3,P_AO_REMAP_REG0);
	writel(2,P_AHB_ARBDEC_REG);
	
	f_serial_puts("remap arm arc\n");

	//3. turn off romboot clock
	writel(readl(P_HHI_GCLK_MPEG1)&0x7fffffff,P_HHI_GCLK_MPEG1);
	
	f_serial_puts("off romboot clock\n");

	//4. Release ISO for A9 domain.
	//setbits_le32(P_AO_RTI_PWR_CNTL_REG0,1<<4);

	//reset A9
	writel(0xF,P_RESET4_REGISTER);// -- reset arm.ww
//	writel(1<<14,P_RESET2_REGISTER);// -- reset arm.mali
	delay_ms(1);
//	clrbits_le32(P_HHI_SYS_CPU_CLK_CNTL,1<<19); // release A9 reset

//	f_serial_puts("arm reboot\n");
//	wait_uart_empty();
    f_serial_puts("arm reboot\n");

}
#endif
#define _UART_DEBUG_COMMUNICATION_
int main(void)
{
	unsigned cmd;
	char c;
	int i = 0,j;
	timer_init();
#ifdef POWER_OFF_VDDIO	
	f_serial_puts("sleep ... off\n");
#else
	f_serial_puts("sleep .......\n");
#endif
		
	while(1){
		
		cmd = readl(P_AO_RTI_STATUS_REG0);
		if(cmd == 0)
		{
			delay_ms(10);
			continue;
		}
		c = (char)cmd;
		if(c == 't')
		{
//#if (defined(POWER_OFF_VDDIO) || defined(POWER_OFF_HDMI_VCC) || defined(POWER_OFF_AVDD33) || defined(POWER_OFF_AVDD25))
//			init_I2C();
//#endif
			copy_reboot_code();
			enter_power_down();
			//test_arc_core();
			break;
		}
		else if(c == 'q')
		{
				f_serial_puts(" - quit command loop\n");
				writel(0,P_AO_RTI_STATUS_REG0);
			  break;
		}
		else
		{
				f_serial_puts(" - cmd no support (ARC)\n");
		}
		//command executed
		writel(0,P_AO_RTI_STATUS_REG0);
	}
	
	while(1){
	    udelay(6000);
	    cmd = readl(P_AO_RTI_STATUS_REG1);
	    c = (char)cmd;
	    if(c == 0)
	    {
	        udelay(6000);
	        cmd = readl(P_AO_RTI_STATUS_REG1);
	        c = (char)cmd;
	        if((c == 0)||(c!='r'))
	        {
	            #ifdef _UART_DEBUG_COMMUNICATION_
	            serial_put_hex(cmd,32);
	            f_serial_puts(" arm boot fail\n\n");
	            wait_uart_empty();
	            #endif
	            #if 0 //power down 
	            cmd = readl(P_AO_GPIO_O_EN_N);
	            cmd &= ~(1<<6);
	            cmd &= ~(1<<22);
	            writel(cmd,P_AO_GPIO_O_EN_N);
	            #endif
	        }
		} else if ( cmd == 1 )
		{
			serial_put_hex(cmd,32);
			f_serial_puts(" ARM has started running\n");
			wait_uart_empty();
		} else if ( cmd == 2 )
		{
			serial_put_hex(cmd,32);
			f_serial_puts(" Reenable SEC\n");
			wait_uart_empty();
	}
	    else if(c=='r')
	    {
	        writel(0,0xc8100030);
	        f_serial_puts("arm boot succ\n");
	        wait_uart_empty();
	        #ifdef _UART_DEBUG_COMMUNICATION_
	        //f_serial_puts("arm boot succ\n");
	        //wait_uart_empty();
	        #endif
	    }
	    else
	    {
	        #ifdef _UART_DEBUG_COMMUNICATION_
	        serial_put_hex(cmd,32);
	        f_serial_puts(" arm unkonw state\n");
	        wait_uart_empty();
	        #endif
	    }
	    //cmd='f';
	    //writel(cmd,P_AO_RTI_STATUS_REG1);
	    
		asm(".long 0x003f236f"); //add sync instruction.
		asm("SLEEP");
	}
	return 0;
}

unsigned pll_settings[4];
unsigned mpll_settings[10];
unsigned viidpll_settings[4];
unsigned vidpll_settings[4];

#define CONFIG_SYS_PLL_SAVE
void store_restore_plls(int flag)
{
    int i;
    if(flag)
    {
#ifdef CONFIG_SYS_PLL_SAVE 
		pll_settings[0]=readl(P_HHI_SYS_PLL_CNTL);
		pll_settings[1]=readl(P_HHI_SYS_PLL_CNTL2);
		pll_settings[2]=readl(P_HHI_SYS_PLL_CNTL3);
		pll_settings[3]=readl(P_HHI_SYS_PLL_CNTL4);

		for(i=0;i<10;i++)//store mpll
		{
			mpll_settings[i]=readl(P_HHI_MPLL_CNTL + 4*i);
		}
/*
		viidpll_settings[0]=readl(P_HHI_VIID_PLL_CNTL);
		viidpll_settings[1]=readl(P_HHI_VIID_PLL_CNTL2);
		viidpll_settings[2]=readl(P_HHI_VIID_PLL_CNTL3);
		viidpll_settings[3]=readl(P_HHI_VIID_PLL_CNTL4);

		vidpll_settings[0]=readl(P_HHI_VID_PLL_CNTL);
		vidpll_settings[1]=readl(P_HHI_VID_PLL_CNTL2);
		vidpll_settings[2]=readl(P_HHI_VID_PLL_CNTL3);
		vidpll_settings[3]=readl(P_HHI_VID_PLL_CNTL4);
*/
#endif //CONFIG_SYS_PLL_SAVE

		save_ddr_settings();
		return;
    }    
    
#ifdef CONFIG_SYS_PLL_SAVE 

	//temp define
#define P_HHI_MPLL_CNTL5         CBUS_REG_ADDR(HHI_MPLL_CNTL5)

	/*
	//to find bandgap is disabled!
	if(!(readl(P_HHI_MPLL_CNTL5) & 1))
	{
		wait_uart_empty();
		f_serial_puts("\nERROR! Stop here! SYS PLL bandgap disabled!\n");
	    wait_uart_empty();
		while(1);
	}
	*/
	
#ifdef CONFIG_AML_PMU
    printf_arc("store_restore_plls, in\n");
#endif
	do{
		//BANDGAP reset for SYS_PLL,VIID_PLL,MPLL lock fail
		//Note: once SYS PLL is up, there is no need to 
		//          use AM_ANALOG_TOP_REG1 for VIID, MPLL
		//          lock fail
		writel(readl(P_HHI_MPLL_CNTL5)&(~1),P_HHI_MPLL_CNTL5); 
    #ifdef CONFIG_AML_PMU
		udelay(3 * 750);
    #else
		udelay(3);
    #endif
		writel(readl(P_HHI_MPLL_CNTL5)|1,P_HHI_MPLL_CNTL5); 
    #ifdef CONFIG_AML_PMU
		udelay(30 * 750);
    #else
		udelay(30); //1ms in 32k for bandgap bootup
    #endif
		
		writel(1<<29,P_HHI_SYS_PLL_CNTL);		
		writel(pll_settings[1],P_HHI_SYS_PLL_CNTL2);
		writel(pll_settings[2],P_HHI_SYS_PLL_CNTL3);
		writel(pll_settings[3],P_HHI_SYS_PLL_CNTL4);
		writel(pll_settings[0] & ~(1<<30|1<<29),P_HHI_SYS_PLL_CNTL);
		//M6_PLL_WAIT_FOR_LOCK(HHI_SYS_PLL_CNTL);

    #ifdef CONFIG_AML_PMU
		udelay(10 * 750);
    #else
		udelay(10); //wait 100us for PLL lock
    #endif
	}while((readl(P_HHI_SYS_PLL_CNTL)&0x80000000)==0);
writel(pll_settings[0],P_HHI_SYS_PLL_CNTL);//restore it
#ifdef CONFIG_AML_PMU
    printf_arc("store_restore_plls, SYS_PLL out\n");
#endif

	do{
		//no need to do bandgap reset
		writel(1<<29,P_HHI_MPLL_CNTL);
		for(i=1;i<10;i++)
			writel(mpll_settings[i],P_HHI_MPLL_CNTL+4*i);

    #ifdef CONFIG_AML_PMU //add
		writel((mpll_settings[0] |1<<30),P_HHI_MPLL_CNTL);
		udelay(24 * 100);
    #endif
		writel((mpll_settings[0] & ~(1<<30))|1<<29,P_HHI_MPLL_CNTL);
		writel(mpll_settings[0] & ~(1<<29),P_HHI_MPLL_CNTL);
    #ifdef CONFIG_AML_PMU
		udelay(10 * 750);
    #else
		udelay(10); //wait 200us for PLL lock		
    #endif
	}while((readl(P_HHI_MPLL_CNTL)&0x80000000)==0);
	writel(mpll_settings[0],P_HHI_MPLL_CNTL);//restore it
#ifdef CONFIG_AML_PMU
    printf_arc("store_restore_plls, HHI_PLL out\n");
#endif
/*
	do{
		//no need to do bandgap reset
		writel(1<<29,P_HHI_VIID_PLL_CNTL);		
		writel(viidpll_settings[1],P_HHI_VIID_PLL_CNTL2);
		writel(viidpll_settings[2],P_HHI_VIID_PLL_CNTL3);
		writel(viidpll_settings[3],P_HHI_VIID_PLL_CNTL4);

		writel(viidpll_settings[0] & ~(3<<29),P_HHI_VIID_PLL_CNTL);

		//writel((viidpll_settings[0] & ~(1<<30))|1<<29,P_HHI_VIID_PLL_CNTL);//ask Knight Shi why fail
		//writel(viidpll_settings[0] & ~(1<<29),P_HHI_VIID_PLL_CNTL);
		udelay(10); //wait 200us for PLL lock		
	}while((readl(P_HHI_VIID_PLL_CNTL)&0x80000000)==0);
	writel(viidpll_settings[0],P_HHI_VIID_PLL_CNTL);//restore it
*/
#ifdef CONFIG_AML_PMU
    udelay(3 * 750);
#else
	udelay(3);
#endif

#endif //CONFIG_SYS_PLL_SAVE

}

void store_vid_pll()
{
    do{
        //no need to do bandgap reset
        writel(1<<29,P_HHI_VID_PLL_CNTL);
        //writel((vidpll_settings[0] & ~(1<<30))|1<<29,P_HHI_VID_PLL_CNTL);
        writel(vidpll_settings[1],P_HHI_VID_PLL_CNTL2);
        writel(vidpll_settings[2],P_HHI_VID_PLL_CNTL3);
        writel(vidpll_settings[3],P_HHI_VID_PLL_CNTL4);

        writel(vidpll_settings[0] & ~(3<<29),P_HHI_VID_PLL_CNTL);
        udelay(24000); //wait 200us for PLL lock
    }while((readl(P_HHI_VID_PLL_CNTL)&0x80000000)==0);
    writel(vidpll_settings[0],P_HHI_VID_PLL_CNTL);//restore it
}

void __raw_writel(unsigned val,unsigned reg)
{
	(*((volatile unsigned int*)(reg)))=(val);
	asm(".long 0x003f236f"); //add sync instruction.
}

unsigned __raw_readl(unsigned reg)
{
	asm(".long 0x003f236f"); //add sync instruction.
	return (*((volatile unsigned int*)(reg)));
}
