#include "qspi_flash.h"
#include <string.h>
#include <stdbool.h>

/*
   qspi-d0  -----  PD11
	 qspi-d1  -----  PD12
	 qspi-d2  -----  PE2
	 qspi-d3  -----  PD13
	 qspi-cs  -----  PB6
	 qspi-clk -----  PB2
*/

__attribute__((section (".RAM_D2")))static QSPI_HandleTypeDef   qspi_handle;
__attribute__((section (".RAM_D2")))volatile bool               qspi_cmd_end_flag   =false;
__attribute__((section (".RAM_D2")))volatile bool               qspi_tx_end_flag    =false;
__attribute__((section (".RAM_D2")))volatile bool               qspi_rx_end_flag    =false;
__attribute__((section (".RAM_D2")))volatile bool               qspi_state_end_flag =false;

static void qspi_flash_w25q128_enterQSPIMODE(void);

/* QSPI-W25Q128写命令，就一个命令阶段，没有其他的阶段，适用于写使/失能开关 */
static void qspi_flash_w25q128_wcmd(uint8_t wcmd)
{
	QSPI_CommandTypeDef cmd;
	while(HAL_QSPI_STATE_BUSY==HAL_QSPI_GetState(&qspi_handle));//BUSY状态下无法写寄存器
	cmd.Instruction=wcmd;//指定命令
	cmd.Address=0;//没地址阶段，随便给
	cmd.AlternateBytes=0;//没交替字节阶段，随便给
	cmd.AddressSize=QSPI_ADDRESS_8_BITS;//没地址阶段，随便给
	cmd.AlternateBytesSize=QSPI_ALTERNATE_BYTES_8_BITS;//没交替字节阶段，随便给
	cmd.DummyCycles=0;//就发一个命令，没有空周期
	cmd.InstructionMode=QSPI_INSTRUCTION_1_LINE;//命令阶段采用单线发送，符合手册
	cmd.AddressMode=QSPI_ADDRESS_NONE;//就发一个命令，没有地址
	cmd.AlternateByteMode=QSPI_ALTERNATE_BYTES_NONE;////就发一个命令，没有交替字节
	cmd.DataMode=QSPI_DATA_NONE;//就发一个命令，没有数据
	cmd.NbData=0;//没有数据阶段，这个数据长度给0
	cmd.DdrMode=QSPI_DDR_MODE_DISABLE;//双倍速率模式，即双边沿全部采样，这里器件不支持
	cmd.DdrHoldHalfCycle=QSPI_DDR_HHC_ANALOG_DELAY;//双倍速率模式下的时钟延迟，这里器件没有双倍速率功能，随便给
	cmd.SIOOMode=QSPI_SIOO_INST_EVERY_CMD;//指令只第一次发送一次后面无需发送，这个不符合w25q128的特性，它是有很多命令的
	HAL_QSPI_Command_IT(&qspi_handle,&cmd);
	
	while(!qspi_cmd_end_flag);//轮询标志位
	qspi_cmd_end_flag=false;
}

