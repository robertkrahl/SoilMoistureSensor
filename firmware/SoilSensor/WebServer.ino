#include <rBase64.h>

bool SERVER_STOP = false;       //check stop server
bool connect_wifi = false;

String getContentType(String filename){
  if(server.hasArg("download")) return "application/octet-stream";
  else if(filename.endsWith(".htm")) return "text/html";
  else if(filename.endsWith(".html")) return "text/html";
  else if(filename.endsWith(".css")) return "text/css";
  else if(filename.endsWith(".js")) return "application/javascript";
  else if(filename.endsWith(".png")) return "image/png";
  else if(filename.endsWith(".gif")) return "image/gif";
  else if(filename.endsWith(".jpg")) return "image/jpeg";
  else if(filename.endsWith(".ico")) return "image/x-icon";
  else if(filename.endsWith(".xml")) return "text/xml";
  else if(filename.endsWith(".pdf")) return "application/x-pdf";
  else if(filename.endsWith(".zip")) return "application/x-zip";
  else if(filename.endsWith(".gz")) return "application/x-gzip";
  return "text/plain";
}

bool handleFileRead(String path){
  if(path.endsWith("/")) path += "index.html";
  String contentType = getContentType(path);
  File file = SPIFFS.open(path, "r");
  if (file == NULL)
    return false;
  size_t sent = server.streamFile(file, contentType);
  file.close();
  return true;
}

String toStringWifiMode(int mod) {
  String mode;
  switch (mod) {
    case 0:
      mode = "OFF";
      break;
    case 1:
      mode = "STA";
      break;
    case 2:
      mode = "AP";
      break;
    case 3:
      mode = "AP+STA";
      break;
    case 4:
      mode = "----";
      break;
    default:
      break;
  }
  return mode;
}

WiFiMode_t intToWifiMode(int mod) {
  WiFiMode_t mode;
  switch (mod) {
    case 0:
      mode = WIFI_OFF;
      break;
    case 1:
      mode = WIFI_STA;
      break;
    case 2:
      mode = WIFI_AP;
      break;
    case 3:
      mode = WIFI_AP_STA;
      break;
    case 4:
      break;
    default:
      break;
  }
  return mode;
}

String toStringWifiStatus(int state) {
  String status;
  switch (state) {
    case 0:
      status = "connecting";
      break;
    case 1:
      status = "unknown status";
      break;
    case 2:
      status = "wifi scan completed";
      break;
    case 3:
      status = "got IP address";
      // statements
      break;
    case 4:
      status = "connection failed";
      break;
    default:
      break;
  }
  return status;
}

String toStringEncryptionType(int thisType) {
  String eType;
  switch (thisType) {
    case ENC_TYPE_WEP:
      eType = "WEP";
      break;
    case ENC_TYPE_TKIP:
      eType = "WPA";
      break;
    case ENC_TYPE_CCMP:
      eType = "WPA2";
      break;
    case ENC_TYPE_NONE:
      eType = "None";
      break;
    case ENC_TYPE_AUTO:
      eType = "Auto";
      break;
  }
  return eType;
}

IPAddress stringToIP(String address) {
  int p1 = address.indexOf('.'), p2 = address.indexOf('.', p1 + 1), p3 = address.indexOf('.', p2 + 1); //, 4p = address.indexOf(3p+1);
  String ip1 = address.substring(0, p1), ip2 = address.substring(p1 + 1, p2), ip3 = address.substring(p2 + 1, p3), ip4 = address.substring(p3 + 1);

  return IPAddress(ip1.toInt(), ip2.toInt(), ip3.toInt(), ip4.toInt());
}

String ipToString(IPAddress ip) {
  String res = "";
  for (int i = 0; i < 3; i++) {
    res += String((ip >> (8 * i)) & 0xFF) + ".";
  }
  res += String(((ip >> 8 * 3)) & 0xFF);
  return res;
}

String newSSID_param;

void handleWebServer(){
  if(connect_wifi){
    String newPASSWORD_param;
    JsonArray jarray = getJsonConfig()["network"]["wlan"]["clients"];
    if(jarray.size() > 0) {
      for(uint8_t i = 0; i < jarray.size(); i++) {
        String wpaSsid = getJsonConfig()["network"]["wlan"]["clients"][i]["ssid"];
        if(wpaSsid = newSSID_param){
          int pskLen = getJsonConfig()["network"]["wlan"]["clients"][i]["psk"].as<unsigned int>();
          if(pskLen < 2) continue;
          char base64Psk[32];
          if(i == 0) readClient1Psk(base64Psk);
          if(i == 1) readClient2Psk(base64Psk);
          newPASSWORD_param = String(base64Psk);
        }
      }
    }
    if(newPASSWORD_param = ""){
      connect_wifi = false;
      return;
    }
    if(serialDebug) Serial.println("Connecting to network " + newSSID_param);
    WiFi.begin(newSSID_param.c_str(),newPASSWORD_param.c_str());
    delay(500);  // VB: exactly 500, and here!
    connect_wifi = false;
  }
  server.handleClient();
}

