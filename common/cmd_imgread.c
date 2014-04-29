/*
 * \file        cmd_imgread.c
 * \brief       command to read the actual size of boot.img/recovery.img and logo.img
 *
 * \version     1.0.0
 * \date        2013/10/29
 * \author      Sam.Wu <yihui.wu@amlgic.com>
 *
 * Copyright (c) 2013 Amlogic Inc.. All Rights Reserved.
 *
 */
#include <common.h>
#include <amlogic/storage_if.h>

#define debugP(fmt...) //printf("L%d:", __LINE__),printf(fmt)
#define errorP(fmt...) printf("Err imgread(L%d):", __LINE__),printf(fmt)
#define wrnP(fmt...)   printf("wrn:"fmt)

#define IMG_PRELOAD_SZ  (1U<<20) //Total read 1M at first to read the image header
#define RES_OLD_FMT_READ_SZ (8U<<20)

#ifdef CONFIG_OF_LIBFDT
#include <libfdt.h>
#define DTB_PRELOAD_SZ  (4U<<10) //Total read 4k at first to read the image header

static int do_image_read_dtb(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
    unsigned    kernel_size;
    unsigned    ramdisk_size;
    boot_img_hdr *hdr_addr = NULL;
    int genFmt = 0;
    unsigned actualBootImgSz = 0;
    unsigned dtbSz = 0;
    const char* const partName = argv[1];
    unsigned char* loadaddr = 0;
    int rc = 0;
    const int PreloadSz = DTB_PRELOAD_SZ;
    unsigned char* dtbLoadAddr = 0;
    int DtbStartOffset = 0;
    unsigned char* DtbDestAddr = (unsigned char*)CONFIG_DTB_LOAD_ADDR;

    debugP("argc %d, argv:%s, %s, %s\n", argc, argv[0], argv[1], argv[2]);
    if(2 < argc){
        loadaddr = (unsigned char*)simple_strtoul(argv[2], NULL, 16);
    }
    else{
        loadaddr = (unsigned char*)simple_strtoul(getenv("loadaddr"), NULL, 16);
    }
    hdr_addr = (boot_img_hdr*)loadaddr;

    if(argc > 3){
        DtbDestAddr = (unsigned char*)simple_strtoul(argv[3], NULL, 16);
    }

    rc = store_read_ops((unsigned char*)partName, loadaddr, 0, PreloadSz);
    if(rc){
        errorP("Fail to read 0x%xB from part[%s] at offset 0\n", PreloadSz, partName);
        return __LINE__;
    }

    genFmt = genimg_get_format(hdr_addr);
    if(IMAGE_FORMAT_ANDROID != genFmt) {
        errorP("Fmt unsupported!genFmt 0x%x != 0x%x\n", genFmt, IMAGE_FORMAT_ANDROID);
        return __LINE__;
    }

    kernel_size     =(hdr_addr->kernel_size + (hdr_addr->page_size-1)+hdr_addr->page_size)&(~(hdr_addr->page_size -1));
    ramdisk_size    =(hdr_addr->ramdisk_size + (hdr_addr->page_size-1))&(~(hdr_addr->page_size -1));
    dtbSz           = hdr_addr->second_size;
    DtbStartOffset = kernel_size + ramdisk_size;
    actualBootImgSz = DtbStartOffset + dtbSz;
    debugP("kernel_size 0x%x, page_size 0x%x, totalSz 0x%x\n", hdr_addr->kernel_size, hdr_addr->page_size, kernel_size);
    debugP("ramdisk_size 0x%x, totalSz 0x%x\n", hdr_addr->ramdisk_size, ramdisk_size);
    debugP("dtbSz 0x%x\n", dtbSz);
    dtbLoadAddr =   loadaddr + DtbStartOffset;

    if(actualBootImgSz > PreloadSz)
    {
        dtbLoadAddr = loadaddr + PreloadSz;

        rc = store_read_ops((unsigned char*)partName, dtbLoadAddr, DtbStartOffset, dtbSz);
        if(rc){
            errorP("Fail to read 0x%xB from part[%s] at offset 0x%x\n", dtbSz, partName, DtbStartOffset);
            return __LINE__;
        }
    }

    memcpy(DtbDestAddr, dtbLoadAddr, dtbSz);
    if(fdt_check_header(DtbDestAddr)){
        errorP("dtb head check failed\n");
        return __LINE__;
    }

    return 0;
}
#else
static int do_image_read_dtb(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
    errorP("unsupported as CONFIG_OF_LIBFDT undef\n");
    return __LINE__;
}
#endif//#ifdef CONFIG_OF_LIBFDT


