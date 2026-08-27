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

static QSPI_HandleTypeDef   qspi_handle;

static void qspi_flash_w25q128_enterQSPIMODE(void);

/* QSPI-W25Q128写命令，就一个命令阶段，没有其他的阶段，适用于写使/失能开关 */
static void qspi_flash_w25q128_wcmd(uint8_t wcmd,uint8_t useqspi)
{
	QSPI_CommandTypeDef cmd;
	while(HAL_QSPI_STATE_BUSY==HAL_QSPI_GetState(&qspi_handle));//BUSY状态下无法写寄存器
	cmd.Instruction=wcmd;//指定命令
	cmd.Address=0;//没地址阶段，随便给
	cmd.AlternateBytes=0;//没交替字节阶段，随便给
	cmd.AddressSize=QSPI_ADDRESS_8_BITS;//没地址阶段，随便给
	cmd.AlternateBytesSize=QSPI_ALTERNATE_BYTES_8_BITS;//没交替字节阶段，随便给
	cmd.DummyCycles=0;//就发一个命令，没有空周期
	if(useqspi) cmd.InstructionMode=QSPI_INSTRUCTION_4_LINES;//命令阶段指定
	else        cmd.InstructionMode=QSPI_INSTRUCTION_1_LINE;
	cmd.AddressMode=QSPI_ADDRESS_NONE;//就发一个命令，没有地址
	cmd.AlternateByteMode=QSPI_ALTERNATE_BYTES_NONE;////就发一个命令，没有交替字节
	cmd.DataMode=QSPI_DATA_NONE;//就发一个命令，没有数据
	cmd.NbData=0;//没有数据阶段，这个数据长度给0
	cmd.DdrMode=QSPI_DDR_MODE_DISABLE;//双倍速率模式，即双边沿全部采样，这里器件不支持
	cmd.DdrHoldHalfCycle=QSPI_DDR_HHC_ANALOG_DELAY;//双倍速率模式下的时钟延迟，这里器件没有双倍速率功能，随便给
	cmd.SIOOMode=QSPI_SIOO_INST_EVERY_CMD;//指令只第一次发送一次后面无需发送，这个不符合w25q128的特性，它是有很多命令的
	HAL_QSPI_Command(&qspi_handle,&cmd,HAL_MAX_DELAY);
}

/* 写状态寄存器函数 */
static void qspi_flash_w25q128_wStateReg(uint8_t wcmd,uint8_t val,uint8_t useqspi)
{
	QSPI_CommandTypeDef cmd;
	while(HAL_QSPI_STATE_BUSY==HAL_QSPI_GetState(&qspi_handle));//BUSY状态下无法写寄存器
	cmd.Instruction=wcmd;//指定写寄存器命令
	cmd.Address=0;//没地址阶段，随便给
	cmd.AlternateBytes=val;//交替字节为伴随的字节值，不借助数据阶段
	cmd.AddressSize=QSPI_ADDRESS_8_BITS;//没地址阶段，随便给
	cmd.AlternateBytesSize=QSPI_ALTERNATE_BYTES_8_BITS;//交替字节阶段发送一个字节
	cmd.DummyCycles=0;//就发一个命令+交替字节，没有空周期
	if(useqspi) cmd.InstructionMode=QSPI_INSTRUCTION_4_LINES;//命令阶段指定
	else        cmd.InstructionMode=QSPI_INSTRUCTION_1_LINE;
	cmd.AddressMode=QSPI_ADDRESS_NONE;//就发一个命令+交替字节，没有地址
	if(useqspi) cmd.AlternateByteMode=QSPI_ALTERNATE_BYTES_4_LINES;//交替字节指定
	else        cmd.AlternateByteMode=QSPI_ALTERNATE_BYTES_1_LINE;
	cmd.DataMode=QSPI_DATA_NONE;//就发一个命令+交替字节，没有数据
	cmd.NbData=0;//没有数据阶段，这个数据长度给0
	cmd.DdrMode=QSPI_DDR_MODE_DISABLE;//双倍速率模式，即双边沿全部采样，这里器件不支持
	cmd.DdrHoldHalfCycle=QSPI_DDR_HHC_ANALOG_DELAY;//双倍速率模式下的时钟延迟，这里器件没有双倍速率功能，随便给
	cmd.SIOOMode=QSPI_SIOO_INST_EVERY_CMD;//指令只第一次发送一次后面无需发送，这个不符合w25q128的特性，它是有很多命令的
	HAL_QSPI_Command(&qspi_handle,&cmd,HAL_MAX_DELAY);
}

