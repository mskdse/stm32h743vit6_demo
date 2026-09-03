#include "drv_fmc_lcd.h"
#include <string.h>

/*

D0--->PD14
D1--->PD15
D2--->PD0
D3--->PD1
D4--->PE7
D5--->PE8
D6--->PE9
D7--->PE10
D8--->PE11
D9--->PE12
D10--->PE13
D11--->PE14
D12--->PE15
D13--->PD8
D14--->PD9
D15--->PD10

RST--->PE0
BL--->PE1

CS--->PD7
WEnable--->PD5
REnable--->PD4
D/C--->PE3

*/

/* CS引脚接到FMC的BANK1的第一片区域，而且D/C复用为A19。
   访问命令的话地址肯定为0x60000000
   访问数据的话地址肯定为0x60080000(第19位置位),但是由于16位宽访问下
	 它会访问右移一位的地址，所以我们必须手动向左移一位是0x60100000(第20位置位)，访问命令的话不用管，因为此时D/C无论如何都是0
*/
#define FMC_LCD_ADDR_CMD     ((uint32_t)0x60000000)
#define FMC_LCD_ADDR_DATA    ((uint32_t)0x60100000)

__attribute__((section (".RAM_D2")))static DMA_HandleTypeDef dma1_cfg;

void drv_fmc_dma_m2m_init(void(*Callback)(DMA_HandleTypeDef *_hdma))
{
	__HAL_RCC_DMA1_CLK_ENABLE();
	memset(&dma1_cfg,0,sizeof(DMA_HandleTypeDef));
	dma1_cfg.Instance=DMA1_Stream1;
	dma1_cfg.Init.Direction=DMA_MEMORY_TO_MEMORY;
	dma1_cfg.Init.FIFOMode=DMA_FIFOMODE_ENABLE;
	dma1_cfg.Init.FIFOThreshold=DMA_FIFO_THRESHOLD_1QUARTERFULL;
	dma1_cfg.Init.MemBurst=DMA_MBURST_SINGLE;
	dma1_cfg.Init.MemDataAlignment=DMA_MDATAALIGN_HALFWORD;
	dma1_cfg.Init.MemInc=DMA_MINC_DISABLE;
	dma1_cfg.Init.Mode=DMA_NORMAL;
	dma1_cfg.Init.PeriphBurst=DMA_PBURST_SINGLE;
	dma1_cfg.Init.PeriphDataAlignment=DMA_PDATAALIGN_HALFWORD;
	dma1_cfg.Init.PeriphInc=DMA_PINC_ENABLE;
	dma1_cfg.Init.Priority=DMA_PRIORITY_VERY_HIGH;
	dma1_cfg.Init.Request=DMA_REQUEST_MEM2MEM;
	HAL_DMA_Init(&dma1_cfg);
	
	HAL_DMA_RegisterCallback(&dma1_cfg,HAL_DMA_XFER_CPLT_CB_ID,Callback);
	
	HAL_NVIC_SetPriority(DMA1_Stream1_IRQn,10,0);
	HAL_NVIC_EnableIRQ(DMA1_Stream1_IRQn);
}

void drv_fmc_dma_m2m_tranf(uint16_t* buf,uint16_t len)
{
	HAL_DMA_Start_IT(&dma1_cfg,(uint32_t)buf,FMC_LCD_ADDR_DATA,len);
}

void DMA1_Stream1_IRQHandler(void)
{
	HAL_DMA_IRQHandler(&dma1_cfg);
}

