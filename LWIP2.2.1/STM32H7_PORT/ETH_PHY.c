#include "ETH_PHY.h"
#include "FreeRTOS.h"
#include "task.h"

/* 为 Keil ARMCC 手动提供 lwIP 所需的 errno 定义 */
__attribute__((section (".RAM_D2")))int errno;

__attribute__((section (".RAM_D2")))ETH_HandleTypeDef EthHandle;

static ETH_PHY_DRIVER_TYPE ETH_PHY_DRIVER[ETH_PHY_COUNT]=
{
	{
		.phy=ETH_PHY_LAN8720A,
		
		.phy_addr_reg=0x12,//存储该PHY的物理地址的寄存器地址号是18
		
		.phy_bsr_reg=0x01,//存储PHY芯片的通用基础状态寄存器的地址
		.phy_bcr_reg=0x00,//存储PHY芯片的通用基础控制寄存器的地址
		
		.phy_pscsr_reg=0x1F,//存储PHY芯片的厂家自定义状态的相关寄存器的地址
		.phy_pscsr_reg_auto_neg_complte_mask=(0x01<<12),//该寄存器指示自动协商是否完成的位
		.phy_pscsr_reg_speed_mask=(0x01<<2),//指示100M/10M的位
		.phy_pscsr_reg_duplex_mask=(0x01<<4)//指示半/全双工的位
	},
	{
		.phy=ETH_PHY_DP83848,
		
		.phy_addr_reg=0x19,
		
		.phy_bsr_reg=0x01,
		.phy_bcr_reg=0x00,
		
		.phy_pscsr_reg=0x10,
		.phy_pscsr_reg_auto_neg_complte_mask=(0x01<<4),
		.phy_pscsr_reg_speed_mask=(0x01<<1),
		.phy_pscsr_reg_duplex_mask=(0x01<<2)
	}
};

/*******************************************************************************
                       PHI IO Functions
*******************************************************************************/
/* 直接摘抄SDK中的以太网的PHY的IO通用函数方便移植 */
/**
  * @brief  Initializes the MDIO interface GPIO and clocks.
  * @param  None
  * @retval 0 if OK, -1 if ERROR
  */
static int32_t ETH_PHY_IO_Init(void)
{
  /* We assume that MDIO GPIO configuration is already done
     in the ETH_MspInit() else it should be done here
  */

  /* Configure the MDIO Clock */
  HAL_ETH_SetMDIOClockRange(&EthHandle);

  return 0;
}

/**
  * @brief  Read a PHY register through the MDIO interface.
  * @param  DevAddr: PHY port address
  * @param  RegAddr: PHY register address
  * @param  pRegVal: pointer to hold the register value
  * @retval 0 if OK -1 if Error
  */
static int32_t ETH_PHY_IO_ReadReg(uint32_t DevAddr, uint32_t RegAddr, uint32_t *pRegVal)
{
  if(HAL_ETH_ReadPHYRegister(&EthHandle, DevAddr, RegAddr, pRegVal) != HAL_OK)
  {
    return -1;
  }

  return 0;
}

/**
  * @brief  Write a value to a PHY register through the MDIO interface.
  * @param  DevAddr: PHY port address
  * @param  RegAddr: PHY register address
  * @param  RegVal: Value to be written
  * @retval 0 if OK -1 if Error
  */
static int32_t ETH_PHY_IO_WriteReg(uint32_t DevAddr, uint32_t RegAddr, uint32_t RegVal)
{
  if(HAL_ETH_WritePHYRegister(&EthHandle, DevAddr, RegAddr, RegVal) != HAL_OK)
  {
    return -1;
  }

  return 0;
}

