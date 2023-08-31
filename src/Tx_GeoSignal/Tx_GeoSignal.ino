/*
 * This code borrows from the following libraries:
 * https://github.com/sparkfun/SparkFun_ADS122C04_ADC_Arduino_Library
 * https://github.com/sparkfun/SparkFun_ADIN1110_Arduino_Library
 * https://github.com/sparkfun/Arduino_Apollo3 
 * 
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
#include "sfe_spe_advanced.h"
#include <Wire.h>
#include <SparkFun_ADS122C04_ADC_Arduino_Library.h> 

SFE_ADS122C04 geoADC;

// settings for the SPIE ADIN1110 function board
/* Extra 4 bytes for FCS and 2 bytes for the frame header */
#define MAX_FRAME_BUF_SIZE  (MAX_FRAME_SIZE + 4 + 2)
#define MIN_PAYLOAD_SIZE    (46)

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

#define FRAME_HEADER_SIZE 14

uint8_t frame_header[FRAME_HEADER_SIZE] =
{
      MAC_ADDR_0_0, MAC_ADDR_0_1, MAC_ADDR_0_2, MAC_ADDR_0_3, MAC_ADDR_0_4, MAC_ADDR_0_5,
      MAC_ADDR_1_0, MAC_ADDR_1_1, MAC_ADDR_1_2, MAC_ADDR_1_3, MAC_ADDR_1_4, MAC_ADDR_1_5,
      0x08, 0x00,
};

sfe_spe_advanced adin1110;

/* Number of buffer descriptors to use for both Tx and Rx in this example */
#define BUFF_DESC_COUNT     (6)
#define FRAME_SIZE          (1518)

HAL_ALIGNED_PRAGMA(4)
static uint8_t rxBuf[BUFF_DESC_COUNT][MAX_FRAME_BUF_SIZE] HAL_ALIGNED_ATTRIBUTE(4);

HAL_ALIGNED_PRAGMA(4)
static uint8_t txBuf[BUFF_DESC_COUNT][MAX_FRAME_BUF_SIZE] HAL_ALIGNED_ATTRIBUTE(4);
bool txBufAvailable[BUFF_DESC_COUNT];

/* geophone signal parameters */
float maxamplitude = 0.0; // max wave amplitude, as voltage from ADC
float reported_maxamplitude = 0.0; // max amplitude of vibration/wave/shock
float bkground_level = 0.3; // needs to be fine tuned (background level of abs of geophone signal, in mV)
float reported_wvfreq; // for future use (wave frequency)
unsigned long last_report;
unsigned long last_toggle;
uint8_t sample_data_num = 0;
uint8_t txBuffidx = 0;
adi_eth_BufDesc_t       txBufDesc[BUFF_DESC_COUNT];
adi_eth_BufDesc_t       rxBufDesc[BUFF_DESC_COUNT];

// Single Pair Ethernet callback functions
static void txCallback(void *pCBParam, uint32_t Event, void *pArg)
{
    adi_eth_BufDesc_t       *pTxBufDesc;

    pTxBufDesc = (adi_eth_BufDesc_t *)pArg;
    /* Buffer has been written to the ADIN1110 Tx FIFO, we mark it available */
    /* to re-submit to the Tx queue with updated contents. */
    for (uint32_t i = 0; i < BUFF_DESC_COUNT; i++) {
        if (&txBuf[i][0] == pTxBufDesc->pBuf) {
            txBufAvailable[i] = true;
            break;
        }
    }
}

static void rxCallback(void *pCBParam, uint32_t Event, void *pArg)
{
    adin1110_DeviceHandle_t hDevice = (adin1110_DeviceHandle_t)pCBParam;
    adi_eth_BufDesc_t       *pRxBufDesc;
    uint32_t                idx;

    pRxBufDesc = (adi_eth_BufDesc_t *)pArg;

    Serial.println("Recieved: ");
    for(int i = 0; i < pRxBufDesc->trxSize; i++)
    {
      Serial.print(pRxBufDesc->pBuf[i]);
    }
    Serial.println();
  
    /* Since we're not doing anything with the Rx buffer in this example, */
    /* we are re-submitting it to the queue. */
    adin1110.submitRxBuffer(pRxBufDesc);
}