/* fmc初始化函数 */
void drv_fmc_lcd_init(void)
{
	GPIO_InitTypeDef           gpio_init_structure;
	FMC_NORSRAM_InitTypeDef    fmc_cfg;
	FMC_NORSRAM_TimingTypeDef  fmc_lcd_cfg;
	
	__HAL_RCC_FMC_CLK_ENABLE();
	__HAL_RCC_GPIOD_CLK_ENABLE();
	__HAL_RCC_GPIOE_CLK_ENABLE();
	
	/* D0-15以及一些复用的信号引脚 */
  gpio_init_structure.Mode      = GPIO_MODE_AF_PP;
  gpio_init_structure.Pull      = GPIO_NOPULL;
  gpio_init_structure.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
  gpio_init_structure.Alternate = GPIO_AF12_FMC;
  gpio_init_structure.Pin       = GPIO_PIN_0|GPIO_PIN_1|GPIO_PIN_4|GPIO_PIN_5|GPIO_PIN_7|
	                                GPIO_PIN_8|GPIO_PIN_9|GPIO_PIN_10|GPIO_PIN_14|GPIO_PIN_15;
  HAL_GPIO_Init(GPIOD, &gpio_init_structure);
	gpio_init_structure.Pin       = GPIO_PIN_3|GPIO_PIN_7|GPIO_PIN_8|GPIO_PIN_9|GPIO_PIN_10|
	                                GPIO_PIN_11|GPIO_PIN_12|GPIO_PIN_13|GPIO_PIN_14|GPIO_PIN_15;
	HAL_GPIO_Init(GPIOE, &gpio_init_structure);
	
	/* 背光和复位引脚加上上拉控制 */
	gpio_init_structure.Mode      = GPIO_MODE_OUTPUT_PP;
	gpio_init_structure.Pull      = GPIO_PULLUP;
	gpio_init_structure.Pin       = GPIO_PIN_0|GPIO_PIN_1;
	HAL_GPIO_Init(GPIOE, &gpio_init_structure);
	
	/* 初始化FMC外设，由于NE引脚为NE1,所以控制区域在BANK1的第一片区域 */
  memset(&fmc_cfg,0,sizeof(FMC_NORSRAM_InitTypeDef));
	memset(&fmc_lcd_cfg,0,sizeof(FMC_NORSRAM_TimingTypeDef));
	
	fmc_cfg.AsynchronousWait=FMC_ASYNCHRONOUS_WAIT_DISABLE;//无等待信号
	fmc_cfg.BurstAccessMode=FMC_BURST_ACCESS_MODE_DISABLE;//禁止突发，我的时序是异步的
	fmc_cfg.ContinuousClock=FMC_CONTINUOUS_CLOCK_SYNC_ONLY;//没有时钟功能，不需要给
	fmc_cfg.DataAddressMux=FMC_DATA_ADDRESS_MUX_DISABLE;//地址和数据线不复用
	fmc_cfg.ExtendedMode=FMC_EXTENDED_MODE_DISABLE;//不需要扩展模式来完成不同的读写模式
	fmc_cfg.MemoryDataWidth=FMC_NORSRAM_MEM_BUS_WIDTH_16;//数据总线宽度支持16位
	fmc_cfg.MemoryType=FMC_MEMORY_TYPE_NOR;//器件属性是Nor-FLash时序
	fmc_cfg.NSBank=FMC_NORSRAM_BANK1;//第一片区域
	fmc_cfg.PageSize=FMC_PAGE_SIZE_NONE;//没有突发功能，不需要配置页
	fmc_cfg.WaitSignal=FMC_WAIT_SIGNAL_DISABLE;//没有等待信号
	fmc_cfg.WaitSignalActive=FMC_WAIT_TIMING_BEFORE_WS;//没有等待信号，随便给
	fmc_cfg.WaitSignalPolarity=FMC_WAIT_SIGNAL_POLARITY_LOW;//没有等待信号，随便给
	fmc_cfg.WriteBurst=FMC_WRITE_BURST_DISABLE;//写的话也没有突发功能
	fmc_cfg.WriteFifo=FMC_WRITE_FIFO_ENABLE;//允许写缓存
	fmc_cfg.WriteOperation=FMC_WRITE_OPERATION_ENABLE;//允许写
	
	/* 时序配置，需要结合LCD手册,FMC挂载在AHB3总线上，总线时钟200MHZ,也就是该fmc内核5ns计数一次 */
	fmc_lcd_cfg.AccessMode=FMC_ACCESS_MODE_B;//模拟8080时序用模式B
	fmc_lcd_cfg.AddressHoldTime=2;//lcd手册给的数据保持周期是10ns
	fmc_lcd_cfg.AddressSetupTime=10;//lcd手册里面地址建立周期，写的话15ns，不过读取id的话要45ns，为了兼容这些操作给50ns
	fmc_lcd_cfg.BusTurnAroundDuration=0;//总线周转周期，给多少无所谓都是固定的，D模式和复用模式才有用，其他固定
	fmc_lcd_cfg.CLKDivision=0;//没有时钟信号，不需要配置分频
	fmc_lcd_cfg.DataLatency=0;//不是同步器件，没有同步存储器的数据延迟
	fmc_lcd_cfg.DataSetupTime=2;//lcd手册给的数据建立周期是10ns
	FMC_NORSRAM_Init(FMC_NORSRAM_DEVICE,&fmc_cfg);
	FMC_NORSRAM_Timing_Init(FMC_NORSRAM_DEVICE,&fmc_lcd_cfg,FMC_NORSRAM_BANK1);
	
	/* ll库内部没有使能，这里必须调用 */
	__FMC_NORSRAM_ENABLE(FMC_NORSRAM_DEVICE,FMC_NORSRAM_BANK1);
	__FMC_ENABLE();
}

/* lcd复位函数 */
void drv_fmc_lcd_rst(void)
{
		HAL_GPIO_WritePin(GPIOE,GPIO_PIN_0,GPIO_PIN_SET);
		HAL_Delay(1);
		HAL_GPIO_WritePin(GPIOE,GPIO_PIN_0,GPIO_PIN_RESET);
		HAL_Delay(10);
		HAL_GPIO_WritePin(GPIOE,GPIO_PIN_0,GPIO_PIN_SET);
		HAL_Delay(120); 
}

/* lcd是否打开背光函数 */
void drv_fmc_bl_ctrl(bool isopen)
{
	HAL_GPIO_WritePin(GPIOE,GPIO_PIN_1,((GPIO_PIN_RESET==isopen)?GPIO_PIN_RESET:GPIO_PIN_SET));
}

/* 写命令 */
void drv_fmc_write_cmd(uint16_t cmd)
{
	*((volatile uint16_t*)FMC_LCD_ADDR_CMD)=cmd;
}

/* 写数据 */
void drv_fmc_write_data(uint16_t data)
{
	*((volatile uint16_t*)FMC_LCD_ADDR_DATA)=data;
}

/* 读数据 */
uint16_t drv_fmc_read_data(void)
{
	return *((volatile uint16_t*)FMC_LCD_ADDR_DATA);
}

