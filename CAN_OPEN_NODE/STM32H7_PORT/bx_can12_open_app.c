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

static CO_t *CO = NULL;
static uint8_t pendingNodeId;//待生效的ID
static uint8_t activeNodeId;//当前生效的ID
static uint16_t pendingBitRate = 5000;//500Kbps
static CO_ReturnError_t err;
static uint32_t errInfo = 0;
static uint32_t heapMemoryUsed = 0;
static volatile uint32_t canopen_1ms_tick  = 0;
static volatile bool     canopen_sync_flag = false;
static volatile bool     canopen_eeprom_flag = false;
static volatile uint32_t last_tick = 0;
static volatile uint32_t now;

/* 串口接收的缓冲区，字节个数，CAN2主站接收的信息，用于控制主站用 */
static uint16_t uart1_size;
static uint8_t  uart1_buf[16];
static RX_FIFO_TYPE    fdcan2_rsmg;

/* 编辑/读取对象字典里面的映射值 */
static OD_IO_t   io;//输入输出句柄
static OD_size_t countWritten;//实际写入字节
static OD_size_t countRead;//实际读取字节
static uint8_t   tpdo0_indx0 = 11;// 写 0x2000:01
static uint16_t  tpdo0_indx1 = 22;// 写 0x2000:02
static uint8_t   tpdo0_indx3 = 0; // 读 0x2000:03
static uint16_t  tpdo0_indx4 = 0; // 读 0x2000:04
static uint8_t   tpdo0_indx3_last = 0; // 0x2000:03的上一次的值，用于比较然后打印 
static uint16_t  tpdo0_indx4_last = 0; // 0x2000:04的上一次的值，用于比较然后打印 

/* 定义存储条目（哪些变量需要保存）*/
#define EEPROM_STORAGE_ASIZE     3
static CO_storage_entry_t storageEntries[EEPROM_STORAGE_ASIZE] = 
{
	 /* 保存心跳时间 */
	{
		.addr           = &OD_PERSIST_COMM.x1017_producerHeartbeatTime,  // 变量地址
		.len            = sizeof(OD_PERSIST_COMM.x1017_producerHeartbeatTime), // 变量长度
		.subIndexOD     = 0x02,        // 归到 0x1010:02 这组
		.attr           = CO_storage_cmd | CO_storage_restore | CO_storage_auto, // 属性
	},
	/* 保存厂商ID */
	{
		.addr           = &OD_PERSIST_COMM.x1018_identity.vendor_ID,
		.len            = sizeof(OD_PERSIST_COMM.x1018_identity.vendor_ID),
		.subIndexOD     = 0x02,
		.attr           = CO_storage_cmd | CO_storage_restore,
	},
	/* 保存设备的NodeID,该ID上电的时候借助这个结构读出来然后我们更新之后断电前写入 */
	{
		.addr           = &pendingNodeId,
		.len            = sizeof(pendingNodeId),
		.subIndexOD     = 0x02,
		.attr           = CO_storage_cmd | CO_storage_restore | CO_storage_auto,
	}
};
static CO_storage_t storage;

static void Error_Handler(void)
{
  while(1)
  {
  }
}
static bool_t LSS_cfgStore_callback(void* object, uint8_t id, uint16_t bitRate);

