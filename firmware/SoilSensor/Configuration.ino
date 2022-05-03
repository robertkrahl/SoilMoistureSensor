#include <ArduinoJson.h>

#define CONFIGFILENAME "/device.json"

extern DynamicJsonDocument jsonConfig(2048);

int loadConfigFile() 
{
  Serial.print(F("Load configuration file..."));
  File file = SPIFFS.open(CONFIGFILENAME,"r");
  if(!file) {
    Serial.println(F("FAILED (Cannot open)"));
    return -22;
  }
  // Deserialize the JSON document
  DeserializationError error = deserializeJson(jsonConfig, file);
  if (error) {
    Serial.println("FAILED (Cannot read: " + String(error.c_str()) +")");
    file.close();
    return -23;
  }

  file.close();
  Serial.println(F("OK"));
  return 0;
}

int storeConfigFile() 
{
  Serial.print(F("Saving configuration file..."));
  File file = SPIFFS.open(CONFIGFILENAME,"w");
  if(!file) {
    Serial.println(F("FAILED (Cannot open)"));
    return -22;
  }
  // Serialize JSON to file
  if (serializeJsonPretty(jsonConfig, file) == 0) {
    Serial.println(F("FAILED (Cannot write)"));
    file.close();
    return -24;
  }
  file.close();
  Serial.println(F("OK"));
  return 0;
}

// Wrapper functions to access JSON document
DynamicJsonDocument getJsonConfig() {
  return jsonConfig;
}

void setJsonConfig(DynamicJsonDocument doc) {
  jsonConfig = doc;
}
