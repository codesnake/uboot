#include "../v2_burning_i.h"
#include "usb_pcd.h"
#include "platform.h"

#define MYDBG(fmt ...) printf("OPT]"fmt)

#ifdef CONFIG_CMD_AML   
#define USB_BURN_POWER_CONTROL  1
#endif// #ifdef CONFIG_CMD_AML   

#ifndef CONFIG_UNIFY_KEY_MANAGE
int v2_key_command(const int argc, char * const argv[], char *info)
{

    DWN_ERR("burn key not supported as CONFIG_UNIFY_KEY_MANAGE undef!!\n");
    return OPT_DOWN_FAIL;
}
#endif//#ifndef CONFIG_UNIFY_KEY_MANAGE

static inline int str2long(const char *p, unsigned *num)
{
	char *endptr;
	*num = simple_strtoul(p, &endptr, 0);
	return (*p != '\0' && *endptr == '\0') ? 1 : 0;
}

static inline int str2longlong(char *p, unsigned long long *num)
{
	char *endptr;
    
	*num = simple_strtoull(p, &endptr, 16);
	if(*endptr!='\0')
	{
	    switch(*endptr)
	    {
	        case 'g':
	        case 'G':
	            *num<<=10;
	        case 'm':
	        case 'M':
	            *num<<=10;
	        case 'k':
	        case 'K':
	            *num<<=10;
	            endptr++;
	            break;
	    }
	}
	
	return (*p != '\0' && *endptr == '\0') ? 1 : 0;
}

int opimus_func_write_bootloader(u32 addr)
{
	int ret = 0;
    loff_t size = 0;

    size = 0x60000;//FIXME: 256K at most ??

    ret = store_boot_write((u8*)addr, (loff_t)0, size);

	return ret;
}

//[0]write_raw_img [1]part_name [2]address, [3]offset, [4]size
int opimus_func_write_raw_img(int argc, char *argv[], char *info)
{
	int ret = 0;
	u32 addr;
	u64 off, size;
    const char* partName = argv[1];

	if(strcmp(partName, "bootloader") == 0)
	{
        addr = simple_strtoul(argv[2], NULL, 0);
		return opimus_func_write_bootloader(addr);
	}

    addr = simple_strtoul(argv[2], NULL, 0);
    off  = simple_strtoull(argv[3], NULL, 0);
    size = simple_strtoul(argv[4], NULL, 0);

    printf("write_raw_img part %s offset 0x%x, size 0x%x, addr 0x%x\n", argv[1], (u32)off, (u32)size, addr);
    ret = store_write_ops((u8*)partName, (u8*)addr, off, size);
	
	return ret;
}


//read_raw_img partName memAddr partOffset readSzInBytes
int opimus_func_read_raw_img(int argc, char *argv[], char *info)
{
	int ret = 0;
	u32 addr;
	u64 off, size;
    unsigned char* partName = (unsigned char*)argv[1];

    MYDBG("%s, %s, %s, %s\n", __func__, argv[0], argv[1], argv[2]);

	if (!(str2long(argv[2], &addr)))
	{
		sprintf(info, "failed:'%s' is not a number\n", argv[2]);
		return -1;
	}
	if (!(str2longlong(argv[3], &off)))
	{
		sprintf(info, "failed:'%s' is not a number\n", argv[3]);
		return -1;
	}
	if (!(str2longlong(argv[4], &size)))
	{
		sprintf(info, "failed:'%s' is not a number\n", argv[4]);
		return -1;
	}

    ret = store_read_ops(partName, (u8*)addr, off, size);
	
	return ret;
}

