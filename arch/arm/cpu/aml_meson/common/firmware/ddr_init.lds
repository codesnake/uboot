#include <config.h>
#include <asm/arch/romboot.h>
OUTPUT_FORMAT("elf32-littlearm", "elf32-littlearm", "elf32-littlearm")
OUTPUT_ARCH(arm)
ENTRY(_start)
SECTIONS
{
	. = AHB_SRAM_BASE;
	
	. = ALIGN(4);
	.start      :
	{
	  *ddr_init_start.o	(.text)
	
	}
#ifndef CONFIG_M6_SECU_BOOT
	. =  AHB_SRAM_BASE+0x1b0;
	.magic :{
	  *magic.o(.text)
	}
	
	. = AHB_SRAM_BASE+0x200;
#endif
	. = ALIGN(4);
	.setting		:
	{
		* (.setting)
	}
		
	
	. = ALIGN(4);
	.text			:
	{
		*  (.text)
	}
	.rodata : { *(SORT_BY_ALIGNMENT(SORT_BY_NAME(.rodata*))) }
	.data : { *(.data) }
	
	_end = .;
	. = ALIGN(4);
	_bssstart = . ;
	.bss : { *(.bss) }
	. = ALIGN(4);
	_bssend = . ;
}
ASSERT(_end <= AHB_SRAM_BASE + READ_SIZE, "ROM BOOT bootstrap too big");
ASSERT(_bssend <= AHB_SRAM_BASE + READ_SIZE, "ROM BOOT bss  too big");
