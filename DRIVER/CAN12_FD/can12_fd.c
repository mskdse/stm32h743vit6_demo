#include "can12_fd.h"

__attribute__((section (".RAM_D2")))static FDCAN_HandleTypeDef   hfdcan1;
__attribute__((section (".RAM_D2")))static FDCAN_HandleTypeDef   hfdcan2;

__attribute__((section (".RAM_D2")))static FDCAN_TxHeaderTypeDef FCAN1TxHeader_ST;
__attribute__((section (".RAM_D2")))static FDCAN_TxHeaderTypeDef FCAN1TxHeader_EXT;

__attribute__((section (".RAM_D2")))static FDCAN_TxHeaderTypeDef FCAN2TxHeader_ST;
__attribute__((section (".RAM_D2")))static FDCAN_TxHeaderTypeDef FCAN2TxHeader_EXT;

__attribute__((section (".RAM_D2")))static FDCAN_FIFO_TYPE       FACAN1_RX_FIFO;
__attribute__((section (".RAM_D2")))static FDCAN_FIFO_TYPE       FACAN2_RX_FIFO;

/* 初始化CAN1FD的接收筛选器,个数由句柄初始化决定，我配置的是4条标准ID，3条扩展ID */
static void can12_fd_init_filter(FDCAN_HandleTypeDef hfdcan12)
{
   /* 配置标准ID筛选器1，工作在范围模式，筛选出0x100~0x109范围内的报文 */
   FDCAN_FilterTypeDef sFilterConfig;
   sFilterConfig.FilterConfig=FDCAN_FILTER_TO_RXFIFO0;//命中之后放入RXFIFO0,不指定优先级
   sFilterConfig.FilterID1=0x100;
   sFilterConfig.FilterID2=0x109;
   sFilterConfig.FilterIndex=0;//第一个筛选器在缓冲区的索引为0
   sFilterConfig.FilterType=FDCAN_FILTER_RANGE;//筛选器工作在范围模式
   sFilterConfig.IdType=FDCAN_STANDARD_ID;//标准ID
   sFilterConfig.IsCalibrationMsg=0;//就给0就行，表示该消息是正常消息而不是丢弃该消息然后校准时间
   sFilterConfig.RxBufferIndex=0;//不用管就给0，我没有给它分配内存
   HAL_FDCAN_ConfigFilter(&hfdcan12,&sFilterConfig);

   /* 配置标准ID筛选器2，工作在双ID过滤模式，筛选出0x200或者0x205的报文 */
   sFilterConfig.FilterConfig=FDCAN_FILTER_TO_RXFIFO0_HP;//命中之后放入RXFIFO0并且给高优先级
   sFilterConfig.FilterID1=0x200;
   sFilterConfig.FilterID2=0x205;
   sFilterConfig.FilterIndex=1;//第二个筛选器在缓冲区的索引为1
   sFilterConfig.FilterType=FDCAN_FILTER_DUAL;//筛选器工作在双ID过滤模式
   sFilterConfig.IdType=FDCAN_STANDARD_ID;
   sFilterConfig.IsCalibrationMsg=0;
   sFilterConfig.RxBufferIndex=0;
   HAL_FDCAN_ConfigFilter(&hfdcan12,&sFilterConfig);
	 
	 /* 配置标准ID筛选器3，工作在掩码过滤模式，可以筛选出0x2fx(x的十进制范围为8-15)的报文 */
   sFilterConfig.FilterConfig=FDCAN_FILTER_TO_RXFIFO0;//命中之后放入RXFIFO,不指定优先级
   sFilterConfig.FilterID1=0x2f8;
   sFilterConfig.FilterID2=0x2f8;
   sFilterConfig.FilterIndex=2;//第三个筛选器在缓冲区的索引为2
   sFilterConfig.FilterType=FDCAN_FILTER_MASK;//筛选器工作在掩码过滤模式
   sFilterConfig.IdType=FDCAN_STANDARD_ID;
   sFilterConfig.IsCalibrationMsg=0;
   sFilterConfig.RxBufferIndex=0;
   HAL_FDCAN_ConfigFilter(&hfdcan12,&sFilterConfig);
	 
	 /* 配置标准ID筛选器4，工作在非扩展ID范围模式，可以筛选出0x300-0x309的报文 */
   sFilterConfig.FilterConfig=FDCAN_FILTER_TO_RXFIFO0;//命中之后放入RXFIFO,不指定优先级
   sFilterConfig.FilterID1=0x300;
   sFilterConfig.FilterID2=0x309;
   sFilterConfig.FilterIndex=3;//第四个筛选器在缓冲区的索引为3
   sFilterConfig.FilterType=FDCAN_FILTER_RANGE_NO_EIDM;//筛选器工作在非扩展ID范围模式，其实就是第一种范围模式，它会关闭扩展ID
   sFilterConfig.IdType=FDCAN_STANDARD_ID;
   sFilterConfig.IsCalibrationMsg=0;
   sFilterConfig.RxBufferIndex=0;
   HAL_FDCAN_ConfigFilter(&hfdcan12,&sFilterConfig);
	 
	 /* 配置扩展ID筛选器1，工作在范围模式，筛选出0x1000000~0x1000009范围内的报文 */
   sFilterConfig.FilterConfig=FDCAN_FILTER_TO_RXFIFO1;//命中之后放入RXFIFO1,不指定优先级
   sFilterConfig.FilterID1=0x1000000;
   sFilterConfig.FilterID2=0x1000009;
   sFilterConfig.FilterIndex=0;//第一个筛选器在缓冲区的索引为0
   sFilterConfig.FilterType=FDCAN_FILTER_RANGE;//筛选器工作在范围模式
   sFilterConfig.IdType=FDCAN_EXTENDED_ID;//扩展ID
   sFilterConfig.IsCalibrationMsg=0;//就给0就行，表示该消息是正常消息而不是丢弃该消息然后校准时间
   sFilterConfig.RxBufferIndex=0;//不用管就给0，我没有给它分配内存
   HAL_FDCAN_ConfigFilter(&hfdcan12,&sFilterConfig);

   /* 配置扩展ID筛选器2，工作在双ID过滤模式，筛选出0x2000000或者0x2000005的报文 */
   sFilterConfig.FilterConfig=FDCAN_FILTER_TO_RXFIFO1_HP;//命中之后放入RXFIFO1并且给高优先级
   sFilterConfig.FilterID1=0x2000000;
   sFilterConfig.FilterID2=0x2000005;
   sFilterConfig.FilterIndex=1;//第二个筛选器在缓冲区的索引为1
   sFilterConfig.FilterType=FDCAN_FILTER_DUAL;//筛选器工作在双ID过滤模式
   sFilterConfig.IdType=FDCAN_EXTENDED_ID;
   sFilterConfig.IsCalibrationMsg=0;
   sFilterConfig.RxBufferIndex=0;
   HAL_FDCAN_ConfigFilter(&hfdcan12,&sFilterConfig);
	 
	 /* 配置扩展ID筛选器3，工作在掩码过滤模式，可以筛选出0x2f0000x(x的十进制为8-15)的报文 */
   sFilterConfig.FilterConfig=FDCAN_FILTER_TO_RXFIFO1;//命中之后放入RXFIF1,不指定优先级
   sFilterConfig.FilterID1=0x2f00008;
   sFilterConfig.FilterID2=0x2f00008;
   sFilterConfig.FilterIndex=2;//第三个筛选器在缓冲区的索引为2
   sFilterConfig.FilterType=FDCAN_FILTER_MASK;//筛选器工作在掩码过滤模式
   sFilterConfig.IdType=FDCAN_EXTENDED_ID;
   sFilterConfig.IsCalibrationMsg=0;
   sFilterConfig.RxBufferIndex=0;
   HAL_FDCAN_ConfigFilter(&hfdcan12,&sFilterConfig);
	 
#if   USE_CAN_FD_FOMAT
	 /* 配置全局过滤器，拒绝非匹配筛选器的标准帧和扩展帧。拒绝标准和扩展的遥控帧，因为CANFD没有这个机制 */
	 HAL_FDCAN_ConfigGlobalFilter(&hfdcan12, FDCAN_REJECT, FDCAN_REJECT, FDCAN_REJECT_REMOTE, FDCAN_REJECT_REMOTE);
#elif USE_STD_CAN_FOMAT
   /* 配置全局过滤器，拒绝非匹配筛选器的标准帧和扩展帧。允许标准和扩展的遥控帧，因为CAN2.0标准有这个机制 */
	 HAL_FDCAN_ConfigGlobalFilter(&hfdcan12, FDCAN_REJECT, FDCAN_REJECT, FDCAN_FILTER_REMOTE, FDCAN_FILTER_REMOTE);
#endif
}

