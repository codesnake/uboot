/*
 * Command d2pll & ddrtest support.
 */

#include <common.h>
#include <asm/io.h>
#include <asm/arch/io.h>
#include <asm/arch/register.h>

int do_ddr2pll(cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
	char *endp;
	unsigned long pll, zqcr;

	/* need at least two arguments */
	if (argc < 2)
		goto usage;

	pll = simple_strtoul(argv[1], &endp, 16);
	if (*argv[1] == 0 || *endp != 0){
        printf ("Error: Wrong format parament!\n");
        return 1;
    }

    zqcr = simple_strtoul(argv[2], &endp, 16);
	if (*argv[2] == 0 || *endp != 0){
        zqcr = 0;
    }

#ifdef CONFIG_M6TV
    WRITE_CBUS_REG(PREG_STICKY_REG0, zqcr | (0x3c << 24));
#else
    WRITE_CBUS_REG(PREG_STICKY_REG0, 0x2012 | (zqcr << 16));
#endif
    WRITE_CBUS_REG(PREG_STICKY_REG1, pll);
    printf("Set pll done [0x%08x]\n", READ_CBUS_REG(PREG_STICKY_REG1));
    WRITE_CBUS_REG(WATCHDOG_TC, 0x400001);

	return 0;

usage:
	cmd_usage(cmdtp);
	return 1;
}

U_BOOT_CMD(
	d2pll,	5,	1,	do_ddr2pll,
	"DDR set PLL function",
	"DDR PLL set: d2pll PLL [ZQCR], e...g... 0x1022c.\n"
);
