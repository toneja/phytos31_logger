// Basic Datalogger Application for METER PHYTOS31 Leaf-Wetness Sensor
//
// Log data every 5 seconds; convert raw voltage output into % wetness
// Voltage output range can be different for each invididual sensor(?)
//
#define DEBUG 0
#define SAMPLING_FREQ 5 // seconds

#include <Adafruit_ADS1X15.h>
#include <bluefruit.h>
#include <SparkFun_u-blox_GNSS_Arduino_Library.h>
#include <Wire.h>
#include <Adafruit_BME680.h>
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

// BLUETOOTH: OTA updates, bleuart output
BLEDfu bledfu;
BLEUart bleuart;

// GPS: log timestamps
SFE_UBLOX_GNSS g_myGNSS;
char timestamp[19];

// Onboard Temperature
Adafruit_BME680 bme;

// Output files
File csvFile;

void setup() {
#if DEBUG
  serial_init();
#endif
  led_init();
  sensor_init();
  // Required
  if (!ble_init()) { led_error(); }
  if (!sd_init())  { led_error(); }
  if (!ads_init()) { led_error(); }
  if (!gps_init()) { led_error(); }
  // Optional
  bme680_init();
}

void loop() {
  digitalWrite(LED_GREEN, HIGH);
  gps_gettime();
  ads_get();
  log_data();
  bme680_get();
  digitalWrite(LED_GREEN, LOW);
  delay(SAMPLING_FREQ * 1000);
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

void led_error(void) {
  while (1) {
    digitalToggle(LED_GREEN);
    delay(500);
    digitalToggle(LED_BLUE);
    delay(500);
  }
}

void sensor_init(void) {
  delay(1000);
  // I2C
  Wire.begin();
  delay(1000); // give em a sec to wake up
}

#if DEBUG
void serial_init() {
  Serial.begin(115200);
  // while (!Serial) { delay(100); }
  Serial.println("*** PHYTOS 31 DATALOGGER ***");
  Serial.println("Sample every ~5 seconds...\n");
}
#endif

bool sd_init(void) {
  if (SD.begin()) {
    csvFile = SD.open("PHYTOS31.csv", FILE_WRITE);
    if (csvFile) {
      if (csvFile.size() == 0) {
        csvFile.println("Date,Time,Wetness,Voltage");
        csvFile.flush();
      }
      return true;
    } else {
#if DEBUG
      Serial.println("ERROR: unable to write to CSV file.");
#endif
    }
  } else {
#if DEBUG
    Serial.println("ERROR: No SD Card found.");
#endif
  }
  return false;
}

bool ads_init(void) {
  ads.setGain(GAIN_FOUR); // 4x gain   +/- 1.024V  1 bit = 0.5mV
  if (ads.begin()) {
    return true;
  } else {
#if DEBUG
    Serial.println("ERROR: ADC Not detected.");
#endif
    return false;
  }
}

void ads_get(void) {
  adc0 = ads.readADC_SingleEnded(0);
  wetRaw = ads.computeVolts(adc0);
  wetPercent = ((wetRaw - wetMin) / (wetMax - wetMin) * 100.0);
}

bool ble_init(void) {
  Bluefruit.configPrphBandwidth(BANDWIDTH_MAX);
  Bluefruit.configPrphConn(92, BLE_GAP_EVENT_LENGTH_MIN, 16, 16);
  if (!Bluefruit.begin()) {
#if DEBUG
    Serial.println("ERROR: Unable to start Bluetooth.");
#endif
    return false;
  }
  Bluefruit.setTxPower(8);    // Check bluefruit.h for supported values
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
  return true;
}

void connect_callback(uint16_t conn_handle) {
  BLEConnection* connection = Bluefruit.Connection(conn_handle);
  char central_name[32] = { 0 };
  connection->getPeerName(central_name, sizeof(central_name));
#if DEBUG
  Serial.print("Connected to ");
  Serial.println(central_name);
#endif
}

void disconnect_callback(uint16_t conn_handle, uint8_t reason) {
  (void) conn_handle;
  (void) reason;
#if DEBUG
  Serial.print("Disconnected, reason = 0x");
  Serial.println(reason, HEX);
#endif
}

bool gps_init(void) {
  if (!g_myGNSS.begin()) {
#if DEBUG
    Serial.println("ERROR: GPS not detected.");
#endif
    return false;
  }
  g_myGNSS.setI2COutput(COM_TYPE_UBX);
  g_myGNSS.saveConfigSelective(VAL_CFG_SUBSEC_IOPORT);
  // Wait on the GPS for timestamps
#if DEBUG
  Serial.print("Searching for GPS fix...");
#endif
  while (g_myGNSS.getFixType() == 0) {
    digitalToggle(LED_GREEN);
    digitalToggle(LED_BLUE);
    delay(200);
#if DEBUG
    Serial.print(".");
#endif
  }
#if DEBUG
  Serial.println(" GPS fix acquired.");
#endif
  return true;
}

void gps_gettime(void) {
  sprintf(timestamp,
          "%d-%02d-%02d,%02d:%02d:%02d",
          g_myGNSS.getYear(), g_myGNSS.getMonth(), g_myGNSS.getDay(),
          g_myGNSS.getHour(), g_myGNSS.getMinute(), g_myGNSS.getSecond());
}

void bme680_init(void) {
  if (!bme.begin(0x76)) {
#if DEBUG
    Serial.println("WARNING: BME not detected.");
#endif
  }
  bme.setTemperatureOversampling(BME680_OS_8X);
  // save power
  bme.setGasHeater(0, 0);
}

void bme680_get(void) {
  bme.performReading();
#if DEBUG
  Serial.print("Temperature = ");
  Serial.print(bme.temperature * 1.8 + 32);
  Serial.println(" *F");
#endif
}

void log_data(void) {
  snprintf(wetness, sizeof(wetness), "%.2f%%", wetPercent);
  if (bleuart.notifyEnabled()) { bleuart.print(wetness); }
  csvFile.print(timestamp);
  csvFile.print(",");
  csvFile.print(wetness);
  csvFile.print(",");
  csvFile.println(wetRaw, 6);
  csvFile.flush();
#if DEBUG
  Serial.print("WETNESS PERCENTAGE: ");
  Serial.println(wetness);
  Serial.print("WETNESS VOLTAGE: ");
  Serial.print(wetRaw, 6);
  Serial.println(" V");
#endif
}