/* 等待状态寄存器函数 */
static bool qspi_flash_w25q128_waitStateReg(uint8_t wcmd,uint8_t match,uint8_t mask,uint8_t useqspi)
{
	/* 轮询状态寄存器1，确保它的第一位WEL为1，第0位BUSY为0 */
	QSPI_CommandTypeDef      cmd;
	uint8_t                  rech;
	while(HAL_QSPI_STATE_BUSY==HAL_QSPI_GetState(&qspi_handle));//BUSY状态下无法写寄存器
	cmd.Instruction=wcmd;//指定读哪个状态寄存器
	cmd.Address=0;//没地址阶段，随便给
	cmd.AlternateBytes=0;//没交替字节阶段，随便给
	cmd.AddressSize=QSPI_ADDRESS_8_BITS;//没地址阶段，随便给
	cmd.AlternateBytesSize=QSPI_ALTERNATE_BYTES_8_BITS;//没交替字节阶段，随便给
	cmd.DummyCycles=0;//就发一个命令，没有空周期
	if(useqspi) cmd.InstructionMode=QSPI_INSTRUCTION_4_LINES;//命令阶段区分
	else        cmd.InstructionMode=QSPI_INSTRUCTION_1_LINE;
	cmd.AddressMode=QSPI_ADDRESS_NONE;//就发一个命令，没有地址
	cmd.AlternateByteMode=QSPI_ALTERNATE_BYTES_NONE;////就发一个命令，没有交替字节
	if(useqspi) cmd.DataMode=QSPI_DATA_4_LINES;//数据阶段区分
	else        cmd.DataMode=QSPI_DATA_1_LINE;
	cmd.NbData=1;//收一个
	cmd.DdrMode=QSPI_DDR_MODE_DISABLE;//双倍速率模式，即双边沿全部采样，这里器件不支持
	cmd.DdrHoldHalfCycle=QSPI_DDR_HHC_ANALOG_DELAY;//双倍速率模式下的时钟延迟，这里器件没有双倍速率功能，随便给
	cmd.SIOOMode=QSPI_SIOO_INST_EVERY_CMD;//指令只第一次发送一次后面无需发送，这个不符合w25q128的特性，它是有很多命令的
  HAL_QSPI_Command(&qspi_handle,&cmd,HAL_MAX_DELAY);
	HAL_QSPI_Receive(&qspi_handle,&rech,HAL_MAX_DELAY);
	if((rech&mask)!=match) return false;
	return true;
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
	gpio_cfg.Pull=GPIO_NOPULL;
	gpio_cfg.Speed=GPIO_SPEED_FREQ_VERY_HIGH;
	
	gpio_cfg.Alternate=GPIO_AF10_QUADSPI;
	gpio_cfg.Pin=GPIO_PIN_6;
	HAL_GPIO_Init(GPIOB,&gpio_cfg);
	
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
	qspi_handle.Init.ClockPrescaler=20-1;//时钟分频，QSPI挂载在AHB3总线上时钟为200MHZ，W25Q128最大支持104MHZ
	qspi_handle.Init.FifoThreshold=4;//FIFO阈值线，官方源码为4，所以我们保持一致
	qspi_handle.Init.SampleShifting=QSPI_SAMPLE_SHIFTING_HALFCYCLE;//采样移位，比如说SPI是上升沿采样，现在延时半个周期再采样
	qspi_handle.Init.FlashSize=23;//容量等于2^(flashsize+1),所以w25q128的128MB对应要填写23
	qspi_handle.Init.ChipSelectHighTime=QSPI_CS_HIGH_TIME_2_CYCLE;//CS片选线到第一个时钟命令之间延时2个时钟周期
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
	__HAL_QSPI_CLEAR_FLAG(&qspi_handle,QSPI_FLAG_TO);
	__HAL_QSPI_CLEAR_FLAG(&qspi_handle,QSPI_FLAG_SM);
	__HAL_QSPI_CLEAR_FLAG(&qspi_handle,QSPI_FLAG_TC);
	__HAL_QSPI_CLEAR_FLAG(&qspi_handle,QSPI_FLAG_TE);
	
	qspi_flash_w25q128_enterQSPIMODE();//让元器件进入QSPI模式
}

