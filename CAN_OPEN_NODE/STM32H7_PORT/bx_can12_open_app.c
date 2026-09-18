#include "bx_can12_open_app.h"

#define NMT_CONTROL \
    (CO_NMT_ERR_ON_ERR_REG | \
     CO_ERR_REG_GENERIC_ERR | \
     CO_ERR_REG_COMMUNICATION)
		 
#define FIRST_HB_TIME        500
#define SDO_SRV_TIMEOUT_TIME 1000
#define SDO_CLI_TIMEOUT_TIME 500
#define SDO_CLI_BLOCK        false
#define OD_STATUS_BITS       NULL

CO_t *CO = NULL;
static uint8_t pendingNodeId = 10;
static uint8_t activeNodeId = 10;
static uint16_t pendingBitRate = 1000;
CO_ReturnError_t err;
uint32_t errInfo = 0;
uint32_t heapMemoryUsed = 0;
volatile uint32_t canopen_1ms_tick  = 0;
volatile bool     canopen_sync_flag = false;
volatile uint32_t last_tick = 0;
volatile uint32_t now;

/* 串口接收的缓冲区，字节个数，CAN2主站接收的信息，用于控制主站用 */
static uint16_t uart1_size;
static uint8_t  uart1_buf[256];
RX_FIFO_TYPE    fdcan2_rsmg;

/* 编辑/读取对象字典里面的映射值 */
OD_IO_t   io;//输入输出句柄
OD_size_t countWritten;//实际写入字节
OD_size_t countRead;//实际读取字节
uint8_t   tpdo0_indx0 = 11;// 写 0x2000:01
uint16_t  tpdo0_indx1 = 22;// 写 0x2000:02
uint8_t   tpdo0_indx3 = 0; // 读 0x2000:03
uint16_t  tpdo0_indx4 = 0; // 读 0x2000:04
uint8_t   tpdo0_indx3_last = 0; // 0x2000:03的上一次的值，用于比较然后打印 
uint16_t  tpdo0_indx4_last = 0; // 0x2000:04的上一次的值，用于比较然后打印 

static void Error_Handler(void)
{
  while(1)
  {
  }
}

/* 初始化can open协议栈 */
void bx_can12_open_app_init(void)
{
	/* 初始化CAN1从站-CAN2主站的硬件外设驱动 */
	bx_can12_init(true,true);
	
	/* 创建一个CAN OPEN对象,heapMemoryUsed表示分配的字节个数 */
	CO = CO_new(NULL, &heapMemoryUsed);
	if(CO == NULL) Error_Handler();
  
	/* 配置CAN OPEN从站，实际我们里面什么都不用干 */
	CO_CANsetConfigurationMode(FDCAN1);
	
	/* 禁用CAN外设，里面我们也不用干啥，复位个标志位就行 */
	CO_CANmodule_disable(CO->CANmodule);
  
	/* 初始化CAN模块，pendingBitRate填写实际的波特率，保持一致 */
	err = CO_CANinit(CO, FDCAN1, pendingBitRate);
	if(err != CO_ERROR_NO) Error_Handler();
  
	/* 根据对象字典里面的参数准备LSS 地址 */
	CO_LSS_address_t lssAddress = 
	{
			.identity = 
		 {
				.vendorID = OD_PERSIST_COMM.x1018_identity.vendor_ID,
				.productCode = OD_PERSIST_COMM.x1018_identity.productCode,
				.revisionNumber = OD_PERSIST_COMM.x1018_identity.revisionNumber,
				.serialNumber = OD_PERSIST_COMM.x1018_identity.serialNumber
		 }
	};
  
	/* 初始化LSS,ID和波特率我们可以手动指定先可以指定我们设定的默认值 */
	err = CO_LSSinit
	(
			CO,
			&lssAddress,
			&pendingNodeId,
			&pendingBitRate
	);
	if(err != CO_ERROR_NO) Error_Handler();
  
	/* 如果设定值与我们提前给的不相符，那就要在这让它们一致 */
	activeNodeId = pendingNodeId;
  
	/* 初始化CAN OPEN协议栈 */
	err = CO_CANopenInit
	(
    CO,                    // CANopen 实例
    NULL,                  // NMT 错误回调（NULL = 不用）
    NULL,                  // NMT 状态变化回调（NULL = 不用）
    OD,                    // 对象字典
    OD_STATUS_BITS,        // 状态位指针（NULL = 不用）
    NMT_CONTROL,           // NMT 控制标志
    FIRST_HB_TIME,         // 第一次心跳时间（500ms）
    SDO_SRV_TIMEOUT_TIME,  // SDO 服务器超时（1000ms）
    SDO_CLI_TIMEOUT_TIME,  // SDO 客户端超时（500ms）
    SDO_CLI_BLOCK,         // SDO 客户端块传输（false = 不用）
    activeNodeId,          // NodeID
    &errInfo               // 错误信息输出
  );
	if(err != CO_ERROR_NO && err != CO_ERROR_NODE_ID_UNCONFIGURED_LSS) Error_Handler();
  
	/* 根据对象字典初始化PDO，包括RPDO和TPDO */
	err = CO_CANopenInitPDO(CO,CO->em,OD,activeNodeId,&errInfo);
	if(err != CO_ERROR_NO && err != CO_ERROR_NODE_ID_UNCONFIGURED_LSS) Error_Handler();
  
	/* 设定CAN的工作模式为normal，这玩意跟我们之前复位标志位相反，需要置位标志位，这里
	   是一一对应的，当然我们也可以加上对外设的操作比如说之前真的把CAN外设设定为配置态，
		 然后现在真的把它设置为默认工作态
	*/
	CO_CANsetNormalMode(CO->CANmodule);
	
	/* 提前给TPDO0的两个映射的参数写一个初始值 */
	OD_getSub(OD_ENTRY_H2000_TPDO_MY_DATA, 1, &io, true);
	io.write(&io.stream, &tpdo0_indx0, sizeof(tpdo0_indx0), &countWritten);
	usart1_my_printf("TPDO0 id=0x18a 0x2000:01->%d wlen=%d\r\n",tpdo0_indx0,countWritten);
	OD_getSub(OD_ENTRY_H2000_TPDO_MY_DATA, 2, &io, true);
	io.write(&io.stream, &tpdo0_indx1, sizeof(tpdo0_indx1), &countWritten);
	usart1_my_printf("TPDO0 id=0x18a 0x2000:02->%d wlen=%d\r\n",tpdo0_indx1,countWritten);
}