int optimus_simg2part (int argc, char * const argv[], char *info)
{
	int ret = -1;
	const char* partition_name = argv[1];
	u8* simg_addr = (u8*)simple_strtoul(argv[2], NULL, 16);
    u32 pktSz     = simple_strtoul(argv[3], NULL, 0);
    const unsigned memAddrTop = OPTIMUS_DOWNLOAD_SPARSE_INFO_FOR_VERIFY;//this address backup the chunk info, don't overwrite it!

	if (argc < 4) {
        sprintf(info, "failed: used [%s partName memAddr, pktSz]\n", argv[0]);
        DWN_ERR(info);
        return __LINE__;
	}
    if(!pktSz || !simg_addr){
        sprintf(info, "simg_addr(%s) or pktSz(%s)error\n", argv[2], argv[3]);
        DWN_ERR(info);
        return __LINE__;
    }

    if((unsigned)simg_addr + pktSz > memAddrTop) {
        sprintf(info, "failed:simg_addr(0x%p) + pktSz(0x%x) > memAddrTop(0x%x)\n", simg_addr, pktSz, memAddrTop);
        DWN_ERR(info);
        return __LINE__;
    }
	
#if  0
	ret = simg_write_to_partition(partition_name, simg_addr);
#else      /* -----  not 0  ----- */
#if  0
    ret = optimus_simg_probe(simg_addr, pktSz);
    if(!ret){
        sprintf(info, "failed:format error, not a sparse image at addr %s\n", argv[2]);
        DWN_ERR(info);
        return __LINE__;
    }
#endif     /* -----  0  ----- */
    ret = optimus_parse_img_download_info(partition_name, pktSz, "sparse", "store", 0);
    if(ret){
        sprintf(info, "failed:init download info for part(%s)\n", partition_name);
        DWN_ERR(info);
        return __LINE__;
    }
    unsigned writeLen = optimus_download_img_data(simg_addr, pktSz, info);
    if(writeLen != pktSz){
        DWN_ERR("failed when burn simg!, want(0x%x), write(0x%x)\n", pktSz, writeLen);
        return __LINE__;
    }
#endif     /* -----  not 0  ----- */

	return ret;
}

int optimus_sha1sum (int argc, char * const argv[], char *info)
{
	unsigned buffermax = 64<<20;
	unsigned sha1_addr = 0x81000000;
	int ret = -1;
	char *partition_name;
	unsigned long long Bits_need_read = 0,partition_offset = 0;
	unsigned long long verify_len = 0;
	u8 output[20];
	char *sha1_verify = NULL;
	char sha1_value[41];
    int i = 1;

	memset(sha1_value, 0, sizeof(sha1_value));

    if(3 == argc)//test to sha1sum memory: sha1sum memAddr, length
    {
        unsigned verify_len = 0;
        unsigned char* pBuf = NULL;

        pBuf = (unsigned char*)simple_strtoul(argv[1], NULL, 0);
        verify_len = simple_strtoul(argv[2], NULL, 0);

        printf("Gen sha1sum: addr 0x%p, len 0x%x\n", pBuf, verify_len);
        sha1_csum(pBuf, verify_len, output);

        sprintf(sha1_value, "%02x", output[0]);
        for(i = 1; i < 20; ++i)
        {
            sprintf(sha1_value, "%02x", output[0]);
            for(; i < 20; ++i)
            {
                sprintf(sha1_value, "%s%02x", sha1_value, output[i]);
            }
            /*sprintf(sha1_value, "%s%02x", sha1_value, output[i]);*/
        }
        printf("gen sha1sum %s\n", sha1_value);
        return 0;
    }

	if (argc != 4)//argv:cmd,partition_name,verify_len,sha1_verify
	{
		printf("bad args---sha1sum cmd need 3 args\n");
		strcpy(info, "failed:need 3 args");
		return -1;
	}
	
	partition_name = argv[1];
	
	verify_len = simple_strtoull (argv[2], NULL, 10);
	sha1_verify = argv[3];

//circularly verify 128M datas readed in memory 0x81000000
	sha1_context ctx;
	sha1_starts (&ctx);
	
	Bits_need_read = verify_len;
	partition_offset = 0;
	while(Bits_need_read > buffermax)
    {
		ret = store_read_ops((unsigned char*)partition_name, (unsigned char *) sha1_addr, partition_offset, buffermax);
        if(ret){
            DWN_ERR("Fail to read data from %s at offset %llx size %x\n", partition_name, partition_offset, buffermax);
            return __LINE__;
        }
		sha1_update (&ctx, (unsigned char *) sha1_addr, buffermax);
		Bits_need_read = Bits_need_read - buffermax;
		partition_offset += buffermax;
		printf("current Bits_need_read is 0x%llx, buffermax is : 0x%x\n",Bits_need_read,buffermax);
		for(i = 0; i < 5; i++)sprintf(sha1_value,"%s%08x", sha1_value, (unsigned)ctx.state[i]);//5*8=40
		printf("current sha1_value is  %s\n", sha1_value);
	}
	
	ret = store_read_ops((unsigned char*)partition_name, (unsigned char *) sha1_addr, partition_offset, Bits_need_read);
    if(ret){
        DWN_ERR("Fail to read data from %s at offset %llx size %x\n", partition_name, partition_offset, buffermax);
        return __LINE__;
    }

    sha1_update (&ctx, (unsigned char *) sha1_addr, Bits_need_read);
	sha1_finish (&ctx, output);
//?????????????
	printf("SHA1 for %s %08x,verify_len:0x%llx ==>",partition_name, sha1_addr,verify_len);

	for (i = 0; i < 20; i++)
		{
		printf("%02x", output[i]);
		//sha1_value[i] = output[i];
		sprintf(sha1_value,"%s%02x", sha1_value, output[i]);
		}
	//sha1_value[21] = '\0';
	printf("\n");
	
	printf("sha1_verify= %s\n", sha1_verify);
	printf("sha1_value = %s\n", sha1_value);
	if(strncmp(sha1_verify, sha1_value, 40) == 0)
	{	
		ret = 0;
		strcpy(info, "success");
	}
	else
	{	
		ret = -1;
		strcpy(info, "failed");
	}

	return ret;
}


