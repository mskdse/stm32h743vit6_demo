#include "LWIP_APP.h"

static struct netif gnetif; /* network interface structure */

void LWIP_2_2_1_INIT(void)
{
  tcpip_init(NULL, NULL);

  ip_addr_t ipaddr;
  ip_addr_t netmask;
  ip_addr_t gw;

#if LWIP_DHCP
  ip_addr_set_zero_ip4(&ipaddr);
  ip_addr_set_zero_ip4(&netmask);
  ip_addr_set_zero_ip4(&gw);
  
  taskENTER_CRITICAL();
  printf("======================================\r\n");
  printf("\r\n[INIT] USE DHCP\r\n");
  printf("======================================\r\n");
  taskEXIT_CRITICAL();
#else
  IP_ADDR4(&ipaddr,IP_ADDR0,IP_ADDR1,IP_ADDR2,IP_ADDR3);
  IP_ADDR4(&netmask,NETMASK_ADDR0,NETMASK_ADDR1,NETMASK_ADDR2,NETMASK_ADDR3);
  IP_ADDR4(&gw,GW_ADDR0,GW_ADDR1,GW_ADDR2,GW_ADDR3);

//  taskENTER_CRITICAL();
//  printf("======================================\r\n");
//  printf("\r\n[INIT] USE STARTIC IP\r\n");
//  printf("  IP Address : %s\r\n", ipaddr_ntoa(&netif->ip_addr));
//  printf("  Subnet Mask: %s\r\n", ipaddr_ntoa(&netif->netmask));
//  printf("  Gateway    : %s\r\n", ipaddr_ntoa(&netif->gw));
//  printf("  MAC Address: %02X:%02X:%02X:%02X:%02X:%02X\r\n",
//          netif->hwaddr[0], netif->hwaddr[1], netif->hwaddr[2],
//          netif->hwaddr[3], netif->hwaddr[4], netif->hwaddr[5]);
//  printf("======================================\r\n");
//  taskEXIT_CRITICAL();
#endif /* LWIP_DHCP */

  /* add the network interface */
  netif_add(&gnetif, &ipaddr, &netmask, &gw, NULL, &ethernetif_init, &tcpip_input);

  /*  Registers the default network interface. */
  netif_set_default(&gnetif);

  ethernet_link_status_updated(&gnetif);

#if LWIP_NETIF_LINK_CALLBACK
  netif_set_link_callback(&gnetif, ethernet_link_status_updated);

  /* 创建网线插拔检测线程 (原生 FreeRTOS) */
  xTaskCreate(ethernet_link_thread,           /* 任务入口函数 */
              "EthLink",                      /* 任务名称 */
              configMINIMAL_STACK_SIZE * 2,   /* 栈大小 (单位：Word，即 4字节) */
              (void *)&gnetif,                /* 传递给任务的参数：网卡结构体指针 */
              tskIDLE_PRIORITY + 3,           /* 任务优先级 (对应 osPriorityNormal) */
              NULL);                          /* 任务句柄，不需要则填 NULL */
#endif

#if LWIP_DHCP
  /* 启动 DHCP 客户端线程 (原生 FreeRTOS) */
  xTaskCreate(DHCP_Thread,                    /* 任务入口函数 */
              "DHCP",                         /* 任务名称 */
              configMINIMAL_STACK_SIZE * 2,   /* 栈大小 (单位：Word) */
              (void *)&gnetif,                /* 传递给任务的参数：网卡结构体指针 */
              tskIDLE_PRIORITY + 2,           /* 任务优先级 (对应 osPriorityBelowNormal) */
              NULL);                          /* 任务句柄 */
#endif
}
