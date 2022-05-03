#include <Adafruit_ADS1015.h>
#include <EEPROM.h>
#include <math.h>
#include <Wire.h>

bool hardwareDebug = false;

// ADS1115 ADC needs 200µA max supply current, NTC10k needs 165µA
// enable/disable to prevent self heating of NTC an reduce energy consumption
#define ADC_NTC_EN_PIN 13
// Capacitive soil moisture sensor with TL555C needs 4mA supply current
#define SENS_EN_PIN 15
// Connect GPIO16 to RST pin for deep sleep mode

#define WIFI_LED 14

// Set ESP ADC mode to internal, to measure ESP Vcc
ADC_MODE(ADC_VCC);

// ADS1115 device init with address
Adafruit_ADS1115 ads(0x48);

typedef union{
    float numeric;
    char buf[4];
} Float;
Float AirValue;
Float WaterValue;

/*
 * EEPROM memory allocation
 * 0 - 31 byte, non-volatile device config data
 * 32 - 63 byte, MQTT psk
 * 64 - 95 byte, Client[0] psk
 * 96 - 127 byte, Client[1] psk
 */

int writeEEPROM(int startAdr, int len, char* writeString) {
  EEPROM.begin(512); //Max bytes of eeprom to use
  yield();
  //delay(10);
  if(hardwareDebug) {
    Serial.println();
    Serial.print(F("[DEBUG] EEPROM write: "));
  }
  //write to eeprom
  for (int i = 0; i < len; i++) {
    EEPROM.write(startAdr + i, writeString[i]);
    if(hardwareDebug) Serial.print(writeString[i]);
  }
  int ret = EEPROM.commit();
  EEPROM.end();   
  if(hardwareDebug) Serial.println();
  return ret;      
}

void readEEPROM(int startAdr, int maxLength, char* dest) {
  EEPROM.begin(512);
  for (int i = 0; i < maxLength; i++) {
    dest[i] = char(EEPROM.read(startAdr + i));
  }
  EEPROM.end();
  if(hardwareDebug) {  
    Serial.print(F("[DEBUG] EEPROM read: "));
    Serial.println(dest);
  }
}

void readMqttPsk(char *passwd) {
  readEEPROM(32, 32, passwd);
}

int writeMqttPsk(char *passwd) {
  return writeEEPROM(32, 32, passwd);
}

void readClient1Psk(char *passwd) {
  readEEPROM(64, 32, passwd);
}

int writeClient1Psk(char *passwd) {
  return writeEEPROM(64, 32, passwd);
}

void readClient2Psk(char *passwd) {
  readEEPROM(96, 32, passwd);
}

int writeClient2Psk(char *passwd) {
  return writeEEPROM(96, 32, passwd);
}

void setEspDeviceInformation() {

  DynamicJsonDocument doc = getJsonConfig();
  doc["system"]["serialNumber"] = String(ESP.getChipId(), HEX);
  doc["system"]["flashSize"] = String((int)((float)ESP.getFlashChipRealSize()/1000.0), DEC) + " kB";
  doc["system"]["flashSpeed"] = String((int)((float)ESP.getFlashChipSpeed()/1000000.0), DEC) + " MHz";
  doc["system"]["cpuSpeed"] = String(ESP.getCpuFreqMHz(), DEC) + " MHz";
  doc["system"]["heapFree"] = String(ESP.getFreeHeap()) + " Bytes";
  doc["system"]["version"]["boot"] = String(ESP.getBootVersion());
  doc["system"]["version"]["firmware"] = String(FW_VERSION);
  setJsonConfig(doc);
}

void hardwareInit() {
  Serial.println(F("Initialize device hardware"));
  pinMode(ADC_NTC_EN_PIN, OUTPUT);
  pinMode(SENS_EN_PIN, OUTPUT);
  pinMode(WIFI_LED, OUTPUT);
  
  // Initially disable esp led
  digitalWrite(WIFI_LED, LOW);
  // Initally disable soil moisture sensor
  digitalWrite(SENS_EN_PIN, LOW);

  // Enable ADC before init
  digitalWrite(ADC_NTC_EN_PIN, HIGH);
  delay(100);
  ads.begin();
  ads.setGain(GAIN_ONE); // Setting 1x gain   +/- 4.096V  1 bit = 2mV
  // Disable ADC after init
  digitalWrite(ADC_NTC_EN_PIN, LOW);

  readEEPROM(1, 4, AirValue.buf);
  if (AirValue.numeric <= 0) AirValue.numeric = 2.66;
  readEEPROM(5, 4, WaterValue.buf);
  if (WaterValue.numeric <= 0) WaterValue.numeric = 1.4;
  
}

