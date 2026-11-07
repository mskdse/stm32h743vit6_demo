#include "wm8978.h"

void wm8978_borad_init(void)
{
	__HAL_RCC_SPI2_CLK_ENABLE();
	__HAL_RCC_DMA2_CLK_ENABLE();
	__HAL_RCC_GPIOB_CLK_ENABLE();
	__HAL_RCC_GPIOC_CLK_ENABLE();
}

void wm8978_init(void)
{
}