/* 写状态寄存器函数 */
static void qspi_flash_w25q128_wStateReg(uint8_t wcmd,uint8_t val)
{
	QSPI_CommandTypeDef cmd;
	while(HAL_QSPI_STATE_BUSY==HAL_QSPI_GetState(&qspi_handle));//BUSY状态下无法写寄存器
	cmd.Instruction=wcmd;//指定写寄存器命令
	cmd.Address=0;//没地址阶段，随便给
	cmd.AlternateBytes=val;//交替字节为伴随的字节值，不借助数据阶段
	cmd.AddressSize=QSPI_ADDRESS_8_BITS;//没地址阶段，随便给
	cmd.AlternateBytesSize=QSPI_ALTERNATE_BYTES_8_BITS;//交替字节阶段发送一个字节
	cmd.DummyCycles=0;//就发一个命令+交替字节，没有空周期
	cmd.InstructionMode=QSPI_INSTRUCTION_1_LINE;//命令阶段采用单线发送，符合手册
	cmd.AddressMode=QSPI_ADDRESS_NONE;//就发一个命令+交替字节，没有地址
	cmd.AlternateByteMode=QSPI_ALTERNATE_BYTES_1_LINE;//交替字节采用单线发送，因为这里还没使能QE标志位
	cmd.DataMode=QSPI_DATA_NONE;//就发一个命令+交替字节，没有数据
	cmd.NbData=0;//没有数据阶段，这个数据长度给0
	cmd.DdrMode=QSPI_DDR_MODE_DISABLE;//双倍速率模式，即双边沿全部采样，这里器件不支持
	cmd.DdrHoldHalfCycle=QSPI_DDR_HHC_ANALOG_DELAY;//双倍速率模式下的时钟延迟，这里器件没有双倍速率功能，随便给
	cmd.SIOOMode=QSPI_SIOO_INST_EVERY_CMD;//指令只第一次发送一次后面无需发送，这个不符合w25q128的特性，它是有很多命令的
	HAL_QSPI_Command_IT(&qspi_handle,&cmd);

	while(!qspi_cmd_end_flag);//轮询标志位
	qspi_cmd_end_flag=false;
}

/* 等待状态寄存器函数 */
static void qspi_flash_w25q128_waitStateReg(uint8_t wcmd,uint8_t match,uint8_t mask)
{
	/* 轮询状态寄存器1，确保它的第一位WEL为1，第0位BUSY为0 */
	QSPI_AutoPollingTypeDef  autocfg;
	QSPI_CommandTypeDef      cmd;
	while(HAL_QSPI_STATE_BUSY==HAL_QSPI_GetState(&qspi_handle));//BUSY状态下无法写寄存器
	cmd.Instruction=wcmd;//指定读哪个状态寄存器
	cmd.Address=0;//没地址阶段，随便给
	cmd.AlternateBytes=0;//没交替字节阶段，随便给
	cmd.AddressSize=QSPI_ADDRESS_8_BITS;//没地址阶段，随便给
	cmd.AlternateBytesSize=QSPI_ALTERNATE_BYTES_8_BITS;//没交替字节阶段，随便给
	cmd.DummyCycles=0;//就发一个命令，没有空周期
	cmd.InstructionMode=QSPI_INSTRUCTION_1_LINE;//命令阶段采用单线发送，符合手册
	cmd.AddressMode=QSPI_ADDRESS_NONE;//就发一个命令，没有地址
	cmd.AlternateByteMode=QSPI_ALTERNATE_BYTES_NONE;////就发一个命令，没有交替字节
	cmd.DataMode=QSPI_DATA_1_LINE;//需要接收一个字节的状态寄存器的数据
	cmd.NbData=0;//这种情况下数据长度给多少都无所谓，HAL_QSPI_AutoPolling_IT函数内部会将StatusBytesSize赋值给NbData，也就是说这情况下这玩意没有用。
	cmd.DdrMode=QSPI_DDR_MODE_DISABLE;//双倍速率模式，即双边沿全部采样，这里器件不支持
	cmd.DdrHoldHalfCycle=QSPI_DDR_HHC_ANALOG_DELAY;//双倍速率模式下的时钟延迟，这里器件没有双倍速率功能，随便给
	cmd.SIOOMode=QSPI_SIOO_INST_EVERY_CMD;//指令只第一次发送一次后面无需发送，这个不符合w25q128的特性，它是有很多命令的
	autocfg.Match=match;//期待值
	autocfg.Mask=mask;//掩码
	autocfg.Interval=0x10;//每128个时钟周期轮询一次
	autocfg.StatusBytesSize=1;//读取一个字节即状态寄存器1
	autocfg.MatchMode=QSPI_MATCH_MODE_AND;//与逻辑
	autocfg.AutomaticStop=QSPI_AUTOMATIC_STOP_ENABLE;//匹配成功停止轮询
	HAL_QSPI_AutoPolling_IT(&qspi_handle,&cmd,&autocfg);//轮询模式用这个函数，自动发送命令并且自动读取状态
	
	while(!qspi_state_end_flag);//轮询标志位
	qspi_state_end_flag=false;
}