/* 初始化can open协议栈 */
void bx_can12_open_app_init(void)
{
	/* 初始化CAN1从站-CAN2主站的硬件外设驱动 */
	bx_can12_init(true,true);
	
	/* 创建一个CAN OPEN对象,heapMemoryUsed表示分配的字节个数 */
	CO = CO_new(NULL, &heapMemoryUsed);
	if(CO == NULL) Error_Handler();
  
	/* 让CAN模块进入配置态，实际我们里面什么都不用干，或者你想真的把CAN模块工作在配置态也行 */
	CO_CANsetConfigurationMode(FDCAN1);
	
	/* 禁用CAN外设，里面我们也不用干啥，复位个标志位就行，或者你想真的把CAN模块失能也行 */
	CO_CANmodule_disable(CO->CANmodule);
  
	/* 初始化CAN模块，pendingBitRate填写实际的波特率，保持一致，或者是你真的想用这个参数初始化CAN模块也行 */
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
	
	/* 初始化EEPROM及存储条目，该函数的调用必须在CO_CANopenInit和CO_LSSinit之前，它会恢复我的ID和其他参数 */
	err = CO_storageEeprom_init
	(
	  &storage, 
	  CO->CANmodule, 
	  NULL, 
    OD_ENTRY_H1010_storeParameters, 
    OD_ENTRY_H1011_restoreDefaultParameters,
    storageEntries, 
		EEPROM_STORAGE_ASIZE, 
		&errInfo
	);
	if(err != CO_ERROR_NO  && err != CO_ERROR_DATA_CORRUPT) Error_Handler();
  
	/* 初始化LSS协议的ID和波特率这两个参数，假如有eeprom的话我们通过此协议动态修改出厂的这两个信息 */
	err = CO_LSSinit(CO,&lssAddress,&pendingNodeId,&pendingBitRate);
	if(err != CO_ERROR_NO) Error_Handler();
	
	/* 注册LSS协议保存参数的回调函数 */
	CO_LSSslave_initCfgStoreCall(CO->LSSslave, NULL,LSS_cfgStore_callback);
  
	/* 确定当前生效的ID，之所以需要两个ID是为了动态分配，因为我们在运行的时候ID不可更改，只有下次上电的时候才可更改
     有了两个ID之后activeNodeId代表当前生效的ID，pendingNodeId代表待生效的ID。pendingNodeId上电先从EEPROM里面读出来
		 然后赋值给activeNodeId，随后如果需要修改设备ID的话修改的是pendingNodeId而不是activeNodeId这样才能满足当前ID不可
		 更改的要求，pendingNodeId更改后断电的时候再写入eeprom，然后下次上电再读出来就完成了ID的更新
  */
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
	usart1_my_printf("TPDO0 id=0x%x 0x2000:01->%d wlen=%d\r\n",0x180+activeNodeId,tpdo0_indx0,countWritten);
	OD_getSub(OD_ENTRY_H2000_TPDO_MY_DATA, 2, &io, true);
	io.write(&io.stream, &tpdo0_indx1, sizeof(tpdo0_indx1), &countWritten);
	usart1_my_printf("TPDO0 id=0x%x 0x2000:02->%d wlen=%d\r\n",0x180+activeNodeId,tpdo0_indx1,countWritten);
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
				usart1_my_printf("RPDO0 id=0x%x 0x2000:03->%d rlen=%d\r\n",0x200+activeNodeId,tpdo0_indx3,countRead);
			}
			OD_getSub(OD_ENTRY_H2000_TPDO_MY_DATA, 4, &io, true);
			io.read(&io.stream,&tpdo0_indx4,sizeof(tpdo0_indx4),&countRead);
			if(tpdo0_indx4!=tpdo0_indx4_last)
			{
				tpdo0_indx4_last=tpdo0_indx4;
				usart1_my_printf("RPDO0 id=0x%x 0x2000:04->%d rlen=%d\r\n",0x200+activeNodeId,tpdo0_indx4,countRead);
			}
		}
		
		/* 每1s处理一次eeprom的自动保存事件，如果变量有自动保存属性并且有更新则会进行更新 */
		if(canopen_eeprom_flag)
		{
			canopen_eeprom_flag=false;
			CO_storageEeprom_auto_process(&storage,false);
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

/* LSS保存参数的回调，我们需要更新pendingNodeId，然后主站可以发送指令去保存pendingNodeId到EEPROM里面。
   或者说这里我们可以直接立马就更新EEPROM，更新完我需要复位通信。
*/
static bool_t LSS_cfgStore_callback(void* object, uint8_t id, uint16_t bitRate)
{
    (void)object;
    (void)bitRate;
    
    /* 更新 pendingNodeId，我设置了自动保存属性，所以说该变量更新后主循环会自动扫描并保存 */
    pendingNodeId = id;
 
    return true;
}

/* 放到1ms的定时器里面一直扫描，为从站协议栈提供心跳，为主站提供SYNC的发送时机 */
void bx_can12_open_app_prc_1ms(void)
{
	static uint8_t canopen_sync_tick=0;
	static uint16_t canopen_eeprom_tick=0;
	
	canopen_1ms_tick++;//心跳节拍
	
	if(++canopen_sync_tick>=100)//模拟的主站100ms发送一次SYNC信号
	{
		canopen_sync_tick=0;
		canopen_sync_flag=true;
	}
	
	if(++canopen_eeprom_tick>=1000)//1s扫描一次需不需要更新eeprom
	{
		canopen_eeprom_tick=0;
		canopen_eeprom_flag=true;
	}
}
