#include <ArduinoJson.h>
#include <ArduinoOTA.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ESP8266WebServer.h>
#include <FS.h>
#include <Hash.h>
#include <PubSubClient.h>
#include <rBase64.h>
#include <WiFiUdp.h>

#include "config.h"

// Global vars
bool serialDebug = false;
bool wifiConnected = false;
bool wakeFlag = false;
unsigned long sleepMicroSec = 0;
ESP8266WebServer server(80);

// Global MQTT vars
WiFiClient espClient;
PubSubClient client(espClient);
bool mqttEnabled = false;
String mqttPsk = "";
String mqttUser = "";
String mqttClientId = "";
String mqttTopic = "";

// MQTT callback function for input topic
void callback(char* topic, byte* payload, unsigned int length) {
  if(topic != "config") {
    Serial.print("[ERROR] ");
    Serial.print(topic);
    Serial.println(" is a invalid input topic");
    return;
  }

  DynamicJsonDocument tmpJson(2048);
  DeserializationError error = deserializeJson(tmpJson, (char*)payload);
  if (error) {
    Serial.println("[ERROR] Cannot validate JSON format: " + String(error.c_str()));
    return;
  }
  
  if(tmpJson["wakeFlag"]) {
    Serial.println("Setting wake flag....");
    wakeFlag = tmpJson["wakeFlag"].as<bool>();
  }
  
  if (tmpJson["network"]["wlan"]["mode"]) {
    uint8_t wlanMode = tmpJson["network"]["wlan"]["mode"].as<unsigned int>();
    DynamicJsonDocument doc = getJsonConfig();
    doc["network"]["wlan"]["mode"] = wlanMode;
    setJsonConfig(doc);
    storeConfigFile();
  }

  if(tmpJson["reboot"]) {
    if(tmpJson["reboot"].as<bool>()) {
      Serial.println("Rebooting via MQTT....");
      delay(500);
      ESP.restart();
    }
  }
}

bool reconnect() {
  uint8_t count = 0;
  // Loop until we're reconnected
  while (!client.connected()) {
    if(serialDebug) Serial.println("[DEBUG] Attempting MQTT connection...");
    if ((mqttPsk == "") || (mqttUser == "")) {
      Serial.println("[ERROR] MQTT credentials missing \"" + mqttPsk + "\" \"" + mqttUser + "\"");
      return false;
    }
    if (mqttClientId == "") {
      Serial.println("[ERROR] MQTT client ID missing.");
      return false;
    }
    // Attempt to connect
    if (client.connect(mqttClientId.c_str(), mqttUser.c_str(), mqttPsk.c_str())) {
      if(serialDebug) Serial.println(F("[DEBUG] MQTT broker connected."));
      // subscribe settings pipe
      client.subscribe("config");
    } 
    else {
      if(count >= 20){
        Serial.println(F("[ERROR] Cannot connect to MQTT broker. Timeout."));
        return false;
      }
      if(serialDebug) {
        Serial.print(F("[WARN] MQTT broker connection failed, rc="));
        Serial.print(client.state());
        Serial.println(F(" try again in 1 second"));
      }
      delay(1000);
      count++;
    }
  }
  return true;
}

