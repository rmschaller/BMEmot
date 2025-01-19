// All defines that set timer values go HERE
#define OLED_DEFAULT_TIME 15                                // Default time to turn on display is seconds
#define WIFI_RESET_TIME 5                                   // Reset WiFi Parameters after xx seconds
#define EEPROM_RESET_TIME 15                                // Reset EEPROM after xx seconds (EEPROM_RESET_TIME ***MUST*** BE > WIFI_RESET_TIME; No checks made)
// MQTT 
#define HA_MQTT_RETRY_TIME 30000                            // HA MQTT server retry counter (30 seconds)
#define MQTT_RETRY_TIME 30000                               // MQTT server retry counter (30 seconds)
#define WEB_TIMEOUT 2000                                    // Web client wait time