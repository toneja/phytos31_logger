// Basic Datalogger Application for METER PHYTOS31 Leaf-Wetness Sensor
//
// Log data every 5 seconds; convert raw voltage output into % wetness
// Voltage output range can be different for each invididual sensor(?)
//
#define DEBUG 0

#include <Adafruit_ADS1X15.h>
#include <bluefruit.h>
#include <SparkFun_u-blox_GNSS_Arduino_Library.h>
#include <Wire.h>
#include "SD.h"

// ADC: ADS1115 -> PHYTOS31
Adafruit_ADS1115 ads;
int16_t adc0;
double wetRaw;
float wetPercent;
// Observed minimum values: 0.350000, 0.355875
float wetMin = 0.350000;
// Observed maximum values: 1.023969, 1.105875
float wetMax = 1.023969;
char wetness[8];

// BLUETOOTH: control sample frequency
BLEDfu bledfu;
BLEUart bleuart;
uint16_t sampling_freq = 5; // seconds
uint16_t new_freq;
String buffer;

// GPS: log timestamps
SFE_UBLOX_GNSS g_myGNSS;
char timestamp[19];

// Output files
File csvFile;
File logFile;

void setup() {
  led_init();
  sensor_init();
  serial_init();
  sd_init();
  ads_init();
  gps_init();
  ble_init();
}

void loop() {
  ads_get();
  gps_gettime();
  log_data();
  if (bleuart.available()) { ble_get(); }
  delay(sampling_freq * 1000);
}

void led_init(void) {
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_BLUE, OUTPUT);
  for (uint8_t i = 0; i < 20; i++) {
    digitalToggle(LED_GREEN);
    delay(100);
    digitalToggle(LED_BLUE);
    delay(100);
  }
}

void sensor_init(void) {
  // 3V3_S
  pinMode(WB_IO2, OUTPUT);
  digitalWrite(WB_IO2, LOW);
  delay(1000);
  digitalWrite(WB_IO2, HIGH);
  // I2C
  Wire.begin();
}

void serial_init() {
  Serial.begin(115200);
#if DEBUG
  while (!Serial) { delay(100); }
#endif
  Serial.println("*** PHYTOS 31 DATALOGGER ***");
  Serial.println("Sample every ~5 seconds...\n");
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
        logFile.println("Sample every ~5 seconds...\n");
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
  Bluefruit.configPrphConn(92, BLE_GAP_EVENT_LENGTH_MIN, 16, 16);
  Bluefruit.begin();
  Bluefruit.setTxPower(4);    // Check bluefruit.h for supported values
  Bluefruit.setName("PHYTOS31 LOGGER");
  Bluefruit.Periph.setConnectCallback(connect_callback);
  Bluefruit.Periph.setDisconnectCallback(disconnect_callback);
  bledfu.begin();
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

void ble_get(void) {
  buffer = "";
  while (bleuart.available()) { buffer += (char)bleuart.read(); }
  new_freq = buffer.toInt() * 1000;
  // Allow new sample frequencies between 1s and 1hr
  if ((new_freq >= 1) && (new_freq <= 60 * 60)) {
    sampling_freq = new_freq;
    Serial.print("BLEUart: Updated sampling frequency: ");
    Serial.print(new_freq);
    Serial.println(" s");
    if (logFile) {
      logFile.print(timestamp);
      logFile.print(": BLEUart: Updated sampling frequency: ");
      logFile.print(new_freq);
      logFile.println(" s");
      logFile.flush();
    }
  }
}

void connect_callback(uint16_t conn_handle) {
  BLEConnection* connection = Bluefruit.Connection(conn_handle);
  char central_name[32] = { 0 };
  connection->getPeerName(central_name, sizeof(central_name));
  Serial.print("Connected to ");
  Serial.println(central_name);
  if (logFile) {
    logFile.print(timestamp);
    logFile.print(": Connected to ");
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
    logFile.print(": Disconnected, reason = 0x");
    logFile.println(reason, HEX);
    logFile.flush();
  }
}

void gps_init(void) {
  g_myGNSS.begin();
  g_myGNSS.setI2COutput(COM_TYPE_UBX);
  g_myGNSS.saveConfigSelective(VAL_CFG_SUBSEC_IOPORT);
  // Wait on the GPS for timestamps
  Serial.print("Searching for GPS fix...");
  while (g_myGNSS.getFixType() == 0) {
    digitalToggle(LED_GREEN);
    digitalToggle(LED_BLUE);
    delay(100);
    Serial.print(".");
  }
  Serial.println(" GPS fix acquired.");
}

void gps_gettime(void) {
  sprintf(timestamp,
          "%d-%02d-%02d,%02d:%02d:%02d",
          g_myGNSS.getYear(), g_myGNSS.getMonth(), g_myGNSS.getDay(),
          g_myGNSS.getHour(), g_myGNSS.getMinute(), g_myGNSS.getSecond());
}

void log_data(void) {
  snprintf(wetness, sizeof(wetness), "%.2f%%", wetPercent);
  Serial.print("WETNESS PERCENTAGE: ");
  Serial.println(wetness);
  bleuart.print(wetness);
  if (csvFile) {
    csvFile.print(timestamp);
    csvFile.print(",");
    csvFile.print(wetness);
    csvFile.print(",");
    csvFile.println(wetRaw);
    csvFile.flush();
  }
#if DEBUG
  Serial.print("WETNESS VOLTAGE: ");
  Serial.print(wetRaw, 6);
  Serial.println(" V");
#endif
}
