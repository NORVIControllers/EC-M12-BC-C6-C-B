#include <STM32LowPower.h>
#include <STM32RTC.h>
#include <ModbusMaster.h>
#include <Wire.h>
#include <Adafruit_ADS1X15.h>
#include <SPI.h>
#include "SD.h"
#include <ArduinoJson.h>
#include <string.h>
#include <stdlib.h>

#define TINY_GSM_MODEM_SIM7070

#define SerialMon Serial

#define GSM_AUTOBAUD_MIN 4800
#define GSM_AUTOBAUD_MAX 9600

// Define how you're planning to connect to the internet.
// This is only needed for this example, not in other code.
#define TINY_GSM_USE_GPRS true

#define GSM_PIN ""

const char* topic = "Values";
// Your GPRS credentials
const char apn[] = "dialogbb";
const char gprsUser[] = "";
const char gprsPass[] = "";

// ThingsBoard Cloud MQTT settings
const char* mqttServer = "mqtt.thingsboard.cloud";
const int mqttPort = 1883;
const char* accessToken = "ImY0U9ltya5kLdHO6lbs";  // Device token from ThingsBoard

#include <TinyGsmClient.h>
#include <PubSubClient.h>

// Just in case someone defined the wrong thing..
#if TINY_GSM_USE_GPRS && not defined TINY_GSM_MODEM_HAS_GPRS
#undef TINY_GSM_USE_GPRS
#undef TINY_GSM_USE_WIFI
#define TINY_GSM_USE_GPRS false
#define TINY_GSM_USE_WIFI true
#endif
#if TINY_GSM_USE_WIFI && not defined TINY_GSM_MODEM_HAS_WIFI
#undef TINY_GSM_USE_GPRS
#undef TINY_GSM_USE_WIFI
#define TINY_GSM_USE_GPRS true
#define TINY_GSM_USE_WIFI false
#endif


#define MAC_ADDRESS_SIZE 18 // Assuming MAC address is in format "XX:XX:XX:XX:XX:XX"
byte mac[6];

#define PUBLISH_INTERVAL 60000  // Publish interval in milliseconds (1 minute)


unsigned long lastPublishTime = 0;
unsigned long int mqtt_interval = 30000;

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

#define GSM_POWER PA1
#define GSM_TX PA2
#define GSM_RX PA3
#define DTR_PIN PB12

#define MISO_PIN PA6
#define MOSI_PIN PA7
#define SCLK_PIN PA5

#define BOOST_EN PA4

#define BAT_VOL PB1

// SD Paramerters
#define SD_chipSelect PB0
Sd2Card card;
SdVolume volume;
SdFile root;

TwoWire Wire2(SDA_PIN, SCL_PIN);

//                      RX    TX
HardwareSerial Serial1(RS485_RX, RS485_TX);
HardwareSerial Serial2(GSM_RX, GSM_TX);


#define BATTERY_SCALE 3.06 ;
uint8_t mqttRetryCount = 0;

ModbusMaster node;

void preTransmission()
{
  digitalWrite(FC, 1);
}

void postTransmission()
{
  digitalWrite(FC, 0);
}


boolean mqttConnect() {
  SerialMon.print("Connecting to ThingsBoard MQTT... ");

  // Generate a random client ID
  String clientId = "STM32Client-" + String(random(0xFFFF), HEX);

  // Connect using access token as username and empty password
  if (mqtt.connect(clientId.c_str(), accessToken, "")) {
    SerialMon.println("connected!");
    return true;
  } else {
    SerialMon.print("failed, rc=");
    SerialMon.println(mqtt.state());
    return false;
  }
}

bool Modem_Init_WithRetry(uint8_t maxRetries = 3) {
  for (uint8_t attempt = 1; attempt <= maxRetries; attempt++) {
    Serial.print("Modem init attempt: ");
    Serial.println(attempt);

    Modem_Init();

    // Check if modem responded properly
    Serial2.println("AT");
    if (waitForModemResponse("OK", "ERROR", 3000)) {
      Serial.println("Modem initialized successfully");
      return true;
    }

    Serial.println("Modem init failed, retrying...");
    delay(3000);
  }

  Serial.println("Modem didn't initialize after retries");

  return false;
}

