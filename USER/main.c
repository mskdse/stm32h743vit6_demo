#include "stm32h7xx.h"
#include "SEGGER_RTT.h"
#include "EventRecorder.h"
#include <stdio.h>
#include "drvp_led.h"
#include "sram_d2_malloc.h"
#include "drvp_key.h"
//#include "base_timer6.h"
#include "pwm_timer2.h"
#include "pwm_timer1.h"
#include "pwm_in_timer5.h"
#include "drvp_eeprom.h"
#include "ff.h"

static void SystemClock_Config(void);
static void Error_Handler(void);
static void MPU_Config(void);
static void CPU_CACHE_Enable(void);
static void FATFS_Init(uint8_t dev,bool ismount);

/* 数组表格定义在 .RAM_RESET_VCTOR 段，这段空间用来存储中断向量表，不得再其他地方再定义这个节区 */
__attribute__((section(".RamVecTable")))static uint8_t RAM_VCTOR_TABLE[0x400];

int main(void)
{
	/* 将中断向量表从FLASH的首地址拷贝到DTCM中然后设置中断向量表的偏移为DTCM首地址也就是数组地址 */
	uint32_t *SouceAddr = (uint32_t *)FLASH_BANK1_BASE;
	uint32_t *DestAddr  = (uint32_t *)RAM_VCTOR_TABLE;
	memcpy(DestAddr, SouceAddr, 0x400);
	SCB->VTOR = (uint32_t)RAM_VCTOR_TABLE;
	
	SEGGER_RTT_Init();//初始化RTT调试
	EventRecorderInitialize(EventRecordAll, 1U);//初始化EventRecoder组件
  EventRecorderStart();//组件Start运行
	
  MPU_Config();//配置MPU内存保护单元
	CPU_CACHE_Enable();//开启Cache
	HAL_Init();//HAL库的初始化
	SystemClock_Config();//配置系统时钟为400MHZ
	
	sram_d2_init();//初始化D2域的SRAM2的最后的20KB的内存管理，用于我们动态使用	
	drvp_led_init();//初始化led
  drvp_key_init();//初始化key
	drvp_eeprom_init();//初始化eeprom
	
	FATFS_Init(0,1);//初始化文件系统
	for(;;)
	{
		uint16_t evt=drvp_key_rfifo();
		if(evt!=KEY_EVENT_ERROR) SEGGER_RTT_printf(0,"key=%d,evt=%d\r\n",(evt>>8)&0xff,evt&0xff);
	}
}

/**
  * @brief  System Clock Configuration
  *         The system Clock is configured as follow : 
  *            System Clock source            = PLL (HSE)
  *            SYSCLK(Hz)                     = 400000000 (CPU Clock)
  *            HCLK(Hz)                       = 200000000 (AXI and AHBs Clock)
  *            AHB Prescaler                  = 2
  *            D1 APB3 Prescaler              = 2 (APB3 Clock  100MHz)
  *            D2 APB1 Prescaler              = 2 (APB1 Clock  100MHz)
  *            D2 APB2 Prescaler              = 2 (APB2 Clock  100MHz)
  *            D3 APB4 Prescaler              = 2 (APB4 Clock  100MHz)
  *            HSE Frequency(Hz)              = 25000000
  *            PLL_M                          = 5
  *            PLL_N                          = 160
  *            PLL_P                          = 2
  *            PLL_Q                          = 4
  *            PLL_R                          = 2
  *            VDD(V)                         = 3.3
  *            Flash Latency(WS)              = 4
  * @param  None
  * @retval None
  */
