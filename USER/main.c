#include "stm32h7xx.h"
#include "SEGGER_RTT.h"
#include "EventRecorder.h"
#include <stdio.h>
#include "drvp_led.h"
#include "sram_d2_malloc.h"
#include "drvp_key.h"
//#include "base_timer6.h"
//#include "pwm_timer2.h"
//#include "pwm_timer1.h"
//#include "pwm_in_timer5.h"
#include "drvp_eeprom.h"
#include "ff.h"
#include "qspi_flash.h"
//#include "drv_lptimer2.h"
#include "drvp_fmc_lcd.h"
//#include "lvgl.h"
//#include "lv_port_disp_template.h"
//#include "lv_demo_benchmark.h"
//#include "bdmamux_pwm.h"
#include "dma1mux_pwm.h"

static void SystemClock_Config(void);
static void Error_Handler(void);
static void MPU_Config(void);
static void CPU_CACHE_Enable(void);

/* 数组表格定义在 .RAM_RESET_VCTOR 段，这段空间用来存储中断向量表，不得再其他地方再定义这个节区 */
__attribute__((section(".RamVecTable")))static uint8_t RAM_VCTOR_TABLE[0x400];

/* 我现在的程序是运行在QSPI_FLASH还是普通FLASH，如果是QSPIFLASH的话，中断向量表必须拷贝0x9000000地址的 */
#define  MY_FLASH_IS_QSPI_FLASH      1

