/*
 * This code borrows from the following libraries:
 * https://github.com/sparkfun/SparkFun_ADS122C04_ADC_Arduino_Library
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
/*
  See more predefined settings at 
  https://github.com/sparkfun/SparkFun_ADS122C04_ADC_Arduino_Library/blob/main/src/SparkFun_ADS122C04_ADC_Arduino_Library.h
  
  Using the Qwiic PT100
  By: Paul Clark (PaulZC)
  Date: May 5th, 2020

  When the ADS122C04 is initialised by .begin, it is configured for raw mode (which disables the IDAC).
  If you want to manually configure the chip, you can. This example demonstrates how.

  The IDAC current source is disabled, the gain is set to 1 and the internal 2.048V reference is selected.
  The conversion is started manually using .start.
  DRDY is checked manually using .checkDataReady.
  The ADC result is read using .readADC.

  readADC returns a uint32_t. The ADC data is returned in the least-significant 24-bits.

  Hardware Connections:
  Plug a Qwiic cable into the PT100 and a BlackBoard
  If you don't have a platform with a Qwiic connection use the SparkFun Qwiic Breadboard Jumper (https://www.sparkfun.com/products/14425)
  Open the serial monitor at 115200 baud to see the output
*/

#include <Wire.h>

#include <SparkFun_ADS122C04_ADC_Arduino_Library.h> // Click here to get the library: http://librarymanager/All#SparkFun_ADS122C0

SFE_ADS122C04 geoADC;

void setup(void)
{
  Serial.begin(115200);
  while (!Serial)
    ; //Wait for user to open terminal
  Serial.println(F("Qwiic PT100 Example"));

  Wire.begin();

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
}

void loop()
{
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
  
  if(0)
  {
  int32_t raw_v = geoADC.readRawVoltage();
  // Convert to Volts (method 1)
  float volts_1 = ((float)raw_v) * 244.14e-9;



  Serial.print(volts_1, 7); // Print the voltage with 7 decimal places
  Serial.println();
  }
 else
  {
  uint32_t raw_ADC_data = geoADC.readADC(); // original
  //int32_t raw_ADC_data = (int32_t) geoADC.readADC();
  // from https://github.com/sparkfun/SparkFun_ADS122C04_ADC_Arduino_Library/blob/main/src/SparkFun_ADS122C04_ADC_Arduino_Library.cpp
  // function int32_t SFE_ADS122C04::readRawVoltage(uint8_t rate)
  //if ((raw_ADC_data.UINT32 & 0x00800000) == 0x00800000)
  //  raw_ADC_data.UINT32 |= 0xFF000000;
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
float volts_1 = ((float)raw_ADC_data) * 244.14e-9;
if (volts_1 > 4.0)
{
  volts_1 = volts_1 - 4.096;
}
Serial.println(volts_1, 7);
  //delay(250); //Don't pound the I2C bus too hard
  }
  
  delay(10);
}