static void SystemClock_Config(void)
{
  RCC_ClkInitTypeDef RCC_ClkInitStruct;
  RCC_OscInitTypeDef RCC_OscInitStruct;
  HAL_StatusTypeDef ret = HAL_OK;
	
	/* 官方的DEMO里面少的一行代码 */
	MODIFY_REG(PWR->CR3,PWR_CR3_SCUEN,0);

  /* The voltage scaling allows optimizing the power consumption when the device is
     clocked below the maximum system frequency, to update the voltage scaling value
     regarding system frequency refer to product datasheet.  */
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  while(!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}
  
  /* Enable HSE Oscillator and activate PLL with HSE as source */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSIState = RCC_HSI_OFF;
  RCC_OscInitStruct.CSIState = RCC_CSI_OFF;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;

  RCC_OscInitStruct.PLL.PLLM = 5;
  RCC_OscInitStruct.PLL.PLLN = 160;
  RCC_OscInitStruct.PLL.PLLFRACN = 0;
  RCC_OscInitStruct.PLL.PLLP = 2;
  RCC_OscInitStruct.PLL.PLLR = 2;
  RCC_OscInitStruct.PLL.PLLQ = 4;

  RCC_OscInitStruct.PLL.PLLVCOSEL = RCC_PLL1VCOWIDE;
  RCC_OscInitStruct.PLL.PLLRGE = RCC_PLL1VCIRANGE_2;
  ret = HAL_RCC_OscConfig(&RCC_OscInitStruct);
  if(ret != HAL_OK)
  {
    Error_Handler();
  }
  
/* Select PLL as system clock source and configure  bus clocks dividers */
  RCC_ClkInitStruct.ClockType = (RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_D1PCLK1 | RCC_CLOCKTYPE_PCLK1 | \
                                 RCC_CLOCKTYPE_PCLK2  | RCC_CLOCKTYPE_D3PCLK1);

  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.SYSCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB3CLKDivider = RCC_APB3_DIV2;  
  RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV2; 
  RCC_ClkInitStruct.APB2CLKDivider = RCC_APB2_DIV2; 
  RCC_ClkInitStruct.APB4CLKDivider = RCC_APB4_DIV2; 
  ret = HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4);
  if(ret != HAL_OK)
  {
    Error_Handler();
  }
  
  /*
  Note : The activation of the I/O Compensation Cell is recommended with communication  interfaces
          (GPIO, SPI, FMC, QSPI ...)  when  operating at  high frequencies(please refer to product datasheet)       
          The I/O Compensation Cell activation  procedure requires :
        - The activation of the CSI clock
        - The activation of the SYSCFG clock
        - Enabling the I/O Compensation Cell : setting bit[0] of register SYSCFG_CCCSR
  
          To do this please uncomment the following code 
*/
  __HAL_RCC_CSI_ENABLE() ;
  
  __HAL_RCC_SYSCFG_CLK_ENABLE() ;
	
	/* AXI-SRAM和SRAM4这些在时钟树手册里面是隐式使能的，所以其他的SRAM1/2/3需要我们手动使能 */
	__HAL_RCC_D2SRAM1_CLK_ENABLE();
	__HAL_RCC_D2SRAM2_CLK_ENABLE();
	__HAL_RCC_D2SRAM3_CLK_ENABLE();
	
  HAL_EnableCompensationCell();
}

/**
  * @brief  This function is executed in case of error occurrence.
  * @param  None
  * @retval None
  */
static void Error_Handler(void)
{
  /* User may add here some code to deal with this error */
  while(1)
  {
  }
}

/**
  * @brief  Configure the MPU attributes as Write Through for External SDRAM.
  * @note   The Base Address is SDRAM_DEVICE_ADDR
  *         The Configured Region Size is 32MB because same as SDRAM size.
  * @param  None
  * @retval None
  */
static void MPU_Config(void)
{
  MPU_Region_InitTypeDef MPU_InitStruct;
  
  /* Disable the MPU */
  HAL_MPU_Disable();

  /* Configure the MPU as Strongly ordered for not defined regions */
  MPU_InitStruct.Enable = MPU_REGION_ENABLE;
  MPU_InitStruct.BaseAddress = 0x00;
  MPU_InitStruct.Size = MPU_REGION_SIZE_4GB;
  MPU_InitStruct.AccessPermission = MPU_REGION_NO_ACCESS;
  MPU_InitStruct.IsBufferable = MPU_ACCESS_NOT_BUFFERABLE;
  MPU_InitStruct.IsCacheable = MPU_ACCESS_NOT_CACHEABLE;
  MPU_InitStruct.IsShareable = MPU_ACCESS_SHAREABLE;
  MPU_InitStruct.Number = MPU_REGION_NUMBER0;
  MPU_InitStruct.TypeExtField = MPU_TEX_LEVEL0;
  MPU_InitStruct.SubRegionDisable = 0x87;
  MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_DISABLE;

  HAL_MPU_ConfigRegion(&MPU_InitStruct);

  /* Enable the MPU */
  HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);
}

/**
  * @brief  CPU L1-Cache enable.
  * @param  None
  * @retval None
  */
static void CPU_CACHE_Enable(void)
{
  /* Enable I-Cache */
  SCB_EnableICache();

  /* Enable D-Cache */
  SCB_EnableDCache();
}

/**
  * @brief  This function handles SysTick Handler.
  * @param  None
  * @retval None
  */
__attribute__((section(".ITCM_CODE"), used))void SysTick_Handler(void)
{
  static uint8_t keycnt=0;
  static uint8_t eepromcnt=0;
  
	HAL_IncTick();
	
  drvp_led_prc_1ms();
	
  if(++keycnt>=10)
  {
    keycnt=0;
    drvp_key_prc_10ms();
  }
	
	if(++eepromcnt>=10)
  {
    eepromcnt=0;
    drvp_eeprom_prc_10ms();
  }
}

/* 文件系统初始化函数 */
#define NOT_FILE_SYS     0  //一开始没有文件系统，可以打开宏定义开关来创建一下
__attribute__((section (".RAM_D2"))) FATFS   fs;
__attribute__((section (".RAM_D2"))) FIL     file;
__attribute__((section (".RAM_D2"))) FRESULT res;
static void FATFS_Init(uint8_t dev,bool ismount)
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
    res = f_mount(ismount ? &fs : NULL,volume,ismount ? 1 : 0);
    SEGGER_RTT_printf(0,"%s %s res: %d\r\n",ismount ? "f_mount" : "f_unmount",volume,res);
#endif
}