volatile adi_eth_LinkStatus_e linkStatus;
void cbLinkChange(void *pCBParam, uint32_t Event, void *pArg)
{
    linkStatus = *(adi_eth_LinkStatus_e *)pArg;
    if(linkStatus ==ADI_ETH_LINK_STATUS_UP )
    {
      Serial.println("Connected!");
    }
    else
    {
      Serial.println("Disconnected!");
    }
}

void setup() 
{
    Serial.begin(115200);
    while (!Serial) {
      ; // wait for serial port to connect
    }
    Wire.begin();
    // setup first the ADS122C04 ADC
    Serial.println(F("Setting the ADC...."));
    if (geoADC.begin() == false) //Connect to the PT100 using the defaults: Address 0x45 and the Wire port
    {
        Serial.println(F("Qwiic PT100 not detected at default I2C address. Please check wiring. Freezing."));
        while (1)
        ;
    }

    // The ADS122C04 will now be configured for raw mode.
    // We can override the ADC mode using these commands:

    geoADC.setInputMultiplexer(ADS122C04_MUX_AIN1_AIN0); // Route AIN1 and AIN0 to AINP and AINN
    geoADC.setGain(ADS122C04_GAIN_1); // Set the gain to 1
    geoADC.enablePGA(ADS122C04_PGA_DISABLED); // Disable the Programmable Gain Amplifier
    geoADC.setDataRate(ADS122C04_DATA_RATE_175SPS); // Set the data rate (samples per second) to 20 or 45 or 90 or 175 or...
    geoADC.setOperatingMode(ADS122C04_OP_MODE_NORMAL); // Disable turbo mode
    geoADC.setConversionMode(ADS122C04_CONVERSION_MODE_SINGLE_SHOT); // Use single shot mode
    geoADC.setVoltageReference(ADS122C04_VREF_INTERNAL); // Use the internal 2.048V reference
    geoADC.enableInternalTempSensor(ADS122C04_TEMP_SENSOR_OFF); // Disable the temperature sensor
    geoADC.setDataCounter(ADS122C04_DCNT_DISABLE); // Disable the data counter (Note: the library does not currently support the data count)
    geoADC.setDataIntegrityCheck(ADS122C04_CRC_DISABLED); // Disable CRC checking (Note: the library does not currently support data integrity checking)
    geoADC.setBurnOutCurrent(ADS122C04_BURN_OUT_CURRENT_OFF); // Disable the burn-out current
    geoADC.setIDACcurrent(ADS122C04_IDAC_CURRENT_OFF); // Disable the IDAC current
    geoADC.setIDAC1mux(ADS122C04_IDAC1_DISABLED); // Disable IDAC1
    geoADC.setIDAC2mux(ADS122C04_IDAC2_DISABLED); // Disable IDAC2

    geoADC.enableDebugging(Serial); //Enable debug messages on Serial
    geoADC.printADS122C04config(); //Print the configuration
    geoADC.disableDebugging(); //Enable debug messages on Serial
    adi_eth_Result_e resultSPE;
  
    Serial.println(F("ADC successfully set"));

    /****** System Init *****/
    resultSPE = adin1110.begin();
    if(resultSPE != ADI_ETH_SUCCESS) Serial.println("No MACPHY device found");
    else Serial.println("Adin1110 for Tx found!");

    resultSPE = adin1110.addAddressFilter(&macAddr[0][0], NULL, 0);
    if(resultSPE != ADI_ETH_SUCCESS) Serial.println("adin1110_AddAddressFilter");

    resultSPE = adin1110.addAddressFilter(&macAddr[1][0], NULL, 0);
    if(resultSPE != ADI_ETH_SUCCESS) Serial.println("adin1110_AddAddressFilter");

    resultSPE = adin1110.syncConfig();
    if(resultSPE != ADI_ETH_SUCCESS) Serial.println("adin1110_SyncConfig");

    resultSPE = adin1110.registerCallback(cbLinkChange, ADI_MAC_EVT_LINK_CHANGE);
    if(resultSPE != ADI_ETH_SUCCESS) Serial.println("adin1110_RegisterCallback (ADI_MAC_EVT_LINK_CHANGE)");

    /* Prepare Tx/Rx buffers */
    for (uint32_t i = 0; i < BUFF_DESC_COUNT; i++)
    {
        txBufAvailable[i] = true;

        //Submit All rx buffersryzen
        rxBufDesc[i].pBuf = &rxBuf[i][0];
        rxBufDesc[i].bufSize = MAX_FRAME_BUF_SIZE;
        rxBufDesc[i].cbFunc = rxCallback;
        
        resultSPE = adin1110.submitRxBuffer(&rxBufDesc[i]);
    }

    resultSPE = adin1110.enable();
    DEBUG_RESULT("Device enable error", resultSPE, ADI_ETH_SUCCESS);

    /* Wait for link to be established */
    Serial.print("Device Configured, waiting for connection...");
    unsigned long prev = millis();
    unsigned long now;
    do
    {
        now = millis();
        if( (now - prev) >= 1000)
        {
          prev = now;
          Serial.print(".");
        }
    } while (linkStatus != ADI_ETH_LINK_STATUS_UP);
    randomSeed(3.14159);
}

