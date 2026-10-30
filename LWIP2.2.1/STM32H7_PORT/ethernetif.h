#ifndef __ETHERNETIF_H__
#define __ETHERNETIF_H__


#include "lwip/err.h"
#include "lwip/netif.h"

/* Exported types ------------------------------------------------------------*/
/* Structure that include link thread parameters */
/* Exported functions ------------------------------------------------------- */
err_t ethernetif_init(struct netif *netif);
void ethernet_link_thread( void * argument );
#endif
