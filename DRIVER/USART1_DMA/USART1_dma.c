#include "usart1_dma.h"
#include <string.h>
#include <stdarg.h>
#include <stdio.h>   // for vsnprintf

#define USART1_RX_FIFO_SIZE    1024
typedef struct
{
  volatile uint16_t write;
  volatile uint16_t read;
  uint8_t FIFO[USART1_RX_FIFO_SIZE];
  volatile uint16_t comptleflag;
}usart1_rx_fifo;
__attribute__((section (".RAM_D2")))static usart1_rx_fifo    USART1_RX_FIFO;

#define USART1_TX_FIFO_SIZE    1024
__attribute__((section (".RAM_D2")))static volatile uint8_t usart1_tx_comptileflag;
__attribute__((section (".RAM_D2")))static uint8_t          USART1_TX_FIFO[USART1_TX_FIFO_SIZE];

__attribute__((section (".RAM_D2")))static USART_HandleTypeDef        usart1_handle;
__attribute__((section (".RAM_D2")))static DMA_HandleTypeDef         dma1_usart_tx_cfg;
__attribute__((section (".RAM_D2")))static DMA_HandleTypeDef         dma1_usart_rx_cfg;

static void dma1_usart1_tx_comptle(DMA_HandleTypeDef *_hdma);

void usart1_dma_init(uint32_t badue)
{
	__HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_USART1_CLK_ENABLE();
	__HAL_RCC_DMA1_CLK_ENABLE();

	GPIO_InitTypeDef           gpio_init_structure;
    gpio_init_structure.Mode      = GPIO_MODE_AF_PP;
	gpio_init_structure.Pull      = GPIO_NOPULL;
	gpio_init_structure.Speed     = GPIO_SPEED_FREQ_HIGH;
	gpio_init_structure.Alternate = GPIO_AF7_USART1;
	gpio_init_structure.Pin       = GPIO_PIN_9|GPIO_PIN_10;
    HAL_GPIO_Init(GPIOA,&gpio_init_structure);
    
	memset(&usart1_handle,0,sizeof(UART_HandleTypeDef));
	memset(&dma1_usart_tx_cfg,0,sizeof(DMA_HandleTypeDef));
	memset(&dma1_usart_rx_cfg,0,sizeof(DMA_HandleTypeDef));
	memset(USART1_TX_FIFO,0,USART1_TX_FIFO_SIZE);
	memset(&USART1_RX_FIFO,0,sizeof(usart1_rx_fifo));
	usart1_tx_comptileflag=1;//第一次默认置1

	usart1_handle.Instance=USART1;
	usart1_handle.Init.BaudRate=badue;
	usart1_handle.Init.ClockPrescaler=UART_PRESCALER_DIV1;
	usart1_handle.Init.Mode=UART_MODE_TX_RX;
	usart1_handle.Init.Parity=UART_PARITY_NONE;
	usart1_handle.Init.StopBits=UART_STOPBITS_1;
	usart1_handle.Init.WordLength=UART_WORDLENGTH_8B;
	usart1_handle.Init.CLKLastBit=USART_LASTBIT_DISABLE;
	usart1_handle.Init.CLKPhase=USART_PHASE_1EDGE;
	usart1_handle.Init.CLKPolarity=USART_POLARITY_LOW;
	HAL_USART_Init(&usart1_handle);

	dma1_usart_tx_cfg.Instance=DMA1_Stream2;//DMA发送
	dma1_usart_tx_cfg.Init.Direction=DMA_MEMORY_TO_PERIPH;//方向来自于内存到外设，其实本例子是DMA控制GPIO用m2m也是可以的
	dma1_usart_tx_cfg.Init.FIFOMode=DMA_FIFOMODE_DISABLE;//不需要突发，FIFO无意义
	dma1_usart_tx_cfg.Init.FIFOThreshold=DMA_FIFO_THRESHOLD_FULL;//随便给，反正没有
	dma1_usart_tx_cfg.Init.MemBurst=DMA_MBURST_SINGLE;//无需突发功能
	dma1_usart_tx_cfg.Init.MemDataAlignment=DMA_MDATAALIGN_BYTE;//串口是字节
	dma1_usart_tx_cfg.Init.MemInc=DMA_MINC_ENABLE;//内存地址递增
	dma1_usart_tx_cfg.Init.Mode=DMA_NORMAL;//单次模式
	dma1_usart_tx_cfg.Init.PeriphBurst=DMA_PBURST_SINGLE;//无需突发功能
	dma1_usart_tx_cfg.Init.PeriphDataAlignment=DMA_PDATAALIGN_BYTE;//串口是字节
	dma1_usart_tx_cfg.Init.PeriphInc=DMA_PINC_DISABLE;//外设地址不递增
	dma1_usart_tx_cfg.Init.Priority=DMA_PRIORITY_MEDIUM;//优先级随便给
	dma1_usart_tx_cfg.Init.Request=DMA_REQUEST_USART1_TX;//DMA请求是USART1_TX
	HAL_DMA_Init(&dma1_usart_tx_cfg);
	__HAL_DMA_DISABLE(&dma1_usart_tx_cfg);//禁止它发送，结合单次模式控制它发送
	__HAL_DMA_ENABLE_IT(&dma1_usart_tx_cfg,DMA_IT_TC);//打开它的DMA发送完成中断
	HAL_DMA_RegisterCallback(&dma1_usart_tx_cfg,HAL_DMA_XFER_CPLT_CB_ID,dma1_usart1_tx_comptle);
	HAL_NVIC_SetPriority(DMA1_Stream2_IRQn,14,0);
	HAL_NVIC_EnableIRQ(DMA1_Stream2_IRQn);
    
//	dma1_usart_rx_cfg.Instance=DMA1_Stream3;//DMA接收，需要配合串口空闲中断
//	dma1_usart_rx_cfg.Init.Direction=DMA_PERIPH_TO_MEMORY;//方向来自于外设到内存
//	dma1_usart_rx_cfg.Init.FIFOMode=DMA_FIFOMODE_DISABLE;//不需要突发，FIFO无意义
//	dma1_usart_rx_cfg.Init.FIFOThreshold=DMA_FIFO_THRESHOLD_FULL;//随便给，反正没有
//	dma1_usart_rx_cfg.Init.MemBurst=DMA_MBURST_SINGLE;//无需突发功能
//	dma1_usart_rx_cfg.Init.MemDataAlignment=DMA_MDATAALIGN_BYTE;//串口是字节
//	dma1_usart_rx_cfg.Init.MemInc=DMA_MINC_ENABLE;//内存地址递增
//	dma1_usart_rx_cfg.Init.Mode=DMA_CIRCULAR;//循环模式，需要配合我们的循环队列使用，达到高吞吐量
//	dma1_usart_rx_cfg.Init.PeriphBurst=DMA_PBURST_SINGLE;//无需突发功能
//	dma1_usart_rx_cfg.Init.PeriphDataAlignment=DMA_PDATAALIGN_BYTE;//串口是字节
//	dma1_usart_rx_cfg.Init.PeriphInc=DMA_PINC_DISABLE;//外设地址不递增
//	dma1_usart_rx_cfg.Init.Priority=DMA_PRIORITY_MEDIUM;//优先级随便给
//	dma1_usart_rx_cfg.Init.Request=DMA_REQUEST_USART1_RX;//DMA请求来自于USART1_RX
//	HAL_DMA_Init(&dma1_usart_rx_cfg);
//	__HAL_UART_ENABLE_IT(&usart1_handle,UART_IT_IDLE);//打开它的空闲中断
//	HAL_NVIC_SetPriority(USART1_IRQn,13,0);
//	HAL_NVIC_EnableIRQ(USART1_IRQn);
//	__HAL_DMA_ENABLE(&dma1_usart_rx_cfg);//使能它接收，随时待命
	//HAL_USART_Receive_DMA(&usart1_handle,USART1_RX_FIFO.FIFO,USART1_RX_FIFO_SIZE);//开启DMA请求
}

