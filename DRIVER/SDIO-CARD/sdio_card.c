#include "sdio_card.h"
#include <string.h>

__attribute__((section (".RAM_D2")))static SD_HandleTypeDef       SDHandle;
__attribute__((section (".RAM_D2")))static HAL_SD_CardInfoTypeDef pCardInfo;
__attribute__((section (".RAM_D2")))static volatile uint8_t       RxCplt,TxCplt;
__attribute__((section (".RAM_D2")))static volatile bool          sd_initialized;

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

/**
  * @brief Rx Transfer completed callbacks
  * @param hsd: SD handle
  * @retval None
  */
void HAL_SD_RxCpltCallback(SD_HandleTypeDef *hsd)
{
  RxCplt=1;
}

/**
  * @brief Tx Transfer completed callbacks
  * @param hsd: SD handle
  * @retval None
  */
void HAL_SD_TxCpltCallback(SD_HandleTypeDef *hsd)
{
  TxCplt=1;
}

/**
  * @brief SD error callbacks
  * @param hsd: SD handle
  * @retval None
  */
void HAL_SD_ErrorCallback(SD_HandleTypeDef *hsd)
{
  Error_Handler(8);
}

static void sdio_sd_card_borad_init(void)
{
	 /* DeInit GPIO pins can be done in the application 
  (by surcharging this __weak function) */
	memset(&SDHandle,0,sizeof(SD_HandleTypeDef));
	memset(&pCardInfo,0,sizeof(HAL_SD_CardInfoTypeDef));
	RxCplt=0;
	TxCplt=0;
  
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

  /* NVIC configuration for SDIO interrupts */
  HAL_NVIC_SetPriority(SDMMC1_IRQn, 8, 0);
  HAL_NVIC_EnableIRQ(SDMMC1_IRQn);
}

void sdio_sd_card_reset(void)
{
    HAL_SD_DeInit(&SDHandle);
    __HAL_RCC_SDMMC1_FORCE_RESET();
    for(volatile int i=0;i<100;i++);
    __HAL_RCC_SDMMC1_RELEASE_RESET();
    memset(&SDHandle,0,sizeof(SD_HandleTypeDef));
    memset(&pCardInfo,0,sizeof(HAL_SD_CardInfoTypeDef));
    RxCplt=0;
    TxCplt=0;
	  sd_initialized=false;
}

void sdio_sd_card_init(void)
{
	sdio_sd_card_borad_init();
	
  SDHandle.Instance = SDMMC1;
  HAL_SD_DeInit(&SDHandle);
    
  /* if CLKDIV = 0 then SDMMC Clock frequency = SDMMC Kernel Clock
     else SDMMC Clock frequency = SDMMC Kernel Clock / [2 * CLKDIV]. 
     SDMMC Kernel Clock = 200MHz, SDMMC Clock frequency = 50MHz  */
  SDHandle.Init.ClockEdge           = SDMMC_CLOCK_EDGE_FALLING;
  SDHandle.Init.ClockPowerSave      = SDMMC_CLOCK_POWER_SAVE_DISABLE;
  SDHandle.Init.BusWide             = SDMMC_BUS_WIDE_4B;
  SDHandle.Init.HardwareFlowControl = SDMMC_HARDWARE_FLOW_CONTROL_DISABLE;
  SDHandle.Init.ClockDiv            = 2;
  
  if(HAL_SD_Init(&SDHandle) != HAL_OK)
  {
    Error_Handler(1);
  }
	
	if(Wait_SDCARD_Ready() != HAL_OK)
  {
    Error_Handler(2);
  }
	
	if(HAL_SD_GetCardInfo(&SDHandle,&pCardInfo) != HAL_OK)
  {
    Error_Handler(3);
  }
	
	sd_initialized=true;
}

bool sdio_sd_card_write(uint32_t start_block_num,const uint8_t* buf,uint32_t block_num)
{
	uint32_t loop = SD_TIMEOUT;
	TxCplt = 0;
	if(Wait_SDCARD_Ready() != HAL_OK)
	{
		Error_Handler(4);
		return false;
	}
	if(HAL_SD_WriteBlocks_DMA(&SDHandle, buf, start_block_num, block_num) != HAL_OK)
	{
		Error_Handler(5);
		return false;
	}
  while(loop > 0)
  {
    loop--;
    if(TxCplt) return true;
  }
  return false;
}

bool sdio_sd_card_read(uint32_t start_block_num,uint8_t* buf,uint32_t block_num)
{
	uint32_t loop = SD_TIMEOUT;
	if(Wait_SDCARD_Ready() != HAL_OK)
	{
		Error_Handler(6);
		return false;
	}
	RxCplt = 0;
	if(HAL_SD_ReadBlocks_DMA(&SDHandle, buf, start_block_num, block_num) != HAL_OK)
	{
		Error_Handler(7);
		return false;
	}
	while(loop > 0)
  {
    loop--;
    if(RxCplt) return true;
  }
	return false;
}

HAL_SD_CardInfoTypeDef sdio_sd_card_info(void)
{
	return pCardInfo;
}

/* ---------------- SD 卡热插拔状态机，由于我没有CD热插拔引脚所以只能通过软件来实现热插拔 ---------------- */
#define SD_SCAN_DEBOUNCE_CNT   5      /* 连续 5 次(即500ms)检测到同状态才确认变化 */

__attribute__((section (".RAM_D2")))static sdio_sd_event_cb_t s_sd_event_cb;
__attribute__((section (".RAM_D2")))static bool               s_sd_present;   /* 当前确认的在位状态 */
__attribute__((section (".RAM_D2")))static bool               s_sd_last_raw;  /* 上一次原始检测结果 */
__attribute__((section (".RAM_D2")))static uint8_t            s_sd_debounce;  /* 去抖计数 */

/* 注册回调 */
void sdio_sd_card_register_cb(sdio_sd_event_cb_t cb)
{
	  s_sd_present=false;
	  s_sd_last_raw=false;
	  s_sd_debounce=0;
    s_sd_event_cb=cb;
}

/* 非阻塞检测卡是否在位：发 CMD13，短超时 */
static bool sdio_sd_card_check_present(void)
{
	  if(!sd_initialized)                                      return false;    
    if(HAL_SD_CARD_TRANSFER==HAL_SD_GetCardState(&SDHandle)) return true;
    return false;
}

/* 100ms 周期调用：非阻塞状态机 + 事件回调 */
void sdio_sd_card_prc_100ms(void)
{
	  if(!sd_initialized)
    {
        sdio_sd_card_init();
        return;
    }
	  
    bool raw = sdio_sd_card_check_present();

    /* 原始状态和上次不同，重置去抖计数 */
    if (raw != s_sd_last_raw)
    {
        s_sd_last_raw = raw;
        s_sd_debounce = 0;
        return;   /* 等下一次扫描再确认 */
    }

    /* 原始状态稳定，累计去抖 */
    if (++s_sd_debounce < SD_SCAN_DEBOUNCE_CNT) return;

    /* 去抖完成，判断是否真的变化 */
    if (raw == s_sd_present) return;   /* 状态没变 */
        
    /* 状态确认变化，更新并触发事件 */
    s_sd_present = raw;

    if (s_sd_event_cb) s_sd_event_cb(raw ? SDIO_SD_EVENT_INSERT : SDIO_SD_EVENT_REMOVE);
}


/**
  * @brief  This function handles SD interrupt request.
  * @param  None
  * @retval None
  */
void SDMMC1_IRQHandler(void)
{
  HAL_SD_IRQHandler(&SDHandle);
}