/* QSPI-W25Q128初始化 */
void qspi_flash_w25q128_init(void)
{
	__HAL_RCC_QSPI_CLK_ENABLE();
	__HAL_RCC_QSPI_FORCE_RESET();
	__HAL_RCC_QSPI_RELEASE_RESET();
	__HAL_RCC_GPIOB_CLK_ENABLE();
	__HAL_RCC_GPIOD_CLK_ENABLE();
	__HAL_RCC_GPIOE_CLK_ENABLE();
	
	GPIO_InitTypeDef gpio_cfg;
	gpio_cfg.Mode=GPIO_MODE_AF_PP;
	gpio_cfg.Pull=GPIO_PULLUP;
	gpio_cfg.Speed=GPIO_SPEED_FREQ_VERY_HIGH;
	
	gpio_cfg.Alternate=GPIO_AF10_QUADSPI;
	gpio_cfg.Pin=GPIO_PIN_6;
	HAL_GPIO_Init(GPIOB,&gpio_cfg);
	
	gpio_cfg.Pull=GPIO_NOPULL;
	gpio_cfg.Alternate=GPIO_AF9_QUADSPI;
	gpio_cfg.Pin=GPIO_PIN_11;
	HAL_GPIO_Init(GPIOD,&gpio_cfg);
	
	gpio_cfg.Alternate=GPIO_AF9_QUADSPI;
	gpio_cfg.Pin=GPIO_PIN_12;
	HAL_GPIO_Init(GPIOD,&gpio_cfg);
	
	gpio_cfg.Alternate=GPIO_AF9_QUADSPI;
	gpio_cfg.Pin=GPIO_PIN_2;
	HAL_GPIO_Init(GPIOE,&gpio_cfg);
	
	gpio_cfg.Alternate=GPIO_AF9_QUADSPI;
	gpio_cfg.Pin=GPIO_PIN_13;
	HAL_GPIO_Init(GPIOD,&gpio_cfg);
	
	gpio_cfg.Alternate=GPIO_AF9_QUADSPI;
	gpio_cfg.Pin=GPIO_PIN_2;
	HAL_GPIO_Init(GPIOB,&gpio_cfg);
	
	/* 初始化QSPI器件 */
	HAL_StatusTypeDef   ret;
	memset(&qspi_handle,0,sizeof(QSPI_HandleTypeDef));//清空句柄
	while(HAL_QSPI_STATE_BUSY==HAL_QSPI_GetState(&qspi_handle));//BUSY状态下无法写寄存器
	qspi_handle.Instance=QUADSPI;//选择QSPI器件
	qspi_handle.Init.ClockPrescaler=200-1;//时钟分频，QSPI挂载在AHB3总线上时钟为200MHZ，W25Q128最大支持104MHZ
	qspi_handle.Init.FifoThreshold=4;//FIFO阈值线，官方源码为4，所以我们保持一致
	qspi_handle.Init.SampleShifting=QSPI_SAMPLE_SHIFTING_NONE;//采样移位，比如说SPI是上升沿采样，现在延时半个周期再采样，这个不需要
	qspi_handle.Init.FlashSize=23;//容量等于2^(flashsize+1),所以w25q128的128MB对应要填写23
	qspi_handle.Init.ChipSelectHighTime=QSPI_CS_HIGH_TIME_1_CYCLE;//CS片选线到第一个时钟命令之间延时1个时钟周期
	qspi_handle.Init.ClockMode=QSPI_CLOCK_MODE_0;//QPSI模式0
	qspi_handle.Init.FlashID=QSPI_FLASH_ID_1;//FLASH器件ID，双闪存模式下有用，只有一个器件而且只有一个存储器那就默认ID1
	qspi_handle.Init.DualFlash=QSPI_DUALFLASH_DISABLE;//禁止双闪存模式
	ret=HAL_QSPI_Init(&qspi_handle);
	if(HAL_OK!=ret)
	{
		while(1)
		{
		}
	}
	
	HAL_NVIC_SetPriority(QUADSPI_IRQn,10,0);
	HAL_NVIC_EnableIRQ(QUADSPI_IRQn);
	
	qspi_flash_w25q128_enterQSPIMODE();//让元器件进入QSPI模式
}

