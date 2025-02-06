// hardware config goes here
// OLED 128x64 displays
// Pin connections for I2C OLED
// OLED pin -> NODEMCU pin
// VCC -> any 3.3V NODEMCU pin
// GND -> any NODEMCU GND
// SCL -> D4 (GPIO2)
// SDA -> D2 (GPIO4)
#define OLED_SDA  D2
#define OLED_SCL  D1
#define OLED_ADDR 0x3C                                      // I2C address for OLED, some might use 3D
#define OLED_SWITCH D0
// Apparently the Dn NodeMCU pins are no longer defined, so here they are
#ifndef D1
#define D0 16                 // cannot use for interrupts (also WAKE)
#define D1 5                  // this is also the SCL (I2C)
#define D2 4                  // this is also the SDA (I2C)
#define D3 0                  // this is also the FLASH pin on the Wemos
#define D4 2                  // Built In LED
#define D5 14                 // this is also SCLK
#define D6 12                 // this is also MISO
#define D7 13                 // this is also MOSI
#define D8 15                 // this is also CS
#define D9 3                  // this is the serial interface - RXDO
#define D10 1                 // this is the serial interface - TXDO
#endif

// All defines that set timer values go HERE
#define OLED_DEFAULT_TIME 15                                // Default time to turn on display is seconds
#define WIFI_RESET_TIME 5                                   // Reset WiFi Parameters after xx seconds
#define EEPROM_RESET_TIME 15                                // Reset EEPROM after xx seconds (EEPROM_RESET_TIME ***MUST*** BE > WIFI_RESET_TIME; No checks made)
// MQTT
#define DEFAULT_HA_MQTT_SERVER "192.168.66.18"
#define DEFAULT_HA_MQTT_USER ""
#define DEFAULT_HA_MQTT_PASSWORD ""
#define DEFAULT_MQTT_SERVER "192.168.66.2"                 // use MQTT p/o Hone assistant
#define DEFAULT_MQTT_USER ""                                // Your User Name
#define DEFAULT_MQTT_PASSWORD ""                            // Your User Password
#define MQTT_SERVER_SIZE 32                                 // Max characters -1 for HA server address
#define MQTT_USER_SIZE 32                                   // Max characters -1 for HA user name
#define MQTT_PASSWORD_SIZE 32                               // Max characters -1 for HA user password
#define raw_topic "BME280Motion/rawData"
#define info_topic "BME280Motion/info"
#define req_topic "BME280Motion/req"
#define MQTT_ENVIROMENTAL_INTERVAL ENVIROMENTAL_INTERVAL*60   // interval at which to log the environmental values to MQTT server
#define MQTT_RETRY_TIME 30000                               // MQTT server retry counter (30 seconds)
#define HA_MQTT_RETRY_TIME 30000                            // HA MQTT server retry counter (30 seconds)
#define MQTT_RETRY_TIME 30000                               // MQTT server retry counter (30 seconds)
// end MQTT section
#define PORT 80                                             // The web server will use the standard port (80) or the one specified here
#define WEB_TIMEOUT 2000                                    // Web client wait time
#define ENVIROMENTAL_INTERVAL 1000                          // interval at which to read the environmental values
#define MOTION_INTERVAL 500                                 // interval at which to read the motion sensor
#define SAMPLE_INTERVAL 250                                 // interval to read display turn on switch
#define DISPLAY_INTERVAL 10 * 1000                          // how long to keep display on (10 seconds)
#define INFO_INTERVAL 10 * 60 * 1000                        // how long between sending the info packet (10 minutes)