// Generic mapping function for all data types
template <class X, class M, class N, class O, class Q>
X map_Generic(X x, M in_min, N in_max, O out_min, Q out_max){
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

#define MAX_ANALOG_VALUE    32768.0 // 16 bit resolution

// NTC Type NXF T15 XH 103 FA 2 B 100
// Resistance at 25 degrees C
#define THERMISTOR_NOMINAL  10000.0
#define TEMPERATURE_NOMINAL 25   
// 
#define DIVIDER_RESISTOR    9900.0
// The beta coefficient of the thermistor (for XH 3350-3399)
#define BCOEFFICIENT        3380
#define SAMPLES             5

String getSensorsDataJson() {
  // Enable hardware first and wait max 100ms
  digitalWrite(SENS_EN_PIN, HIGH);
  digitalWrite(ADC_NTC_EN_PIN, HIGH);
  delay(50);
  
  int32_t espVcc;
  float adcVoltage[4] = {0};

  for(uint8_t j = 0; j < SAMPLES; j++) {
    // CH0 Solar cell voltage with 100k + 150k bridge
    // CH1 Battery voltage with 100k + 68k bridge
    // CH2 NTC10k + 10k resistor
    // CH3 Capacitive soil moisture sensor channel
    for(uint8_t i = 0; i < 4; i++) {
      adcVoltage[i] += (ads.readADC_SingleEnded(i) * (4.096 / MAX_ANALOG_VALUE));
    }
    espVcc += ESP.getVcc();
  }
 
  // Disable hardware
  digitalWrite(ADC_NTC_EN_PIN, LOW);
  digitalWrite(SENS_EN_PIN, LOW);

  // Calculate average values
  for(uint8_t i = 0; i < 4; i++) {
    adcVoltage[i] /= SAMPLES;
  }

  float resistorValue = ((3.3 * DIVIDER_RESISTOR) / adcVoltage[2]) - DIVIDER_RESISTOR;
  if(hardwareDebug) {
    Serial.print("NTC Resistor value: ");
    Serial.print(resistorValue / 1000.0);
    Serial.println("kOhm");
  }

  // Calculate temperature with Steinhart-Rule
  // Formulas from Adafruit
  float steinhart;
  steinhart = resistorValue / THERMISTOR_NOMINAL;     // (R/Ro)
  steinhart = log(steinhart);                         // ln(R/Ro)
  steinhart /= BCOEFFICIENT;                          // 1/B * ln(R/Ro)
  steinhart += 1.0 / (TEMPERATURE_NOMINAL + 273.15);  // + (1/To)
  steinhart = 1.0 / steinhart;                        // Invert
  steinhart -= 273.15;                                // convert absolute temp K to °C
  if(hardwareDebug) {
    Serial.print("Temperature: ");
    Serial.print(steinhart);
    Serial.println(" °C");
  }

  int soilmoisturepercent = map_Generic(adcVoltage[3], AirValue.numeric, WaterValue.numeric, 0, 100);
  if(hardwareDebug){
    Serial.print("Soil moisture: ");
    Serial.print(soilmoisturepercent);
    Serial.println(" %");
  }

  float voltageBattery = adcVoltage[1] * ((100.0 + 68.0) / 100.0);
  if(hardwareDebug){
    Serial.print("Battery voltage: ");
    Serial.print(voltageBattery);
    Serial.println(" V");
  }

  float voltageSolarCell = adcVoltage[0]* ((100.0 + 150.0) / 100.0);
  if(hardwareDebug) {
    Serial.print("Solar cell voltage: ");
    Serial.print(voltageSolarCell);
    Serial.println(" V");
  }

  float voltageSupply = ((float) espVcc / (float) SAMPLES / 1024.0);

  String scanResp = "";
  scanResp += "{\"result\": {";
  scanResp += "\"vcc\":\"";
  scanResp += String(voltageSupply);
  scanResp += "\",";
  scanResp += "\"vbat\":\"";
  scanResp += String(voltageBattery);
  scanResp += "\",";
  scanResp += "\"vsolar\":\"";
  scanResp += String(voltageSolarCell);
  scanResp += "\",";
  scanResp += "\"tempsoil\":\"";
  scanResp += String(steinhart);
  scanResp += "\",";
  scanResp += "\"humsoil\":\"";
  scanResp += String(soilmoisturepercent);
  scanResp += "\"";
  scanResp += "}}";

  return scanResp;
}

float getSoilVoltage(){
  
  digitalWrite(SENS_EN_PIN, HIGH);
  digitalWrite(ADC_NTC_EN_PIN, HIGH);
  delay(50);
  
  float adcVoltage = 0;

  for(uint8_t j = 0; j < SAMPLES; j++) {
    // CH3 Capacitive soil moisture sensor channel
    adcVoltage += (ads.readADC_SingleEnded(3) * (4.096 / MAX_ANALOG_VALUE));
  }
 
  // Disable hardware
  digitalWrite(ADC_NTC_EN_PIN, LOW);
  digitalWrite(SENS_EN_PIN, LOW);

  // Calculate average values
  adcVoltage /= SAMPLES;

  return adcVoltage;
}

int setSoilAir() {
  AirValue.numeric = getSoilVoltage();
  if(serialDebug){
    Serial.print("[DEBUG] Calibrate soil moisture air value to: ");
    Serial.print(AirValue.numeric, 2);
    Serial.println(" V");
  }
  return writeEEPROM(1, 4, AirValue.buf);
}

int setSoilWater() {
  WaterValue.numeric = getSoilVoltage();
  if(serialDebug){
    Serial.print("[DEBUG] Calibrate soil moisture water value to: ");
    Serial.print(WaterValue.numeric, 2);
    Serial.println(" V");
  }
  return writeEEPROM(5, 4, WaterValue.buf);
}
