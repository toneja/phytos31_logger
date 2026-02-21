// Basic Datalogger Application for METER PHYTOS31 Leaf-Wetness Sensor
//
// Log data every 2 seconds; convert raw voltage output into % wetness
// Voltage output range can be different for each invididual sensor(?)
//

#include <Adafruit_ADS1X15.h>
Adafruit_ADS1115 ads;

#define DEBUG 1

void setup() {
  Serial.begin(115200);
  while (!Serial) { delay(100); }
  Serial.println("*** PHYTOS 31 DATALOGGER ***");
  Serial.println("Sample every 2 seconds...\n");
  ads.setGain(GAIN_FOUR); // 4x gain   +/- 1.024V  1 bit = 0.5mV
  ads.begin();
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
  delay(2000);
}