/* 初始化CAN12FD */
void can12_fd_init(void)
{
    /* 摘抄SDK中的时钟和GPIO的初始化流程 */
    GPIO_InitTypeDef         GPIO_InitStruct;
    RCC_PeriphCLKInitTypeDef RCC_PeriphClkInit;

    __HAL_RCC_FDCAN_FORCE_RESET();
    __HAL_RCC_FDCAN_RELEASE_RESET();
     /* 官方默认的是用锁相环PLL1_Q作为FDCAN的时钟源，这里会引发歧义
        寄存器查看，锁相环Q的时钟是200MHZ，而并非我们正常认知是用APB1总线
	      然后时钟频率是100MHZ。
  	*/
    __HAL_RCC_GPIOB_CLK_ENABLE();
    RCC_PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_FDCAN;
    RCC_PeriphClkInit.FdcanClockSelection = RCC_FDCANCLKSOURCE_PLL;
    HAL_RCCEx_PeriphCLKConfig(&RCC_PeriphClkInit);
    __HAL_RCC_FDCAN_CLK_ENABLE();

    GPIO_InitStruct.Pin       = GPIO_PIN_9;
    GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull      = GPIO_PULLUP;
    GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF9_FDCAN1;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
    GPIO_InitStruct.Pin       = GPIO_PIN_8;
    GPIO_InitStruct.Alternate = GPIO_AF9_FDCAN1;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    HAL_NVIC_SetPriority(FDCAN1_IT0_IRQn, 0, 1);
    HAL_NVIC_SetPriority(FDCAN1_IT1_IRQn, 0, 1);
    HAL_NVIC_SetPriority(FDCAN_CAL_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(FDCAN1_IT0_IRQn);
    HAL_NVIC_EnableIRQ(FDCAN1_IT1_IRQn);
    HAL_NVIC_EnableIRQ(FDCAN_CAL_IRQn);

    /* 完成CANFD1的初始化工作，不使用buffer因为HAL库没更新解析函数，只使用常规的FIFO */
    memset(&hfdcan1,0,sizeof(FDCAN_HandleTypeDef));
	memset(&hfdcan2,0,sizeof(FDCAN_HandleTypeDef));
	memset(&FCAN1TxHeader_ST,0,sizeof(FDCAN_TxHeaderTypeDef));
	memset(&FCAN1TxHeader_EXT,0,sizeof(FDCAN_TxHeaderTypeDef));
	memset(&FCAN2TxHeader_ST,0,sizeof(FDCAN_TxHeaderTypeDef));
	memset(&FCAN2TxHeader_EXT,0,sizeof(FDCAN_TxHeaderTypeDef));
	memset(&FACAN1_RX_FIFO,0,sizeof(FDCAN_FIFO_TYPE));
	memset(&FACAN2_RX_FIFO,0,sizeof(FDCAN_FIFO_TYPE));

#if   USE_CAN_FD_FOMAT
    /* 设置仲裁阶段MBPS为500KBPS，公式为1/((1/25MHZ)*(1+seg1+seg2))BPS */
    hfdcan1.Init.NominalPrescaler=8;//仲裁阶段时钟8分频25MHZ，1tq=1/(25MHZ)s
    hfdcan1.Init.NominalSyncJumpWidth=10;
    hfdcan1.Init.NominalTimeSeg1=37;
    hfdcan1.Init.NominalTimeSeg2=12;
    
     /* 设置数据阶段MBPS为1MBPS */
    hfdcan1.Init.DataPrescaler=8;//数据阶段时钟8分频25MHZ，1tq=1/(25MHZ)s
    hfdcan1.Init.DataSyncJumpWidth=10;
    hfdcan1.Init.DataTimeSeg1=18;
    hfdcan1.Init.DataTimeSeg2=6;
#elif USE_STD_CAN_FOMAT
    /* CAN2.0标准格式不允许变速，我们必须统一给它500KBPS或者1MBPS，不得发生变速
		   而且采样点保持一致，这里我们指定1MBPS，采样点=(1+18)/(1+18+6)=76%，注意不要
  		超过它们各自的数据范围 */
    hfdcan1.Init.NominalPrescaler=8;
    hfdcan1.Init.NominalSyncJumpWidth=10;
    hfdcan1.Init.NominalTimeSeg1=18;
    hfdcan1.Init.NominalTimeSeg2=6;
//    hfdcan1.Init.DataPrescaler=;不用给，反正没有变速机制会自动与仲裁段一致
//    hfdcan1.Init.DataSyncJumpWidth=;
//    hfdcan1.Init.DataTimeSeg1=;
//    hfdcan1.Init.DataTimeSeg2=;
#endif

#if   USE_CAN_FD_FOMAT
    hfdcan1.Init.FrameFormat=FDCAN_FRAME_FD_BRS;//工作在CANFD模式并且有波特率变速，允许收发最大数据长度64字节的报文
		
		hfdcan1.Init.RxBufferSize=FDCAN_DATA_BYTES_8;//接收buffer可以接收数据长度为8的数据帧
    hfdcan1.Init.RxBuffersNbr=0;//不给他分配RAM，即我们不使用接收buffer

    hfdcan1.Init.RxFifo0ElmtSize=FDCAN_DATA_BYTES_64;//接收FIFO0可以接收数据长度为64的数据帧
    hfdcan1.Init.RxFifo0ElmtsNbr=10;//接收FIFO0可以缓存10条数据长度为64的数据帧

    hfdcan1.Init.RxFifo1ElmtSize=FDCAN_DATA_BYTES_64;//接收FIFO1可以接收数据长度为64的数据帧
    hfdcan1.Init.RxFifo1ElmtsNbr=10;//接收FIFO1可以缓存10条数据长度为64的数据帧
		
		hfdcan1.Init.TxElmtSize=FDCAN_DATA_BYTES_64;//允许发送64个字节长度的数据帧
#elif USE_STD_CAN_FOMAT
    hfdcan1.Init.FrameFormat=FDCAN_FRAME_CLASSIC;//工作在CAN2.0标准模式，只允许收发最大8字节数据长度的报文
		
		hfdcan1.Init.RxBufferSize=FDCAN_DATA_BYTES_8;
    hfdcan1.Init.RxBuffersNbr=0;

    hfdcan1.Init.RxFifo0ElmtSize=FDCAN_DATA_BYTES_8;
    hfdcan1.Init.RxFifo0ElmtsNbr=30;

    hfdcan1.Init.RxFifo1ElmtSize=FDCAN_DATA_BYTES_8;
    hfdcan1.Init.RxFifo1ElmtsNbr=30;
		
		hfdcan1.Init.TxElmtSize=FDCAN_DATA_BYTES_8;
#endif
    	
    hfdcan1.Instance=FDCAN1;
	hfdcan1.Init.Mode=FDCAN_MODE_EXTERNAL_LOOPBACK;//工作模式，一般就关注正常模式和内外环回测试就行
    hfdcan1.Init.MessageRAMOffset=0;//FDCAN1占用的10KB的共享RAM的偏移量为0从头使用，FDCAN2可以设置为一半的位置
    hfdcan1.Init.ProtocolException=ENABLE;//收到的报文协议异常则判定格式错误
    hfdcan1.Init.AutoRetransmission=ENABLE;//使能自动重传模式
    hfdcan1.Init.TransmitPause=ENABLE;//使能传输暂停机制，传输完成一帧之后让出一段时间，如果还是总线空闲则可以继续发送
		
    hfdcan1.Init.StdFiltersNbr=4;//标准ID的过滤器有4条
    hfdcan1.Init.ExtFiltersNbr=3;//扩展ID的过滤器有3条

    hfdcan1.Init.TxBuffersNbr=0;//发送缓冲区个数为0，不需要使用
    hfdcan1.Init.TxEventsNbr=0;//事件发送缓存个数为0不需要用
    hfdcan1.Init.TxFifoQueueElmtsNbr=30;//30个发送FIFO或者发送队列的长度，选队列还是FIFO由下面控制
    hfdcan1.Init.TxFifoQueueMode=FDCAN_TX_FIFO_OPERATION;//选择FIFO模式
	  HAL_FDCAN_Init(&hfdcan1);

    /* 初始化4条标准ID筛选器和3条扩展ID筛选器 */
    can1_fd_init_filter();
		
		/* 配置接收FIFO0和1的水位线中断，只要收到一个数据就产生中断 */
    HAL_FDCAN_ConfigFifoWatermark(&hfdcan1, FDCAN_CFG_RX_FIFO0, 1);
	  HAL_FDCAN_ConfigFifoWatermark(&hfdcan1, FDCAN_CFG_RX_FIFO1, 1);

    /* 使能水位线中断，第三个参数跟发送buffer索引相关的，不用管 */
    HAL_FDCAN_ActivateNotification(&hfdcan1, FDCAN_IT_RX_FIFO0_WATERMARK, 0);
		HAL_FDCAN_ActivateNotification(&hfdcan1, FDCAN_IT_RX_FIFO1_WATERMARK, 0);
		
#if   USE_CAN_FD_FOMAT
		pTxHeader_ST.BitRateSwitch=FDCAN_BRS_ON;//允许波特率变速
		pTxHeader_ST.ErrorStateIndicator=FDCAN_ESI_ACTIVE;//当前处于主动错误状态
		pTxHeader_ST.FDFormat=FDCAN_FD_CAN;//帧格式为FDCAN
		pTxHeader_ST.IdType=FDCAN_STANDARD_ID;
		pTxHeader_ST.TxEventFifoControl=FDCAN_NO_TX_EVENTS;//没有发送事件FIFO
		pTxHeader_ST.TxFrameType=FDCAN_DATA_FRAME;//数据帧，CANFD没有遥控帧
		
		pTxHeader_EXT.BitRateSwitch=FDCAN_BRS_ON;
		pTxHeader_EXT.ErrorStateIndicator=FDCAN_ESI_ACTIVE;
		pTxHeader_EXT.FDFormat=FDCAN_FD_CAN;
		pTxHeader_EXT.IdType=FDCAN_EXTENDED_ID;
		pTxHeader_EXT.TxEventFifoControl=FDCAN_NO_TX_EVENTS;
		pTxHeader_EXT.TxFrameType=FDCAN_DATA_FRAME;
#elif USE_STD_CAN_FOMAT
    pTxHeader_ST.BitRateSwitch=FDCAN_BRS_OFF;//不允许波特率变速，CAN2.0没有这个机制
		pTxHeader_ST.ErrorStateIndicator=FDCAN_ESI_ACTIVE;//当前处于主动错误状态,CAN2.0没有这个位，随便给
		pTxHeader_ST.FDFormat=FDCAN_CLASSIC_CAN;//帧格式为标准CAN
		pTxHeader_ST.IdType=FDCAN_STANDARD_ID;
		pTxHeader_ST.TxEventFifoControl=FDCAN_NO_TX_EVENTS;//没有发送事件FIFO
		pTxHeader_ST.TxFrameType=FDCAN_DATA_FRAME;//数据帧,CAN2.0标准允许有遥控帧，我们可以在函数中指定参数，这里先默认数据帧
		
		pTxHeader_EXT.BitRateSwitch=FDCAN_BRS_OFF;
		pTxHeader_EXT.ErrorStateIndicator=FDCAN_ESI_ACTIVE;
		pTxHeader_EXT.FDFormat=FDCAN_CLASSIC_CAN;
		pTxHeader_EXT.IdType=FDCAN_EXTENDED_ID;
		pTxHeader_EXT.TxEventFifoControl=FDCAN_NO_TX_EVENTS;
		pTxHeader_EXT.TxFrameType=FDCAN_DATA_FRAME;
#endif
		
		/* 开启FDCAN1/2 */
		HAL_FDCAN_Start(&hfdcan1);
}

/* 发送函数，区分为标准帧和扩展帧 */
void can12_fd_send_msg_std(FDCAN_GlobalTypeDef *CANIndex,uint16_t id,uint32_t dlc,uint8_t* pdata,uint8_t msgid,bool is_data_frame)
{
/* 如果是CANFD的话只允许发生数据帧，如果是CAN2.0标准的话可以根据参数指定是数
	 据帧还是遥控帧，并且CANFD可以自由指定长度，而CAN2.0协议必须限制最大长度 */
	if(FDCAN1==CANIndex)
	{
#if   USE_CAN_FD_FOMAT
        FCAN1TxHeader_ST.TxFrameType=FDCAN_DATA_FRAME;
	    FCAN1TxHeader_ST.DataLength=dlc;
	   (void)is_data_frame;
#elif USE_STD_CAN_FOMAT
		if(is_data_frame)         FCAN1TxHeader_ST.TxFrameType=FDCAN_DATA_FRAME;
		else                      FCAN1TxHeader_ST.TxFrameType=FDCAN_REMOTE_FRAME;
	    if(dlc>FDCAN_DLC_BYTES_8) FCAN1TxHeader_ST.DataLength=FDCAN_DLC_BYTES_8;
	    else                      FCAN1TxHeader_ST.DataLength=dlc;
#endif
		FCAN1TxHeader_ST.Identifier=id;
		FCAN1TxHeader_ST.MessageMarker=msgid;
		HAL_FDCAN_AddMessageToTxFifoQ(&hfdcan1,&FCAN1TxHeader_ST,pdata);
	}
	else if(FDCAN2==CANIndex)
	{
#if   USE_CAN_FD_FOMAT
        FCAN2TxHeader_ST.TxFrameType=FDCAN_DATA_FRAME;
	    FCAN2TxHeader_ST.DataLength=dlc;
	   (void)is_data_frame;
#elif USE_STD_CAN_FOMAT
		if(is_data_frame)         FCAN2TxHeader_ST.TxFrameType=FDCAN_DATA_FRAME;
		else                      FCAN2TxHeader_ST.TxFrameType=FDCAN_REMOTE_FRAME;
	    if(dlc>FDCAN_DLC_BYTES_8) FCAN2TxHeader_ST.DataLength=FDCAN_DLC_BYTES_8;
	    else                      FCAN2TxHeader_ST.DataLength=dlc;
#endif
		FCAN2TxHeader_ST.Identifier=id;
		FCAN2TxHeader_ST.MessageMarker=msgid;
		HAL_FDCAN_AddMessageToTxFifoQ(&hfdcan2,&FCAN2TxHeader_ST,pdata);
	}
}
void can12_fd_send_msg_ext(FDCAN_GlobalTypeDef *CANIndex,uint32_t id,uint32_t dlc,uint8_t* pdata,uint8_t msgid,bool is_data_frame)
{
    if(FDCAN1==CANIndex)
	{
#if   USE_CAN_FD_FOMAT
        FCAN1TxHeader_EXT.TxFrameType=FDCAN_DATA_FRAME;
	    FCAN1TxHeader_EXT.DataLength=dlc;
	   (void)is_data_frame;
#elif USE_STD_CAN_FOMAT
		if(is_data_frame)         FCAN1TxHeader_EXT.TxFrameType=FDCAN_DATA_FRAME;
		else                      FCAN1TxHeader_EXT.TxFrameType=FDCAN_REMOTE_FRAME;
	    if(dlc>FDCAN_DLC_BYTES_8) FCAN1TxHeader_EXT.DataLength=FDCAN_DLC_BYTES_8;
	    else                      FCAN1TxHeader_EXT.DataLength=dlc;
#endif
		FCAN1TxHeader_EXT.Identifier=id;
		FCAN1TxHeader_EXT.MessageMarker=msgid;
		HAL_FDCAN_AddMessageToTxFifoQ(&hfdcan1,&FCAN1TxHeader_EXT,pdata);
	}
	else if(FDCAN2==CANIndex)
	{
#if   USE_CAN_FD_FOMAT
        FCAN2TxHeader_EXT.TxFrameType=FDCAN_DATA_FRAME;
	    FCAN2TxHeader_EXT.DataLength=dlc;
	   (void)is_data_frame;
#elif USE_STD_CAN_FOMAT
		if(is_data_frame)         FCAN2TxHeader_EXT.TxFrameType=FDCAN_DATA_FRAME;
		else                      FCAN2TxHeader_EXT.TxFrameType=FDCAN_REMOTE_FRAME;
	    if(dlc>FDCAN_DLC_BYTES_8) FCAN2TxHeader_EXT.DataLength=FDCAN_DLC_BYTES_8;
	    else                      FCAN2TxHeader_EXT.DataLength=dlc;
#endif
		FCAN2TxHeader_EXT.Identifier=id;
		FCAN2TxHeader_EXT.MessageMarker=msgid;
		HAL_FDCAN_AddMessageToTxFifoQ(&hfdcan2,&FCAN2TxHeader_EXT,pdata);
	}
}

/* 接收函数，从队列中提取我们的数据 */
bool can12_fd_get_msg(FDCAN_GlobalTypeDef *CANIndex,RX_FIFO_TYPE* rmsg)
{
	if(FDCAN1==CANIndex)
	{
		if(FACAN1_RX_FIFO.read!=FACAN1_RX_FIFO.write)
		{	
			__disable_irq();
			memcpy(&rmsg->RxHeader,&FACAN1_RX_FIFO.fifo[FACAN1_RX_FIFO.read].RxHeader,sizeof(FDCAN_RxHeaderTypeDef));
			memcpy(rmsg->pdata,FACAN1_RX_FIFO.fifo[FACAN1_RX_FIFO.read].pdata,64);
			FACAN1_RX_FIFO.read=(FACAN1_RX_FIFO.read+1)&(FDCAN_FIFO_SIZE-1);
			__enable_irq();
			return true;
		}
	}
	else if(FDCAN2==CANIndex)
	{
		if(FACAN2_RX_FIFO.read!=FACAN2_RX_FIFO.write)
		{	
			__disable_irq();
			memcpy(&rmsg->RxHeader,&FACAN2_RX_FIFO.fifo[FACAN2_RX_FIFO.read].RxHeader,sizeof(FDCAN_RxHeaderTypeDef));
			memcpy(rmsg->pdata,FACAN2_RX_FIFO.fifo[FACAN2_RX_FIFO.read].pdata,64);
			FACAN2_RX_FIFO.read=(FACAN2_RX_FIFO.read+1)&(FDCAN_FIFO_SIZE-1);
			__enable_irq();
			return true;
		}
	}
	return false;
}

/* 接收消息回调，我们把它们接收到队列当中缓存起来 */
void HAL_FDCAN_RxFifo0Callback(FDCAN_HandleTypeDef *hfdcan, uint32_t RxFifo0ITs)
{
  if((RxFifo0ITs & FDCAN_IT_RX_FIFO0_WATERMARK) != RESET)
  {
		__disable_irq();
		if(FDCAN1==hfdcan->Instance)
		{
		    HAL_FDCAN_GetRxMessage(&hfdcan1,
								   FDCAN_RX_FIFO0,
								   &FACAN1_RX_FIFO.fifo[FACAN1_RX_FIFO.write].RxHeader,
								   FACAN1_RX_FIFO.fifo[FACAN1_RX_FIFO.write].pdata);
			FACAN1_RX_FIFO.write=(FACAN1_RX_FIFO.write+1)&(FDCAN_FIFO_SIZE-1);
		}
		else if(FDCAN2==hfdcan->Instance)
		{
			HAL_FDCAN_GetRxMessage(&hfdcan2,
								   FDCAN_RX_FIFO0,
								   &FACAN2_RX_FIFO.fifo[FACAN2_RX_FIFO.write].RxHeader,
								   FACAN2_RX_FIFO.fifo[FACAN2_RX_FIFO.write].pdata);
			FACAN2_RX_FIFO.write=(FACAN2_RX_FIFO.write+1)&(FDCAN_FIFO_SIZE-1);
		}
		__enable_irq();
  }
}
void HAL_FDCAN_RxFifo1Callback(FDCAN_HandleTypeDef *hfdcan, uint32_t RxFifo1ITs)
{
  if((RxFifo1ITs & FDCAN_IT_RX_FIFO1_WATERMARK) != RESET)
  {
	__disable_irq();
	if(FDCAN1==hfdcan->Instance)
	{
		HAL_FDCAN_GetRxMessage(&hfdcan1,
							   FDCAN_RX_FIFO1,
							   &FACAN1_RX_FIFO.fifo[FACAN1_RX_FIFO.write].RxHeader,
							   FACAN1_RX_FIFO.fifo[FACAN1_RX_FIFO.write].pdata);
		FACAN1_RX_FIFO.write=(FACAN1_RX_FIFO.write+1)&(FDCAN_FIFO_SIZE-1);
	}
	else if(FDCAN2==hfdcan->Instance)
	{
		HAL_FDCAN_GetRxMessage(&hfdcan2,
							   FDCAN_RX_FIFO1,
							   &FACAN2_RX_FIFO.fifo[FACAN2_RX_FIFO.write].RxHeader,
							   FACAN2_RX_FIFO.fifo[FACAN2_RX_FIFO.write].pdata);
		FACAN2_RX_FIFO.write=(FACAN2_RX_FIFO.write+1)&(FDCAN_FIFO_SIZE-1);
	}
	__enable_irq();
  }
}

/* 中断相关 */
void FDCAN1_IT0_IRQHandler(void)
{
  HAL_FDCAN_IRQHandler(&hfdcan1);
}
void FDCAN2_IT0_IRQHandler(void)
{
  HAL_FDCAN_IRQHandler(&hfdcan2);
}
void FDCAN1_IT1_IRQHandler(void)
{
  HAL_FDCAN_IRQHandler(&hfdcan1);
}
void FDCAN2_IT1_IRQHandler(void)
{
  HAL_FDCAN_IRQHandler(&hfdcan2);
}
void FDCAN_CAL_IRQHandler(void)
{
  HAL_FDCAN_IRQHandler(&hfdcan1);
  HAL_FDCAN_IRQHandler(&hfdcan2);
}
