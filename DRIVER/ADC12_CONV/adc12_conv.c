#include "adc12_conv.h"
#include <string.h>

__attribute__((section (".RAM_D2"))) static ADC_HandleTypeDef adc12_handle;
__attribute__((section (".RAM_D2"))) static TIM_HandleTypeDef tim6_handle;

__attribute__((section (".RAM_D1")))uint32_t           ADC12_CH_DATA[2][ADC12_CH_DATA_SIZE];
__attribute__((section (".RAM_D2")))volatile uint8_t   ADC12_USE_BUF_INDEX;//当前使用的是哪个buffer
__attribute__((section (".RAM_D2")))volatile uint8_t   ADC12_CONV_END_FLAG;//当前使用的是哪个buffer

static void adc12_conv_borad_init(void)
{
    GPIO_InitTypeDef          GPIO_InitStruct;
    static DMA_HandleTypeDef  hdma;
    
    // 清空结构体，防止随机值导致初始化异常
    memset(&adc12_handle, 0, sizeof(ADC_HandleTypeDef));
    memset(&GPIO_InitStruct, 0, sizeof(GPIO_InitTypeDef));
    memset(&hdma, 0, sizeof(DMA_HandleTypeDef));
	  memset(ADC12_CH_DATA[0], 0, sizeof(ADC12_CH_DATA[0]));
	  memset(ADC12_CH_DATA[1], 0, sizeof(ADC12_CH_DATA[1]));
	  memset(&tim6_handle,0,sizeof(TIM_HandleTypeDef));
	
	  ADC12_USE_BUF_INDEX=0;
	  ADC12_CONV_END_FLAG=0;
	
	  __HAL_RCC_TIM6_CLK_ENABLE();
		
    
    /*##-1- 复位 ADC 外设 ##################################################*/
    ADCx_FORCE_RESET();       // 软件强制复位 ADC1/ADC2
    ADCx_RELEASE_RESET();     // 释放复位
    
    /* 关闭 ADC 时钟 (自动复位 ADC 公共组内的所有 ADC 实例) */
    __HAL_RCC_ADC12_CLK_DISABLE();

    /*##-2- 关闭外设和 GPIO 时钟 ##########################################*/
    /* 反初始化 ADC 通道的 GPIO 引脚 */
    HAL_GPIO_DeInit(ADCx_CHANNEL4_GPIO_PORT, ADCx_CHANNEL4_PIN); // 关闭 ADC1 通道4 的引脚
    HAL_GPIO_DeInit(ADCx_CHANNEL8_GPIO_PORT, ADCx_CHANNEL8_PIN); // 关闭 ADC2 通道8 的引脚
    
    /*##-3- 使能外设和 GPIO 时钟 ###########################################*/
    /* 使能 GPIO 时钟 */
    ADCx_CHANNEL4_PIN_CLK_ENABLE(); // 打开 ADC1 通道4 对应引脚的时钟
    ADCx_CHANNEL8_PIN_CLK_ENABLE(); // 打开 ADC2 通道8 对应引脚的时钟
    
    /* 使能 ADC 外设时钟 */
    ADCx_CLK_ENABLE();

    /* 使能 DMA 时钟 */
    DMAx_CHANNELx_CLK_ENABLE();
  
    /*##- 4- 配置外设 GPIO ##########################################*/
    /* 配置 ADC1 通道 4 的引脚 */
    GPIO_InitStruct.Pin = ADCx_CHANNEL4_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;   // 模拟输入模式
    GPIO_InitStruct.Pull = GPIO_NOPULL;        // 不使用上下拉电阻，模拟输入必须悬空
    HAL_GPIO_Init(ADCx_CHANNEL4_GPIO_PORT, &GPIO_InitStruct);
    
    /* 配置 ADC2 通道 8 的引脚 */
    GPIO_InitStruct.Pin = ADCx_CHANNEL8_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;   // 模拟输入模式
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(ADCx_CHANNEL8_GPIO_PORT, &GPIO_InitStruct);
    
    /*##- 5- 配置 DMA ##########################################*/
    hdma.Instance = DMA2_Stream0;                            // 使用 DMA2 的 Stream 0
    hdma.Init.Direction = DMA_PERIPH_TO_MEMORY;              // 传输方向：外设(ADC)到内存
    hdma.Init.FIFOMode = DMA_FIFOMODE_ENABLE;                // 使能 FIFO 模式
    hdma.Init.FIFOThreshold = DMA_FIFO_THRESHOLD_FULL;       // FIFO 阈值设为满，即积攒满之后再进行突发传输
    hdma.Init.MemBurst = DMA_MBURST_SINGLE;                  // 内存突发传输：单次传输
    hdma.Init.MemDataAlignment = DMA_MDATAALIGN_WORD;        // 内存数据对齐：字对齐 (32位)，因为双重ADC共享数据寄存器是32位
    hdma.Init.MemInc = DMA_MINC_ENABLE;                      // 内存地址自增，每次传输后内存指针向后移动
    hdma.Init.Mode = DMA_NORMAL;                             // 单次模式 (注意：这里代码注释写了配合软件双缓冲，但配置的是NORMAL模式，与上方ADC的DMA循环模式不符，需按实际需求确认)
    hdma.Init.PeriphBurst = DMA_PBURST_SINGLE;               // 外设突发传输：单次传输
    hdma.Init.PeriphDataAlignment = DMA_PDATAALIGN_WORD;     // 外设数据对齐：字对齐 (32位)
    hdma.Init.PeriphInc = DMA_PINC_DISABLE;                  // 外设地址不自增，因为 ADC 数据寄存器是固定的
    hdma.Init.Priority = DMA_PRIORITY_VERY_HIGH;             // DMA 优先级：非常高
    hdma.Init.Request = DMA_REQUEST_ADC1;                    // DMA 请求源：ADC1 (双重模式下，由主 ADC1 发起 DMA 请求)
    HAL_DMA_Init(&hdma);
    
		__HAL_DMA_ENABLE_IT(&hdma, DMA_IT_TC);//开启全满中断
		
		/* 将 DMA 句柄与 ADC 句柄关联起来 */
    __HAL_LINKDMA(&adc12_handle, DMA_Handle, hdma);
    
    /* 配置 DMA 中断并开启 */
    HAL_NVIC_SetPriority(DMA2_Stream0_IRQn, 6, 0); // 设置 DMA2_Stream0 中断优先级为 7
    HAL_NVIC_EnableIRQ(DMA2_Stream0_IRQn);        // 使能 DMA2_Stream0 中断
}

