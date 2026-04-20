#include <STM32LowPower.h>
#include <Wire.h>
#include "RTClib.h"
#include <SPI.h>
#include "SD.h"
#include <ArduinoJson.h>
#include <ModbusMaster.h>

#define TINY_GSM_MODEM_SIM7070
#define SerialMon Serial

#define GSM_AUTOBAUD_MIN 4800
#define GSM_AUTOBAUD_MAX 9600

#define TINY_GSM_USE_GPRS true
#define GSM_PIN ""

const char* apn = "dialogbb";
const char* gprsUser = "";
const char* gprsPass = "";

const char* broker = "mqtt.sensoper.net";
const char* username = "Administrator";
const char* password = "Sens1234Oper";

#include <TinyGsmClient.h>
#include <PubSubClient.h>

#ifdef DUMP_AT_COMMANDS
#include <StreamDebugger.h>
StreamDebugger debugger(Serial2, SerialMon);
TinyGsm modem(debugger);
#else
TinyGsm modem(Serial2);
#endif
TinyGsmClient client(modem);
PubSubClient mqtt(client);

uint32_t lastReconnectAttempt = 0;

#define RS485_RX PB7
#define RS485_TX PB6
#define FC PB5
#define SCL_PIN PA9
#define SDA_PIN PA10
#define GSM_POWER PC13
#define GSM_TX PA2
#define GSM_RX PA3
#define MISO_PIN PA6
#define MOSI_PIN PA7
#define SCLK_PIN PA5
#define BOOST_EN PA4
#define SD_chipSelect PA0

Sd2Card card;
SdVolume volume;
SdFile root;

TwoWire Wire2(SDA_PIN, SCL_PIN);
HardwareSerial Serial1(RS485_RX, RS485_TX); // RS485
HardwareSerial Serial2(GSM_RX, GSM_TX);     // GSM

RTC_DS3231 rtc;
char daysOfTheWeek[7][12] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};

ModbusMaster node;

void preTransmission() { digitalWrite(FC, 1); }
void postTransmission() { digitalWrite(FC, 0); }

boolean mqttConnect() {
  SerialMon.print("Connecting to ");
  SerialMon.print(broker);

  String clientId = "GsmClient-" + String(random(0xFFFF), HEX);
  boolean status = mqtt.connect(clientId.c_str(), username, password);

  if (!status) {
    SerialMon.println(" fail");
    return false;
  }
  SerialMon.println(" success");
  return mqtt.connected();
}

void setup() {
  Serial.begin(9600);
  pinMode(FC, OUTPUT);
  pinMode(GSM_POWER, OUTPUT);
  pinMode(BOOST_EN, OUTPUT);

  LowPower.begin();
  Serial1.begin(9600); // RS485
  Serial2.begin(9600); // GSM
  SPI.begin();

  node.begin(1, Serial1); // XY-MD02 default ID = 1
  node.preTransmission(preTransmission);
  node.postTransmission(postTransmission);

  // Power-cycle GSM
  digitalWrite(GSM_POWER, HIGH);
  delay(200);
  digitalWrite(GSM_POWER, LOW);
  delay(2000);

  TinyGsmAutoBaud(Serial2, GSM_AUTOBAUD_MIN, GSM_AUTOBAUD_MAX);
  delay(6000);

  modem.restart();
  modem.waitForNetwork();
  modem.gprsConnect(apn, gprsUser, gprsPass);

  mqtt.setServer(broker, 1881);
}

void loop() {
  Serial.println("System is awake!");
  digitalWrite(BOOST_EN, HIGH); // Enable RS485

  // Ensure network
  if (!modem.isNetworkConnected()) modem.waitForNetwork();
  if (!modem.isGprsConnected()) modem.gprsConnect(apn, gprsUser, gprsPass);

  if (!mqtt.connected()) {
    uint32_t t = millis();
    if (t - lastReconnectAttempt > 10000L) {
      lastReconnectAttempt = t;
      if (mqttConnect()) lastReconnectAttempt = 0;
    }
    return;
  }

  mqtt.loop();

  // === Read Modbus from XY-MD02 ===
  uint8_t result = node.readInputRegisters(0x0001, 2);// read Temp + Hum
  float temperature = NAN, humidity = NAN;

  if (result == node.ku8MBSuccess) {
    int16_t tempRaw = (int16_t)node.getResponseBuffer(0);   // signed
    uint16_t humRaw = node.getResponseBuffer(1);            // unsigned
    temperature = tempRaw / 10.0;
    humidity    = humRaw / 10.0;

    Serial.print("Temperature: "); Serial.println(temperature);
    Serial.print("Humidity: "); Serial.println(humidity);
  } else {
    Serial.println("Modbus read failed");
    Serial.println(result, HEX);
  }

  // Create JSON
  StaticJsonDocument<200> doc;
  doc["Serial"] = "bdc6bdfb-8e7f-440a-a229-1a799a34a9d1";
  doc["Temperature"] = temperature;
  doc["Humidity"] = humidity; 

  String jsonString;
  serializeJson(doc, jsonString);

  String topic = "NORVI/EC-M12-BC-C6-B";
  mqtt.publish(topic.c_str(), jsonString.c_str());
  SerialMon.print("Published: ");
  SerialMon.println(jsonString);

  delay(2000);

  digitalWrite(BOOST_EN, LOW);
  LowPower.shutdown(30000); // sleep 30s
}