/* 写使能命令 */
static void qspi_flash_w25q128_wEnable(uint8_t useqspi)
{
	qspi_flash_w25q128_wcmd(0x06,useqspi);//发送写使能命令
}

/* 写失能命令 */
__attribute__((unused))static void qspi_flash_w25q128_wDisable(uint8_t useqspi)
{
	qspi_flash_w25q128_wcmd(0x04,useqspi);
}

/* 进入QSPI模式命令 */
static void qspi_flash_w25q128_enterQSPIMODE(void)
{
	/* 退出QSPI模式,等待QE位为0 */
	qspi_flash_w25q128_wcmd(0xFF,1);
	while(!qspi_flash_w25q128_waitStateReg(0x35,0x00,0x02,1));
	
	/* 先将状态寄存器2的第一位设置为1使用QE使能 */
	qspi_flash_w25q128_wEnable(0);
	qspi_flash_w25q128_wStateReg(0x31,0x02,0);//使能QE位
	
	/* 轮询状态寄存器2，确保它的第一位QE位为1 */
	while(!qspi_flash_w25q128_waitStateReg(0x35,0x02,0x02,0));
	
	HAL_Delay(50);//等待FLASH稳定，实践发现50ms最好
	
	/* 最后使用进入QSPI模式命令 */
	qspi_flash_w25q128_wcmd(0x38,0);
}

/* 读取ID测试，这个函数采用QSPI模式，我需要9Fh命令这个支持QSPI模式而且一次性读取8位MID,16位ID */ 
uint32_t qspi_flash_w25q128_read_id(void) 
{ 
	QSPI_CommandTypeDef cmd; 
	 
	/* 轮询状态寄存器1，期待它的第0位BUSY为0 */ 
	while(!qspi_flash_w25q128_waitStateReg(0x05,0x00,0x01,1)); 
	 
	while(HAL_QSPI_STATE_BUSY==HAL_QSPI_GetState(&qspi_handle));//BUSY状态下无法写寄存器 
	cmd.Instruction=0x9F;//指定命令 
	cmd.Address=0;//无地址阶段，随便给 
	cmd.AlternateBytes=0;//无交替字节，随便给
	cmd.AddressSize=QSPI_ADDRESS_8_BITS;//无地址阶段，随便给
	cmd.AlternateBytesSize=QSPI_ALTERNATE_BYTES_8_BITS;//无交替字节，随便给
	cmd.DummyCycles=0;//空周期需要0个 
	cmd.InstructionMode=QSPI_INSTRUCTION_4_LINES;//命令阶段采用4线发送，符合手册 
	cmd.AddressMode=QSPI_ADDRESS_NONE;//无地址阶段，符合手册 
	cmd.AlternateByteMode=QSPI_ALTERNATE_BYTES_NONE;//无交替字节阶段，符合手册 
	cmd.DataMode=QSPI_DATA_4_LINES;//数据采用4线接收，符合手册 
	cmd.NbData=3;//有数据阶段，这个数据长度给3 
	cmd.DdrMode=QSPI_DDR_MODE_DISABLE;//双倍速率模式，即双边沿全部采样，这里器件不支持 
	cmd.DdrHoldHalfCycle=QSPI_DDR_HHC_ANALOG_DELAY;//双倍速率模式下的时钟延迟，这里器件没有双倍速率功能，随便给 
	cmd.SIOOMode=QSPI_SIOO_INST_EVERY_CMD;//指令只第一次发送一次后面无需发送，这个不符合w25q128的特性，它是有很多命令的 
	HAL_QSPI_Command(&qspi_handle,&cmd,HAL_MAX_DELAY);  
	 
	uint8_t id[3]={0}; 
	HAL_QSPI_Receive(&qspi_handle,id,HAL_MAX_DELAY); 
	return (uint32_t)(((id[1]<<8)|id[2])|(id[0]<<16)); 
}

