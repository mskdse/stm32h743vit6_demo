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

