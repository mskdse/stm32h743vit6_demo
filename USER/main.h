#ifndef _MAIN_H_
#define _MAIN_H_

/* 我现在的程序是运行在QSPI_FLASH还是普通FLASH，如果是QSPIFLASH的话，中断向量表必须拷贝0x9000000地址的 */
#define  MY_FLASH_IS_QSPI_FLASH      1

/* 是否运行LVGL图形库 */
#define USE_LVGL_RUN                 0

#include "stm32h7xx.h"
#include "SEGGER_RTT.h"
#include "EventRecorder.h"
#include <stdio.h>
#include "drvp_led.h"
#include "sram_d2_malloc.h"
#include "drvp_key.h"
//#include "base_timer6.h"
//#include "pwm_timer2.h"
//#include "pwm_timer1.h"
//#include "pwm_in_timer5.h"
#include "drvp_eeprom.h"
#include "ff.h"
#include "qspi_flash.h"
#include "drv_lptimer2.h"
#include "drvp_fmc_lcd.h"
#include "lvgl.h"
#include "lv_port_disp_template.h"
#include "lv_demo_benchmark.h"
#include "bdmamux_pwm.h"
#include "dma1mux_pwm.h"
#include "usart1_dma.h"
#include "spi_flash.h"
#include "can1_fd.h"

#endif
