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

__attribute__((section (".RAM_D2")))static drv_fmc_lcd_type* drv_lcd_pot=&drv_lcd;

/* LCD读取ID函数 */
uint32_t drvp_fmc_lcd_readid(void)
{
	uint8_t id[3];
	drv_lcd_pot->wcmd(0x04);
	drv_lcd_pot->rdata();//dummy
	id[0]=drv_lcd_pot->rdata();
	id[1]=drv_lcd_pot->rdata();
	id[2]=drv_lcd_pot->rdata();
	return (uint32_t)((id[2]<<16)|(id[1]<<8)|(id[0]<<0));
}

/* 是否打开显示 */
void drvp_fmc_lcd_display_ctrl(bool isopen)
{
	if(true==isopen)
	{
		drv_lcd_pot->wcmd(0x29);
		drv_lcd_pot->bl(true);
	}
	else
	{
		drv_lcd_pot->wcmd(0x28);
		drv_lcd_pot->bl(false);		
	}
}

/* 设置显示窗口 */
void drvp_fmc_lcd_set_wid(uint16_t xstart,uint16_t xstop,
	                        uint16_t ystart,uint16_t ystop)
{
	drv_lcd_pot->wcmd(0x2A);
	drv_lcd_pot->wdata((xstart>>8)&0xff);
	drv_lcd_pot->wdata(xstart&(0xff));
	drv_lcd_pot->wdata(((xstop-1)>>8)&0xff);
	drv_lcd_pot->wdata((xstop-1)&(0xff));
	
	drv_lcd_pot->wcmd(0x2B);
	drv_lcd_pot->wdata((ystart>>8)&0xff);
	drv_lcd_pot->wdata(ystart&(0xff));
	drv_lcd_pot->wdata(((ystop-1)>>8)&0xff);
	drv_lcd_pot->wdata((ystop-1)&(0xff));
}

/* 写显存 */
void drvp_fmc_lcd_writebuf(uint16_t* wbuf,uint32_t size)
{
	drv_lcd_pot->wcmd(0x2C);
	for(uint32_t i=0;i<size;i++) drv_lcd_pot->wdata(wbuf[i]);
}

/* 写单个颜色的显存 */
void drvp_fmc_lcd_writesigle(uint16_t signlecolor,uint32_t size)
{
	drv_lcd_pot->wcmd(0x2C);
	for(uint32_t i=0;i<size;i++) drv_lcd_pot->wdata(signlecolor);
}

/* 读取显存 */
void drvp_fmc_lcd_readbuf(uint16_t* rbuf,uint32_t size)
{
	drv_lcd_pot->wcmd(0x2E);
	for(uint32_t i=0;i<size;i++) rbuf[i]=drv_lcd_pot->rdata();
}

/* 清屏函数 */
void drvp_fmc_lcd_clear(void)
{
	drvp_fmc_lcd_set_wid(0,DRVP_LCD_WIGTH,0,DRVP_LCD_LENGTH);
	drvp_fmc_lcd_writesigle(RGB565_BLACK,(DRVP_LCD_WIGTH*DRVP_LCD_LENGTH));
}

/* 设置坐标轴定义以及显存扫描方向 */
void drvp_fmc_lcd_set_axis_scan(uint8_t my,uint8_t mx,uint8_t mv,uint8_t ml,uint8_t mh)
{
	uint8_t madctrl;
	/* 读取当前的坐标轴定义以及显存扫描方向 */
	drv_lcd_pot->wcmd(0x0B);
	drv_lcd_pot->rdata();
	madctrl=drv_lcd_pot->rdata();
	
	/* 设置坐标轴以及显存扫描方向 */
  if(my) madctrl|=(0x01<<7);
	else   madctrl&=~(0x01<<7);
	if(mx) madctrl|=(0x01<<6);
	else   madctrl&=~(0x01<<6);
	if(mv) madctrl|=(0x01<<5);
	else   madctrl&=~(0x01<<5);
	if(ml) madctrl|=(0x01<<4);
	else   madctrl&=~(0x01<<4);
	if(mh) madctrl|=(0x01<<2);
	else   madctrl&=~(0x01<<2);
	
	drv_lcd_pot->wcmd(0x36);
	drv_lcd_pot->wdata(madctrl);
}

