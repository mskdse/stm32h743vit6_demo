#include "CO_eeprom.h"
#include "drvp_eeprom.h"
#include <string.h>

/* 全局地址分配指针，避免和你的应用数据冲突 */
static size_t eeprom_next_addr = 0;

/* 初始化 */
bool_t CO_eeprom_init(void* storageModule)
{
    (void)storageModule;
    drvp_eeprom_init();
    eeprom_next_addr = 0;   /* 从 0 开始分配 */
    return true;
}

/* 分配地址：每次调用返回一段空闲区域的起始地址 */
size_t CO_eeprom_getAddr(void* storageModule, bool_t isAuto, size_t len, bool_t* overflow)
{
    (void)storageModule;
    (void)isAuto;

    size_t addr = eeprom_next_addr;

    /* 检查是否超出 EEPROM 容量 */
    if ((addr + len) > EEPROM_MAX_SIZE) 
		{
			*overflow = true;
			return 0;
    }

    *overflow = false;
    eeprom_next_addr += len;

    /* 按页对齐，避免跨页写 */
    if (eeprom_next_addr % EEPROM_PAGE_SIZE != 0) eeprom_next_addr += EEPROM_PAGE_SIZE - (eeprom_next_addr % EEPROM_PAGE_SIZE);

    return addr;
}

/* 读一块数据 */
void CO_eeprom_readBlock(void* storageModule, uint8_t* data, size_t eepromAddr, size_t len)
{
    (void)storageModule;
    drvp_eeprom_readbytes(eepromAddr, data, len);
}

/* 写一块数据 */
bool_t CO_eeprom_writeBlock(void* storageModule, uint8_t* data, size_t eepromAddr, size_t len)
{
   (void)storageModule;
	  drvp_eeprom_writebytes(eepromAddr, data, len);
    return true;
}

/* 计算 CRC16 */
uint16_t CO_eeprom_getCrcBlock(void* storageModule, size_t eepromAddr, size_t len)
{
    (void)storageModule;

    uint8_t buf[32];
    uint16_t crc = 0;
    size_t remaining = len;
    size_t offset = 0;

    while (remaining > 0) {
        size_t chunk = (remaining > sizeof(buf)) ? sizeof(buf) : remaining;
        drvp_eeprom_readbytes((eepromAddr + offset), buf, chunk);

        for (size_t i = 0; i < chunk; i++) {
            crc ^= (uint16_t)buf[i] << 8;
            for (int b = 0; b < 8; b++) {
                if (crc & 0x8000) crc = (crc << 1) ^ 0x1021;
                else              crc <<= 1;
            }
        }

        offset += chunk;
        remaining -= chunk;
    }

    return crc;
}

/* 更新单字节：先读出来比较，不同才写 */
bool_t CO_eeprom_updateByte(void* storageModule, uint8_t data, size_t eepromAddr)
{
    (void)storageModule;

    uint8_t old;
    drvp_eeprom_readbytes(eepromAddr, &old, 1);

    if (old == data) return true;   /* 值相同，不写 */
	
	  drvp_eeprom_writebytes(eepromAddr, &data, 1);
    return true;
}
