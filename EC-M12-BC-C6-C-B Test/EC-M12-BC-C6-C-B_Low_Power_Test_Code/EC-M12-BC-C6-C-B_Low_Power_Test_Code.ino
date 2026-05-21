
/*
 * EC-M12-BC-C6-C-B Low Power
 2025.12.31
 */

#include <STM32LowPower.h>
#include <Wire.h>
#include <Adafruit_ADS1X15.h>
#include <SPI.h>
#include "SD.h"

#define SCL_PIN PA9
#define SDA_PIN PA10

#define GSM_POWER PA1
#define GSM_TX PA2
#define GSM_RX PA3

#define MISO_PIN PA6
#define MOSI_PIN PA7
#define SCLK_PIN PA5

#define RS485_RX PB7
#define RS485_TX PB6
#define FC PB5

#define BOOST_EN PA4
#define BAT_READ PB1

// SD Paramerters
#define SD_chipSelect PB0
Sd2Card card;
SdVolume volume;
SdFile root;

TwoWire Wire2(SDA_PIN, SCL_PIN);

HardwareSerial Serial1(RS485_RX, RS485_TX);
HardwareSerial Serial2(GSM_RX, GSM_TX);

uint32_t ShutdownPeriod = 60000;     //Shutdown period in milliseconds


void setup() {
  Serial.begin(9600);
  delay(500);
  pinMode(GSM_POWER, OUTPUT); 
  pinMode(GSM_POWER, OUTPUT); 
  pinMode(BOOST_EN, OUTPUT); 
  Serial.println("EC-M12-BC-C6-C-B Low Power");
 
  digitalWrite(BOOST_EN, LOW);     // Disable RS485 / INA196 / Booster
  delay(1000);
//  digitalWrite(GSM_POWER, HIGH);   // Turn off GSM
//  delay(2000);

  LowPower.begin();                   //Initiate low power mode

  Serial1.begin(9600);
  delay(100); 
  Serial2.begin(9600); 
  delay(100);
  
  SPI.begin();
  delay(2000);

  digitalWrite(GSM_POWER, HIGH);   // Set GSM_RESET pin LOW to reset
  delay(200);                     // Hold reset for 200ms
  digitalWrite(GSM_POWER, LOW);  // Release reset
  delay(2000);

  Wire2.begin();
  delay(1000);

  digitalWrite(BOOST_EN, HIGH);     // Enable RS485 / INA196 / Booster / SD Card
  delay(500);

  SD_CHECK();
  delay(1000);

}

void loop() {
  digitalWrite(FC, HIGH); 
  delay(10);// Make FLOW CONTROL pin HIGH
  Serial1.println("RS485 01 SUCCESS");     // Send RS485 SUCCESS serially
  delay(100);                              // Wait for transmission of data
  digitalWrite(FC, LOW);                   // Receiving mode ON                                                 
  delay(100);
  while (Serial1.available()) {  // Check if data is available
    char c = Serial1.read();     // Read data from RS485
    Serial.write(c);             // Print data on serial monitor
  }
  delay(100);
  
  while (Serial.available()) {
    int inByte = Serial.read();
    Serial2.write(inByte);
  }

  while (Serial2.available()) {
    int inByte = Serial2.read();
    Serial.write(inByte);
  }

  int batReadRaw = analogRead(BAT_READ);
  Serial.print("Bat Read Raw: "); Serial.print(batReadRaw); // Need to be converted this raw value into voltage value
  
  delay(2000);
  Serial.println("Entering low power mode...");

  Serial2.print("AT+CEREG?");
  Serial2.print("AT+CPSMS=1,,,\"00000011\",\"00000101\"");
  Serial2.print("AT+CSCLK=1");

  digitalWrite(GSM_POWER, HIGH);   // Turn off GSM
  delay(2000);
  digitalWrite(BOOST_EN, LOW);     // Disable RS485 / INA196 / Booster / SD Card
  delay(1500);

  Serial2.end();
  delay(1000);
  SPI.end();
  delay(1000);
  Wire2.end();
  delay(1000);
 
  digitalWrite(BOOST_EN, LOW);     // Disable RS485 / INA196 / Booster / SD Card
  delay(1000);
  //digitalWrite(GSM_POWER, HIGH);   // Turn off GSM
  ///delay(2000);                     
  LowPower.shutdown(ShutdownPeriod);         //Enters to the shutdown mode for 2 min.

}


void SD_CHECK(){
  Serial.print("\nInitializing SD card...");
   if (!card.init(SPI_HALF_SPEED, SD_chipSelect)) 
    {   Serial.println("CARD NOT FOUND"); 
    }
    else 
    { 
      Serial.println("SD WORKING"); 
    }
    
    Serial.print("\nCard type: ");
    Serial.println(card.type());
    if (!volume.init(card)) {
      Serial.println("Could not find FAT16/FAT32 partition.\nMake sure you've formatted the card");
      return;
    }
    if (!SD.begin(SD_chipSelect)) {
      Serial.println("Card failed, or not present");
      // don't do anything more:
      return;
    } 

}