int main(void)
{
	/* 将中断向量表从FLASH的首地址拷贝到DTCM中然后设置中断向量表的偏移为DTCM首地址也就是数组地址 */
	__disable_irq();
	__DSB();
	__ISB();
#if MY_FLASH_IS_QSPI_FLASH
	uint32_t *SouceAddr = (uint32_t *)0x90000000;
#else
	uint32_t *SouceAddr = (uint32_t *)FLASH_BANK1_BASE;
#endif
	uint32_t *DestAddr  = (uint32_t *)RAM_VCTOR_TABLE;
	memcpy(DestAddr, SouceAddr, 0x400);
	SCB->VTOR = (uint32_t)RAM_VCTOR_TABLE;
	__DSB();
	__ISB();
	__enable_irq();
	
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
	drvp_fmc_lcd_init();//初始化lcd
	drvp_fmc_lcd_set_axis_scan(0,1,1,0,0);//设置LCD的坐标轴适配开发板以及显存扫描方向
//	lv_init();//LVGL初始化
//	lv_port_disp_init();//LVGL底层支持初始化
//	lv_demo_benchmark();//允许LVGL的测试Demo
  dma1mux_pwm_init(3);
	SEGGER_RTT_printf(0,"APP_TASK_RUN......\r\n");
	for(;;)
	{		
		uint16_t x=drvp_key_rfifo();
		if(x!=KEY_EVENT_ERROR && ((x&0xff)==E_EVENT_CLICK))
		{
			static uint8_t i=1;
			dma1mux_pwm_set_cnt(i);
			i++;
			if(i>=9) i=1;
		}
//    lv_task_handler();
//		HAL_Delay(10);
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
	
	/* 配置QSPI，使它cache性能最高只用于执行代码，如果需要运行其他的比如文件系统存储需要重新加个QSPI配置并改变cache属性 */
	MPU_InitStruct.Enable = MPU_REGION_ENABLE;
  MPU_InitStruct.BaseAddress = 0x90000000;//QSPI_BASE地址
  MPU_InitStruct.Size = MPU_REGION_SIZE_16MB;//W25Q128是16MB
  MPU_InitStruct.AccessPermission = MPU_REGION_FULL_ACCESS;
  MPU_InitStruct.IsBufferable = MPU_ACCESS_BUFFERABLE;
  MPU_InitStruct.IsCacheable = MPU_ACCESS_CACHEABLE;
  MPU_InitStruct.IsShareable = MPU_ACCESS_NOT_SHAREABLE;//用来执行代码，那它就是只读，不存在多总线访问，所以开启共享改变cache属性
  MPU_InitStruct.Number = MPU_REGION_NUMBER1;
  MPU_InitStruct.TypeExtField = MPU_TEX_LEVEL1;
  MPU_InitStruct.SubRegionDisable = 0x0;
  MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_ENABLE;//可以执行代码
  HAL_MPU_ConfigRegion(&MPU_InitStruct);
	
	/* 配置FMC，确保时序正确，必须将其配置为Device或者store模式，不允许执行代码，ST官方配置是Device模式 */
	MPU_InitStruct.Enable = MPU_REGION_ENABLE;
  MPU_InitStruct.BaseAddress = 0x60000000;//FMC的BANK1的第一片区域的地址
  MPU_InitStruct.Size = MPU_REGION_SIZE_64MB;//BANK1总共是4*64MB,我们只用了第一片区域64MB
  MPU_InitStruct.AccessPermission = MPU_REGION_FULL_ACCESS;
  MPU_InitStruct.IsBufferable = MPU_ACCESS_BUFFERABLE;
  MPU_InitStruct.IsCacheable = MPU_ACCESS_NOT_CACHEABLE;
  MPU_InitStruct.IsShareable = MPU_ACCESS_SHAREABLE;//Device模式强制共享，这个给什么都行
  MPU_InitStruct.Number = MPU_REGION_NUMBER2;
  MPU_InitStruct.TypeExtField = MPU_TEX_LEVEL0;
  MPU_InitStruct.SubRegionDisable = 0x0;
  MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_DISABLE;//不可以执行代码
  HAL_MPU_ConfigRegion(&MPU_InitStruct);
	
	/* 配置SRAM4为NoCache属性，这片区域给BDMA用其他的比如说USB,以太网这些不用这个因为路径太长了 */
	MPU_InitStruct.Enable           = MPU_REGION_ENABLE;
	MPU_InitStruct.BaseAddress      = 0x38000000;
	MPU_InitStruct.Size             = ARM_MPU_REGION_SIZE_64KB;
	MPU_InitStruct.AccessPermission = MPU_REGION_FULL_ACCESS;
	MPU_InitStruct.IsBufferable     = MPU_ACCESS_NOT_BUFFERABLE;
	MPU_InitStruct.IsCacheable      = MPU_ACCESS_NOT_CACHEABLE;
	MPU_InitStruct.IsShareable      = MPU_ACCESS_SHAREABLE;
	MPU_InitStruct.Number           = MPU_REGION_NUMBER3;
	MPU_InitStruct.TypeExtField     = MPU_TEX_LEVEL1;
	MPU_InitStruct.SubRegionDisable = 0x00;
	MPU_InitStruct.DisableExec      = MPU_INSTRUCTION_ACCESS_DISABLE;
	HAL_MPU_ConfigRegion(&MPU_InitStruct);

  /* 配置SRAM1+2为NoCache属性，这片区域给我们日常使用，就是把他当作普通SRAM对待 */
	MPU_InitStruct.Enable           = MPU_REGION_ENABLE;
	MPU_InitStruct.BaseAddress      = 0x30000000;
	MPU_InitStruct.Size             = ARM_MPU_REGION_SIZE_256KB;
	MPU_InitStruct.AccessPermission = MPU_REGION_FULL_ACCESS;
	MPU_InitStruct.IsBufferable     = MPU_ACCESS_NOT_BUFFERABLE;
	MPU_InitStruct.IsCacheable      = MPU_ACCESS_NOT_CACHEABLE;
	MPU_InitStruct.IsShareable      = MPU_ACCESS_SHAREABLE;
	MPU_InitStruct.Number           = MPU_REGION_NUMBER4;
	MPU_InitStruct.TypeExtField     = MPU_TEX_LEVEL1;
	MPU_InitStruct.SubRegionDisable = 0x00;
	MPU_InitStruct.DisableExec      = MPU_INSTRUCTION_ACCESS_DISABLE;
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
	
//	lv_tick_inc(1);
}