/* 写使能命令 */
static void qspi_flash_w25q128_wEnable(void)
{
	qspi_flash_w25q128_wcmd(0x06);//发送写使能命令
	
	/* 轮询状态寄存器1，期待它的第一位WEL为1，第0位BUSY为0 */
	qspi_flash_w25q128_waitStateReg(0x05,0x02,0x03);
}

/* 写失能命令 */
static void qspi_flash_w25q128_wDisable(void)
{
	qspi_flash_w25q128_wcmd(0x04);
	
	/* 轮询状态寄存器1，期待它的第一位WEL为0，第0位BUSY为0 */
	qspi_flash_w25q128_waitStateReg(0x05,0x00,0x03);
}

/* 进入QSPI模式命令 */
static void qspi_flash_w25q128_enterQSPIMODE(void)
{
/* 如果初次使用该器件，我还没有让它进入QSPI模式那就执行下面的序列 */
#if NOT_ENTER_QSPI_MODE
		/* 先将状态寄存器2的第一位设置为1使用QE使能 */
	qspi_flash_w25q128_wEnable();
	qspi_flash_w25q128_wStateReg(0x31,0x02);//使能QE位,QE位是一个非易失位，所以只需一次即可
	qspi_flash_w25q128_waitBusy();
	qspi_flash_w25q128_wDisable();
	
	/* 轮询状态寄存器2，确保它的第一位QE位为1 */
	qspi_flash_w25q128_waitStateReg(0x35,0x02,0x02);
	
	/* 最后使用进入QSPI模式命令 */
	qspi_flash_w25q128_wcmd(0x38);
#else /* 如果已经使能了它的QSPI模式以及状态寄存器的一系列配置之后再也不需要写它的状态寄存器 */
	
	/* 之所以连着读9次是因为刚上电有段时间不稳定有可能丢读取，所以通过多次读取的方式跳过这段时间 */
	qspi_flash_w25q128_waitStateReg(0x05,0x00,0x01);//测试读取三个状态寄存器，期待BUSY为0
	qspi_flash_w25q128_waitStateReg(0x35,0x02,0x02);//期待QE为1
	qspi_flash_w25q128_waitStateReg(0x15,0x60,0x60);//期待默认读输出强度
	
	qspi_flash_w25q128_waitStateReg(0x05,0x00,0x01);//测试读取三个状态寄存器，期待BUSY为0
	qspi_flash_w25q128_waitStateReg(0x35,0x02,0x02);//期待QE为1
	qspi_flash_w25q128_waitStateReg(0x15,0x60,0x60);//期待默认读输出强度
	
	qspi_flash_w25q128_waitStateReg(0x05,0x00,0x01);//测试读取三个状态寄存器，期待BUSY为0
	qspi_flash_w25q128_waitStateReg(0x35,0x02,0x02);//期待QE为1
	qspi_flash_w25q128_waitStateReg(0x15,0x60,0x60);//期待默认读输出强度
#endif
}