/* LCD初始化函数 */
void drvp_fmc_lcd_init(void)
{
	/* 关闭背光 */
	drv_lcd_pot->bl(false);
	HAL_Delay(10);
	
	/* 底层驱动初始化 */
	drv_lcd_pot->init();
	
	/* 复位 */
	drv_lcd_pot->rst();
	
	/* 写入配置，这个厂商都会给的 */
	drv_lcd_pot->wcmd(0x11);
	HAL_Delay(120); //Delay 120ms
	//--------display and color format setting-----------//
	drv_lcd_pot->wcmd(0x36);
	drv_lcd_pot->wdata(0x00);
	drv_lcd_pot->wcmd(0x3a);
	drv_lcd_pot->wdata(0x05);
	//--------ST7789V Frame rate setting------------//
	drv_lcd_pot->wcmd(0xb2);
	drv_lcd_pot->wdata(0x0c);
	drv_lcd_pot->wdata(0x0c);
	drv_lcd_pot->wdata(0x00);
	drv_lcd_pot->wdata(0x33);
	drv_lcd_pot->wdata(0x33);
	drv_lcd_pot->wcmd(0xb7);
	drv_lcd_pot->wdata(0x35);
	//-----------ST7789V Power setting---------------//
	drv_lcd_pot->wcmd(0xbb);
	drv_lcd_pot->wdata(0x28);
	drv_lcd_pot->wcmd(0xc0);
	drv_lcd_pot->wdata(0x2c);
	drv_lcd_pot->wcmd(0xc2);
	drv_lcd_pot->wdata(0x01);
	drv_lcd_pot->wcmd(0xc3);
	drv_lcd_pot->wdata(0x0b);
	drv_lcd_pot->wcmd(0xc4);
	drv_lcd_pot->wdata(0x20);
	drv_lcd_pot->wcmd(0xc6);
	drv_lcd_pot->wdata(0x0f);
	drv_lcd_pot->wcmd(0xd0);
	drv_lcd_pot->wdata(0xa4);
	drv_lcd_pot->wdata(0xa1);
	//------------ST7789V gamma setting-------------//
	drv_lcd_pot->wcmd(0xe0);
	drv_lcd_pot->wdata(0xd0);
	drv_lcd_pot->wdata(0x01);
	HAL_Delay(5);
	drv_lcd_pot->wdata(0x08);
	drv_lcd_pot->wdata(0x0f);
	drv_lcd_pot->wdata(0x11);
	drv_lcd_pot->wdata(0x2a);
	drv_lcd_pot->wdata(0x36);
	drv_lcd_pot->wdata(0x55);
	drv_lcd_pot->wdata(0x44);
	drv_lcd_pot->wdata(0x3a);
	drv_lcd_pot->wdata(0x0b);
	drv_lcd_pot->wdata(0x06);
	drv_lcd_pot->wdata(0x11);
	drv_lcd_pot->wdata(0x20);
	drv_lcd_pot->wcmd(0xe1);
	drv_lcd_pot->wdata(0xd0);
	drv_lcd_pot->wdata(0x02);
	drv_lcd_pot->wdata(0x07);
	drv_lcd_pot->wdata(0x0a);
	drv_lcd_pot->wdata(0x0b);
	drv_lcd_pot->wdata(0x18);
	drv_lcd_pot->wdata(0x34);
	drv_lcd_pot->wdata(0x43);
	drv_lcd_pot->wdata(0x4a);
	drv_lcd_pot->wdata(0x2b);
	drv_lcd_pot->wdata(0x1b);
	drv_lcd_pot->wdata(0x1c);
	drv_lcd_pot->wdata(0x22);
	drv_lcd_pot->wdata(0x1f);
	drv_lcd_pot->wcmd(0x29);
	
	/* 清屏 */
	drvp_fmc_lcd_set_wid(0,DRVP_LCD_WIGTH,0,DRVP_LCD_LENGTH);
	drvp_fmc_lcd_writesigle(RGB565_BLACK,(DRVP_LCD_WIGTH*DRVP_LCD_LENGTH));
	
	/* 重新打开背光 */
	drv_lcd_pot->bl(true);
	HAL_Delay(10);
}