/* 擦除扇区函数 */
void qspi_flash_erase_sector(uint32_t addr)
{
	while(!qspi_flash_w25q128_waitStateReg(0x05,0x00,0x01,1));//轮询状态寄存器1，期待它的第0位BUSY为0
	
	qspi_flash_w25q128_wEnable(1);
	
	QSPI_CommandTypeDef cmd;
	while(HAL_QSPI_STATE_BUSY==HAL_QSPI_GetState(&qspi_handle));//BUSY状态下无法写寄存器 
	cmd.Instruction=0x20;//指定命令 
	cmd.Address=addr;//指定地址
	cmd.AlternateBytes=0;//无交替字节，随便给
	cmd.AddressSize=QSPI_ADDRESS_24_BITS;//24位地址阶段
	cmd.AlternateBytesSize=QSPI_ALTERNATE_BYTES_8_BITS;//无交替字节，随便给
	cmd.DummyCycles=0;//空周期需要0个 
	cmd.InstructionMode=QSPI_INSTRUCTION_4_LINES;//命令阶段采用4线发送，符合手册 
	cmd.AddressMode=QSPI_ADDRESS_4_LINES;//地址阶段四线，符合手册 
	cmd.AlternateByteMode=QSPI_ALTERNATE_BYTES_NONE;//无交替字节阶段，符合手册 
	cmd.DataMode=QSPI_DATA_NONE;//无数据阶段，符合手册 
	cmd.NbData=0;//无数据阶段 
	cmd.DdrMode=QSPI_DDR_MODE_DISABLE;//双倍速率模式，即双边沿全部采样，这里器件不支持 
	cmd.DdrHoldHalfCycle=QSPI_DDR_HHC_ANALOG_DELAY;//双倍速率模式下的时钟延迟，这里器件没有双倍速率功能，随便给 
	cmd.SIOOMode=QSPI_SIOO_INST_EVERY_CMD;//指令只第一次发送一次后面无需发送，这个不符合w25q128的特性，它是有很多命令的 
	HAL_QSPI_Command(&qspi_handle,&cmd,HAL_MAX_DELAY);
}

/* 擦除半块函数，半块的话32KB */
void qspi_flash_erase_half_block(uint32_t addr)
{
	while(!qspi_flash_w25q128_waitStateReg(0x05,0x00,0x01,1));//轮询状态寄存器1，期待它的第0位BUSY为0
	
	qspi_flash_w25q128_wEnable(1);
	
	QSPI_CommandTypeDef cmd;
	while(HAL_QSPI_STATE_BUSY==HAL_QSPI_GetState(&qspi_handle));//BUSY状态下无法写寄存器 
	cmd.Instruction=0x52;//指定命令 
	cmd.Address=addr;//指定地址
	cmd.AlternateBytes=0;//无交替字节，随便给
	cmd.AddressSize=QSPI_ADDRESS_24_BITS;//24位地址阶段
	cmd.AlternateBytesSize=QSPI_ALTERNATE_BYTES_8_BITS;//无交替字节，随便给
	cmd.DummyCycles=0;//空周期需要0个 
	cmd.InstructionMode=QSPI_INSTRUCTION_4_LINES;//命令阶段采用4线发送，符合手册 
	cmd.AddressMode=QSPI_ADDRESS_4_LINES;//地址阶段四线，符合手册 
	cmd.AlternateByteMode=QSPI_ALTERNATE_BYTES_NONE;//无交替字节阶段，符合手册 
	cmd.DataMode=QSPI_DATA_NONE;//无数据阶段，符合手册 
	cmd.NbData=0;//无数据阶段 
	cmd.DdrMode=QSPI_DDR_MODE_DISABLE;//双倍速率模式，即双边沿全部采样，这里器件不支持 
	cmd.DdrHoldHalfCycle=QSPI_DDR_HHC_ANALOG_DELAY;//双倍速率模式下的时钟延迟，这里器件没有双倍速率功能，随便给 
	cmd.SIOOMode=QSPI_SIOO_INST_EVERY_CMD;//指令只第一次发送一次后面无需发送，这个不符合w25q128的特性，它是有很多命令的 
	HAL_QSPI_Command(&qspi_handle,&cmd,HAL_MAX_DELAY);
}