/* 读取ID测试，这个函数采用QSPI模式，我需要94h命令这个支持QSPI模式 */ 
uint8_t qspi_flash_w25q128_read_id(void) 
{ 
	QSPI_CommandTypeDef cmd; 
	 
	/* 轮询状态寄存器1，期待它的第0位BUSY为0 */ 
	qspi_flash_w25q128_waitStateReg(0x05,0x00,0x01); 
	 
	while(HAL_QSPI_STATE_BUSY==HAL_QSPI_GetState(&qspi_handle));//BUSY状态下无法写寄存器 
	cmd.Instruction=0x94;//指定命令 
	cmd.Address=0;//地址阶段手册指定给0 
	cmd.AlternateBytes=0xF0;//1个交替字节手册指定给0xFxh，这里我就给0xF0 
	cmd.AddressSize=QSPI_ADDRESS_24_BITS;//地址阶段24位0 
	cmd.AlternateBytesSize=QSPI_ALTERNATE_BYTES_8_BITS;//1个交替字节 
	cmd.DummyCycles=4;//空周期需要4个 
	cmd.InstructionMode=QSPI_INSTRUCTION_1_LINE;//命令阶段采用1线发送，符合手册 
	cmd.AddressMode=QSPI_ADDRESS_4_LINES;//地址阶段采用4线发送，符合手册 
	cmd.AlternateByteMode=QSPI_ALTERNATE_BYTES_4_LINES;//交替字节阶段采用4线发送，符合手册 
	cmd.DataMode=QSPI_DATA_4_LINES;//数据采用4线接收，符合手册 
	cmd.NbData=2;//有数据阶段，这个数据长度给2 
	cmd.DdrMode=QSPI_DDR_MODE_DISABLE;//双倍速率模式，即双边沿全部采样，这里器件不支持 
	cmd.DdrHoldHalfCycle=QSPI_DDR_HHC_ANALOG_DELAY;//双倍速率模式下的时钟延迟，这里器件没有双倍速率功能，随便给 
	cmd.SIOOMode=QSPI_SIOO_INST_EVERY_CMD;//指令只第一次发送一次后面无需发送，这个不符合w25q128的特性，它是有很多命令的 
	HAL_QSPI_Command_IT(&qspi_handle,&cmd); 
	 
	while(!qspi_cmd_end_flag);//轮询标志位 
	qspi_cmd_end_flag=false; 
	 
	uint8_t id[2]={0}; 
	HAL_QSPI_Receive_IT(&qspi_handle,id); 
	while(!qspi_rx_end_flag);//轮询标志位 
	qspi_rx_end_flag=false; 
	 
	return id[1]; 
}

/* 命令+地址+交替字节+空周期这段可裁剪时间的发送完成回调 */
__attribute__((section(".ITCM_CODE"), used))void HAL_QSPI_CmdCpltCallback(QSPI_HandleTypeDef *hqspi)
{
	if(QUADSPI==hqspi->Instance) qspi_cmd_end_flag=true;
}

/* 数据发送完成回调 */
__attribute__((section(".ITCM_CODE"), used))void HAL_QSPI_TxCpltCallback(QSPI_HandleTypeDef *hqspi)
{
	if(QUADSPI==hqspi->Instance) qspi_tx_end_flag=true;
}

/* 数据接收完成回调 */
__attribute__((section(".ITCM_CODE"), used))void HAL_QSPI_RxCpltCallback(QSPI_HandleTypeDef *hqspi)
{
	if(QUADSPI==hqspi->Instance) qspi_rx_end_flag=true;
}

/* 状态轮询完成回调，其实就是W25Q128的BUSY标志位是否变回默认态的回调 */
__attribute__((section(".ITCM_CODE"), used))void HAL_QSPI_StatusMatchCallback(QSPI_HandleTypeDef *hqspi)
{
	if(QUADSPI==hqspi->Instance) qspi_state_end_flag=true;
}

/* 总中断 */
__attribute__((section(".ITCM_CODE"), used))void QUADSPI_IRQHandler(void)
{
	HAL_QSPI_IRQHandler(&qspi_handle);
}
