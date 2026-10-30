#ifndef _ETH_PHY_H_
#define _ETH_PHY_H_

#include "stm32h7xx_hal.h"

#define  ETH_PHY_MAX_DEV_ADDR                 2 //我的单片机顶多驱动2个网口，他的物理层地址顶多也就0/1

#define  ETH_PHY_STATUS_READ_ERROR            ((int32_t)-5)
#define  ETH_PHY_STATUS_WRITE_ERROR           ((int32_t)-4)
#define  ETH_PHY_STATUS_ERROR                 ((int32_t)-1)
#define  ETH_PHY_STATUS_OK                    ((int32_t) 0)
#define  ETH_PHY_STATUS_LINK_DOWN             ((int32_t) 1)
#define  ETH_PHY_STATUS_100MBITS_FULLDUPLEX   ((int32_t) 2)
#define  ETH_PHY_STATUS_100MBITS_HALFDUPLEX   ((int32_t) 3)
#define  ETH_PHY_STATUS_10MBITS_FULLDUPLEX    ((int32_t) 4)
#define  ETH_PHY_STATUS_10MBITS_HALFDUPLEX    ((int32_t) 5)
#define  ETH_PHY_STATUS_AUTONEGO_NOTDONE      ((int32_t) 6)

typedef enum
{
  ETH_PHY_LAN8720A,
  ETH_PHY_DP83848,
  ETH_PHY_COUNT
}ETH_PHY_TYPE;

typedef struct
{
	ETH_PHY_TYPE phy;//选择哪个PHY
	
	uint8_t      phy_addr;//PHY物理地址初始化的时候读出来
	uint8_t      phy_addr_reg;//存储PHY芯片的物理地址的寄存器地址
	
	uint8_t      phy_bsr_reg;//存储PHY芯片的通用基础状态寄存器的地址
	uint8_t      phy_bcr_reg;//存储PHY芯片的通用基础控制寄存器的地址
	
	uint8_t      phy_pscsr_reg;//存储PHY芯片的厂家自定义状态的相关寄存器的地址
	uint32_t      phy_pscsr_reg_auto_neg_complte_mask;//指示自动协商是否完成的位
	uint32_t      phy_pscsr_reg_speed_mask;//指示100M/10M的位
	uint32_t      phy_pscsr_reg_duplex_mask;//指示半/全双工的位
}ETH_PHY_DRIVER_TYPE;

extern ETH_HandleTypeDef EthHandle;
int32_t ETH_PHY_Init(ETH_PHY_TYPE PHY);
int32_t ETH_PHY_GetLinkState(ETH_PHY_TYPE PHY);
int32_t ETH_PHY_StartAutoNego(ETH_PHY_TYPE PHY);
void    ETH_PHY_SetLinkState(ETH_PHY_TYPE PHY,uint32_t LinkState);

#endif