/* 下面是真正的移植层，通过枚举来区分不同的PHY完成移植，自行翻阅相关PHY手册，引脚定义的话也是 */
static void ETH_PHY_BORAD_Init(ETH_PHY_TYPE PHY)
{
	if(ETH_PHY_DP83848==PHY)
	{
	  GPIO_InitTypeDef GPIO_InitStructure = {0};
	  /* Ethernett MSP init: RMII Mode

				RX_CLK  --------------> PA1
				TXD0    --------------> PB12
				TXD1    --------------> PB13
				RXD0    --------------> PC4
				RXD1    --------------> PC5
				TX_EN   --------------> PB11
				RX_DV   --------------> PA7

				MDC     --------------> PC1
				MDIO    --------------> PA2
		
        RST     --------------> PA5
	  */

	  /* Enable GPIOs clocks */
	  __HAL_RCC_GPIOA_CLK_ENABLE();
		__HAL_RCC_GPIOB_CLK_ENABLE();
	  __HAL_RCC_GPIOC_CLK_ENABLE();
	  __HAL_RCC_GPIOG_CLK_ENABLE();

	  /* Configure PA1, PA2 , PA7 */
	  GPIO_InitStructure.Pin =  GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_7;
	  GPIO_InitStructure.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
	  GPIO_InitStructure.Mode = GPIO_MODE_AF_PP;
	  GPIO_InitStructure.Pull = GPIO_NOPULL ;
	  GPIO_InitStructure.Alternate = GPIO_AF11_ETH;
	  HAL_GPIO_Init(GPIOA, &GPIO_InitStructure);

	  /* Configure PB11, PG12 and PG13 */
	  GPIO_InitStructure.Pin = GPIO_PIN_11 | GPIO_PIN_12 | GPIO_PIN_13;
	  HAL_GPIO_Init(GPIOB, &GPIO_InitStructure);

	  /* Configure PC1, PC4 and PC5 */
	  GPIO_InitStructure.Pin = GPIO_PIN_1 | GPIO_PIN_4 | GPIO_PIN_5;
	  HAL_GPIO_Init(GPIOC, &GPIO_InitStructure);

	  GPIO_InitStructure.Pin =  GPIO_PIN_5;
	  GPIO_InitStructure.Speed = GPIO_SPEED_FREQ_HIGH;
	  GPIO_InitStructure.Mode = GPIO_MODE_OUTPUT_PP;
	  GPIO_InitStructure.Pull = GPIO_PULLUP ;
	  HAL_GPIO_Init(GPIOA, &GPIO_InitStructure);

	  HAL_GPIO_WritePin(GPIOA,GPIO_PIN_5,GPIO_PIN_RESET);
	  vTaskDelay(100);
	  HAL_GPIO_WritePin(GPIOA,GPIO_PIN_5,GPIO_PIN_SET);
	  vTaskDelay(100);

	  /* Enable the Ethernet global Interrupt */
	  HAL_NVIC_SetPriority(ETH_IRQn, 0x5, 0);
	  HAL_NVIC_EnableIRQ(ETH_IRQn);

	  /* Enable Ethernet clocks */
	  __HAL_RCC_ETH1MAC_CLK_ENABLE();
	  __HAL_RCC_ETH1TX_CLK_ENABLE();
	  __HAL_RCC_ETH1RX_CLK_ENABLE();

	  /* 显式选择 RMII 模式 */
		__HAL_RCC_SYSCFG_CLK_ENABLE(); // 确保 SYSCFG 时钟开启
		SYSCFG->PMCR &= ~SYSCFG_PMCR_EPIS_SEL;
		SYSCFG->PMCR |= SYSCFG_PMCR_EPIS_SEL_2;
		/* 关键：虚读一次，确保配置同步到 ETH 外设 */
		(void)SYSCFG->PMCR;
	}
	else if(ETH_PHY_LAN8720A==PHY)
	{
	}
}

