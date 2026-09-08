#include "can1_fd.h"
#include <string.h>

__attribute__((section (".RAM_D2")))static FDCAN_HandleTypeDef hfdcan1;

/* 初始化CAN1FD的接收筛选器,个数由句柄初始化决定，我配置的是4条标准ID，4条扩展ID */
static can1_fd_init_filter(void)
{
   /* 配置标准ID筛选器1，工作在范围模式，筛选出0x100~0x109范围内的报文 */
   FDCAN_FilterTypeDef sFilterConfig;
   sFilterConfig.FilterConfig=FDCAN_FILTER_TO_RXFIFO0;//命中之后放入RXFIFO0
   sFilterConfig.FilterID1=0x100;
   sFilterConfig.FilterID2=0x109;
   sFilterConfig.FilterIndex=0;//第一个筛选器在缓冲区的索引为0
   sFilterConfig.FilterType=FDCAN_FILTER_RANGE;//筛选器工作在范围模式
   sFilterConfig.IdType=FDCAN_STANDARD_ID;//标准ID
   sFilterConfig.IsCalibrationMsg=0;//就给0就行，表示该消息是正常消息而不是丢弃该消息然后校准时间
   sFilterConfig.RxBufferIndex=0;//不用管就给0，我没有给它分配内存
   HAL_FDCAN_ConfigFilter(&hfdcan1,&sFilterConfig);

   /* 配置标准ID筛选器2，工作在双ID过滤模式，筛选出0x200或者0x205的报文 */
   sFilterConfig.FilterConfig=FDCAN_FILTER_TO_RXFIFO0_HP;//命中之后放入RXFIFO0并且给高优先级
   sFilterConfig.FilterID1=0x200;
   sFilterConfig.FilterID2=0x205;
   sFilterConfig.FilterIndex=1;//第一个筛选器在缓冲区的索引为1
   sFilterConfig.FilterType=FDCAN_FILTER_DUAL;//筛选器工作在双ID过滤模式
   sFilterConfig.IdType=FDCAN_STANDARD_ID;
   sFilterConfig.IsCalibrationMsg=0;
   sFilterConfig.RxBufferIndex=0;
   HAL_FDCAN_ConfigFilter(&hfdcan1,&sFilterConfig);
}

/* 初始化CAN1FD */
void can1_fd_init(void)
{
    /* 摘抄SDK中的时钟和GPIO的初始化流程 */
    GPIO_InitTypeDef  GPIO_InitStruct;
    RCC_PeriphCLKInitTypeDef RCC_PeriphClkInit;

    __HAL_RCC_FDCAN_FORCE_RESET();
    __HAL_RCC_FDCAN_RELEASE_RESET();

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
    hfdcan1.Instance=FDCAN1;

    /* 设置仲裁阶段MBPS为500MBPS，公式为1/((1/25MHZ)*(1+seg1+seg2))BPS */
    hfdcan1.Init.NominalPrescaler=4;//仲裁阶段时钟4分频25MHZ，1tq=1/(25MHZ)s
    hfdcan1.Init.NominalSyncJumpWidth=10;
    hfdcan1.Init.NominalTimeSeg1=19;
    hfdcan1.Init.NominalTimeSeg2=30;
    
     /* 设置数据阶段MBPS为1MBPS */
    hfdcan1.Init.DataPrescaler=4;//数据阶段时钟4分频25MHZ，1tq=1/(25MHZ)s
    hfdcan1.Init.DataSyncJumpWidth=10;
    hfdcan1.Init.DataTimeSeg1=10;
    hfdcan1.Init.DataTimeSeg2=14;

    hfdcan1.Init.FrameFormat=FDCAN_FRAME_FD_BRS;//工作在CANFD模式并且有波特率变速
    hfdcan1.Init.MessageRAMOffset=0;//FDCAN1占用的10KB的共享RAM的偏移量为0从头使用，FDCAN2可以设置为一半的位置
    hfdcan1.Init.Mode=FDCAN_MODE_INTERNAL_LOOPBACK;//工作模式，一般就关注正常模式和环回测试就行
    hfdcan1.Init.ProtocolException=ENABLE;//收到的报文协议异常则判定格式错误
    hfdcan1.Init.AutoRetransmission=ENABLE;//使能自动重传模式
    
    hfdcan1.Init.StdFiltersNbr=4;//标准ID的过滤器有4条
    hfdcan1.Init.ExtFiltersNbr=4;//扩展ID的过滤器有4条

    hfdcan1.Init.RxBufferSize=FDCAN_DATA_BYTES_8;//接收buffer可以接收数据长度为8的数据帧
    hfdcan1.Init.RxBuffersNbr=0;//不给他分配RAM，即我们不使用接收buffer

    hfdcan1.Init.RxFifo0ElmtSize=FDCAN_DATA_BYTES_8;//接收FIFO0可以接收数据长度为8的数据帧
    hfdcan1.Init.RxFifo0ElmtsNbr=10;//接收FIFO0可以缓存10条数据长度为8的数据帧

    hfdcan1.Init.RxFifo1ElmtSize=FDCAN_DATA_BYTES_64;//接收FIFO1可以接收数据长度为64的数据帧
    hfdcan1.Init.RxFifo1ElmtsNbr=5;//接收FIFO1可以缓存5条数据长度为64的数据帧

    hfdcan1.Init.TransmitPause=ENABLE;//使能传输暂停机制，传输完成一帧之后让出一段时间，如果还是总线空闲则可以继续发送
    hfdcan1.Init.TxBuffersNbr=0;//发送缓冲区个数为0，不需要使用
    hfdcan1.Init.TxElmtSize=FDCAN_DATA_BYTES_64;//允许发送64个字节长度的数据帧
    hfdcan1.Init.TxEventsNbr=0;//事件发送缓存个数为0不需要用
    hfdcan1.Init.TxFifoQueueElmtsNbr=20;//20个发送FIFO或者发送队列的长度，选队列还是FIFO由下面控制
    hfdcan1.Init.TxFifoQueueMode=FDCAN_TX_FIFO_OPERATION;//选择FIFO模式
	HAL_FDCAN_Init(&hfdcan1);

    /* 初始化4条标准ID筛选器和4条扩展ID筛选器 */
    can1_fd_init_filter();
}

/* 中断相关 */
void FDCAN1_IT0_IRQHandler(void)
{
  HAL_FDCAN_IRQHandler(&hfdcan1);
}
void FDCAN2_IT0_IRQHandler(void)
{
  HAL_FDCAN_IRQHandler(&hfdcan1);
}
void FDCAN1_IT1_IRQHandler(void)
{
  HAL_FDCAN_IRQHandler(&hfdcan1);
}
void FDCAN2_IT1_IRQHandler(void)
{
  HAL_FDCAN_IRQHandler(&hfdcan1);
}
void FDCAN_CAL_IRQHandler(void)
{
  HAL_FDCAN_IRQHandler(&hfdcan1);
}