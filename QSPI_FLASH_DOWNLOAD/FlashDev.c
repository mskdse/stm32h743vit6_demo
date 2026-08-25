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
 
#include "FlashOS.h"        // FlashOS Structures


struct FlashDevice const FlashDevice  =  
{
   FLASH_DRV_VERS,             // Driver Version, do not modify!
   "H743VIT6_QSPI_FLASH",   // Device Name 
   EXTSPI,                     // Device Type                   选择外部QSPI接口
   0x90000000,                 // Device Start Address          QSPI映射空间的起始地址
   0x01000000,                 // Device Size in Bytes         (16MB)
   4096,                       // Programming Page Size         别真给页大小，给扇区大小
   0,                          // Reserved, must be 0
   0xFF,                       // Initial Content of Erased Memory
   2000,                        // Program Page Timeout 100 mSec 扇区编程时间
   3000,                       // Erase Sector Timeout 3000 mSec 扇区擦除时间

   // 扇区描述：4KB扇区，重复4096次（只需要写第一行！）
   0x001000, 0x000000,         // 第一个扇区：大小4KB，起始地址偏移0
   SECTOR_END
};
