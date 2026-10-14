#include "sdio_card.h"
#include <string.h>

__attribute__((section (".RAM_D2")))static SD_HandleTypeDef       SDHandle;
__attribute__((section (".RAM_D2")))static HAL_SD_CardInfoTypeDef pCardInfo;
__attribute__((section (".RAM_D2")))static sdio_sd_card_callback  SD_Card_Calk;

#define SD_TIMEOUT             ((uint32_t)0x00100000U)

/**
  * @brief  This function is executed in case of error occurrence.
  * @param  None
  * @retval None
  */
static void Error_Handler(uint8_t errno)
{
	printf("sd_error:%d\r\n",errno);
}

/**
  * @brief  Wait SD Card ready status
  * @param  None
  * @retval None
  */
static uint8_t Wait_SDCARD_Ready(void)
{
  uint32_t loop = SD_TIMEOUT;
  
  /* Wait for the Erasing process is completed */
  /* Verify that SD card is ready to use after the Erase */
  while(loop > 0)
  {
    loop--;
    if(HAL_SD_GetCardState(&SDHandle) == HAL_SD_CARD_TRANSFER) return HAL_OK;
  }
  return HAL_ERROR;
}

static void sdio_sd_card_borad_init(void)
{
	 /* DeInit GPIO pins can be done in the application 
  (by surcharging this __weak function) */
	memset(&SDHandle,0,sizeof(SD_HandleTypeDef));
	memset(&pCardInfo,0,sizeof(HAL_SD_CardInfoTypeDef));
  
    /* Enable GPIOs clock */
  __HAL_RCC_GPIOB_CLK_DISABLE();
  __HAL_RCC_GPIOC_CLK_DISABLE();
  __HAL_RCC_GPIOD_CLK_DISABLE();
  
  /* Disable SDMMC1 clock */
  __HAL_RCC_SDMMC1_CLK_DISABLE();
	
	for(int i=0;i<0xFFF;i++) __NOP();
	
	GPIO_InitTypeDef gpio_init_structure;
  
  /* Enable SDIO clock */
  __HAL_RCC_SDMMC1_CLK_ENABLE();
  
  /* Enable GPIOs clock */
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();

  gpio_init_structure.Mode      = GPIO_MODE_AF_PP;
  gpio_init_structure.Pull      = GPIO_NOPULL;
  gpio_init_structure.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
  
  /* D0(PC8), D1(PC9), D2(PC10), D3(PC11), CK(PC12), CMD(PD2) */
  /* Common GPIO configuration */
  gpio_init_structure.Alternate = GPIO_AF12_SDIO1;
  
  /* GPIOC configuration */
  gpio_init_structure.Pin = GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 | GPIO_PIN_11 | GPIO_PIN_12;
  HAL_GPIO_Init(GPIOC, &gpio_init_structure);

  /* GPIOD configuration */
  gpio_init_structure.Pin = GPIO_PIN_2;
  HAL_GPIO_Init(GPIOD, &gpio_init_structure);

  __HAL_RCC_SDMMC1_FORCE_RESET();
  __HAL_RCC_SDMMC1_RELEASE_RESET();
}

static void sdio_sd_card_init(void)
{
	sdio_sd_card_borad_init();
	
  SDHandle.Instance = SDMMC1;
  HAL_SD_DeInit(&SDHandle);
    
  /* if CLKDIV = 0 then SDMMC Clock frequency = SDMMC Kernel Clock
     else SDMMC Clock frequency = SDMMC Kernel Clock / [2 * CLKDIV]. 
     SDMMC Kernel Clock = 200MHz, SDMMC Clock frequency = 20MHz  */
  SDHandle.Init.ClockEdge           = SDMMC_CLOCK_EDGE_FALLING;
  SDHandle.Init.ClockPowerSave      = SDMMC_CLOCK_POWER_SAVE_DISABLE;
  SDHandle.Init.BusWide             = SDMMC_BUS_WIDE_4B;
  SDHandle.Init.HardwareFlowControl = SDMMC_HARDWARE_FLOW_CONTROL_DISABLE;
  SDHandle.Init.ClockDiv            = 5;
  
  if(HAL_SD_Init(&SDHandle) != HAL_OK)
  {
    Error_Handler(1);
		return;
  }
	
	if(Wait_SDCARD_Ready() != HAL_OK)
  {
    Error_Handler(2);
		return;
  }
	
	if(HAL_SD_GetCardInfo(&SDHandle,&pCardInfo) != HAL_OK)
  {
    Error_Handler(3);
		return;
  }
}

bool sdio_sd_card_write(uint32_t start_block_num,const uint8_t* buf,uint32_t block_num)
{
	if(Wait_SDCARD_Ready() != HAL_OK)
	{
    printf("sd_write wait error\r\n");
		Error_Handler(4);
		return false;
	}
  
	if(HAL_SD_WriteBlocks(&SDHandle, buf, start_block_num, block_num,2000) != HAL_OK)
	{
    printf("sd_write error\r\n");
		Error_Handler(5);
		return false;
	}

  return true;
}

bool sdio_sd_card_read(uint32_t start_block_num,uint8_t* buf,uint32_t block_num)
{
	if(Wait_SDCARD_Ready() != HAL_OK)
	{
    printf("sd_read wait error\r\n");
		Error_Handler(6);
		return false;
	}

	if(HAL_SD_ReadBlocks(&SDHandle, buf, start_block_num, block_num,2000) != HAL_OK)
	{
    printf("sd_read error\r\n");
		Error_Handler(7);
		return false;
	}

	return true;
}

HAL_SD_CardInfoTypeDef sdio_sd_card_info(void)
{
	return pCardInfo;
}

void sdio_sd_card_register_clk(sdio_sd_card_callback clk)
{
	SD_Card_Calk=clk;
}

void sdio_sd_card_prc_200ms(void)
{
    static bool     s_present  = false;//是否在位
    static uint8_t  s_debounce = 0;
    static bool     s_last_raw = false;
    bool            s_raw;

		if(!s_present) sdio_sd_card_init();//SD卡不在位尝试初始化
		s_raw=(HAL_SD_CARD_TRANSFER==HAL_SD_GetCardState(&SDHandle));//发送一次CMD13命令探测一次

    /* 去抖 */
    if (s_raw != s_last_raw)
    { 
		  s_last_raw = s_raw; 
		  s_debounce = 0; 
		  return; 
		}
		
    if (++s_debounce < 3) return;
    s_debounce = 0;

    if (s_raw == s_present) return;

    s_present = s_raw;

    if (s_raw)
    {
        /* 插入：此时 sdio_sd_card_init 已经成功，取容量 */
        HAL_SD_GetCardInfo(&SDHandle, &pCardInfo);   // 更新全局
        if(SD_Card_Calk) SD_Card_Calk(INSERT);
    }
    else
    {
        /* 拔出：反初始化，清缓存 */
        HAL_SD_DeInit(&SDHandle);
        if(SD_Card_Calk) SD_Card_Calk(PULL);
    }
}