/* 初始化函数，完成底层PHY的初始化工作以及找到指定PHY的物理地址 */
int32_t ETH_PHY_Init(ETH_PHY_TYPE PHY)
{
    uint32_t regvalue = 0, addr = 0;
    int32_t status = ETH_PHY_STATUS_ERROR;

	 ETH_PHY_BORAD_Init(PHY);
	 
	 ETH_PHY_IO_Init();
	 
	 ETH_PHY_DRIVER[PHY].phy_addr=ETH_PHY_MAX_DEV_ADDR;//一开始不知道我这个PHY芯片的物理地址是多少，直接给理论上最大值，但是实际上是不可能达到

	 for(addr = 0; addr < ETH_PHY_MAX_DEV_ADDR; addr++)
	 {
	   if(ETH_PHY_IO_ReadReg(addr, ETH_PHY_DRIVER[PHY].phy_addr_reg, &regvalue) < 0)//穷举读取存储PHY芯片地址的物理地址的寄存器地址
	   {
			 status = ETH_PHY_STATUS_ERROR;
			 continue;
	   }

	   if((regvalue & 0x3) == addr)//一旦读取成功，此时的addr就是我的PHY芯片的物理地址，regvalue里面存储的PHY地址值也是，但是为了保险起见需要再次对比
	   {
			 ETH_PHY_DRIVER[PHY].phy_addr = addr;/* 对比成功，将这个值存储起来，这里的&3是因为PHY的物理地址本身就没多少，拿这个单片机来说
																							顶多就双网口0/1，并且我发现这种寄存器存储他物理地址的位都是低位所以不需要专门设定掩码 */
			 status = ETH_PHY_STATUS_OK;
			 break;
	   }
	 }
	 
	 if(ETH_PHY_DRIVER[PHY].phy_addr==ETH_PHY_MAX_DEV_ADDR) status = ETH_PHY_STATUS_ERROR;//没有找到PHY芯片的物理地址，那么返回错误

   return status;
}

/* 获取链路状态，既能检查双工状态又能检查是否插拔因为会读取link指示的位 */
int32_t ETH_PHY_GetLinkState(ETH_PHY_TYPE PHY)
{
  uint32_t readval = 0;

  /* 读取一下PHY的基础通用状态寄存器看一下是否能读出来  */
  if(ETH_PHY_IO_ReadReg(ETH_PHY_DRIVER[PHY].phy_addr, ETH_PHY_DRIVER[PHY].phy_bsr_reg, &readval) < 0) return ETH_PHY_STATUS_READ_ERROR;
  
  /* 通用状态寄存器的第2位存储PHY的链路状态，这个都是通用的位置，看一下是down还是up */
  if((readval & 0x04) == 0)                                                                           return ETH_PHY_STATUS_LINK_DOWN;

  /* 确定是up之后读取一下通用控制寄存器看一下是否能读出来 */
  if(ETH_PHY_IO_ReadReg(ETH_PHY_DRIVER[PHY].phy_addr, ETH_PHY_DRIVER[PHY].phy_bcr_reg, &readval) < 0) return ETH_PHY_STATUS_READ_ERROR;
  
  /* 检查一下是否开启了自动协商，这个bcr寄存器也是通用寄存器，第12位固定是自动协商使能标志位 */
  if(!(readval & (0x01<<12)))
  {
	//这种通用寄存器的定义都是标准的，第13位标志速度，第8位标志双工模式 
    if((readval&(0x01<<13))&&(readval&(0x01<<8))) return ETH_PHY_STATUS_100MBITS_FULLDUPLEX;
    else if (readval&(0x01<<13))                  return ETH_PHY_STATUS_100MBITS_HALFDUPLEX;
    else if (readval&(0x01<<8))                   return ETH_PHY_STATUS_10MBITS_FULLDUPLEX;
    else                                          return ETH_PHY_STATUS_10MBITS_HALFDUPLEX;
  }
  else /* 如果开启了自动协商那么直接读取各个厂家自定义的一些状态读取的寄存器能迅速分辨速度和双工模式以及自动协商是否完成 */
  {
    if(ETH_PHY_IO_ReadReg(ETH_PHY_DRIVER[PHY].phy_addr, ETH_PHY_DRIVER[PHY].phy_pscsr_reg, &readval) < 0)               return ETH_PHY_STATUS_READ_ERROR;

    /* 检查一下自动协商是否完成 */
    if(!(readval & ETH_PHY_DRIVER[PHY].phy_pscsr_reg_auto_neg_complte_mask))                                            return ETH_PHY_STATUS_AUTONEGO_NOTDONE;

    if(!(readval&(ETH_PHY_DRIVER[PHY].phy_pscsr_reg_speed_mask))&&(ETH_PHY_DRIVER[PHY].phy_pscsr_reg_duplex_mask))      return ETH_PHY_STATUS_100MBITS_FULLDUPLEX;
    else if(!(readval&(ETH_PHY_DRIVER[PHY].phy_pscsr_reg_speed_mask))&&!(ETH_PHY_DRIVER[PHY].phy_pscsr_reg_duplex_mask))return ETH_PHY_STATUS_100MBITS_HALFDUPLEX;
    else if((readval&(ETH_PHY_DRIVER[PHY].phy_pscsr_reg_speed_mask))&&(ETH_PHY_DRIVER[PHY].phy_pscsr_reg_duplex_mask))  return ETH_PHY_STATUS_10MBITS_FULLDUPLEX;
    else if((readval&(ETH_PHY_DRIVER[PHY].phy_pscsr_reg_speed_mask))&&!(ETH_PHY_DRIVER[PHY].phy_pscsr_reg_duplex_mask)) return ETH_PHY_STATUS_10MBITS_HALFDUPLEX;
  }
	
	return ETH_PHY_STATUS_10MBITS_HALFDUPLEX;
}

