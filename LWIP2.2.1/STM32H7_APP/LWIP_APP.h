#ifndef __LWIP_APP_H__
#define __LWIP_APP_H__

#include "ethernetif.h"
#include "lwip/netif.h"
#include "lwip/tcpip.h"
#include "app_ethernet.h"
#include "FreeRTOS.h"
#include "task.h"
#if LWIP_DHCP
#include "lwip/dhcp.h"
#endif
#include "lwiperf.h"


void LWIP_2_2_1_INIT(void);
void LWIP_2_2_1_PERFORM_TEST(void);

#endif 