void modemPowerToggle() {
  Serial.println("Toggling modem PWRKEY...");

  digitalWrite(GSM_POWER, LOW);
  delay(1500);                  // 1 second pulse
  digitalWrite(GSM_POWER, HIGH);

  delay(5000);                  // wait for modem to react
}

bool PowerOffModem_WithVerify(uint8_t maxRetries = 3) {
  for (uint8_t attempt = 1; attempt <= maxRetries; attempt++) {
    Serial.print("Modem power OFF attempt: ");
    Serial.println(attempt);

    // Step 1: Graceful software shutdown
    Serial2.println("AT+CPOWD=1");

    if (waitForModemResponse("NORMAL POWER DOWN", NULL, 7000)) {
      // Small delay to allow URCs to finish
      delay(500);

      // Verify modem is really OFF
      Serial2.println("AT");
      if (!waitForModemResponse("OK", NULL, 3000)) {
        Serial.println("Modem is OFF (no response)");
        return true;
      }

      Serial.println("Modem still ON after CPOWD, using fallback PWRKEY");
    } else {
      Serial.println("CPOWD failed, using fallback PWRKEY");
    }

    // Step 2: Hardware PWRKEY toggle as fallback
    modemPowerToggle();

    // Flush UART buffer
    while (Serial2.available()) Serial2.read();

    // Verify modem is OFF
    Serial2.println("AT");
    if (!waitForModemResponse("OK", NULL, 3000)) {
      Serial.println("Modem is OFF after PWRKEY fallback");
      return true;
    }

    Serial.println("Modem still ON, retrying...");
    delay(2000); // small delay before next attempt
  }

  Serial.println("Modem can't be powered OFF after 3 attempts");
  return false;
}

bool waitForModemResponse(const char* successToken,
                          const char* errorToken,
                          uint32_t timeoutMs)
{
  unsigned long start = millis();

  while (millis() - start < timeoutMs) {
    if (Serial2.available()) {
      String resp = Serial2.readString();
      Serial.print(resp);

      if (successToken && resp.indexOf(successToken) >= 0) {
        return true;
      }
      if (errorToken && resp.indexOf(errorToken) >= 0) {
        return false;
      }
    }
  }
  return false; // timeout
}

void setup() {
  Serial.begin(9600);
  pinMode(FC, OUTPUT);
  pinMode(GSM_POWER, OUTPUT);
  pinMode(DTR_PIN, OUTPUT);
  pinMode(BOOST_EN, OUTPUT);
  pinMode(BAT_VOL, INPUT_ANALOG);
  analogReadResolution(12);
  delay(200);
  Serial.println("Hello...");
  delay(500);

  digitalWrite(DTR_PIN, LOW);

  LowPower.begin();                   //Initiate low power mode

  Serial1.begin(9600);
  delay(100);
  Serial2.begin(9600);
  delay(100);

  node.begin(1, Serial1);                           //Slave ID as 1
  node.preTransmission(preTransmission);
  delay(10);
  node.postTransmission(postTransmission);


  SPI.begin();
  delay(1000);

  if (!Modem_Init_WithRetry()) {
   Serial.println("Going to shutdown due to modem init failure...");
   LowPower.shutdown(10000);  // sleep 10 sec, then retry
  }

  // MQTT Broker setup

  mqtt.setServer(mqttServer, mqttPort);
  mqtt.setKeepAlive(mqtt_interval / 1000);
  mqtt.setSocketTimeout(mqtt_interval / 1000);

  Wire2.begin();
  delay(1000);

}