void initWebServer(){
  if(serialDebug) Serial.println("[DEBUG] Setting up webserver.");
  // Respond JSON array like: [{"rssi": "-65", "enc": "WPA2", "ssid": "Network1"}]
  server.on("/wifiScan", []() {
    int tot = WiFi.scanNetworks();
    String scanResp = "";
    if (tot == 0) {
      server.send(200, "text/plain", "{\"result\": false, \"error\":\"No networks found\"}");
    }
    if (tot == -1 ) {
      server.send(500, "text/plain", "{\"result\": false, \"error\":\"Error during scanning\"}");
    }
    // returns json array of networks
    scanResp += "[ ";
    for (int netIndex = 0; netIndex < tot; netIndex++) {
      if(WiFi.encryptionType(netIndex) == 2) continue; // ignore TKIP (WPA)
      if(WiFi.encryptionType(netIndex) == 5) continue; // ignore WEP
      if(WiFi.encryptionType(netIndex) == 7) continue; // ignore NONE
      scanResp += "{\"enc\" : \"";
      scanResp += toStringEncryptionType (WiFi.encryptionType(netIndex));
      scanResp += "\",";
      scanResp += "\"ssid\":\"";
      scanResp += WiFi.SSID(netIndex);
      scanResp += "\",";
      scanResp += "\"rssi\" :\"";
      scanResp += WiFi.RSSI(netIndex);
      scanResp += "\"}";
      if (netIndex != tot - 1)
        scanResp += ",";
    }
    scanResp += "]";
    server.send(200, "text/plain", scanResp);
  });

  server.on("/wifiConnect", []() {
    newSSID_param = server.arg("ssid");
    server.send(200, "text/plain", "{\"result\": true}");
    connect_wifi = true;
  });

  server.on("/writeConfig", []() {
    String jsonContent = server.arg("plain");
    Serial.println(jsonContent);
    
    DynamicJsonDocument tmpJson(2048);
    // Deserialize the JSON document
    DeserializationError error = deserializeJson(tmpJson, jsonContent);
    if (error) {
      String errMsg = "Failed to parse transmitted json file: ";
      errMsg += error.c_str();
      Serial.print("[ERROR] ");
      Serial.println(errMsg);
      String resp = "{\"result\": false, \"error\": \"";
      resp += errMsg;
      resp += "\"}";
      server.send(200, "text/plain", resp);
      return;
    }
    // write psk'S into eeprom
    String mqttPsk = tmpJson["services"]["mqtt"]["psk"];
    if(mqttPsk.length() > 2) {
      // get base64 psk
      rBase64generic<250> mybase64;
      int decodeLen = rbase64.decode(mqttPsk);
      //write clear psk into eeprom
      if(!writeMqttPsk(rbase64.result())) {
        Serial.println("[ERROR] Write MQTT psk failed.");
      }
      tmpJson["services"]["mqtt"]["psk"] = strlen(rbase64.result());
    }
    JsonArray jarray = tmpJson["network"]["wlan"]["clients"];
    if(jarray.size() > 0) {
      for(uint8_t i = 0; i < jarray.size(); i++) {
        String wpsk = tmpJson["network"]["wlan"]["clients"][i]["psk"];
        if(wpsk.length() > 2) {
          int decodeLen = rbase64.decode(wpsk);
          //write clear psk into eeprom
          if(i == 0) {
            if(!writeClient1Psk(rbase64.result())) {
              Serial.println("[ERROR] Write WLAN1 psk failed.");
            }
          }
          if(i == 1) {
            if(!writeClient2Psk(rbase64.result())) {
              Serial.println("[ERROR] Write WLAN2 psk failed.");
            }
          }
          tmpJson["network"]["wlan"]["clients"][i]["psk"] = strlen(rbase64.result());
        }
      }
    }
    // change boot version to reload static infos @ reboot
    tmpJson["system"]["version"]["boot"] = "0";
    if(serialDebug) Serial.println("[DEBUG] Copy conf tmpfile.");
    setJsonConfig(tmpJson);
    delay(100);
    int ret = storeConfigFile();
    if (ret < 0) {
      String msg = "{\"result\": false, \"error\":\"";
      msg += ret;
      msg += "\"}";
      server.send(200, "text/plain", msg);  
    }
    else {
      server.send(200, "text/plain", "{\"result\": true}");
    } 
  });

  server.on("/reboot", []() {
    server.send(200, "text/plain", "{\"result\": true}");
    delay(500);
    ESP.restart();
  });

  server.on("/calibSensor", []() {
    String limit = server.arg("limit");
    int ret = 0;
    if(limit == "water") {
      setSoilWater();
    }
    else if (limit == "air") {
      setSoilAir();
    }
    if(ret)
      server.send(200, "text/plain", "{\"result\": true}");
    else
      server.send(200, "text/plain", "{\"result\": false, \"error\":\"save limit failed\"}");
  });
  
  // Respond with JSON object
  server.on("/sensor/getValues", []() {
    String resp = getSensorsDataJson();
    server.send(200, "text/plain", resp);
  });

  server.on("/update", HTTP_POST, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
    delay(500);
    ESP.restart();
  }, []() {
    HTTPUpload& upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
      Serial.setDebugOutput(true);
      WiFiUDP::stopAll();
      Serial.printf("Update: %s\n", upload.filename.c_str());
      uint32_t maxSketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
      if (!Update.begin(maxSketchSpace)) { //start with max available size
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
      if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_END) {
      if (Update.end(true)) { //true to set the size to the current progress
        Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
      } else {
        Update.printError(Serial);
      }
      Serial.setDebugOutput(false);
    }
    yield();
  });
  //called when the url is not defined here
  //use it to load content from SPIFFS
  server.onNotFound([](){
    if(!handleFileRead(server.uri()))
      server.send(404, "text/plain", "FileNotFound");
  });

  server.begin();
  delay(5); // VB: exactly 5, no more or less, no yield()!
}
