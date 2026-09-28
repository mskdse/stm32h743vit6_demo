#include "ltdc_lcd.h"
#include <string.h>
/*
  使用RGB565格式因为引脚不足所以该板子最大支持RGB666这里为了节省内存开销选择RGB565格式。
  然后屏幕的R0-R4需要对应板子的LTDC_R3-R7，是这种对应关系所以引脚定义如下
  R0-PB0
   1-PA5
   2-PC0
   3-PB1
   4-PE15

  G0-PA6
   1-PE11
   2-PB10
   3-PB11
   4-PC7
   5-PD3

  B0-PD10
   1-PE12
   2-PA3
   3-PB8
   4-PB9

  CLK-PE14
  DE-PE13
  HSYNC-PC6
  VSYNC-PA4

  BL-PB7 TIM4_CH2_PWM调光通道
*/
__attribute__((section (".RAM_D2")))static LTDC_HandleTypeDef  LtdcHandle;
__attribute__((section (".RAM_D2")))static TIM_HandleTypeDef   bl_htim4;
__attribute__((section (".RAM_D2")))static TIM_OC_InitTypeDef  bl_pwm_Config;
__attribute__((section(".RAM_D1"), aligned(4)))static uint16_t LTDC_DISPLAY[(LCD_WIN_X1-LCD_WIN_X0)*(LCD_WIN_Y1-LCD_WIN_Y0)];

/**
  * @brief  This function is executed in case of error occurrence.
  * @param  None
  * @retval None
  */
static void Error_Handler(void)
{
    while(1)
    {
    }
}

