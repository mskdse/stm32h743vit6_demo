/*-----------------------------------------------------------------------*/
/* Low level disk I/O module SKELETON for FatFs     (C)ChaN, 2025        */
/*-----------------------------------------------------------------------*/
/* If a working storage control module is available, it should be        */
/* attached to the FatFs via a glue function rather than modifying it.   */
/* This is an example of glue functions to attach various exsisting      */
/* storage control modules to the FatFs module with a defined API.       */
/*-----------------------------------------------------------------------*/

#include "ff.h"			/* Basic definitions of FatFs */
#include "diskio.h"		/* Declarations FatFs MAI */

#include "qspi_flash.h"

/* Example: Mapping of physical drive number for each drive */
#define DEV_QSPI_FLASH	0	/* Map FTL to physical drive 0 */


/*-----------------------------------------------------------------------*/
/* Get Drive Status                                                      */
/*-----------------------------------------------------------------------*/

DSTATUS disk_status (
	BYTE pdrv		/* Physical drive nmuber to identify the drive */
)
{
	DSTATUS stat=0;
	uint32_t flashid;

	switch (pdrv) 
 {
	case DEV_QSPI_FLASH :
	{
		flashid = qspi_flash_w25q128_read_id();
	  if(FLASH_ID==(flashid&FLASH_ID_MASK)) stat&=~STA_NOINIT;
	  else                                  stat|= STA_NOINIT;
		return stat;
	}
	}
	return STA_NOINIT;
}



/*-----------------------------------------------------------------------*/
/* Inidialize a Drive                                                    */
/*-----------------------------------------------------------------------*/

DSTATUS disk_initialize (
	BYTE pdrv				/* Physical drive nmuber to identify the drive */
)
{
	DSTATUS stat;

	switch (pdrv) 
 {
	case DEV_QSPI_FLASH :
	{
		qspi_flash_w25q128_init();
	  stat=disk_status(DEV_QSPI_FLASH);
		return stat;
	}
	}
	return STA_NOINIT;
}



/*-----------------------------------------------------------------------*/
/* Read Sector(s)                                                        */
/*-----------------------------------------------------------------------*/

DRESULT disk_read (
	BYTE pdrv,		/* Physical drive nmuber to identify the drive */
	BYTE *buff,		/* Data buffer to store read data */
	LBA_t sector,	/* Start sector in LBA */
	UINT count		/* Number of sectors to read */
)
{
	switch (pdrv) 
 {
	case DEV_QSPI_FLASH :
	{
		qspi_flash_read((sector<<SECTOR_SIZE_BIT),buff,(count<<SECTOR_SIZE_BIT));
		return RES_OK;
	}
	}
	return RES_PARERR;
}



/*-----------------------------------------------------------------------*/
/* Write Sector(s)                                                       */
/*-----------------------------------------------------------------------*/

#if FF_FS_READONLY == 0

DRESULT disk_write (
	BYTE pdrv,			/* Physical drive nmuber to identify the drive */
	const BYTE *buff,	/* Data to be written */
	LBA_t sector,		/* Start sector in LBA */
	UINT count			/* Number of sectors to write */
)
{
	switch (pdrv) 
 {
	case DEV_QSPI_FLASH :
	{
		for(int i=0;i<count;i++) qspi_flash_erase_sector(((sector+i)<<SECTOR_SIZE_BIT));
		qspi_flash_write((sector<<SECTOR_SIZE_BIT),(uint8_t*)buff,(count<<SECTOR_SIZE_BIT));
		return RES_OK;
	}
	}
	return RES_PARERR;
}

#endif


/*-----------------------------------------------------------------------*/
/* Miscellaneous Functions                                               */
/*-----------------------------------------------------------------------*/

DRESULT disk_ioctl (
	BYTE pdrv,		/* Physical drive nmuber (0..) */
	BYTE cmd,		/* Control code */
	void *buff		/* Buffer to send/receive control data */
)
{
	DRESULT res=RES_OK;

	switch (pdrv) 
	{
	case DEV_QSPI_FLASH :
	{
     if(GET_SECTOR_COUNT==cmd)      *(LBA_t*)buff=(W25Q128_SIZE/SECTOR_SIZE);
		 else if(GET_SECTOR_SIZE==cmd)  *(WORD*) buff=SECTOR_SIZE;
		 else if(GET_BLOCK_SIZE==cmd)   *(DWORD*)buff=1;
	}
  }
	return res;
}

