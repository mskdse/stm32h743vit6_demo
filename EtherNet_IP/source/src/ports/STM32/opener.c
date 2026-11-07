#include <stdint.h>
#include <stdlib.h>
#include "FreeRTOS.h"
#include "task.h"
#include "lwip/netif.h"
#include "lwip/ip4_addr.h"
#include "generic_networkhandler.h"
#include "opener_api.h"
#include "cipethernetlink.h"
#include "ciptcpipinterface.h"
#include "trace.h"
#include "networkconfig.h"
#include "doublylinkedlist.h"
#include "cipconnectionobject.h"
#include "xorshiftrandom.h"

#define OPENER_TASK_PRIORITY       (tskIDLE_PRIORITY + 2U)
#define OPENER_TASK_STACK_SIZE     2048U

static TaskHandle_t opener_ThreadId = NULL;
volatile int g_end_stack = 1;

static void opener_thread(void *argument);

void opener_init(struct netif *netif)
{
    EipStatus eip_status;
    uint8_t iface_mac[6];
    EipUint16 unique_connection_id;

    /* 确保网卡已经启用、链路正常并且已经获得IP */
    if (netif == NULL ||
        !netif_is_up(netif) ||
        !netif_is_link_up(netif) ||
        ip4_addr_isany_val(*netif_ip4_addr(netif)))
    {
        OPENER_TRACE_WARN("Ethernet interface not ready\r\n");
        return;
    }

    /* 初始化连接链表 */
    DoublyLinkedListInitialize(&connection_list,
                               CipConnectionObjectListArrayAllocator,
                               CipConnectionObjectListArrayFree);

    /* 获取MAC地址 */
    eip_status = IfaceGetMacAddress(netif, iface_mac);
    if (eip_status != kEipStatusOk)
    {
        OPENER_TRACE_ERR("Get MAC address failed\r\n");
        return;
    }
		
		uint32_t random_seed = (uint32_t)xTaskGetTickCount() ^ 0x9E3779B9U;
		for(int i = 0; i < 6; i++)
		{
				random_seed = (random_seed << 5) | (random_seed >> 27),
				random_seed ^= iface_mac[i];
		}
		if(random_seed == 0) random_seed = 0x6D2B79F5U;
		SetXorShiftSeed(random_seed);

    /* 产品序列号，正式产品应使用设备唯一序列号 */
    SetDeviceSerialNumber(123456789U);

    /* 生成本次协议栈启动使用的连接ID种子 */
    unique_connection_id = (EipUint16)(
        xTaskGetTickCount() ^
        ((EipUint16)iface_mac[4] << 8) ^
        iface_mac[5]);

    if (unique_connection_id == 0)
        unique_connection_id = 1;

    /* 初始化CIP协议栈 */
    eip_status = CipStackInit(unique_connection_id);
    if (eip_status != kEipStatusOk)
    {
        OPENER_TRACE_ERR("CipStackInit failed: %d\r\n", (int)eip_status);
        return;
    }

    /* 配置Ethernet Link对象 */
    CipEthernetLinkSetMac(iface_mac);

    /* 设置设备主机名 */
    GetHostName(netif, &g_tcpip.hostname);

    /* 获取当前网络参数 */
    eip_status = IfaceGetConfiguration(
        netif, &g_tcpip.interface_configuration);

    if (eip_status != kEipStatusOk)
    {
        OPENER_TRACE_ERR("Get network configuration failed\r\n");
        ShutdownCipStack();
        return;
    }

    /* 初始化EtherNet/IP网络处理层 */
    eip_status = NetworkHandlerInitialize();
    if (eip_status != kEipStatusOk)
    {
        OPENER_TRACE_ERR("NetworkHandlerInitialize failed: %d\r\n",
                         (int)eip_status);
        ShutdownCipStack();
        return;
    }

    g_end_stack = 0;

    /* 创建FreeRTOS任务 */
    if (xTaskCreate(opener_thread,
                    "OpENer",
                    OPENER_TASK_STACK_SIZE,
                    netif,
                    OPENER_TASK_PRIORITY,
                    &opener_ThreadId) != pdPASS)
    {
        OPENER_TRACE_ERR("Create OpENer task failed\r\n");

        g_end_stack = 1;
        NetworkHandlerFinish();
        ShutdownCipStack();
        opener_ThreadId = NULL;
        return;
    }

    OPENER_TRACE_INFO("OpENer task started, free heap: %u\r\n",
                      (unsigned int)xPortGetFreeHeapSize());
}

static void opener_thread(void *argument)
{
    struct netif *netif = (struct netif *)argument;
    EipStatus eip_status;

    while (!g_end_stack)
    {
        eip_status = NetworkHandlerProcessCyclic();

        if (eip_status != kEipStatusOk)
        {
            OPENER_TRACE_ERR("OpENer network handler error\r\n");
            g_end_stack = 1;
            break;
        }

        if (!IfaceLinkIsUp(netif))
        {
            OPENER_TRACE_WARN("Ethernet link down\r\n");
            g_end_stack = 1;
            break;
        }
    }

    /* 关闭网络处理层 */
    NetworkHandlerFinish();

    /* 释放CIP协议栈资源 */
    ShutdownCipStack();

    OPENER_TRACE_INFO("OpENer task stopped\r\n");

    opener_ThreadId = NULL;
    vTaskDelete(NULL);
}