/* 初始化所有的时钟和GPIO，摘抄SDK源码实例 */
void ltdc_lcd_borad_init(void)
{
  memset(&LtdcHandle,0,sizeof(LTDC_HandleTypeDef));
  memset(&bl_htim4,0,sizeof(TIM_HandleTypeDef));
  memset(&bl_pwm_Config,0,sizeof(TIM_OC_InitTypeDef));
	memset(LTDC_DISPLAY,0,sizeof(LTDC_DISPLAY));
	for(int i=0;i<(640*400);i++) LTDC_DISPLAY[i]=0x0000;

  /*##-1- Reset peripherals ##################################################*/
  /* Enable LTDC reset state */
  __HAL_RCC_LTDC_FORCE_RESET();
  
  /* Release LTDC from reset state */ 
  __HAL_RCC_LTDC_RELEASE_RESET();

  for(int i=0;i<0xFFF;i++) __nop();

  GPIO_InitTypeDef GPIO_Init_Structure;
  
  /*##-1- Enable peripherals and GPIO Clocks #################################*/  
  /* Enable the LTDC Clock */
  __HAL_RCC_LTDC_CLK_ENABLE();
  /* Enable GPIOs clock */
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOE_CLK_ENABLE();
  __HAL_RCC_TIM4_CLK_ENABLE();

  /*** LTDC Pins configuration ***/
  GPIO_Init_Structure.Pin       = GPIO_PIN_3 | GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_6; 
  GPIO_Init_Structure.Mode      = GPIO_MODE_AF_PP;
  GPIO_Init_Structure.Pull      = GPIO_PULLUP;
  GPIO_Init_Structure.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_Init_Structure.Alternate = GPIO_AF14_LTDC;  
  HAL_GPIO_Init(GPIOA, &GPIO_Init_Structure);

  GPIO_Init_Structure.Pin       = GPIO_PIN_0 | GPIO_PIN_1; 
  GPIO_Init_Structure.Alternate = GPIO_AF9_LTDC; 
  HAL_GPIO_Init(GPIOB, &GPIO_Init_Structure); 
  
  GPIO_Init_Structure.Pin       = GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 | GPIO_PIN_11; 
  GPIO_Init_Structure.Alternate = GPIO_AF14_LTDC; 
  HAL_GPIO_Init(GPIOB, &GPIO_Init_Structure);
 
  GPIO_Init_Structure.Pin       = GPIO_PIN_0 | GPIO_PIN_6 | GPIO_PIN_7; 
  GPIO_Init_Structure.Alternate = GPIO_AF14_LTDC;
  HAL_GPIO_Init(GPIOC, &GPIO_Init_Structure); 
  
  GPIO_Init_Structure.Pin       = GPIO_PIN_3 | GPIO_PIN_10; 
  GPIO_Init_Structure.Alternate = GPIO_AF14_LTDC;
  HAL_GPIO_Init(GPIOD, &GPIO_Init_Structure);

  GPIO_Init_Structure.Pin       = GPIO_PIN_11 | GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15; 
  GPIO_Init_Structure.Alternate = GPIO_AF14_LTDC;
  HAL_GPIO_Init(GPIOE, &GPIO_Init_Structure);

  GPIO_Init_Structure.Pin       = GPIO_PIN_7; 
  GPIO_Init_Structure.Alternate = GPIO_AF2_TIM4;
  HAL_GPIO_Init(GPIOB, &GPIO_Init_Structure);
  HAL_GPIO_WritePin(GPIOB,GPIO_PIN_7,GPIO_PIN_RESET);
  for(int i=0;i<0xFFF;i++) __nop();

  /* Set LTDC Interrupt to the lowest priority */
  HAL_NVIC_SetPriority(LTDC_IRQn, 0xF, 0);   

  /* Enable LTDC Interrupt */
  HAL_NVIC_EnableIRQ(LTDC_IRQn); 

    /*## LTDC Clock Configuration ###########################################*/  
  /* AMPIRE640480 typical PCLK is 25 MHz so the PLL3R is configured to provide this clock */ 
  /* AMPIRE640480 LCD clock configuration */
  /* PLL3_VCO Input = HSE_VALUE/PLL3M = 5 Mhz */
  /* PLL3_VCO Output = PLL3_VCO Input * PLL3N = 800 Mhz */
  /* PLLLCDCLK = PLL3_VCO Output/PLL3R = 800/80 = 10Mhz */
  /* LTDC clock frequency = PLLLCDCLK = 10 Mhz */    
  RCC_PeriphCLKInitTypeDef  PeriphClkInitStruct;
  PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_LTDC;
  PeriphClkInitStruct.PLL3.PLL3M = 5;    
  PeriphClkInitStruct.PLL3.PLL3N = 160;
  PeriphClkInitStruct.PLL3.PLL3FRACN = 0;
  PeriphClkInitStruct.PLL3.PLL3P = 2;
  PeriphClkInitStruct.PLL3.PLL3Q = 2;
  PeriphClkInitStruct.PLL3.PLL3R = 80;
  PeriphClkInitStruct.PLL3.PLL3VCOSEL = RCC_PLL3VCOWIDE;
  PeriphClkInitStruct.PLL3.PLL3RGE = RCC_PLL3VCIRANGE_2;
  if(HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct)!=HAL_OK) Error_Handler();

  /* 初始化PWM信号 */
  bl_htim4.Instance=TIM4;
  bl_htim4.Init.Prescaler=20-1;//0.1us计数一次
  bl_htim4.Init.CounterMode=TIM_COUNTERMODE_UP;
  bl_htim4.Init.Period=1-1;//ARR先给1
  bl_htim4.Init.ClockDivision=TIM_CLOCKDIVISION_DIV1;
  bl_htim4.Init.RepetitionCounter=0;
  bl_htim4.Init.AutoReloadPreload=TIM_AUTORELOAD_PRELOAD_ENABLE;
  HAL_TIM_PWM_Init(&bl_htim4);
  
  bl_pwm_Config.OCFastMode=TIM_OCFAST_DISABLE;
  bl_pwm_Config.OCIdleState=TIM_OCIDLESTATE_SET;
  bl_pwm_Config.OCMode=TIM_OCMODE_PWM1;
  bl_pwm_Config.OCPolarity=TIM_OCPOLARITY_HIGH;
  bl_pwm_Config.Pulse=0;//占空比0%
  HAL_TIM_PWM_ConfigChannel(&bl_htim4,&bl_pwm_Config,TIM_CHANNEL_2);

  HAL_TIM_PWM_Stop(&bl_htim4, TIM_CHANNEL_2);
}

/* LCD背光灯的频率和占空比设置,占空比范围0-100 */
void ltdc_lcd_bl_set(uint32_t freq,uint8_t paluse)
{
  HAL_TIM_PWM_Stop(&bl_htim4, TIM_CHANNEL_2);
  HAL_TIM_PWM_DeInit(&bl_htim4);

  bl_htim4.Init.Period=10000000/freq-1;// 1/freq*10000000
  HAL_TIM_PWM_Init(&bl_htim4);
  bl_pwm_Config.Pulse=(bl_htim4.Init.Period+1)*paluse/100;
  HAL_TIM_PWM_ConfigChannel(&bl_htim4,&bl_pwm_Config,TIM_CHANNEL_2);

  HAL_TIM_PWM_Start(&bl_htim4, TIM_CHANNEL_2);
}

