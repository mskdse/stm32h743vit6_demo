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

/* 如果程序下载到QSPI_FLASH，那就不要给它移植文件系统除非你愿意牺牲cache性能或者处理cache一致性问题并且还要确保不要把程序空间擦写掉
   或者可以把QSPI_FLASH运行程序和存储数据分块，分别设置不同的Cache属性。
*/
/* 程序上API做了兼容，QSPI和SPI，只是名字加了个q而已 */
#include "qspi_flash.h"
#include "spi_flash.h"
#include "sdio_card.h"
#include <string.h>

/* Example: Mapping of physical drive number for each drive */
//#define DEV_QSPI_FLASH	0	/* Map FTL to physical drive 0 */
#define DEV_SPI_FLASH	       0
#define DEV_SDIO_CARD	       1

/* 用来处理SDIO+DMA的四字节对齐的问题，之前的做法强制让结构体四字节对齐根本是无效的，它只能保证初始是四字节对齐 */
__attribute__((section(".RAM_D1"), aligned(4)))static uint8_t sd_align_buf[512];

/*-----------------------------------------------------------------------*/
/* Get Drive Status                                                      */
/*-----------------------------------------------------------------------*/

DSTATUS disk_status (
	BYTE pdrv		/* Physical drive nmuber to identify the drive */
)
{
	DSTATUS stat=0;
	uint32_t flashid;
	HAL_SD_CardInfoTypeDef sd_info;

	switch (pdrv) 
 {
		case DEV_SPI_FLASH :
		{
			flashid = spi_flash_w25q128_read_id();
			if(FLASH_ID==(flashid&FLASH_ID_MASK)) stat&=~STA_NOINIT;
			else                                  stat|= STA_NOINIT;
			return stat;
		}
		case DEV_SDIO_CARD :
		{
			sd_info = sdio_sd_card_info();
			if(512==sd_info.BlockSize) stat&=~STA_NOINIT;
			else                       stat|= STA_NOINIT;
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
		case DEV_SPI_FLASH :
		{
			spi_flash_w25q128_init();
			stat=disk_status(DEV_SPI_FLASH);
			return stat;
		}
		case DEV_SDIO_CARD :
		{
			sdio_sd_card_init();
			stat=disk_status(DEV_SDIO_CARD);
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
		case DEV_SPI_FLASH :
		{	
			spi_flash_read((sector<<SECTOR_SIZE_BIT),buff,(count<<SECTOR_SIZE_BIT));
			return RES_OK;
		}
		case DEV_SDIO_CARD:
		{
				uint8_t *p = buff;

				/* 地址 4 字节对齐：直接走 DMA */
				if (((uint32_t)p & 0x3) == 0)
				{
						if (sdio_sd_card_read(sector, p, count)) return RES_OK;
						return RES_ERROR;
				}

				/* 地址不对齐：逐块读到对齐缓冲区再 memcpy 回去 */
				for (uint32_t i = 0; i < count; i++)
				{
						if (!sdio_sd_card_read((sector + i), sd_align_buf, 1)) return RES_ERROR;
						memcpy(p + i * 512, sd_align_buf, 512);
				}
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
		case DEV_SPI_FLASH :
		{
			for(int i=0;i<count;i++) spi_flash_erase_sector_4k(((sector+i)<<SECTOR_SIZE_BIT));
			spi_flash_write((sector<<SECTOR_SIZE_BIT),(uint8_t*)buff,(count<<SECTOR_SIZE_BIT));
			return RES_OK;
		}
		case DEV_SDIO_CARD:
		{
				const uint8_t *p = buff;

				/* 地址 4 字节对齐：直接走 DMA */
				if (((uint32_t)p & 0x3) == 0)
				{
						if (sdio_sd_card_write(sector, p, count)) return RES_OK;
						return RES_ERROR;
				}

				/* 地址不对齐：逐块 memcpy 到对齐缓冲区再写 */
				for (uint32_t i = 0; i < count; i++)
				{
						memcpy(sd_align_buf, p + i * 512, 512);
						if (!sdio_sd_card_write((sector + i), sd_align_buf, 1)) return RES_ERROR;
				}
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
	HAL_SD_CardInfoTypeDef sd_info;
	switch (pdrv) 
	{
		case DEV_SPI_FLASH :
		{
			 if(GET_SECTOR_COUNT==cmd)      *(LBA_t*)buff=(W25Q128_SIZE/SECTOR_SIZE);
			 else if(GET_SECTOR_SIZE==cmd)  *(WORD*) buff=SECTOR_SIZE;
			 else if(GET_BLOCK_SIZE==cmd)   *(DWORD*)buff=1;
		}return RES_OK;
		case DEV_SDIO_CARD :
		{
			 sd_info = sdio_sd_card_info();
			 if(GET_SECTOR_COUNT==cmd)      *(LBA_t*)buff=sd_info.BlockNbr;
			 else if(GET_SECTOR_SIZE==cmd)  *(WORD*) buff=sd_info.BlockSize;
			 else if(GET_BLOCK_SIZE==cmd)   *(DWORD*)buff=1;
		}return RES_OK;
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
#include <stdio.h>
#include "usart1_dma.h"
#define NOT_FILE_SYS     0  //一开始没有文件系统，可以打开宏定义开关来创建一下
__attribute__((section (".RAM_D1"))) FATFS   fs;
__attribute__((section (".RAM_D1"))) FIL     file;
__attribute__((section (".RAM_D1"))) FRESULT res;
#if NOT_FILE_SYS
__attribute__((section (".RAM_D1"),aligned(4)))BYTE work[(FF_MAX_SS*2)];
#endif
void FATFS_TEST_INIT(uint8_t dev,bool ismount)
{
    char volume[4];        // "0:" + '\0'，足够支持 0~9
    char filepath[32];
    memset(&fs,0,sizeof(FATFS));
    memset(&file,0,sizeof(FIL));
    memset(&res,0,sizeof(FRESULT));
#if NOT_FILE_SYS
	  memset(work,0,sizeof(work));
#endif
    snprintf(volume,sizeof(volume),"%u:",dev);
#if NOT_FILE_SYS
    /* ==================== 1. 创建卷 ==================== */
	  MKFS_PARM opt;
    if(dev==DEV_SPI_FLASH)
    {
        opt.fmt     = FM_FAT;
        opt.n_fat   = 1;
        opt.align   = 0;
        opt.n_root  = 0;
        opt.au_size = 0;
    }
		else if(dev==DEV_SDIO_CARD)
		{
			  opt.fmt     = FM_FAT32;
        opt.n_fat   = 1;
        opt.align   = 0;
        opt.n_root  = 0;
        opt.au_size = 0;
		}
    res = f_mkfs(volume, &opt, work, FF_MAX_SS * 2);
    printf("f_mkfs %s res: %d\r\n",volume,res);
    /* ==================== 2. 挂载文件系统 ==================== */
    res = f_mount(&fs, volume, 1);
    printf("f_mount %s res: %d\r\n",volume,res);
    /* ==================== 3. 创建文件 ==================== */
    snprintf(filepath,sizeof(filepath),"%shello.txt",volume);
    res = f_open(&file,filepath,FA_CREATE_ALWAYS | FA_WRITE);
    printf("f_open %s res: %d\r\n",filepath,res);
    /* ==================== 4. 写入数据 ==================== */
    char text[] = "hello world";
    UINT bytes_written = 0;
    res = f_write(&file,text,strlen(text),&bytes_written);
    printf("f_write res: %d, len: %u\r\n",res,bytes_written);
    /* ==================== 5. 关闭文件 ==================== */
    res = f_close(&file);
    printf("f_close res: %d\r\n",res);
    /* ==================== 6. 重新打开文件 ==================== */
    res = f_open(&file,filepath,FA_READ);
    printf("f_open %s res: %d\r\n",filepath,res);
    /* ==================== 7. 读取文件 ==================== */
    char read_buf[32] = {0};
    UINT bytes_read = 0;
    res = f_read(&file,read_buf,sizeof(read_buf) - 1,&bytes_read);
    printf("f_read res: %d, len: %u\r\n",res,bytes_read);
    if (res == FR_OK && bytes_read > 0)
    {
        read_buf[bytes_read] = '\0';
        printf("read data: [%s]\r\n",read_buf);
    }
    /* ==================== 8. 关闭文件 ==================== */
    res = f_close(&file);
    printf("f_close res: %d\r\n",res);
    /* ==================== 9. 删除文件 ==================== */
    res = f_unlink(filepath);
    printf("f_unlink %s res: %d\r\n",filepath,res);
    /* ==================== 10. 取消挂载 ==================== */
    res = f_mount(&fs, volume, 0);
    printf("f_unmount %s res: %d\r\n",volume,res);
#else
    /* ==================== 挂载/取消挂载 ==================== */
    res = f_mount(&fs,volume,ismount ? 1 : 0);
    printf("%s %s res: %d\r\n",ismount ? "f_mount" : "f_unmount",volume,res);
#endif
}

#endif