static int do_image_read_kernel(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
    unsigned    kernel_size;
    unsigned    ramdisk_size;
    boot_img_hdr *hdr_addr = NULL;
    int genFmt = 0;
    unsigned actualBootImgSz = 0;
    unsigned dtbSz = 0;
    const char* const partName = argv[1];
    unsigned char* loadaddr = 0;
    int rc = 0;
    uint64_t flashReadOff = 0;

    if(2 < argc){
        loadaddr = (unsigned char*)simple_strtoul(argv[2], NULL, 16);
    }
    else{
        loadaddr = (unsigned char*)simple_strtoul(getenv("loadaddr"), NULL, 16);
    }
    hdr_addr = (boot_img_hdr*)loadaddr;

    rc = store_read_ops((unsigned char*)partName, loadaddr, flashReadOff, IMG_PRELOAD_SZ);
    if(rc){
        errorP("Fail to read 0x%xB from part[%s] at offset 0\n", IMG_PRELOAD_SZ, partName);
        return __LINE__;
    }
    flashReadOff = IMG_PRELOAD_SZ;

    genFmt = genimg_get_format(hdr_addr);
    if(IMAGE_FORMAT_ANDROID != genFmt) {
        errorP("Fmt unsupported!genFmt 0x%x != 0x%x\n", genFmt, IMAGE_FORMAT_ANDROID);
        return __LINE__;
    }

    kernel_size     =(hdr_addr->kernel_size + (hdr_addr->page_size-1)+hdr_addr->page_size)&(~(hdr_addr->page_size -1));
    ramdisk_size    =(hdr_addr->ramdisk_size + (hdr_addr->page_size-1))&(~(hdr_addr->page_size -1));
    dtbSz           = hdr_addr->second_size;
    actualBootImgSz = kernel_size + ramdisk_size + dtbSz;
    debugP("kernel_size 0x%x, page_size 0x%x, totalSz 0x%x\n", hdr_addr->kernel_size, hdr_addr->page_size, kernel_size);
    debugP("ramdisk_size 0x%x, totalSz 0x%x\n", hdr_addr->ramdisk_size, ramdisk_size);
    debugP("dtbSz 0x%x, Total actualBootImgSz 0x%x\n", dtbSz, actualBootImgSz);

    if(actualBootImgSz > IMG_PRELOAD_SZ)
    {
        const unsigned leftSz = actualBootImgSz - IMG_PRELOAD_SZ;

        debugP("Left sz 0x%x\n", leftSz);
        rc = store_read_ops((unsigned char*)partName, loadaddr + (unsigned)flashReadOff, flashReadOff, leftSz);
        if(rc){
            errorP("Fail to read 0x%xB from part[%s] at offset 0x%x\n", leftSz, partName, IMG_PRELOAD_SZ);
            return __LINE__;
        }
    }
    debugP("totalSz=0x%x\n", actualBootImgSz);

    return 0;
}

#define AML_RES_IMG_VERSION         0x01
#define AML_RES_IMG_V1_MAGIC_LEN    8
#define AML_RES_IMG_V1_MAGIC        "AML_RES!"//8 chars
#define AML_RES_IMG_ITEM_ALIGN_SZ   16
#define AML_RES_IMG_HEAD_SZ         (AML_RES_IMG_ITEM_ALIGN_SZ * 4)//64
#define AML_RES_ITEM_HEAD_SZ        (AML_RES_IMG_ITEM_ALIGN_SZ * 4)//64

//typedef for amlogic resource image
#pragma pack(push, 4)
typedef struct {
    __u32   crc;    //crc32 value for the resouces image
    __s32   version;//current version is 0x01

    __u8    magic[AML_RES_IMG_V1_MAGIC_LEN];  //resources images magic

    __u32   imgSz;  //total image size in byte
    __u32   imgItemNum;//total item packed in the image

    __u32   alignSz;//AML_RES_IMG_ITEM_ALIGN_SZ
    __u8    reserv[AML_RES_IMG_HEAD_SZ - 8 * 3 - 4];

}AmlResImgHead_t;
#pragma pack(pop)