int optimus_mem_md (int argc, char * const argv[], char *info)
{
	ulong	addr, length = 0x100;
	int	size;
	int rc = 0;

	if ((size = cmd_get_data_size(argv[0], 4)) < 0)
		return 1;

	/* Address is specified since argc > 1
	*/
	addr = simple_strtoul(argv[1], NULL, 16);

	/* If another parameter, it is the length to display.
	 * Length is the number of objects, not number of bytes.
	 */
	if (argc > 2)
		length = simple_strtoul(argv[2], NULL, 16);

	/* Print the lines. */
	print_buffer(addr, (void*)addr, size, length, 16/size);
	strcpy(info, "success");
	return rc;
}

int set_low_power_for_usb_burn(int arg, char* buff)
{
    int ret = 0;

    if(OPTIMUS_WORK_MODE_USB_PRODUCE == optimus_work_mode_get()){
        return 0;//just return ok as usb producing mode as LCD not initialized yet!
    }

#if defined(CONFIG_VIDEO_AMLLCD)
    //axp to low power off LCD, no-charging
    MYDBG("To close LCD\n");
    ret = run_command("video dev disable", 0);
    if(ret){
        if(buff) sprintf(buff, "Fail to close back light");
        printf("Fail to close back light\n");
        /*return __LINE__;*/
    }
#endif// #if defined(CONFIG_VIDEO_AMLLCD)

#if USB_BURN_POWER_CONTROL
    //limit vbus curretn to 500mA, i.e, if hub is 4A, 8 devices at most, arg3 to not set_env as it's not inited yet!!
    MYDBG("set_usbcur_limit 500 0\n");
    ret = run_command("set_usbcur_limit 500 0", 0);
    if(ret){
        if(buff) sprintf(buff, "Fail to set_usb_cur_limit");
        printf("Fail to set_usb_cur_limit\n");
        return __LINE__;
    }
#endif//#if USB_BURN_POWER_CONTROL

    return 0;
}

//I assume that store_inited yet when "bootloader_is_old"!!!!
int optimus_erase_bootloader(char* info)
{
    int ret = 0;

#if ROM_BOOT_SKIP_BOOT_ENABLED
    optimus_enable_romboot_skip_boot();
#else
    if(optimus_storage_init(0)){
        DWN_ERR("Fail in storage_init\n");
        return __LINE__;
    }
    ret = store_erase_ops((u8*)"boot", 0, 0, 0);
#endif// #if ROM_BOOT_SKIP_BOOT_ENABLED

    return ret;
}

int cb_4_dis_connect_intr(void)
{
    if(optimus_burn_complete(OPTIMUS_BURN_COMPLETE__QUERY))
    {
        DWN_MSG("User Want poweroff after disconnect\n");
        optimus_poweroff();
    }

    return 0;
}