void setup() {
  Serial.begin(115200);
  while(!Serial);

  Serial.println(F("Booting wireless soil moisture sensor..."));
  Serial.println(F("Version    " FW_VERSION));
  Serial.println(F("Build date " BUILD_DATE));

  setupOTA();
  hardwareInit();
  if(!SPIFFS.begin()) {
    Serial.println(F("[ERROR] Mounting Filesystem failed."));
    serialDebug = true;
  }
  else {
    loadConfigFile();
    serialDebug = getJsonConfig()["services"]["serial"]["enabled"];
  }
  if(serialDebug) Serial.println(F("[DEBUG] Serial debugging enabed."));

  // get static infos after config changed, to reduce filesystem write operation
  bool configChanged = false;
  if(getJsonConfig()["system"]["version"]["boot"].as<String>() == "0") {
    setEspDeviceInformation();
    configChanged = true;
  }
  
  unsigned int sleepInterval = getJsonConfig()["services"]["sleep"]["interval"].as<unsigned int>();
  if (sleepInterval > 0) {
    sleepMicroSec = 6e8 * sleepInterval;
    if(serialDebug) Serial.println("[DEBUG] Setting sleep interval to " + String(sleepMicroSec) + " us / " + String(sleepInterval * 10) + " min");
  }
  // enable WLAN and connect if available, otherwise fallback to AP mode
  setWiFiConfig();

  if(configChanged) {
    storeConfigFile();
    delay(500);
  }
  
  mqttEnabled = getJsonConfig()["services"]["mqtt"]["enabled"];
  if(mqttEnabled) {
    const char* mqttServer = getJsonConfig()["services"]["mqtt"]["host"].as<const char*>();
    int mqttPort = getJsonConfig()["services"]["mqtt"]["port"].as<unsigned int>();
    if(mqttPort <= 0) mqttPort = 1883;

    if(serialDebug) {
      Serial.print(F("[DEBUG] Enable MQTT and connect to server: "));
      Serial.println(String(mqttServer) + ":" + String(mqttPort));
    }

    // get credentials
    char passwd[32];
    readMqttPsk(passwd);
    mqttPsk = String(passwd);
    String user = getJsonConfig()["services"]["mqtt"]["userName"];
    mqttUser = user;
    // get the "out" topic
    String topic = getJsonConfig()["services"]["mqtt"]["topic"];
    if(topic == "") topic = "soil";
    mqttTopic = topic;
    // get the client ID
    String clientId = getJsonConfig()["services"]["mqtt"]["clientId"];
    String serialNumber = getJsonConfig()["system"]["serialNumber"];
    if(clientId == "") clientId = serialNumber;
    mqttClientId = clientId;
    
    client.setServer(mqttServer, mqttPort);
    client.setCallback(callback);

    bool transferOk = mqttTransfer();

    // goto deep sleep if interval > 0
    if((sleepMicroSec > 0) && (transferOk)) {
      if(!wakeFlag) {
        WiFi.disconnect();
        if(serialDebug) Serial.println("[DEBUG] Going to deep sleep now.");
        delay(500);
        ESP.deepSleep(sleepMicroSec);
      }
    }
  }

  // if sleep interval is zero, webserver will setup, otherwise mqttTransfer() 
  // set the controller to deep sleep and webserver is not neccesarry
  initWebServer();
  delay(500);
}

bool mqttTransfer() {
  if(mqttEnabled && wifiConnected) {
    bool mqttConnected = client.connected();
    if(!mqttConnected) {
      mqttConnected = reconnect();
    }
    
    if(mqttConnected) {
      client.loop();
      if(serialDebug) Serial.println("[DEBUG] Publishing MQTT data.");
      // Once connected, publish an announcement...
      client.publish(mqttTopic.c_str(), getSensorsDataJson().c_str());

      // response delay if messages available
      delay(2000);
      return true;
    }
    return false;
  }
}

// loop handles function if deep sleep is disabled
void loop() {
  ArduinoOTA.handle();
  handleWebServer();
}

void initMDNS(String hostName){
  MDNS.begin(hostName);
  MDNS.setInstanceName(hostName);
  MDNS.addServiceTxt("arduino", "tcp", "fw_version", FW_VERSION);
}

void setupOTA() {
  // Port defaults to 8266
  // ArduinoOTA.setPort(8266);

  // Hostname defaults to esp8266-[ChipID]
  // ArduinoOTA.setHostname("myesp8266");

  // No authentication by default
  // ArduinoOTA.setPassword("admin");

  // Password can be set with it's md5 value as well
  // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
  // ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");

  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_FS
      type = "filesystem";
    }

    // NOTE: if updating FS this would be the place to unmount FS using FS.end()
    Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      Serial.println("Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      Serial.println("Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      Serial.println("Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      Serial.println("Receive Failed");
    } else if (error == OTA_END_ERROR) {
      Serial.println("End Failed");
    }
  });
  ArduinoOTA.begin();
}