#define diff_lt(a,b,max_diff) ((a>=(b+max_diff)) || (a<=(b-max_diff)))

void loop() {

    geoADC.start(); // Start the conversion
  
    unsigned long start_time = millis(); // Record the start time so we can timeout
    boolean drdy = false; // DRDY (1 == new data is ready)
  
    // Wait for DRDY to go valid (by reading Config Register 2)
    // (You could read the DRDY pin instead, especially if you are using continuous conversion mode.)
    while((drdy == false) && (millis() < (start_time + ADS122C04_CONVERSION_TIMEOUT)))
    {
      delay(5); // was 250, but I2C bus is fine with a much smaller delay
      drdy = geoADC.checkDataReady(); // Read DRDY from Config Register 2
    }
  
    // Check if we timed out
    if (drdy == false)
    {
      Serial.println(F("checkDataReady timed out"));
      return;
    }
  
    // Read the raw (signed) ADC data
    // The ADC data is returned in the least-significant 24-bits
    
    uint32_t raw_ADC_data = geoADC.readADC(); // original
    //int32_t raw_ADC_data = (int32_t) geoADC.readADC();
    // from https://github.com/sparkfun/SparkFun_ADS122C04_ADC_Arduino_Library/blob/main/src/SparkFun_ADS122C04_ADC_Arduino_Library.cpp
    // Print the raw ADC data
    if(0)
    {
        Serial.print(F("The raw ADC data is 0x"));
        // Pad the zeros
        if (raw_ADC_data <= 0xFFFFF) Serial.print(F("0"));
        if (raw_ADC_data <= 0xFFFF) Serial.print(F("0"));
        if (raw_ADC_data <= 0xFFF) Serial.print(F("0"));
        if (raw_ADC_data <= 0xFF) Serial.print(F("0"));
        if (raw_ADC_data <= 0xF) Serial.print(F("0"));
        Serial.println(raw_ADC_data, HEX);
    }
    float volts_1 = ((float)raw_ADC_data) * 244.14e-6; // in mV
    if (volts_1 > 4000)
    {
      volts_1 = volts_1 - 4096;
    }
    if (abs(volts_1) > maxamplitude)
    {
        maxamplitude = abs(volts_1);
    }
        
    Serial.println(volts_1, 7);
    delay(10);
    adi_eth_Result_e        resultSPE;
    unsigned long now;
    bool force_report = false;
    bool alarm_report = false;

    float amplitude = volts_1;
    float wvfreq = (float)random(20); // for test Tx/Rx functionality
    int maxamplitude_dec = ( (int)((abs)(maxamplitude)*100) ) % 100;
    int amplitude_dec = ( (int)((abs)(amplitude)*100) ) % 100;
    int wvfreq_dec = ( (int)(wvfreq*100) ) % 100;
    Serial.print("Amp:");
    Serial.print(amplitude);
    Serial.print(" maxamplitude:");
    Serial.println(maxamplitude);

    if(diff_lt(maxamplitude, reported_maxamplitude, 0.1)) force_report = true;
    if(diff_lt(maxamplitude, bkground_level, 1.0)) alarm_report = true;
    now = millis();
    if(now-last_report >= 100 || force_report || alarm_report)
    {
        char output_string[MAX_FRAME_BUF_SIZE];
        uint16_t dataLength;
                 
        //sprintf(output_string, "Amp:%d.%03d",(int)amplitude, amplitude_dec);
        if (alarm_report)
        {
            sprintf(output_string, "ALARM! ALARM!");
            Serial.print("ALARM ");
        }
        else
        {
            //sprintf(output_string, "Amp:%d.%03d",(int)amplitude, amplitude_dec);
            sprintf(output_string, "Max Amp:%d.%03d",(int)maxamplitude, maxamplitude_dec);
            reported_maxamplitude = maxamplitude;
            reported_wvfreq = wvfreq;
        }
        Serial.print("maxamplitude: ");
        Serial.println(maxamplitude);
        //delay(100); // for test, remove
      if (txBufAvailable[txBuffidx] && linkStatus == ADI_ETH_LINK_STATUS_UP)
      {
        /*
        char output_string[MAX_FRAME_BUF_SIZE];
        uint16_t dataLength;
                 
        //sprintf(output_string, "Amp:%d.%03d",(int)amplitude, amplitude_dec);
        sprintf(output_string, "Max Amp:%d.%03d",(int)maxamplitude, maxamplitude_dec);
        */
        //reported_maxamplitude = maxamplitude;
        //reported_wvfreq = wvfreq;
        last_report = now;
        dataLength = strnlen(output_string, MAX_FRAME_BUF_SIZE);
        output_string[dataLength++] = '\0';
        while(dataLength < MIN_PAYLOAD_SIZE) output_string[dataLength++] = '\0';
        memcpy(&txBuf[txBuffidx][0], frame_header, FRAME_HEADER_SIZE);
        memcpy(&txBuf[txBuffidx][FRAME_HEADER_SIZE], output_string, dataLength);

        txBufDesc[txBuffidx].pBuf = &txBuf[txBuffidx][0];
        txBufDesc[txBuffidx].trxSize = FRAME_HEADER_SIZE+dataLength;
        txBufDesc[txBuffidx].bufSize = MAX_FRAME_BUF_SIZE;
        txBufDesc[txBuffidx].egressCapt = ADI_MAC_EGRESS_CAPTURE_NONE;
        txBufDesc[txBuffidx].cbFunc = txCallback;

        txBufAvailable[txBuffidx] = false;

        resultSPE = adin1110.submitTxBuffer(&txBufDesc[txBuffidx]);
        if (resultSPE == ADI_ETH_SUCCESS)
        {
            txBuffidx = (txBuffidx + 1) % BUFF_DESC_COUNT;
        }
        else
        {
            /* If Tx buffer submission fails (for example the Tx queue */
            /* may be full), then mark the buffer unavailable.  */
            txBufAvailable[txBuffidx] = true;
        }
      }
      else if(linkStatus != ADI_ETH_LINK_STATUS_UP)
      {
        Serial.println("Waiting for link to resume sending");
      }
      else
      {
        Serial.println("All transmit buffers full");
      }
    }

    now = millis();
    if(now-last_toggle >= 1000)
    {
      digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
      last_toggle = now;
    }
    
    delay(100);
}
