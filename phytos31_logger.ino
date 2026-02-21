// Basic Datalogger Application for METER PHYTOS31 Leaf-Wetness Sensor
//
// Log data every 2 seconds; convert raw voltage output into % wetness
// Voltage output range can be different for each invididual sensor(?)
//

#include <Adafruit_ADS1X15.h>
#include <bluefruit.h>

Adafruit_ADS1115 ads;
BLEUart bleuart;

#define DEBUG 1

void setup() {
  Serial.begin(115200);
  while (!Serial) { delay(100); }
  Serial.println("*** PHYTOS 31 DATALOGGER ***");
  Serial.println("Sample every 2 seconds...\n");
  ads.setGain(GAIN_FOUR); // 4x gain   +/- 1.024V  1 bit = 0.5mV
  ads.begin();
  Bluefruit.configPrphBandwidth(BANDWIDTH_MAX);
  Bluefruit.begin();
  Bluefruit.setTxPower(4);    // Check bluefruit.h for supported values
  Bluefruit.setName("PHYTOS 31 DATALOGGER");
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

void loop() {
  int16_t adc0 = ads.readADC_SingleEnded(0);
  double wetRaw = ads.computeVolts(adc0);
  // Observed minimum values: 0.350000, 0.355875
  float wetMin = 0.350000;
  // Observed maximum values: 1.023969, 1.105875
  float wetMax = 1.023969;
  float wetPercent = ((wetRaw - wetMin) / (wetMax - wetMin) * 100.0);
#if DEBUG
  Serial.print("WETNESS VOLTAGE: ");
  Serial.print(wetRaw, 6);
  Serial.println(" V");
#endif
  Serial.print("WETNESS PERCENTAGE: ");
  Serial.print(wetPercent);
  Serial.println("%");
  bleuart.print(wetPercent);
  delay(2000);
}

void connect_callback(uint16_t conn_handle) {
  BLEConnection* connection = Bluefruit.Connection(conn_handle);
  char central_name[32] = { 0 };
  connection->getPeerName(central_name, sizeof(central_name));
  Serial.print("Connected to ");
  Serial.println(central_name);
}

void disconnect_callback(uint16_t conn_handle, uint8_t reason) {
  (void) conn_handle;
  (void) reason;
  Serial.print("Disconnected, reason = 0x");
  Serial.println(reason, HEX);
}