/* 擦除块函数，一块的话64KB */
void qspi_flash_erase_block(uint32_t addr)
{
	while(!qspi_flash_w25q128_waitStateReg(0x05,0x00,0x01,1));//轮询状态寄存器1，期待它的第0位BUSY为0
	
	qspi_flash_w25q128_wEnable(1);
	
	QSPI_CommandTypeDef cmd;
	while(HAL_QSPI_STATE_BUSY==HAL_QSPI_GetState(&qspi_handle));//BUSY状态下无法写寄存器 
	cmd.Instruction=0xD8;//指定命令 
	cmd.Address=addr;//指定地址
	cmd.AlternateBytes=0;//无交替字节，随便给
	cmd.AddressSize=QSPI_ADDRESS_24_BITS;//24位地址阶段
	cmd.AlternateBytesSize=QSPI_ALTERNATE_BYTES_8_BITS;//无交替字节，随便给
	cmd.DummyCycles=0;//空周期需要0个 
	cmd.InstructionMode=QSPI_INSTRUCTION_4_LINES;//命令阶段采用4线发送，符合手册 
	cmd.AddressMode=QSPI_ADDRESS_4_LINES;//地址阶段四线，符合手册 
	cmd.AlternateByteMode=QSPI_ALTERNATE_BYTES_NONE;//无交替字节阶段，符合手册 
	cmd.DataMode=QSPI_DATA_NONE;//无数据阶段，符合手册 
	cmd.NbData=0;//无数据阶段 
	cmd.DdrMode=QSPI_DDR_MODE_DISABLE;//双倍速率模式，即双边沿全部采样，这里器件不支持 
	cmd.DdrHoldHalfCycle=QSPI_DDR_HHC_ANALOG_DELAY;//双倍速率模式下的时钟延迟，这里器件没有双倍速率功能，随便给 
	cmd.SIOOMode=QSPI_SIOO_INST_EVERY_CMD;//指令只第一次发送一次后面无需发送，这个不符合w25q128的特性，它是有很多命令的 
	HAL_QSPI_Command(&qspi_handle,&cmd,HAL_MAX_DELAY);
}

/* 全片擦除函数 */
void qspi_flash_erase_chip(void)
{
	while(!qspi_flash_w25q128_waitStateReg(0x05,0x00,0x01,1));//轮询状态寄存器1，期待它的第0位BUSY为0
	
	qspi_flash_w25q128_wEnable(1);
	
	QSPI_CommandTypeDef cmd;
	while(HAL_QSPI_STATE_BUSY==HAL_QSPI_GetState(&qspi_handle));//BUSY状态下无法写寄存器 
	cmd.Instruction=0xC7;//指定命令 
	cmd.Address=0;//无地址，随便给
	cmd.AlternateBytes=0;//无交替字节，随便给
	cmd.AddressSize=QSPI_ADDRESS_24_BITS;//24位地址阶段
	cmd.AlternateBytesSize=QSPI_ALTERNATE_BYTES_8_BITS;//无交替字节，随便给
	cmd.DummyCycles=0;//空周期需要0个 
	cmd.InstructionMode=QSPI_INSTRUCTION_4_LINES;//命令阶段采用4线发送，符合手册 
	cmd.AddressMode=QSPI_ADDRESS_NONE;//无地址阶段，符合手册 
	cmd.AlternateByteMode=QSPI_ALTERNATE_BYTES_NONE;//无交替字节阶段，符合手册 
	cmd.DataMode=QSPI_DATA_NONE;//无数据阶段，符合手册 
	cmd.NbData=0;//无数据阶段 
	cmd.DdrMode=QSPI_DDR_MODE_DISABLE;//双倍速率模式，即双边沿全部采样，这里器件不支持 
	cmd.DdrHoldHalfCycle=QSPI_DDR_HHC_ANALOG_DELAY;//双倍速率模式下的时钟延迟，这里器件没有双倍速率功能，随便给 
	cmd.SIOOMode=QSPI_SIOO_INST_EVERY_CMD;//指令只第一次发送一次后面无需发送，这个不符合w25q128的特性，它是有很多命令的 
	HAL_QSPI_Command(&qspi_handle,&cmd,HAL_MAX_DELAY);
}

