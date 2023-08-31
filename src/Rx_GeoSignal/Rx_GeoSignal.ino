/*
 * This code borrows from the following libraries:
 * https://github.com/sparkfun/SparkFun_ADS122C04_ADC_Arduino_Library
 * https://github.com/sparkfun/SparkFun_ADIN1110_Arduino_Library
 * https://github.com/sparkfun/SparkFun_SerLCD_Arduino_Library
 * https://github.com/sparkfun/Arduino_Apollo3 
 * The licenses were retained, if any license infringement occured kindly leave a message to the owner of this git repository (diplodocuslongus).
 */ 
/*
The MIT License (MIT)

Copyright (c) 2020 SparkFun Electronics

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Analog Devices, firmware and software is subject to software license agreement. See SLA pdf included with this repo. License terms outlines in: 2021-05-20-LWSCADIN1110 Click Thru SLA .pdf


*/
/*---------------------------------------------------------------------------
 *
 * Copyright (c) 2020, 2021 Analog Devices, Inc. All Rights Reserved.
 * This software is proprietary to Analog Devices, Inc.
 * and its licensors.By using this software you agree to the terms of the
 * associated Analog Devices Software License Agreement.
 *
 *---------------------------------------------------------------------------
 */
//
#include "sfe_spe_advanced.h"
#include <Wire.h>
#include <SerLCD.h>
/* Extra 4 bytes for FCS and 2 bytes for the frame header */
#define MAX_FRAME_BUF_SIZE  (MAX_FRAME_SIZE + 4 + 2)

#define MAC_ADDR_0_0        (0x00)
#define MAC_ADDR_0_1        (0xE0)
#define MAC_ADDR_0_2        (0x22)
#define MAC_ADDR_0_3        (0xFE)
#define MAC_ADDR_0_4        (0xDA)
#define MAC_ADDR_0_5        (0xC9)

#define MAC_ADDR_1_0        (0x00)
#define MAC_ADDR_1_1        (0xE0)
#define MAC_ADDR_1_2        (0x22)
#define MAC_ADDR_1_3        (0xFE)
#define MAC_ADDR_1_4        (0xDA)
#define MAC_ADDR_1_5        (0xCA)

uint8_t macAddr[2][6] = {
    {MAC_ADDR_0_0, MAC_ADDR_0_1, MAC_ADDR_0_2, MAC_ADDR_0_3, MAC_ADDR_0_4, MAC_ADDR_0_5},
    {MAC_ADDR_1_0, MAC_ADDR_1_1, MAC_ADDR_1_2, MAC_ADDR_1_3, MAC_ADDR_1_4, MAC_ADDR_1_5},
};

sfe_spe_advanced adin1110;
SerLCD lcd;

/* Number of buffer descriptors to use for both Tx and Rx in this example */
#define BUFF_DESC_COUNT     (6)

HAL_ALIGNED_PRAGMA(4)
static uint8_t rxBuf[BUFF_DESC_COUNT][MAX_FRAME_BUF_SIZE] HAL_ALIGNED_ATTRIBUTE(4);

/* Example configuration */
uint8_t sample_data_num = 0;
uint8_t txBuffidx = 0;
adi_eth_BufDesc_t       rxBufDesc[BUFF_DESC_COUNT];

char display_text[100];
char display_updated = false;

static void rxCallback(void *pCBParam, uint32_t Event, void *pArg)
{
    adin1110_DeviceHandle_t hDevice = (adin1110_DeviceHandle_t)pCBParam;
    adi_eth_BufDesc_t       *pRxBufDesc;
    uint32_t                idx;

    pRxBufDesc = (adi_eth_BufDesc_t *)pArg;

//    Serial.print("Recieved: ");
//
//    Serial.printf((char *)&pRxBufDesc->pBuf[14]);
    memset(display_text, '\0', 100);
    strncpy(display_text, (char *)&pRxBufDesc->pBuf[14], 99);
    display_updated = true;

//    Serial.println();
    
    /* Since we're not doing anything with the Rx buffer in this example, */
    /* we are re-submitting it to the queue. */
    adin1110.submitRxBuffer(pRxBufDesc);
}