int32_t ETH_PHY_StartAutoNego(ETH_PHY_TYPE PHY)
{
  uint32_t readval = 0;
  int32_t  status = ETH_PHY_STATUS_OK;
	uint32_t count=1000;

  if(ETH_PHY_IO_ReadReg(ETH_PHY_DRIVER[PHY].phy_addr, ETH_PHY_DRIVER[PHY].phy_bcr_reg, &readval) >= 0)
  {
    readval |= (0x01<<12);//第12位固定为开启自动协商的意思，因为是通用的寄存器
    if(ETH_PHY_IO_WriteReg(ETH_PHY_DRIVER[PHY].phy_addr, ETH_PHY_DRIVER[PHY].phy_bcr_reg, readval) < 0) status=ETH_PHY_STATUS_WRITE_ERROR;
		
		while(count--)
		{
			if(ETH_PHY_IO_ReadReg(ETH_PHY_DRIVER[PHY].phy_addr, ETH_PHY_DRIVER[PHY].phy_pscsr_reg, &readval) >= 0)
			{
				if(readval & ETH_PHY_DRIVER[PHY].phy_pscsr_reg_auto_neg_complte_mask) break;
			}
			vTaskDelay(1);
		}
		if(!count) status=ETH_PHY_STATUS_AUTONEGO_NOTDONE;
  }
  else status = ETH_PHY_STATUS_READ_ERROR;

  return status;
}

void ETH_PHY_SetLinkState(ETH_PHY_TYPE PHY,uint32_t LinkState)
{
  uint32_t bcrvalue = 0;
  int32_t status = ETH_PHY_STATUS_OK;

  if(ETH_PHY_IO_ReadReg(ETH_PHY_DRIVER[PHY].phy_addr, ETH_PHY_DRIVER[PHY].phy_bcr_reg, &bcrvalue) >= 0)
  {
    /* 自动协商失败关闭自动协商然后清空速度和双工模式的掩码，然后根据或操作设置四种工作模式 */
    bcrvalue &= ~((0x01<<12) | (0x01<<13) | (0x01<<8));

    if(LinkState == ETH_PHY_STATUS_100MBITS_FULLDUPLEX)      bcrvalue |= ((0x01<<13) | (0x01<<8));
    else if(LinkState == ETH_PHY_STATUS_100MBITS_HALFDUPLEX) bcrvalue |= (0x01<<13);
    else if(LinkState == ETH_PHY_STATUS_10MBITS_FULLDUPLEX)  bcrvalue |= (0x01<<8);
    else status = ETH_PHY_STATUS_ERROR;
  }
  else status = ETH_PHY_STATUS_READ_ERROR;

  if(status == ETH_PHY_STATUS_OK)
  {
    if(ETH_PHY_IO_WriteReg(ETH_PHY_DRIVER[PHY].phy_addr, ETH_PHY_DRIVER[PHY].phy_bcr_reg, bcrvalue) < 0) status = ETH_PHY_STATUS_WRITE_ERROR;
  }
}

/**
  * @brief  This function handles Ethernet interrupt request.
  * @param  None
  * @retval None
  */
void ETH_IRQHandler(void)
{
  HAL_ETH_IRQHandler(&EthHandle);
}
