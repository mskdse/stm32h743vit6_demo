#include "drvp_fmc_lcd.h"

typedef struct
{
	void(*init)     (void);
	void(*rst)      (void);
	void(*bl)       (bool);
	void(*wcmd)     (uint16_t);
	void(*wdata)    (uint16_t);
	uint16_t(*rdata)(void);
}drv_fmc_lcd_type;

__attribute__((section (".RAM_D2")))static drv_fmc_lcd_type drv_lcd=
{
	.init = drv_fmc_lcd_init,
	.rst  = drv_fmc_lcd_rst,
	.bl   = drv_fmc_bl_ctrl,
	.wcmd = drv_fmc_write_cmd,
	.wdata= drv_fmc_write_data,
	.rdata= drv_fmc_read_data
};

void drvp_fmc_lcd_init(void)
{
	/* 关闭背光 */
	drv_lcd.bl(false);
	
	/* 底层驱动初始化 */
	drv_lcd.init();
	
	/* 复位 */
	drv_lcd.rst();
	
	/* 写入配置，这个厂商都会给的 */
	drv_lcd.wcmd(0x11);
	HAL_Delay(120); //Delay 120ms
	//--------display and color format setting-----------//
	drv_lcd.wcmd(0x36);
	drv_lcd.wdata(0x00);
	drv_lcd.wcmd(0x3a);
	drv_lcd.wdata(0x05);
	//--------ST7789V Frame rate setting------------//
	drv_lcd.wcmd(0xb2);
	drv_lcd.wdata(0x0c);
	drv_lcd.wdata(0x0c);
	drv_lcd.wdata(0x00);
	drv_lcd.wdata(0x33);
	drv_lcd.wdata(0x33);
	drv_lcd.wcmd(0xb7);
	drv_lcd.wdata(0x35);
	//-----------ST7789V Power setting---------------//
	drv_lcd.wcmd(0xbb);
	drv_lcd.wdata(0x28);
	drv_lcd.wcmd(0xc0);
	drv_lcd.wdata(0x2c);
	drv_lcd.wcmd(0xc2);
	drv_lcd.wdata(0x01);
	drv_lcd.wcmd(0xc3);
	drv_lcd.wdata(0x0b);
	drv_lcd.wcmd(0xc4);
	drv_lcd.wdata(0x20);
	drv_lcd.wcmd(0xc6);
	drv_lcd.wdata(0x0f);
	drv_lcd.wcmd(0xd0);
	drv_lcd.wdata(0xa4);
	drv_lcd.wdata(0xa1);
	//------------ST7789V gamma setting-------------//
	drv_lcd.wcmd(0xe0);
	drv_lcd.wdata(0xd0);
	drv_lcd.wdata(0x01);
	drv_lcd.wdata(0x08);
	drv_lcd.wdata(0x0f);
	drv_lcd.wdata(0x11);
	drv_lcd.wdata(0x2a);
	drv_lcd.wdata(0x36);
	drv_lcd.wdata(0x55);
	drv_lcd.wdata(0x44);
	drv_lcd.wdata(0x3a);
	drv_lcd.wdata(0x0b);
	drv_lcd.wdata(0x06);
	drv_lcd.wdata(0x11);
	drv_lcd.wdata(0x20);
	drv_lcd.wcmd(0xe1);
	drv_lcd.wdata(0xd0);
	drv_lcd.wdata(0x02);
	drv_lcd.wdata(0x07);
	drv_lcd.wdata(0x0a);
	drv_lcd.wdata(0x0b);
	drv_lcd.wdata(0x18);
	drv_lcd.wdata(0x34);
	drv_lcd.wdata(0x43);
	drv_lcd.wdata(0x4a);
	drv_lcd.wdata(0x2b);
	drv_lcd.wdata(0x1b);
	drv_lcd.wdata(0x1c);
	drv_lcd.wdata(0x22);
	drv_lcd.wdata(0x1f);
	drv_lcd.wcmd(0x29);
	
	/* 重新打开背光 */
	drv_lcd.bl(true);
}