void adc12_conv_init(void)
{
    adc12_conv_borad_init();
    
    /* ==================== 配置 ADC1 (主 ADC) ==================== */
    adc12_handle.Instance = ADC1;
    // ADC 时钟预分频：同步时钟 4 分频，来源是 200MHz 的总线时钟 (PCLK)
    adc12_handle.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
    // 连续转换模式：必须禁止，因为是用定时器TRGO信号驱动
    adc12_handle.Init.ContinuousConvMode = DISABLE;
    // 转换数据管理模式：DMA 单次模式，与DMA配置符合
    adc12_handle.Init.ConversionDataManagement = ADC_CONVERSIONDATA_DMA_ONESHOT;
    // 不连续转换模式：禁用 (即在规则序列中一次转换完所有通道)
    adc12_handle.Init.DiscontinuousConvMode = DISABLE;
    // 外部触发：使用 TIM6 的 TRGO 触发采样
    adc12_handle.Init.ExternalTrigConv = ADC_EXTERNALTRIG_T6_TRGO;
    // 外部触发边沿：上升沿触发一次采样
    adc12_handle.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_RISING;
    // 数据左移：无 (即不对转换结果进行移位)
    adc12_handle.Init.LeftBitShift = ADC_LEFTBITSHIFT_NONE;
    // 低功耗自动等待：禁止 (ADC 在空闲时不会自动进入低功耗状态)
    adc12_handle.Init.LowPowerAutoWait = DISABLE;
    // 规则序列转换数量：1 个 (双重 ADC 每个各转换 1 个通道，只要配置 1 即可)
    adc12_handle.Init.NbrOfConversion = 1;
    // 溢出处理：数据溢出时覆盖旧数据
    adc12_handle.Init.Overrun = ADC_OVR_DATA_OVERWRITTEN;
    // 过采样模式：禁止
    adc12_handle.Init.OversamplingMode = DISABLE;
    // 分辨率：16 位
    adc12_handle.Init.Resolution = ADC_RESOLUTION_16B;
    // 扫描模式：禁止 (每个 ADC 只转换一个通道，不需要扫描)
    adc12_handle.Init.ScanConvMode = ADC_SCAN_DISABLE;
    HAL_ADC_Init(&adc12_handle);
		
		/* 开启偏移校准 */
		HAL_ADCEx_Calibration_Start(&adc12_handle, ADC_CALIB_OFFSET, ADC_SINGLE_ENDED);
    
    /* ==================== 配置 ADC1 的通道 ==================== */
    ADC_ChannelConfTypeDef adc_ch_cfg;
    adc_ch_cfg.Channel = ADCx_CHANNEL4;               // 选择 ADC1 的通道 4
    adc_ch_cfg.Offset = 0;                            // 数据偏移：0 (不偏移)
    adc_ch_cfg.OffsetNumber = ADC_OFFSET_NONE;        // 偏移编号：无
    adc_ch_cfg.OffsetRightShift = DISABLE;            // 偏移右移：禁止
    adc_ch_cfg.OffsetSignedSaturation = DISABLE;      // 偏移有符号饱和：禁止
    adc_ch_cfg.Rank = ADC_REGULAR_RANK_1;             // 规则序列中排第 1 个 (即唯一的那个)
    adc_ch_cfg.SamplingTime = ADC_SAMPLETIME_1CYCLE_5; // 采样时间
    adc_ch_cfg.SingleDiff = ADC_SINGLE_ENDED;         // 单端输入模式 (非差分)
    HAL_ADC_ConfigChannel(&adc12_handle, &adc_ch_cfg);
    
    /* ==================== 配置 ADC2 (从 ADC) ==================== */
    adc12_handle.Instance = ADC2; // 切换实例为 ADC2，其余初始化参数沿用刚才 ADC1 的配置
    HAL_ADC_Init(&adc12_handle);
    
    /* 配置 ADC2 的通道 */
    adc_ch_cfg.Channel = ADCx_CHANNEL8;               // 选择 ADC2 的通道 8
    adc_ch_cfg.Offset = 0;                            // 数据偏移：0
    adc_ch_cfg.OffsetNumber = ADC_OFFSET_NONE;        // 偏移编号：无
    adc_ch_cfg.OffsetRightShift = DISABLE;            // 偏移右移：禁止
    adc_ch_cfg.OffsetSignedSaturation = DISABLE;      // 偏移有符号饱和：禁止
    adc_ch_cfg.Rank = ADC_REGULAR_RANK_1;             // 规则序列中排第 1 个
    adc_ch_cfg.SamplingTime = ADC_SAMPLETIME_1CYCLE_5; // 采样时间
    adc_ch_cfg.SingleDiff = ADC_SINGLE_ENDED;         // 单端输入模式
    HAL_ADC_ConfigChannel(&adc12_handle, &adc_ch_cfg);
    
    /* ==================== 配置双重 ADC 同步模式 ==================== */
    // 注意：如果在双重模式下使用 DMA，必须调用这个函数来指定哪个 ADC 是主设备，以及如何同步
    ADC_MultiModeTypeDef multimode = {0};
		adc12_handle.Instance = ADC1;
		multimode.Mode = ADC_DUALMODE_REGSIMULT;              // 规则同步模式：TIM6 触发时，ADC1 和 ADC2 同时采样
		multimode.DualModeData = ADC_DUALMODEDATAFORMAT_32_10_BITS; // 关键：把两个 16 位结果打包成一个 32 位字供 DMA 搬运
		multimode.TwoSamplingDelay = ADC_TWOSAMPLINGDELAY_1CYCLE;   // 两个 ADC 采样之间的延迟，1 个周期即可
		HAL_ADCEx_MultiModeConfigChannel(&adc12_handle, &multimode);
		
		adc12_handle.Instance = ADC2;
		HAL_ADC_Start(&adc12_handle);//先让ADC2就绪
		
		adc12_handle.Instance = ADC1;
		HAL_ADCEx_MultiModeStart_DMA(&adc12_handle,ADC12_CH_DATA[0],ADC12_CH_DATA_SIZE);
		
		/* 配置定时器的TRGO信号驱动ADC */
		tim6_handle.Instance=TIM6;//生成500KHZ的触发信号
		tim6_handle.Init.AutoReloadPreload=TIM_AUTORELOAD_PRELOAD_ENABLE;
		tim6_handle.Init.ClockDivision=TIM_CLOCKDIVISION_DIV1;
		tim6_handle.Init.CounterMode=TIM_COUNTERMODE_UP;
		tim6_handle.Init.Period=400-1;
		tim6_handle.Init.Prescaler=1-1;
		tim6_handle.Init.RepetitionCounter=0;
		HAL_TIM_Base_Init(&tim6_handle);
		
		TIM_MasterConfigTypeDef sMasterConfig;
		sMasterConfig.MasterOutputTrigger = TIM_TRGO_UPDATE;
		sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
		HAL_TIMEx_MasterConfigSynchronization(&tim6_handle, &sMasterConfig);

		HAL_TIM_Base_Start(&tim6_handle);
}

/* ==================== DMA 传输完成回调 ==================== */
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)
{
    if (hadc->Instance == ADC1)
    {
			ADC12_CONV_END_FLAG=1;
			
			if(!ADC12_USE_BUF_INDEX)
			{
				ADC12_USE_BUF_INDEX=1;
				HAL_ADCEx_MultiModeStart_DMA(hadc,ADC12_CH_DATA[1], ADC12_CH_DATA_SIZE);
			}
			else if(ADC12_USE_BUF_INDEX)
			{
				ADC12_USE_BUF_INDEX=0;
				HAL_ADCEx_MultiModeStart_DMA(hadc,ADC12_CH_DATA[0], ADC12_CH_DATA_SIZE);
			}
    }
}

/**
* @brief  处理 DMA2 Stream0 的中断请求
* @param  None
* @retval None
*/
void DMA2_Stream0_IRQHandler(void)
{
  // 调用 HAL 库的 DMA 中断处理函数，内部会回调相应的完成或错误回调
  HAL_DMA_IRQHandler(adc12_handle.DMA_Handle);
}
