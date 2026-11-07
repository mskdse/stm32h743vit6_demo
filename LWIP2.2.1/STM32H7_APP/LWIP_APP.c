#include "LWIP_APP.h"

/* 初始化LWIP的协议栈完成所有线程的初始化工作，创建好可选的热插拔任务和DHCP任务 */
void LWIP_2_2_1_INIT(struct netif* EnterNetIf)
{
  tcpip_init(NULL, NULL);

  ip_addr_t ipaddr;
  ip_addr_t netmask;
  ip_addr_t gw;

#if LWIP_DHCP
  ip_addr_set_zero_ip4(&ipaddr);
  ip_addr_set_zero_ip4(&netmask);
  ip_addr_set_zero_ip4(&gw);
#else
  IP_ADDR4(&ipaddr,IP_ADDR0,IP_ADDR1,IP_ADDR2,IP_ADDR3);
  IP_ADDR4(&netmask,NETMASK_ADDR0,NETMASK_ADDR1,NETMASK_ADDR2,NETMASK_ADDR3);
  IP_ADDR4(&gw,GW_ADDR0,GW_ADDR1,GW_ADDR2,GW_ADDR3);
#endif /* LWIP_DHCP */

  /* add the network interface */
  netif_add(EnterNetIf, &ipaddr, &netmask, &gw, NULL, &ethernetif_init, &tcpip_input);

  /*  Registers the default network interface. */
  netif_set_default(EnterNetIf);

  ethernet_link_status_updated(EnterNetIf);
	
#if LWIP_DHCP
    taskENTER_CRITICAL();
    printf("===============\r\n");
    printf("[INIT] USE DHCP\r\n");
    printf("===============\r\n");
    taskEXIT_CRITICAL();
#else
    taskENTER_CRITICAL(); 
	  printf("========================\r\n");
	  printf("\r\n[INIT] USE STATIC IP\r\n");
	  printf("  IP Address : %s\r\n", ipaddr_ntoa(&EnterNetIf->ip_addr)); 
	  printf("  Subnet Mask: %s\r\n", ipaddr_ntoa(&EnterNetIf->netmask));
	  printf("  Gateway    : %s\r\n", ipaddr_ntoa(&EnterNetIf->gw)); 
	  printf("  MAC Address: %02X:%02X:%02X:%02X:%02X:%02X\r\n",
				      EnterNetIf->hwaddr[0], EnterNetIf->hwaddr[1], EnterNetIf->hwaddr[2],
				      EnterNetIf->hwaddr[3], EnterNetIf->hwaddr[4], EnterNetIf->hwaddr[5]);
	  printf("========================\r\n");
	  taskEXIT_CRITICAL();	
#endif


#if LWIP_NETIF_LINK_CALLBACK
  netif_set_link_callback(EnterNetIf, ethernet_link_status_updated);

  /* 创建网线插拔检测线程 (原生 FreeRTOS) */
  xTaskCreate(ethernet_link_thread,           /* 任务入口函数 */
              "EthLink",                      /* 任务名称 */
              configMINIMAL_STACK_SIZE * 2,   /* 栈大小 (单位：Word，即 4字节) */
              (void *)EnterNetIf,                /* 传递给任务的参数：网卡结构体指针 */
              tskIDLE_PRIORITY + 3,           /* 任务优先级 (对应 osPriorityNormal) */
              NULL);                          /* 任务句柄，不需要则填 NULL */
#endif

#if LWIP_DHCP
  /* 启动 DHCP 客户端线程 (原生 FreeRTOS) */
  xTaskCreate(DHCP_Thread,                    /* 任务入口函数 */
              "DHCP",                         /* 任务名称 */
              configMINIMAL_STACK_SIZE * 2,   /* 栈大小 (单位：Word) */
              (void *)&EtherNetIf,                /* 传递给任务的参数：网卡结构体指针 */
              tskIDLE_PRIORITY + 2,           /* 任务优先级 (对应 osPriorityBelowNormal) */
              NULL);                          /* 任务句柄 */			
#endif						
}

/* 利用iperf工具测试以太网网速,该函数暂时写死了，需要等待DHCP成功才能用 */
#if LWIP_DHCP
void LWIP_2_2_1_PERFORM_TEST(struct netif* EnterNetIf)
{
	while(1)
	{
			if(dhcp_supplied_address(&EtherNetIf))
			{
				lwiperf_start_tcp_server_default(NULL, NULL); 
				return;
			}
			else vTaskDelay(pdMS_TO_TICKS(500));
	}
}
#endif