DWORD get_fattime(void)
{
    // 固定返回时间：2026年8月24日（周一） 12:00:00
    // 位域格式：bit31:25 = 年(2026 - 1980 = 46)，bit24:21 = 月(8)，bit20:16 = 日(24)
    //          bit15:11 = 时(12)，bit10:5 = 分(0)，bit4:0 = 秒/2(0)
    return 0x46681800;
}

/* 文件系统测试函数，测试通过可以关闭以下代码 */
#define USE_FILESYS_DEBUG    0

#if USE_FILESYS_DEBUG

#include <string.h>
#include <stdbool.h>
#include "SEGGER_RTT.h"
#include <stdio.h>
#include "sram_d2_malloc.h"
#define NOT_FILE_SYS     0  //一开始没有文件系统，可以打开宏定义开关来创建一下
__attribute__((section (".RAM_D2"))) FATFS   fs;
__attribute__((section (".RAM_D2"))) FIL     file;
__attribute__((section (".RAM_D2"))) FRESULT res;
void FATFS_TEST_INIT(uint8_t dev,bool ismount)
{
    char volume[4];        // "0:" + '\0'，足够支持 0~9
    char filepath[32];

    memset(&fs,0,sizeof(FATFS));
    memset(&file,0,sizeof(FIL));
    memset(&res,0,sizeof(FRESULT));
    snprintf(volume,sizeof(volume),"%u:",dev);
	
#if NOT_FILE_SYS
    /* ==================== 1. 创建卷 ==================== */
    MKFS_PARM opt =
    {
        .fmt     = FM_FAT,
        .n_fat   = 1,
        .align   = 0,
        .n_root  = 0,
        .au_size = 0
    };
    BYTE *work = (BYTE *)sram_d2_malloc(FF_MAX_SS * 2);
    res = f_mkfs(volume, &opt, work, FF_MAX_SS * 2);
    SEGGER_RTT_printf(0,"f_mkfs %s res: %d\r\n",volume,res);
    sram_d2_free(work);
    work = NULL;
    /* ==================== 2. 挂载文件系统 ==================== */
    res = f_mount(&fs, volume, 1);
    SEGGER_RTT_printf(0,"f_mount %s res: %d\r\n",volume,res);
    /* ==================== 3. 创建文件 ==================== */
    snprintf(filepath,sizeof(filepath),"%shello.txt",volume);
    res = f_open(&file,filepath,FA_CREATE_ALWAYS | FA_WRITE);
    SEGGER_RTT_printf(0,"f_open %s res: %d\r\n",filepath,res);
    /* ==================== 4. 写入数据 ==================== */
    char text[] = "hello world";
    UINT bytes_written = 0;
    res = f_write(&file,text,strlen(text),&bytes_written);
    SEGGER_RTT_printf(0,"f_write res: %d, len: %u\r\n",res,bytes_written);
    /* ==================== 5. 关闭文件 ==================== */
    res = f_close(&file);
    SEGGER_RTT_printf(0,"f_close res: %d\r\n",res);
    /* ==================== 6. 重新打开文件 ==================== */
    res = f_open(&file,filepath,FA_READ);
    SEGGER_RTT_printf(0,"f_open %s res: %d\r\n",filepath,res);
    /* ==================== 7. 读取文件 ==================== */
    char read_buf[32] = {0};
    UINT bytes_read = 0;
    res = f_read(&file,read_buf,sizeof(read_buf) - 1,&bytes_read);
    SEGGER_RTT_printf(0,"f_read res: %d, len: %u\r\n",res,bytes_read);
    if (res == FR_OK && bytes_read > 0)
    {
        read_buf[bytes_read] = '\0';
        SEGGER_RTT_printf(0,"read data: [%s]\r\n",read_buf);
    }
    /* ==================== 8. 关闭文件 ==================== */
    res = f_close(&file);
    SEGGER_RTT_printf(0,"f_close res: %d\r\n",res);
    /* ==================== 9. 删除文件 ==================== */
    res = f_unlink(filepath);
    SEGGER_RTT_printf(0,"f_unlink %s res: %d\r\n",filepath,res);
    /* ==================== 10. 取消挂载 ==================== */
    res = f_mount(&fs, volume, 0);
    SEGGER_RTT_printf(0,"f_unmount %s res: %d\r\n",volume,res);
#else
    /* ==================== 挂载/取消挂载 ==================== */
    res = f_mount(&fs,volume,ismount ? 1 : 0);
    SEGGER_RTT_printf(0,"%s %s res: %d\r\n",ismount ? "f_mount" : "f_unmount",volume,res);
#endif
}

#endif