// EEPROM storage definitions
#define EEPROM_SIZE 512
#define offsetFN 0                                          // 000 char friendlyName[17]
#define offsetTZ offsetFN+FRIENDLY_SIZE                     // 017 byte timeZone - offset in hours
#define offsetCF offsetTZ+1                                 // 018 byte control/state
#define displayPowerup 0                                    //   bit 0 - enable display on powerup; 0 -> off: 1-> on
#define mqttPowerup 1                                       //   bit 1 - enable mqtt on powerup; 0 -> disabled: 1 -> enabled
#define dstEnabled 2                                        //   bit 2 - DST enabled/disabled: 0 -> disabled: 1 -> enabled
#define alarmEnabled 3                                      //   bit 3 - Enable BMEmot Alarm; 0 -> disabled: 1 -> enabled
#define alertEnabled 4                                      //   bit 4 - Enable BMEmot Alert: 0 -> disabled: 1 -> enabled
#define emailEnabled 5                                      //   bit 5 - Enable BMEmot Email: 0 -> disabled: 1 -> enabled
#define porEmailEnabled 6                                   //   bit 6 - Enable Email on POR: 0 -> disabled: 1 -> enabled
#define undefinedEnabled 7                                  //   bit 7 - Undefined
#define offsetSN offsetCF+1                                 // 019 byte BMEmot sequence number; 0 - disabled: 1-99 order to display sensors
#define offsetAC offsetSN+1                                 // 020 float altCorrection (4 bytes)
#define offsetLat offsetAC+4                                // 024 double latitude (8 bytes)
#define offsetLong offsetLat+8                              // 032 double longitude (8 bytes)
#define offsetHACF offsetLong+8                             // 040 byte home assistant control flags
#define haMotionEnabled 0                                   //   bit 0 - Enable HA motion sensor; 0 -> disabled: 1 -> enabled
#define haSensorEnabled 1                                   //   bit 1 - Enable HA enviromental sensor; 0 -> disabled: 1 -> enabled
#define unusedByte offsetHACF+1                             // 041 unused byte
#define offsetMTO unusedByte+1                              // 042 unsigned int motion timeout in seconds
#define offsetSSID 32*(((offsetMTO+2)/32)+1)                // 064 char ssid[32] (max WiFi SSID 31 characters) {needs work when ((offsetMTO+2)%32) == 0}
//#define offsetSSID 32*(((offsetMTO+2)/32)+((offsetMTO+2)%32)?1:0)  // 064 char ssid[32] (max WiFi SSID 31 characters)
#define offsetPASS offsetSSID+SSID_SIZE                     // 096 char pass[32] (max WiFi Password 31 characters)
#define offsetMQTT offsetPASS+PASS_SIZE                     // 128 char mqttServer[32] (max time server address 31 characters)
#define offsetTIME offsetMQTT+MQTT_SERVER_SIZE              // 160 char timeServer[32] (max time server address 31 characters)
#define offsetHAMQTT offsetTIME+TIME_SERVER_SIZE            // 192 char haMqttServer[32] (max server name 31 characters)
#define offsetHAUSER offsetHAMQTT+MQTT_SERVER_SIZE          // 224 char haUser[32] (max user name 31 characters)
#define offsetHAPASS offsetHAUSER+MQTT_USER_SIZE            // 256 char haPass[32] (max user password 31 characters)
#define offsetEOT offsetHAPASS+MQTT_PASS_SIZE               // 288 end of table marker
#define offsetCRC EEPROM_SIZE-2                             // 510 int CRC-16 storage location (2 bytes)
// newtwork settings
// Define the time server
#define DEFAULT_TIME_SERVER "us.pool.ntp.org"               // fall back to regional time server
#define TIME_SERVER_SIZE 32
#define DEFAULT_TIME_ZONE 6                                 // Central Standard Time
// WiFi Setup parameters
#define DEFAULT_SSID ""
#define DEFAULT_PASS ""
#define SSID_SIZE 32                                        // Max characters -1 for WiFi SSID
#define PASS_SIZE 32                                        // Max characters -1 for WiFi Password
// data stats
#define DAILY_MAX 7
#define WEEKLY_MAX 52
#define DATA_MAX 50
// misc
#define DEFAULT_CONTROL 0x87                                // Default initial values for control operations (%10000111)
#define DEFAULT_SEQUENCE_NUMBER 0                           // BME280 app auto add feature defaults to disabled
//#define DEFAULT_LATITUDE 42.100707                          // Default latutide for Arlington Heights from Google
//#define DEFAULT_LONGITUDE -87.987509                        // Default longitude for Arlington Heights
//#define BP_CORRECTION (0.77)                                // pressure adjustment for altitude (720 ft.) (Arlington Heights)
//#define DEFAULT_LATITUDE 42.166191                          // Default latutide for Algonquin IL using phone GPS
//#define DEFAULT_LONGITUDE -88.263921                       // Default longitude for Algonquin IL using phone GPS
//#define DEFAULT_LATITUDE 26.132520                          // Default latutide for Fort Lauderdale from Google
//#define DEFAULT_LONGITUDE -80.107590                        // Default longitude for Fort Lauderdale
//#define BP_CORRECTION (0.00)                                // default pressure adjustment (sea level) (Fort Lauderdale)
#define DEFAULT_LATITUDE 32.332296                          // Default latutide for Tucson using phone
#define DEFAULT_LONGITUDE -111.093299                       // Default longitude for Tucson using phone
//#define SEALEVELPRESSURE_HPA (1013.25)
// Correction factor, inches of mercury. is calculated as follows 
// >>   CF = [760 - (Altitude x 0.026)] ÷ 760 <<
#define BP_CORRECTION (0.923)                                // pressure adjustment for altitude in Tucson (2240 ft.)
//#define BP_CORRECTION (0.970)                                // pressure adjustment for altitude in Algonquin (854 ft.)
#define HA_DEFAULT_CONTROL 0                                // Set default to disable (both motion and environmental sensor)
#define HA_DEFAULT_MOTION 15                                // Set the motion default timeout to 15 seconds (max of 600 seconds)
#define FRIENDLY_SIZE 17                                    // BME board friendly name max characters + 1
#define DEBUG_LEVEL 2                                       // <2 -> Undefined (Basic); 2 -> Motion; 3 -> Enviromental; 4 -> EEPROM Variables; 5 -> EEPROM Dump/Passwords

// Define Board Types (used for pi BMEMot program to identify boards that respond to the INFO command)
#define undfinedBrd 0
#define BMEMotBrd 1
#define MotStatusBrd 2
//#define SumpBrd 3