/* 读取函数，没有限制，手册说的 */
void qspi_flash_read(uint32_t addr,uint8_t* data,uint32_t len)
{
	while(!qspi_flash_w25q128_waitStateReg(0x05,0x00,0x01,1));//轮询状态寄存器1，期待它的第0位BUSY为0
	
	QSPI_CommandTypeDef cmd;
	while(HAL_QSPI_STATE_BUSY==HAL_QSPI_GetState(&qspi_handle));//BUSY状态下无法写寄存器 
	cmd.Instruction=0x0B;//指定命令 
	cmd.Address=addr;//指定地址
	cmd.AlternateBytes=0;//无交替字节，随便给
	cmd.AddressSize=QSPI_ADDRESS_24_BITS;//24位地址阶段
	cmd.AlternateBytesSize=QSPI_ALTERNATE_BYTES_8_BITS;//无交替字节，随便给
	cmd.DummyCycles=2;//空周期需要2个,符合手册 
	cmd.InstructionMode=QSPI_INSTRUCTION_4_LINES;//命令阶段采用4线发送，符合手册 
	cmd.AddressMode=QSPI_ADDRESS_4_LINES;//地址阶段四线，符合手册 
	cmd.AlternateByteMode=QSPI_ALTERNATE_BYTES_NONE;//无交替字节阶段，符合手册 
	cmd.DataMode=QSPI_DATA_4_LINES;//数据阶段采用四线发送，符合手册 
	cmd.NbData=len;//有数据阶段 
	cmd.DdrMode=QSPI_DDR_MODE_DISABLE;//双倍速率模式，即双边沿全部采样，这里器件不支持 
	cmd.DdrHoldHalfCycle=QSPI_DDR_HHC_ANALOG_DELAY;//双倍速率模式下的时钟延迟，这里器件没有双倍速率功能，随便给 
	cmd.SIOOMode=QSPI_SIOO_INST_EVERY_CMD;//指令只第一次发送一次后面无需发送，这个不符合w25q128的特性，它是有很多命令的 
	HAL_QSPI_Command(&qspi_handle,&cmd,HAL_MAX_DELAY);
	
	HAL_QSPI_Receive(&qspi_handle,data,HAL_MAX_DELAY);
}

/* 页写入函数，有256字节大小的限制，而且不能跨页，也就是说实际写入<=256 */
static void qspi_flash_page_write(uint32_t addr,uint8_t* data,uint16_t len)
{
	while(!qspi_flash_w25q128_waitStateReg(0x05,0x00,0x01,1));//轮询状态寄存器1，期待它的第0位BUSY为0
	
	qspi_flash_w25q128_wEnable(1);
	
	QSPI_CommandTypeDef cmd;
	while(HAL_QSPI_STATE_BUSY==HAL_QSPI_GetState(&qspi_handle));//BUSY状态下无法写寄存器 
	cmd.Instruction=0x02;//指定命令 
	cmd.Address=addr;//指定地址
	cmd.AlternateBytes=0;//无交替字节，随便给
	cmd.AddressSize=QSPI_ADDRESS_24_BITS;//24位地址阶段
	cmd.AlternateBytesSize=QSPI_ALTERNATE_BYTES_8_BITS;//无交替字节，随便给
	cmd.DummyCycles=0;//无空周期,符合手册 
	cmd.InstructionMode=QSPI_INSTRUCTION_4_LINES;//命令阶段采用4线发送，符合手册 
	cmd.AddressMode=QSPI_ADDRESS_4_LINES;//地址阶段四线，符合手册 
	cmd.AlternateByteMode=QSPI_ALTERNATE_BYTES_NONE;//无交替字节阶段，符合手册 
	cmd.DataMode=QSPI_DATA_4_LINES;//数据阶段采用四线发送，符合手册 
	cmd.NbData=len;//有数据阶段 
	cmd.DdrMode=QSPI_DDR_MODE_DISABLE;//双倍速率模式，即双边沿全部采样，这里器件不支持 
	cmd.DdrHoldHalfCycle=QSPI_DDR_HHC_ANALOG_DELAY;//双倍速率模式下的时钟延迟，这里器件没有双倍速率功能，随便给 
	cmd.SIOOMode=QSPI_SIOO_INST_EVERY_CMD;//指令只第一次发送一次后面无需发送，这个不符合w25q128的特性，它是有很多命令的 
	HAL_QSPI_Command(&qspi_handle,&cmd,HAL_MAX_DELAY);
	
	HAL_QSPI_Transmit(&qspi_handle,data,HAL_MAX_DELAY);
}