void loop() {
  Serial.println("System is awake!");
  digitalWrite(BOOST_EN, HIGH);     // Enable RS485 / INA196 / Booster
  delay(5000);

  float battery = readBatteryVoltage();

  Serial.print("Battery Voltage: ");
  Serial.println(battery);

  // Make sure we're still registered on the network
  if (!modem.isNetworkConnected()) {
    SerialMon.println("Network disconnected");
    if (!modem.waitForNetwork(180000L, true)) {
      SerialMon.println(" fail");
      delay(10000);
      return;
    }
    if (modem.isNetworkConnected()) {
      SerialMon.println("Network re-connected");
    }

#if TINY_GSM_USE_GPRS
    // and make sure GPRS/EPS is still connected
    if (!modem.isGprsConnected()) {
      SerialMon.println("GPRS disconnected!");
      SerialMon.print(F("Connecting to "));
      SerialMon.print(apn);
      if (!modem.gprsConnect(apn, gprsUser, gprsPass)) {
        SerialMon.println(" fail");
        delay(10000);
        return;
      }
      if (modem.isGprsConnected()) {
        SerialMon.println("GPRS reconnected");
      }
    }
#endif
  }

  if (!mqtt.connected()) {
    SerialMon.println("=== MQTT NOT CONNECTED ===");
    // Reconnect every 10 seconds
    uint32_t t = millis();
    if (t - lastReconnectAttempt > 10000L) {
      lastReconnectAttempt = t;
    if (mqttConnect()) {
      SerialMon.println("MQTT Connected");
      lastReconnectAttempt = 0;
      mqttRetryCount = 0; 
    } else {
      mqttRetryCount++;   
      SerialMon.print("MQTT Retry Count: ");
      SerialMon.println(mqttRetryCount);

      if (mqttRetryCount >= 3) {
        SerialMon.println("MQTT FAILED 3 TIMES -> GOING TO SHUTDOWN");

        // Cleanup before shutdown
        SPI.end();
        delay(1000);
        Wire2.end();
        delay(1000);

        digitalWrite(BOOST_EN, LOW);
        delay(1000);

        if (!PowerOffModem_WithVerify()) {
          Serial.println("Warning: Modem still ON before shutdown");
        }

        LowPower.shutdown(900000);   // Sleep 15 minutes
      }
    }
  }

  delay(100);
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
  doc["Temperature"] = temperature;
  doc["Humidity"] = humidity;
  doc["Battery"] =  battery;

  String jsonString;
  serializeJson(doc, jsonString);

  String topic = "v1/devices/me/telemetry";
  mqtt.publish(topic.c_str(), jsonString.c_str());
  SerialMon.print("Published: ");
  SerialMon.println(jsonString);

  delay(2000);
  Serial.println("Entering low power mode...");

  SPI.end();
  delay(1000);
  Wire2.end();
  delay(1000);


  digitalWrite(BOOST_EN, LOW);
  delay(1000);

  if (!PowerOffModem_WithVerify()) {
     Serial.println("Warning: Modem still ON before shutdown");
  };

  // === Enter low power mode ===
  LowPower.shutdown(900000);


}

float readBatteryVoltage()
{
  const int samples = 10;
  long sum = 0;

  for (int i = 0; i < samples; i++) {
    analogRead(BAT_VOL);   // dummy read for STM32 ADC stability
    delay(2);
    sum += analogRead(BAT_VOL);
    delay(2);
  }

  float raw = sum / (float)samples;

  float vref = 3.3;
  float adcMax = 4095.0;

  float pinVoltage = (raw / adcMax) * vref;

  // final calibrated output
  return pinVoltage * BATTERY_SCALE;
}

void Modem_Init() {
  digitalWrite(GSM_POWER, HIGH);   // Set GSM_RESET pin HIGH to reset
  delay(200);                     // Hold reset for 200ms
  digitalWrite(GSM_POWER, LOW);  // Release reset
  delay(2000);

  SerialMon.println("Wait...");

  // Set GSM module baud rate
  TinyGsmAutoBaud(Serial2, GSM_AUTOBAUD_MIN, GSM_AUTOBAUD_MAX);
  delay(6000);

  SerialMon.println("Initializing modem...");
  modem.restart();

  String modemInfo = modem.getModemInfo();
  SerialMon.print("Modem Info: ");
  SerialMon.println(modemInfo);

#if TINY_GSM_USE_GPRS
  // Unlock your SIM card with a PIN if needed
  if (GSM_PIN && modem.getSimStatus() != 3) {
    modem.simUnlock(GSM_PIN);
  }
#endif

  SerialMon.print("Waiting for network...");
  if (!modem.waitForNetwork()) {
    SerialMon.println(" fail");
    delay(10000);
    return;
  }
  SerialMon.println(" success");

  if (modem.isNetworkConnected()) {
    SerialMon.println("Network connected");
  }

#if TINY_GSM_USE_GPRS
  SerialMon.print(F("Connecting to "));
  SerialMon.print(apn);
  if (!modem.gprsConnect(apn, gprsUser, gprsPass)) {
    SerialMon.println(" fail");
    delay(10000);
    return;
  }
  SerialMon.println(" success");

  if (modem.isGprsConnected()) {
    SerialMon.println("GPRS connected");
  }
#endif

}
