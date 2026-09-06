/**************************************************************************//**
 * @file     FlashDev.c
 * @brief    Flash Device Description for New Device Flash
 * @version  V1.0.0
 * @date     10. January 2018
 ******************************************************************************/
/*
 * Copyright (c) 2010-2018 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the License); you may
 * not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an AS IS BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
 
#include "FlashOS.H"        // FlashOS Structures

/* 0xD0000000这个地址是FMC的SDRAM的地址，我们借助这个地址
   只是为了欺骗FLM算法而已，它给0xD0000000这个参数地址我们就
	 操作0xD0000000-0xD0000000=0这个FLASH的0地址从而改变外部FLASH里面存储的内容
	 但其实我们的程序从始至终都没有访问过0xD0000000这个地址，然后编译的时候只需要把
	 对应的常量的加载ROM改成0xD0000000地址就行，我们其实就是骗了FLM达到往外部FLASH下载
	 东西的功能，这个跟QSPI有很大差别，QSPI支持MCU-XIP所以MCU能访问地址所以能运行，但是
	 我们这个只能下载比如说字库，图片这些。
*/

struct FlashDevice const FlashDevice = 
{
	FLASH_DRV_VERS, /* 驱动版本，勿修改，这个是 MDK 定的 */
	"H743VIT6_SPI_FLASH", /* 算法名，添加算法到 MDK 安装目录会显示此名字 */
	EXTSPI, /* 设备类型 */
	0xD0000000, /* Flash 起始地址 */
	(16*1024*1024), /* Flash 大小， 16MB */
	(4*1024), /* 编程页大小，这里不需要真的按照256字节给，你可以给大点的单位，我这里取4KB */
	0, /* 保留，必须为 0 */
	0xFF, /* 擦除后的数值 */
	2000, /* 页编程等待时间 */
	6000, /* 扇区擦除等待时间 */
	(32*1024), 0x000000, /* 扇区大小，扇区地址，这玩意要根据FlashOS.h里面的宏定义#define SECTOR_NUM   512
	                        来计算，千万不能按实际的给，就是说这个宏定义千万不能动，否则FLM放到那个目录下根本无法识别出来
                           也就是说即使我的一个扇区其实是4KB但是你这个扇区大小也得按照（容量/512）来给
                           那么这个值毫无疑问是16*1024*1024/512=32*1024	*/
	SECTOR_END
};
