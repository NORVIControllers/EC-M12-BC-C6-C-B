/*
 * EC-M12-BC-C6-C-B
 * 2025.12.31
 */

#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include "SD.h"

// ================= I2C =================
#define SCL_PIN PA9
#define SDA_PIN PA10

// ================= GSM =================
#define GSM_POWER PA1
#define GSM_TX    PA2
#define GSM_RX    PA3

// ================= SPI =================
#define MISO_PIN PA6
#define MOSI_PIN PA7
#define SCLK_PIN PA5

// ================= RS485 =================
#define RS485_RX PB7
#define RS485_TX PB6
#define FC       PB5   // DE + RE pin

// ================= OTHER =================
#define BOOST_EN  PA4
#define BAT_READ PB1

// ================= SD =================
#define SD_chipSelect PB0
Sd2Card card;
SdVolume volume;
SdFile root;

#define ADC_VREF 5.55     // STM32 internal reference
#define ADC_MAX  1023.0
#define DIVIDER   2.0 

// ================= OBJECTS =================
TwoWire Wire2(SDA_PIN, SCL_PIN);
HardwareSerial Serial1(RS485_RX, RS485_TX);
HardwareSerial Serial2(GSM_RX, GSM_TX);

// ======================================================
void SD_CHECK();
void I2C_SCAN();
// ======================================================

void setup() {

  Serial.begin(9600);
  delay(500);
  Serial.println("EC-M12-BC-C6-C-B");

  // ---------- GPIO ----------
  pinMode(BOOST_EN, OUTPUT);
  pinMode(GSM_POWER, OUTPUT);
  pinMode(FC, OUTPUT);

  digitalWrite(BOOST_EN, HIGH);
  digitalWrite(FC, LOW);          // RS485 default RX mode

  // ---------- SERIAL ----------
  Serial1.begin(9600);            // RS485
  delay(100);
  Serial2.begin(9600);            // GSM
  delay(100);

  // ---------- GSM POWER ----------
  digitalWrite(GSM_POWER, HIGH);
  delay(200);
  digitalWrite(GSM_POWER, LOW);
  delay(2000);

  // ---------- I2C ----------
  Wire2.begin();

  // ---------- SPI + SD ----------
  SPI.begin();
  delay(1000);
  SD_CHECK();

  Serial.println("SETUP DONE");
}

// ======================================================
void loop() {

  // ================= RS485 TRANSMIT =================
  digitalWrite(FC, HIGH);     // TX mode
  delay(5);

  Serial1.println("RS485 01 SUCCESS");
  Serial1.flush();            // IMPORTANT

  delay(5);
  digitalWrite(FC, LOW);      // RX mode

  // ================= RS485 RECEIVE =================
  unsigned long startTime = millis();
  while (millis() - startTime < 200) {
    if (Serial1.available()) {
      char c = Serial1.read();
      Serial.write(c);
    }
  }

  // ================= BATTERY READ =================
  int raw = analogRead(BAT_READ);

  float adcV = (raw * ADC_VREF) / ADC_MAX;
  float batV = adcV *  DIVIDER;

  Serial.print("Bat Raw: ");
  Serial.print(raw);
  Serial.print("  Voltage: ");
  Serial.println(batV, 2);

  // ================= GSM BRIDGE =================
  while (Serial.available()) {
    Serial2.write(Serial.read());
  }

  while (Serial2.available()) {
    Serial.write(Serial2.read());
  }

  Serial.println("-----------------------");
  delay(1000);
}

// ======================================================
void SD_CHECK() {

  Serial.print("\nInitializing SD card...");
  if (!card.init(SPI_HALF_SPEED, SD_chipSelect)) {
    Serial.println("CARD NOT FOUND");
    return;
  }

  Serial.println("SD WORKING");
  Serial.print("Card type: ");
  Serial.println(card.type());

  if (!volume.init(card)) {
    Serial.println("FAT16/FAT32 NOT FOUND");
    return;
  }

  if (!SD.begin(SD_chipSelect)) {
    Serial.println("SD BEGIN FAILED");
    return;
  }

  Serial.println("SD READY");
}

// ======================================================
void I2C_SCAN() {

  byte error, address;
  int deviceCount = 0;

  Serial.println("I2C SCAN START");

  for (address = 1; address < 127; address++) {
    Wire2.beginTransmission(address);
    error = Wire2.endTransmission();

    if (error == 0) {
      Serial.print("I2C Device @ 0x");
      if (address < 16) Serial.print("0");
      Serial.println(address, HEX);
      deviceCount++;
      delay(1);
    }
  }

  if (deviceCount == 0)
    Serial.println("NO I2C DEVICES FOUND");
  else
    Serial.println("I2C SCAN DONE");
}