/* ltdc初始化函数，初始化好所有的GPIO和时钟以及配置 */
void ltdc_lcd_init(void)
{
  ltdc_lcd_borad_init();
	
		/* 时序参数按照手册中填写，另外不同HSYNC/VSYNC极性下时序宽度是不一样的 */
  LtdcHandle.Instance=LTDC;
	LtdcHandle.Init.TotalHeigh=(VSYNC_LEN+VBP_LEN+LCD_HEIGH+VFP_LEN-1);
  LtdcHandle.Init.TotalWidth=(HSYNC_LEN+HBP_LEN+LCD_WIDTH+HFP_LEN-1);
  LtdcHandle.Init.AccumulatedActiveH=(VSYNC_LEN+VBP_LEN+LCD_HEIGH-1);
  LtdcHandle.Init.AccumulatedActiveW=(HSYNC_LEN+HBP_LEN+LCD_WIDTH-1);
  LtdcHandle.Init.AccumulatedHBP=(HSYNC_LEN+HBP_LEN-1);
  LtdcHandle.Init.AccumulatedVBP=(VSYNC_LEN+VBP_LEN-1);
	LtdcHandle.Init.HorizontalSync=(HSYNC_LEN-1);
	LtdcHandle.Init.VerticalSync=(VSYNC_LEN-1);
  LtdcHandle.Init.Backcolor.Red=0xFF;
  LtdcHandle.Init.Backcolor.Green=0xFF;
  LtdcHandle.Init.Backcolor.Blue=0xFF;
  LtdcHandle.Init.DEPolarity=LTDC_DEPOLARITY_AL;//手册中DE信号高电平有效,但是实测要低电平
  LtdcHandle.Init.HSPolarity=LTDC_HSPOLARITY_AL;//手册中HSYNC信号低电平有效
  LtdcHandle.Init.PCPolarity=LTDC_PCPOLARITY_IIPC;//手册中PCLK信号下降沿的时候传输数据，实测需要给IIPC，高电平的时候数据有效时序正确
  LtdcHandle.Init.VSPolarity=LTDC_VSPOLARITY_AL;//手册中VSYNC信号低电平有效
  if(HAL_LTDC_Init(&LtdcHandle)!=HAL_OK) Error_Handler(); 
	
	/* 图层1设置，该图层在背景层之上，在顶层之下 */
//	LTDC_LayerCfgTypeDef  ltdc_layer1_cfg;
//	memset(&ltdc_layer1_cfg,0,sizeof(LTDC_LayerCfgTypeDef));
//	ltdc_layer1_cfg.Alpha=0xFF;//常数alpha，0xFF/255=100%,也就是说当前层和它的下面一层的融合数据取它当前层(不考虑和alpha0相乘的情况下)
//	ltdc_layer1_cfg.Alpha0=0x00;//随便给，我们不使用ARGB格式所以该参数无效,该参数是默认alpha，假设我窗口没有完全覆盖下面的一层
//	                            //那我ARGB的数据必须指定一个默认ARGB的A的值作为alpha0默认参数，不然窗口以外图形融合就不确切
//	ltdc_layer1_cfg.Backcolor.Blue=0x00;
//	ltdc_layer1_cfg.Backcolor.Green=0x00;
//	ltdc_layer1_cfg.Backcolor.Red=0x00;
//	ltdc_layer1_cfg.BlendingFactor1=LTDC_BLENDING_FACTOR1_CA;//不使用ARGB的apha和常数alpha融合，所以选择这个
//	ltdc_layer1_cfg.BlendingFactor2=LTDC_BLENDING_FACTOR2_CA;//不使用ARGB的apha和常数alpha融合，所以选择这个
//	ltdc_layer1_cfg.FBStartAdress=(uint32_t)LTDC_DISPLAY;
//	ltdc_layer1_cfg.ImageHeight=(LCD_WIN_Y1-LCD_WIN_Y0);
//	ltdc_layer1_cfg.ImageWidth=(LCD_WIN_X1-LCD_WIN_X0);
//	ltdc_layer1_cfg.PixelFormat=LTDC_PIXEL_FORMAT_RGB565;//使用RGB565格式
//	ltdc_layer1_cfg.WindowX0=LCD_WIN_X0;
//	ltdc_layer1_cfg.WindowX1=LCD_WIN_X1;
//	ltdc_layer1_cfg.WindowY0=LCD_WIN_Y0;
//	ltdc_layer1_cfg.WindowY1=LCD_WIN_Y1;
//	if(HAL_LTDC_ConfigLayer(&LtdcHandle,&ltdc_layer1_cfg,LTDC_LAYER_1)!=HAL_OK)        Error_Handler();
}

/**
  * @brief  This function handles LTDC global interrupt request.
  * @param  None
  * @retval None
  */
void LTDC_IRQHandler(void)
{
  HAL_LTDC_IRQHandler(&LtdcHandle);
}