/*Warning: though I have disabled reconnect, it still auto-reconnect after calling WiFi.begin() once. So I have to set this event listener.*/
/*Maybe I don't need this. Please tell me :)*/
void eventListener(WiFiEvent_t event)
{
    if (event == WIFI_EVENT_STAMODE_DISCONNECTED) {
        WiFi.disconnect(); // Wipe the config saved
    }
    if (event == WIFI_EVENT_SOFTAPMODE_STACONNECTED) {
        wakeFlag = true;
        Serial.println ("Setting wake flag.");
    }
    if (event == WIFI_EVENT_SOFTAPMODE_STADISCONNECTED) {
        wakeFlag = false;
        Serial.println ("Clearing wake flag.");
    }
}

void setWiFiConfig(){

  Serial.println("Setting up WLAN.");
  // disable persitent write to flash 
  //http://www.forum-raspberrypi.de/Thread-esp8266-achtung-flash-speicher-schreibzugriff-bei-jedem-aufruf-von-u-a-wifi-begin
  WiFi.persistent(false);      
  
  // Fill mac address in config
  String CLImac = WiFi.macAddress();
  String APmac = WiFi.softAPmacAddress();
  DynamicJsonDocument doc = getJsonConfig();
  doc["network"]["wlan"]["macAddress"] = CLImac;
  doc["network"]["wlan"]["accessPoint"]["macAddress"] = APmac;
  setJsonConfig(doc);
  
  uint8_t wlanMode = doc["network"]["wlan"]["mode"].as<unsigned int>();
  if(wlanMode < 0) 
    wlanMode = WLAN_MODE; // default to AP mode, if invalid

  WiFiMode_t rMode = WiFi.getMode();
  WiFiMode_t wMode = intToWifiMode(wlanMode+1);
  if(rMode != wMode)
    WiFi.mode(wMode);
  
  String tmpHostname = doc["network"]["hostname"];
  if((tmpHostname == "") || (tmpHostname == "null")) tmpHostname = HOSTNAME;
  WiFi.hostname(tmpHostname);
  initMDNS(tmpHostname);

  WiFi.onEvent(eventListener);
  
  // Client mode config (STA / STA+AP)
  if(wMode != WIFI_AP) {
    if (serialDebug) Serial.println(F("[DEBUG] Configure WLAN station."));
    int numOfClients = doc["network"]["wlan"]["clients"].size();
    if(numOfClients <= 0) {
      Serial.println(F("[ERROR] No WLAN station configured, cannot connect to any network"));
      //if((sleepMicroSec <= 0) || (wakeFlag)) {
        wMode = WIFI_AP;
        WiFi.mode(WIFI_AP);
        goto apsetup;
      //}
    }

    // Store PSK length for next pointer
    for(uint8_t i = 0; i < numOfClients; i++) {
      String stationSsid = doc["network"]["wlan"]["clients"][i]["ssid"];
      if((stationSsid == "") || (stationSsid == "null")) {
        if(i < (numOfClients - 1)) break;
        if(i == (numOfClients - 1)) {
          //if((sleepMicroSec <= 0) || (wakeFlag)) {
            Serial.println(F("[ERROR] Invalid SSID, change to AP mode"));
            wMode = WIFI_AP;
            WiFi.mode(WIFI_AP);
            goto apsetup;
          //}
        }
 
      }
      uint8_t pskLen = doc["network"]["wlan"]["clients"][i]["psk"].as<unsigned int>();
      if(pskLen < 2) continue;
      char base64Psk[32];
      if(i == 0) readClient1Psk(base64Psk);
      if(i == 1) readClient2Psk(base64Psk);
      WiFi.begin(stationSsid.c_str(), base64Psk);
      if(serialDebug) Serial.println("[DEBUG] Connecting to network \"" + stationSsid + "\".");
      uint8_t timeoutCount = 0, timeoutCountMax = 20;
      while(WiFi.status() != WL_CONNECTED) {
        if(timeoutCount >= timeoutCountMax) {
          Serial.println(F("Connection Timeout."));
          if(i < (numOfClients - 1)) break;
          if(i == (numOfClients - 1)) {
            Serial.println(F("[ERROR] No connection possible, switching to AP mode"));
            wMode = WIFI_AP;
            WiFi.mode(WIFI_AP);
            goto apsetup;         
          }
        }
        Serial.print(F("."));
        delay(1000);
        timeoutCount++;
      }

      String localAddress = ipToString(WiFi.localIP());
      String localSubnetMask = ipToString(WiFi.subnetMask());
      if(localAddress != "0.0.0.0") {
        if(serialDebug) {
          Serial.println("Local address: " + localAddress);
          Serial.println("Subnet mask: " + localSubnetMask);
        }
        doc["network"]["wlan"]["connectedAp"] = stationSsid;
        doc["network"]["wlan"]["dhcpAddress"] = localAddress;
        doc["network"]["wlan"]["dhcpSubnetMask"] = localSubnetMask;
        setJsonConfig(doc);
        wifiConnected = true;
      }
    }
  }
  
apsetup:
  if (wMode != WIFI_STA) {
    if(serialDebug)Serial.println(F("[DEBUG] Configure WLAN access point."));
    //set default AP
    int apChannel     = doc["network"]["wlan"]["accessPoint"]["channel"].as<unsigned int>();
    String apSsid     = doc["network"]["wlan"]["accessPoint"]["ssid"].as<String>();
    String apPsk      = doc["network"]["wlan"]["accessPoint"]["psk"].as<String>();
    int apMaxCon      = doc["network"]["wlan"]["accessPoint"]["maxConnections"].as<unsigned int>();
    String apIpAddr   = doc["network"]["wlan"]["accessPoint"]["ipAddress"].as<String>();
    String apSubnet   = doc["network"]["wlan"]["accessPoint"]["subnetMask"].as<String>();
    String apGateway  = doc["network"]["wlan"]["accessPoint"]["gateway"].as<String>();

    if((apSsid == "null") || (apSsid == ""))        apSsid = String(AP_SSID);
    if((apPsk == "null") || (apPsk == ""))          apPsk = AP_PSK;
    if((apIpAddr == "null") || (apIpAddr == ""))    apIpAddr = String(AP_IP_ADDR);
    if((apSubnet == "null") || (apSubnet == ""))    apSubnet = AP_SUBNET;
    if((apGateway == "null") || (apGateway == ""))  apGateway = AP_GATEWAY;
    if(apMaxCon <= 0)                               apMaxCon = AP_MAX_CON;
    if(apChannel < 0)                               apChannel = AP_CHANNEL;
    
    IPAddress ipAddress = stringToIP(apIpAddr);
    IPAddress subnet = stringToIP(apSubnet);
    IPAddress gateway = stringToIP(apGateway);

    if(serialDebug) {
      Serial.println("\tSSID:         " + apSsid);
      Serial.println("\tIP address:   " + apIpAddr);
      Serial.println("\tSubnet mask:  " + apSubnet);
      Serial.println("\tGateway:      " + apGateway);
      Serial.println("\tChannel:      " + String(apChannel));
      Serial.println("\tMax connects: " + String(apMaxCon));
    }
    WiFi.softAP(apSsid.c_str(), apPsk, apChannel, false, apMaxCon);
    WiFi.softAPConfig(ipAddress, gateway, subnet);
    if(wMode == WIFI_AP) WiFi.begin();
  }
}
