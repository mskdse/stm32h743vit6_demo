#ifndef _ADC12_CONV_H_
#define _ADC12_CONV_H_

#include "stm32h7xx_hal.h"

/* Definition for ADCx clock resources */
#define ADCx                            ADC1
#define ADCx_CLK_ENABLE()               __HAL_RCC_ADC12_CLK_ENABLE()

#define DMAx_CHANNELx_CLK_ENABLE()      __HAL_RCC_DMA2_CLK_ENABLE()

#define ADCx_FORCE_RESET()              __HAL_RCC_ADC12_FORCE_RESET()
#define ADCx_RELEASE_RESET()            __HAL_RCC_ADC12_RELEASE_RESET()

/* Definition for ADCx Channel Pin */
#define ADCx_CHANNEL4_PIN_CLK_ENABLE()   __HAL_RCC_GPIOC_CLK_ENABLE()
#define ADCx_CHANNEL4_PIN                GPIO_PIN_4
#define ADCx_CHANNEL4_GPIO_PORT          GPIOC

#define ADCx_CHANNEL8_PIN_CLK_ENABLE()   __HAL_RCC_GPIOC_CLK_ENABLE()
#define ADCx_CHANNEL8_PIN                GPIO_PIN_5
#define ADCx_CHANNEL8_GPIO_PORT          GPIOC

/* Definition for ADCx's Channel */
#define ADCx_CHANNEL4                    ADC_CHANNEL_4
#define ADCx_CHANNEL8                    ADC_CHANNEL_8


#define ADC12_CH_DATA_SIZE      32

extern volatile uint8_t  ADC12_USE_BUF_INDEX;
extern volatile uint8_t  ADC12_CONV_END_FLAG;
extern uint32_t          ADC12_CH_DATA[2][ADC12_CH_DATA_SIZE];

void adc12_conv_init(void);

#endif
