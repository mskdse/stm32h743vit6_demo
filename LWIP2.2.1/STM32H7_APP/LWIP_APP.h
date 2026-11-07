#ifndef __LWIP_APP_H__
#define __LWIP_APP_H__

#include "ethernetif.h"
#include "lwip/netif.h"
#include "lwip/tcpip.h"
#include "app_ethernet.h"
#include "FreeRTOS.h"
#include "task.h"
#include "lwip/dhcp.h"
#include "lwiperf.h"

void LWIP_2_2_1_INIT(struct netif* EnterNetIf);

#if LWIP_DHCP
void LWIP_2_2_1_PERFORM_TEST(struct netif* EnterNetIf);
#endif

#endif 