#define LOGO_OLD_FMT_READ_SZ (8U<<20)//if logo format old, read 8M

static int do_image_read_res(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
    const char* const partName = argv[1];
    unsigned char* loadaddr = 0;
    int rc = 0;
    AmlResImgHead_t* pResImgHead = NULL;
    unsigned totalSz    = 0;
    uint64_t flashReadOff = 0;

    if(2 < argc){
        loadaddr = (unsigned char*)simple_strtoul(argv[2], NULL, 16);
    }
    else{
        loadaddr = (unsigned char*)simple_strtoul(getenv("loadaddr"), NULL, 16);
    }
    pResImgHead = (AmlResImgHead_t*)loadaddr;

    rc = store_read_ops((unsigned char*)partName, loadaddr, flashReadOff, IMG_PRELOAD_SZ);
    if(rc){
        errorP("Fail to read 0x%xB from part[%s] at offset 0\n", IMG_PRELOAD_SZ, partName);
        return __LINE__;
    }
    flashReadOff = IMG_PRELOAD_SZ;

    rc = memcmp(pResImgHead->magic, AML_RES_IMG_V1_MAGIC, AML_RES_IMG_V1_MAGIC_LEN);
    if(rc)
    {
        wrnP("Magic error, read old version in size 0x%x\n", RES_OLD_FMT_READ_SZ);

        rc = store_read_ops((unsigned char*)partName, loadaddr + (unsigned)flashReadOff, flashReadOff, RES_OLD_FMT_READ_SZ - flashReadOff);
        return rc;
    }

    //Read the actual size of the new version res imgae
    totalSz = pResImgHead->imgSz;
    if(totalSz > IMG_PRELOAD_SZ )
    {
        const unsigned leftSz = totalSz - flashReadOff;

        rc = store_read_ops((unsigned char*)partName, loadaddr + (unsigned)flashReadOff, flashReadOff, leftSz);
        if(rc){
            errorP("Fail to read 0x%xB from part[%s] at offset 0x%x\n", leftSz, partName, IMG_PRELOAD_SZ);
            return __LINE__;
        }
    }
    debugP("totalSz=0x%x\n", totalSz);

    return 0;
}

static cmd_tbl_t cmd_imgread_sub[] = {
	U_BOOT_CMD_MKENT(dtb,    4, 0, do_image_read_dtb, "", ""),
	U_BOOT_CMD_MKENT(kernel, 3, 0, do_image_read_kernel, "", ""),
	U_BOOT_CMD_MKENT(res,    3, 0, do_image_read_res, "", ""),
};

static int do_image_read(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	cmd_tbl_t *c;

	/* Strip off leading 'bmp' command argument */
	argc--;
	argv++;

	c = find_cmd_tbl(argv[0], &cmd_imgread_sub[0], ARRAY_SIZE(cmd_imgread_sub));

	if (c) {
		return	c->cmd(cmdtp, flag, argc, argv);
	} else {
		cmd_usage(cmdtp);
		return 1;
	}
}

U_BOOT_CMD(
   imgread,         //command name
   5,               //maxargs
   0,               //repeatable
   do_image_read,   //command function
   "Read the image from internal flash with actual size",           //description
   "    argv: <imageType> <part_name> <loadaddr> \n"   //usage
   "    - <image_type> Current support is kernel/res(ource).\n"
   "imgread kernel  --- Read image in fomart IMAGE_FORMAT_ANDROID\n"
   "imgread kernel  --- Load dtb from image boot.img or recovery.img\n"
   "imgread resouce --- Read image packed by 'Amlogic resource packer'\n"
   "    - e.g. \n"
   "        to read boot.img     from part boot     from flash: <imgread kernel boot loadaddr> \n"   //usage
   "        to read dtb          from part boot     from flash: <imgread dtb boot loadaddr [destAddr]> \n"   //usage
   "        to read recovery.img from part recovery from flash: <imgread kernel recovery loadaddr> \n"   //usage
   "        to read logo.img     from part logo     from flash: <imgread res    logo loadaddr> \n"   //usage
);