int optimus_working (const char *cmd, char* buff)
{
    static char cmdBuf[CMD_BUFF_SIZE] = {0};
	int ret = 0;
	int argc = 33;
	char *argv[CONFIG_SYS_MAXARGS + 1];	/* NULL terminated	*/
	/*printf("reboot_mode [%8x, %8x]\n", P_AO_RTI_STATUS_REG1);*/
    const char* optCmd = NULL;

    memset(buff, 0, CMD_BUFF_SIZE);
    memcpy(cmdBuf, cmd, CMD_BUFF_SIZE);
	if ((argc = parse_line (cmdBuf, argv)) == 0)
	{
		strcpy(buff, "failed:no command at all");
		printf("no command at all\n");
		return -1;	/* no command at all */
	}
    optCmd = argv[0];
	
    if(!strcmp("low_power", optCmd))
    {
        ret = set_low_power_for_usb_burn(1, buff);
    }
    else if(strcmp(optCmd, "disk_initial") == 0)
	{
        unsigned  erase = simple_strtoul(argv[1], NULL, 0);

        ret = optimus_storage_init(erase);
	}
    else if(!strcmp(optCmd, "bootloader_is_old"))
    {
        ret = is_tpl_loaded_from_usb();
		if(ret)sprintf(buff, "Failed, bootloader is new\n");
    }
    else if(!strcmp(optCmd, "erase_bootloader"))
    {
        ret = optimus_erase_bootloader(buff);

        if(ret)sprintf(buff, "Failed to erase bootloader\n");
    }
	else if(strcmp(optCmd, "write_raw_img") == 0)
	{
		ret = opimus_func_write_raw_img(argc, argv, buff);
	}
	else if(strcmp(optCmd, "read_raw_img") == 0)
	{
		ret = opimus_func_read_raw_img(argc, argv, buff);
	}
	else if(strcmp(optCmd, "simg2part") == 0)
	{
		ret = optimus_simg2part(argc, argv, buff);
	}
	else if(strcmp(optCmd, "reset") == 0)
	{
        close_usb_phy_clock(0);
		optimus_reset(OPTIMUS_BURN_COMPLETE__REBOOT_NORMAL);
	}
	else if(strcmp(optCmd, "poweroff") == 0)
	{
		optimus_poweroff();
	}
	else if(strncmp(optCmd, "md", 2) == 0)
	{
		ret = optimus_mem_md(argc, argv, buff);
	}
	else if(!strcmp(optCmd, "download") || !strcmp("upload", optCmd))
    {
        ret = optimus_parse_download_cmd(argc, argv);
    }
    else if(!strcmp("key", optCmd))
    {
        ret = v2_key_command(argc, argv, buff);
    }
    else if(!strcmp("verify", optCmd))
    {
        ret = optimus_media_download_verify(argc, argv, buff);
    }
    else if(!strcmp("save_setting", optCmd))
    {
        ret = optimus_set_burn_complete_flag();
    }
    else if(!strcmp("burn_complete", optCmd))
    {
        unsigned choice = simple_strtoul(argv[1], NULL, 0);//0 is poweroff, 1 is reset system

        if(OPTIMUS_BURN_COMPLETE__POWEROFF_AFTER_DISCONNECT != choice) {//disconnect except OPTIMUS_BURN_COMPLETE__POWEROFF_AFTER_DISCONNECT
            close_usb_phy_clock(0);//some platform can't poweroff but dis-connect needed by pc
        }
        ret = optimus_burn_complete(choice);
    }
	else if(strncmp(cmd,"sha1sum",(sizeof("sha1sum")-1)) == 0)
	{
		ret = optimus_sha1sum(argc, argv, buff);		
	}
	else
	{
        int flag = 0;
		ret = run_command(cmd, flag);
        DWN_MSG("ret = %d\n", ret);
        ret = ret < 0 ? ret : 0;
	}

    if(ret)
    {
        memcpy(buff, "failed:", strlen("failed:"));//use memcpy but not strcpy to not overwrite storage/key info
    }
    else
    {
        memcpy(buff, "success", strlen("success"));//use memcpy but not strcpy to not overwrite storage/key info
    }

	printf("info[%s]\n",buff);
	return ret;
}