void usart1_my_printf(const char *format, ...)
{
   //等待上次传输完成
   while(!usart1_tx_comptileflag);
   usart1_tx_comptileflag=0;

   /* 利用C语言可变参填充缓冲区然后发送 */
   va_list args;
    va_start(args, format);
    int len = vsnprintf((char*)USART1_TX_FIFO, sizeof(USART1_TX_FIFO), format, args);
    va_end(args);
	USART1_TX_FIFO[len]='\0';
   
   __HAL_DMA_ENABLE(&dma1_usart_tx_cfg);
   HAL_USART_Transmit_DMA(&usart1_handle,USART1_TX_FIFO,len);
}

//发送完成回调
static void dma1_usart1_tx_comptle(DMA_HandleTypeDef *_hdma)
{
  if(_hdma->Instance==DMA1_Stream2)
  {
     usart1_tx_comptileflag=1;
	 __HAL_DMA_DISABLE(&dma1_usart_tx_cfg);
  }
}

void DMA1_Stream2_IRQHandler(void)
{
  HAL_DMA_IRQHandler(&dma1_usart_tx_cfg);
}

void USART1_IRQHandler(void)
{
   if(USART1->ISR&(0x01<<4))
   {
	USART1->ICR|=(0x01<<4);

     __HAL_DMA_DISABLE(&dma1_usart_rx_cfg);//先关闭DMA1接收方便读取DNCT
	 USART1_RX_FIFO.write=USART1_RX_FIFO_SIZE-DMA1_Stream3->NDTR;//偏移写指针
	 USART1_RX_FIFO.comptleflag=1;//指示接收就绪
     
	 __HAL_DMA_ENABLE(&dma1_usart_rx_cfg);//重新打开
	 HAL_USART_Receive_DMA(&usart1_handle,USART1_RX_FIFO.FIFO,USART1_RX_FIFO_SIZE);//开启DMA请求
   }
}