/* 放到主循环里面一直扫描 */
void bx_can12_open_app_prc(void)
{
	/* 使用串口调试助手发送HEX包然后提供CAN2发送报文，模拟主站发送 */
    if(usart1_fifo_out(uart1_buf,&uart1_size))
    {
		 usart1_my_printf("---------CAN2_TX: ID=0x%x DLC=0X%X---\r\n",*(uint16_t *)&uart1_buf[0],uart1_buf[2]);
		 for(int i=0;i<uart1_size;i++) usart1_my_printf("0x%x->\r\n",uart1_buf[i]);
		 bx_can12_send_msg_std(FDCAN2,*(uint16_t *)&uart1_buf[0],uart1_buf[2],&uart1_buf[3],0);
    }
    
		/* CAN2模拟主站接收CAN1的报文然后回显方便观察 */
    if(bx_can12_get_msg(FDCAN2,&fdcan2_rsmg))
    {
		 usart1_my_printf("---------CAN2_RX: ID=0x%x DLC=0X%X---\r\n",fdcan2_rsmg.RxHeader.Identifier,fdcan2_rsmg.RxHeader.DataLength);
		 for(int i=0;i<fdcan2_rsmg.RxHeader.DataLength;i++) usart1_my_printf("0x%x->\r\n",fdcan2_rsmg.pdata[i]);
    }
		
		/* 模拟主站向从站定期发送SYNC报文，对象字典里面写的是100000us=100ms，所以主站100ms发送一次SYNC报文 */
		if(canopen_sync_flag)
		{
			canopen_sync_flag=false;
			bx_can12_send_msg_std(FDCAN2,0x80,0,NULL,0);//SYNC报文，就一个ID没有数据，CAN2是我们模拟的主站
			
			/* 模拟两个数据100ms变化一次，当SYNC达到设定次数的时候从站会通过TPDO0上传，我们可以观察时间戳看是不是按对象字典里面设定的SYNC次数 */
			tpdo0_indx0+=1;
			tpdo0_indx1+=2;
      OD_getSub(OD_ENTRY_H2000_TPDO_MY_DATA, 1, &io, true);
			io.write(&io.stream, &tpdo0_indx0, sizeof(tpdo0_indx0), &countWritten);
			OD_getSub(OD_ENTRY_H2000_TPDO_MY_DATA, 2, &io, true);
			io.write(&io.stream, &tpdo0_indx1, sizeof(tpdo0_indx1), &countWritten);
			
			/* 每100ms读取一次RPDO映射的0x2000的索引3和4，获取对应的值，由于是对象字典里面设定是异步发送所以立即生效，有变化时打印方便观察 */
			OD_getSub(OD_ENTRY_H2000_TPDO_MY_DATA, 3, &io, true);
			io.read(&io.stream,&tpdo0_indx3,sizeof(tpdo0_indx3),&countRead);
			if(tpdo0_indx3!=tpdo0_indx3_last)
			{
				tpdo0_indx3_last=tpdo0_indx3;
				usart1_my_printf("RPDO0 id=0x20a 0x2000:03->%d rlen=%d\r\n",tpdo0_indx3,countRead);
			}
			OD_getSub(OD_ENTRY_H2000_TPDO_MY_DATA, 4, &io, true);
			io.read(&io.stream,&tpdo0_indx4,sizeof(tpdo0_indx4),&countRead);
			if(tpdo0_indx4!=tpdo0_indx4_last)
			{
				tpdo0_indx4_last=tpdo0_indx4;
				usart1_my_printf("RPDO0 id=0x20a 0x2000:04->%d rlen=%d\r\n",tpdo0_indx4,countRead);
			}
		}
    
		/* 运行扫描函数，处理非实时任务和SYNC/RPDO/TPDO扫描任务 */
    now= canopen_1ms_tick;
    if(now != last_tick)
    {
			uint32_t timeDiff_us = (now - last_tick) * 1000U;
			last_tick = now;
			
			// 1. 处理非实时任务（NMT、SDO、心跳、EMCY）
			CO_process(CO, false, timeDiff_us, NULL);

			// 2. 处理 SYNC，返回本次是否收到 SYNC
			bool_t syncWas = CO_process_SYNC(CO, timeDiff_us, NULL);

			// 3. 处理 RPDO（接收），同步型 PDO 需要 syncWas 为真
			CO_process_RPDO(CO, syncWas, timeDiff_us, NULL);

			// 4. 处理 TPDO（发送），检查事件/定时器/SYNC 触发
			CO_process_TPDO(CO, syncWas, timeDiff_us, NULL);
    }
}

/* 放到1ms的定时器里面一直扫描，为从站协议栈提供心跳，为主站提供SYNC的发送时机 */
void bx_can12_open_app_prc_1ms(void)
{
	static uint8_t canopen_sync_tick=0;
	
	canopen_1ms_tick++;
	
	if(++canopen_sync_tick>=100)
	{
		canopen_sync_tick=0;
		canopen_sync_flag=true;
	}
}