volatile adi_eth_LinkStatus_e    linkStatus;
void cbLinkChange(void *pCBParam, uint32_t Event, void *pArg)
{
    linkStatus = *(adi_eth_LinkStatus_e *)pArg;
    memset(display_text, '\0', 100);
    if(linkStatus ==ADI_ETH_LINK_STATUS_UP )
    {
//      lcd.print("Connected!\r\n");
        
        strncpy(display_text, "\r\nConnected", 99);
        
    }
    else
    {
//      lcd.clear();
//      lcd.print("Disconnected!\r\n");
        strncpy(display_text, "Disconnected", 99);
    }
    display_updated = true;
}

void setup() 
{
    adi_eth_Result_e        result;
    uint32_t                error;
    adin1110_DeviceStruct_t dev;
    adin1110_DeviceHandle_t hDevice = &dev;
    uint32_t                heartbeatCheckTime = 0;
    Wire.begin();
    Wire.setClock(100000);
    lcd.begin(Wire); //Set up the LCD for I2C communication
    lcd.clear(); //Clear the display - this moves the cursor to home position as well
    
    Serial.begin(115200);
    while (!Serial) {
      ; // wait for serial port to connect. Needed for native USB port only
    }
    /****** System Init *****/
        result = adin1110.begin();
    if(result != ADI_ETH_SUCCESS) Serial.println("No MACPHY device found");
    else Serial.println("Rx Adin1110 found!");

    result = adin1110.addAddressFilter(&macAddr[0][0], NULL, 0);
    if(result != ADI_ETH_SUCCESS) Serial.println("adin1110_AddAddressFilter");

    result = adin1110.addAddressFilter(&macAddr[1][0], NULL, 0);
    if(result != ADI_ETH_SUCCESS) Serial.println("adin1110_AddAddressFilter");

    result = adin1110.syncConfig();
    if(result != ADI_ETH_SUCCESS) Serial.println("adin1110_SyncConfig");

    result = adin1110.registerCallback(cbLinkChange, ADI_MAC_EVT_LINK_CHANGE);
    if(result != ADI_ETH_SUCCESS) Serial.println("adin1110_RegisterCallback (ADI_MAC_EVT_LINK_CHANGE)");

    /* Prepare Tx/Rx buffers */
    for (uint32_t i = 0; i < BUFF_DESC_COUNT; i++)
    {
        //Submit All rx buffers
        rxBufDesc[i].pBuf = &rxBuf[i][0];
        rxBufDesc[i].bufSize = MAX_FRAME_BUF_SIZE;
        rxBufDesc[i].cbFunc = rxCallback;
        
        result = adin1110.submitRxBuffer(&rxBufDesc[i]);
    }

    result = adin1110.enable();
    if(result != ADI_ETH_SUCCESS) Serial.println("Device enable error");

    /* Wait for link to be established */
    lcd.print("Waiting for connection...");
    unsigned long prev = millis();
    unsigned long now;
    while (linkStatus != ADI_ETH_LINK_STATUS_UP)
    {
        now = millis();
        if( (now - prev) >= 1000)
        {
          prev = now;
          lcd.print(".");
        }
    }
}

void loop() {
     digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
     /*
     lcd.setCursor(0, 0);
  // print from 0 to 9:
  for (int thisChar = 0; thisChar < 10; thisChar++) {
    lcd.print(thisChar);
    delay(500);
  }

  // set the cursor to (16,1):
  lcd.setCursor(16, 1);
  // set the display to automatically scroll:
  lcd.autoscroll();
  // print from 0 to 9:
  for (int thisChar = 0; thisChar < 10; thisChar++) {
    lcd.print(thisChar);
    delay(500);
  }
  // turn off automatic scrolling
  lcd.noAutoscroll();

  // clear screen for the next loop:
  lcd.clear();
  */
  
     if(display_updated)
     {
        lcd.clear();
        int n = 0;
        int output_size = strnlen(display_text, MAX_FRAME_BUF_SIZE);
        const int chunk_size = 16;
        lcd.clear();
        while(output_size>chunk_size){
          lcd.write((uint8_t *)&display_text[n], chunk_size);
          output_size -= chunk_size;
          n += chunk_size;
        }
        lcd.write((uint8_t *)&display_text[n], output_size);
       display_updated = false;
     }
    Serial.println(display_text);
  
     delay(5);
}
