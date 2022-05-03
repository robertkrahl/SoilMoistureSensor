/*
 * Firmware version and build date
 */

#define BUILD_DATE  __DATE__ " " __TIME__
#define FW_VERSION  "2021.08"
/*
 * Define defaults
 */
#define HOSTNAME    "soilsensor"
#define WLAN_MODE   1           // AP
#define AP_SSID     "SoilSensor"
#define AP_PSK      "12345678"
#define AP_MAX_CON  2
#define AP_CHANNEL  6
#define AP_IP_ADDR  "192.168.4.1"
#define AP_SUBNET   "255.255.255.0"
#define AP_GATEWAY  "192.168.4.1"