/* 写任意字节函数的封装 */
void qspi_flash_write(uint32_t addr,uint8_t* data,uint32_t len)
{
	uint32_t page_remain;
  uint32_t write_len;

	while(len > 0)
	{
			/* 当前地址距离本页末尾还剩多少空间 */
			page_remain = PAGE_SIZE - (addr & (PAGE_SIZE - 1));

			/* 本次最多只能写当前页剩余空间 */
			write_len = (len < page_remain) ? len : page_remain;

			qspi_flash_page_write(addr, data, write_len);

			addr += write_len;
			data += write_len;
			len  -= write_len;
	}
}

/* 让H7内核和W25Q128进入地址映射模式 */
void qspi_flash_Enter_Mmap(void)
{
  while(!qspi_flash_w25q128_waitStateReg(0x05,0x00,0x01,1));//轮询状态寄存器1，期待它的第0位BUSY为0
	
	QSPI_CommandTypeDef           cmd;
	QSPI_MemoryMappedTypeDef  mmapcfg;
	
	while(HAL_QSPI_STATE_BUSY==HAL_QSPI_GetState(&qspi_handle));//BUSY状态下无法写寄存器 
	
	/* 地址映射模式下CMD读命令不需要指定特定的地址和数据长度 */
	cmd.Instruction=0x0B;//指定命令 
	//cmd.Address=addr;//指定地址
	cmd.AlternateBytes=0;//无交替字节，随便给
	cmd.AddressSize=QSPI_ADDRESS_24_BITS;//24位地址阶段
	cmd.AlternateBytesSize=QSPI_ALTERNATE_BYTES_8_BITS;//无交替字节，随便给
	cmd.DummyCycles=2;//空周期需要2个,符合手册 
	cmd.InstructionMode=QSPI_INSTRUCTION_4_LINES;//命令阶段采用4线发送，符合手册 
	cmd.AddressMode=QSPI_ADDRESS_4_LINES;//地址阶段四线，符合手册 
	cmd.AlternateByteMode=QSPI_ALTERNATE_BYTES_NONE;//无交替字节阶段，符合手册 
	cmd.DataMode=QSPI_DATA_4_LINES;//数据阶段采用四线发送，符合手册 
	//cmd.NbData=len;//有数据阶段 
	cmd.DdrMode=QSPI_DDR_MODE_DISABLE;//双倍速率模式，即双边沿全部采样，这里器件不支持 
	cmd.DdrHoldHalfCycle=QSPI_DDR_HHC_ANALOG_DELAY;//双倍速率模式下的时钟延迟，这里器件没有双倍速率功能，随便给 
	cmd.SIOOMode=QSPI_SIOO_INST_EVERY_CMD;//指令只第一次发送一次后面无需发送，这个不符合w25q128的特性，它是有很多命令的 
	mmapcfg.TimeOutActivation=QSPI_TIMEOUT_COUNTER_DISABLE;
	mmapcfg.TimeOutPeriod=0;
	HAL_QSPI_MemoryMapped(&qspi_handle,&cmd,&mmapcfg);
}

/* 让H7内核和W25Q128退出地址映射模式 */
void qspi_flash_Exit_Mmap(void)
{
	HAL_QSPI_Abort(&qspi_handle);
	HAL_QSPI_DeInit(&qspi_handle);
	HAL_QSPI_Init(&qspi_handle);
}
