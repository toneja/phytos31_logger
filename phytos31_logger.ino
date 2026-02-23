// Basic Datalogger Application for METER PHYTOS31 Leaf-Wetness Sensor
//
// Log data every 2 seconds; convert raw voltage output into % wetness
// Voltage output range can be different for each invididual sensor(?)
//

#include <Adafruit_ADS1X15.h>
#include <bluefruit.h>
#include <SparkFun_u-blox_GNSS_Arduino_Library.h>
#include <Wire.h>
#include "SD.h"

Adafruit_ADS1115 ads;
BLEUart bleuart;
SFE_UBLOX_GNSS g_myGNSS;

#define DEBUG 0

int16_t adc0;
double wetRaw;
float wetPercent;
// Observed minimum values: 0.350000, 0.355875
float wetMin = 0.350000;
// Observed maximum values: 1.023969, 1.105875
float wetMax = 1.023969;
String wetness;

File csvFile;
File logFile;

char timestamp[19];

void setup() {
  pinMode(WB_IO2, OUTPUT);
  digitalWrite(WB_IO2, LOW);
  delay(1000);
  digitalWrite(WB_IO2, HIGH);
  Wire.begin();
  serial_init();
  sd_init();
  ads_init();
  ble_init();
  gps_init();
}

void loop() {
  ads_get();
  gps_gettime();
  log_data();
  delay(2000);
}

void serial_init() {
  Serial.begin(115200);
#if DEBUG
  while (!Serial) { delay(100); }
#endif
  Serial.println("*** PHYTOS 31 DATALOGGER ***");
  Serial.println("Sample every 2 seconds...\n");
}

void sd_init(void) {
  Serial.print("Mounting SD Card...");
  for (uint8_t i = 0; i < 10; i++) {
    if (SD.begin()) {
      Serial.println("Card mounted.\n");
      csvFile = SD.open("PHYTOS31.csv", FILE_WRITE);
      logFile = SD.open("PHYTOS31.txt", FILE_WRITE);
      if (logFile) {
        logFile.println("*** PHYTOS 31 DATALOGGER ***");
        logFile.println("Sample every 2 seconds...\n");
        logFile.flush();
      }
      return;
    } else {
      Serial.print(".");
      delay(1000);
      SD.end();
    }
  }
  Serial.println("No SD Card found.\n");
}

void ads_init(void) {
  ads.setGain(GAIN_FOUR); // 4x gain   +/- 1.024V  1 bit = 0.5mV
  ads.begin();
}

void ads_get(void) {
  adc0 = ads.readADC_SingleEnded(0);
  wetRaw = ads.computeVolts(adc0);
  wetPercent = ((wetRaw - wetMin) / (wetMax - wetMin) * 100.0);
}

void ble_init(void) {
  Bluefruit.configPrphBandwidth(BANDWIDTH_MAX);
  Bluefruit.begin();
  Bluefruit.setTxPower(4);    // Check bluefruit.h for supported values
  Bluefruit.setName("PHYTOS31 LOGGER");
  Bluefruit.Periph.setConnectCallback(connect_callback);
  Bluefruit.Periph.setDisconnectCallback(disconnect_callback);
  bleuart.begin();
  Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
  Bluefruit.Advertising.addTxPower();
  Bluefruit.Advertising.addService(bleuart);
  Bluefruit.ScanResponse.addName();
  Bluefruit.Advertising.restartOnDisconnect(true);
  Bluefruit.Advertising.setInterval(32, 244);
  Bluefruit.Advertising.setFastTimeout(30);
  Bluefruit.Advertising.start(0);
}

void connect_callback(uint16_t conn_handle) {
  BLEConnection* connection = Bluefruit.Connection(conn_handle);
  char central_name[32] = { 0 };
  connection->getPeerName(central_name, sizeof(central_name));
  Serial.print("Connected to ");
  Serial.println(central_name);
  if (logFile) {
    logFile.print(timestamp);
    logFile.print(": ");
    logFile.print("Connected to ");
    logFile.println(central_name);
    logFile.flush();
  }
}

void disconnect_callback(uint16_t conn_handle, uint8_t reason) {
  (void) conn_handle;
  (void) reason;
  Serial.print("Disconnected, reason = 0x");
  Serial.println(reason, HEX);
  if (logFile) {
    logFile.print(timestamp);
    logFile.print(": ");
    logFile.print("Disconnected, reason = 0x");
    logFile.println(reason, HEX);
    logFile.flush();
  }
}

void gps_init(void) {
  g_myGNSS.begin();
  g_myGNSS.setI2COutput(COM_TYPE_UBX);
  g_myGNSS.saveConfigSelective(VAL_CFG_SUBSEC_IOPORT);
}

void gps_gettime(void) {
  sprintf(timestamp,
          "%d-%02d-%02d,%02d:%02d:%02d",
          g_myGNSS.getYear(), g_myGNSS.getMonth(), g_myGNSS.getDay(),
          g_myGNSS.getHour(), g_myGNSS.getMinute(), g_myGNSS.getSecond());
#if DEBUG
  Serial.print("GPS Timestamp: ");
  Serial.println(timestamp);
#endif
}

void log_data(void) {
#if DEBUG
  Serial.print("WETNESS VOLTAGE: ");
  Serial.print(wetRaw, 6);
  Serial.println(" V");
#endif
  Serial.print("WETNESS PERCENTAGE: ");
  wetness = (String)wetPercent + "%";
  Serial.println(wetness);
  bleuart.print(wetness);
  if (csvFile) {
    csvFile.print(timestamp);
    csvFile.print(",");
    csvFile.println(wetness);
    csvFile.flush();
  }
}
