#include <Time.h>
#include <TimeLib.h>
#define VERSION "D0.00.12   January 22, 2025"
#include "A.h" //function declarations
#include "timerValues.h"
/* Changes in this version:
 1. Added new feature that shows the amount of daylight gained or lost 
    from the previous day. - rschaller
 2. Corrected a typo of the spelling of longitude - rschaller
 3. Added a space character before the "AM" or "PM" string in the DateTimeString() - rschaller 
 4. The time displayed at the top of the webpage will now indicate time zone and
    whether daylight saving time(DST) or standard time (ST) is being observed. Two
    functions, timeZoneName() and dstStatusLabel() was added to support this feature- rschaller
 5. Added dew point calculation based off of temperature and humidity sensor readings
    dew point reading is only displayed on the webpage. - rschaller
 6. TODO: Add a setting to the config page that allows the user to make the following changes to
    OLED display.  1 - Display barometric pressure only. 2 - Display dewpoint only
    3 - Alternate the display between pressure and dewpoint every 1.5 seconds.  There is
    not enough room on the OLED to display both at the same time. - rschaller
 7. TODO: Update the write eeprom code to save this new configuration change - rschaller */
// Board WeMos D1 R1

// These come from ESP8266 libs I get when I use http://arduino.esp8266.com/stable/package_esp8266com_index.json in Arduino Preferences for Additional Board Manager URLs
// to set up Arduino for ESP8266 development.
#if defined(ESP8266)
#include <ESP8266WiFi.h>
#elif defined(ESP32)
#include <WiFi.h>
#endif
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#if defined(ESP8266)
#include "ESP8266HTTPClient.h"
#elif defined(ESP32)
 #include "HTTPClient.h"
#endif                       // Added for new web server architecture 8/31/20  (Don't know if I need this)#include <ESP8266WebServer.h>               // Added for new web server architecture 8/31/20
#include <ESP8266WebServer.h>
#include <WiFiUdp.h>
#include <Adafruit_ILI9341.h>               // using version 1.0.2 (1.0.1 does not have esp8266 support, https://github.com/adafruit/Adafruit_ILI9341 )
#include <Adafruit_GFX.h>                   // using version 1.1.5
#include <Fonts/FreeSans12pt7b.h>           // Adafruit GFX font definition (Located inside the �Fonts� folder inside Adafruit_GFX)
// This is an ESP8266 specific SSD1306 library (which shows up as "ESP8266 OLED Driver for SSD1306 display" in Library Manager)
//#include <SSD1306.h>                        // this variant comes from https://github.com/adafruit/Adafruit-SSD1331-OLED-Driver-Library-for-Arduino
//#include "C:\Users\cmb002.MBLB\Documents\Arduino\libraries\esp8266-oled-ssd1306-master\SSD1306.h"  // UGLY
#include <ArduinoOTA.h>
#include <EEPROM.h>
#include <math.h>
#include <SSD1306.h>
// Include I/O Libraries
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>

Adafruit_BME280 bme;          // I2C Interface
unsigned long delayTime;

// OLED Definitiions
#include "Open_Sans_Regular_12.h"
SSD1306 display(OLED_ADDR, OLED_SDA, OLED_SCL);             // For I2C

// Both of these are included when you pick the Time library in Arduino IDE library manager
#include <Time.h>
#include <TimeLib.h>

// This is for the mqtt client/server
#include <PubSubClient.h>
//#include <ESP8266WebServer.h>

WiFiClient espClient;
PubSubClient client(espClient);
// This is a second mqtt server for Home Assistant
//#define DEFAULT_MQTT_SERVER "ha.bicyclist1.com"

#define ha_raw_topic "homeassistant/BMEmot"
WiFiClient espClient2;
PubSubClient haclient(espClient2);


// WiFi Access Point
IPAddress local_IP(192,168,4,1);
IPAddress gateway(192,168,4,9);
IPAddress subnet(255,255,255,0);
// Web Server
ESP8266WebServer server(PORT);

// Structure for daily and weekly statistics
struct STATS {
  time_t date;
  int count;
  time_t elapsed;
};

// Structure for raw data
struct RAW {
  time_t startTime;
  time_t endTime;
  int count;
  time_t totalElapsed;
  time_t startDelta;
};

struct STATS daily[DAILY_MAX];
struct STATS weekly[WEEKLY_MAX];
struct RAW rawData[DATA_MAX];
time_t utcTime;                                       // UTC time from internal clock
time_t currentTime;                                   // Local time
byte dstFlag = 0;                                     // Current DST state: 0 -> DST not active; 1 -> DST active
time_t sunRise;                                       // Sunrise Time
time_t yesterdaySunRise = now();                      // Yesterday's Sunrise Time
time_t sunSet;                                        // Sunset Time
time_t yesterdaySunSet = now();                       // Yesterday's Sunset Time
int sensorPin = A0;                                   // define analog input pin
int sensorValue = 0;                                  // initial sensor value
unsigned long previousBME280Millis = millis();        // store last time the environment variables were read
unsigned long previousEnvMillis = millis() - MQTT_ENVIROMENTAL_INTERVAL;    // store last time we logged the environmental values to the MQTT server
unsigned long previousMotionMillis = millis();        // store last time the motion sensor was read
unsigned long previousMqttMillis = 0;                 // initialized in setup...
unsigned long previousInputMillis = 0;                // store last time the input was read
unsigned long previousDisplayMillis = 0;              // store the last time the display was turned on
unsigned long previousInfoMillis = 0;                 // store the last time the info packet was sent
short lastState = 0;
short currentState = 0;                               // 0 = no motion ; 1 = motion detected 
short displayPressure = 0;                            // 0 = dew point displayed on OLED ; 1 = barometric pressure displayed on OLED
int currentCount = 0;
time_t lastStateChange = 0;
time_t startTime = 0;
time_t stopTime = 0;
time_t currentElapsed = 0;
byte timeZone = DEFAULT_TIME_ZONE;                    // Current Time Zone
time_t localTimeOffset = 0;                           // Local Time offset based on time zone and DST
time_t porTime = 0;                                   // Storage for POR time
byte runningAP = 0;                                   // Flag indicating AP mode running to get WiFi Parameters; 0 -> not running: 1 -> running

float temperature;                                    // In *F
float humidity;                                       // Percentage
float dewPoint;                                       // calculated dew point based on temperature and humidity sensor readings
float pressure;                                       // In inHg
float adjPressure;                                    // Presure in inHg after altitute adjustment
unsigned short weeklyPntr = 0;
unsigned short rawPntr = 0;
unsigned short midnightFlag = 0;                      // Don't do midnight processing on powerup
byte dstCheckFlag = 1;
unsigned short endOfDataFlag = 0;
unsigned short firstDelta = 0;                        // First time through start delta = 0
unsigned short porFlag = 0;                           // Initialize POR Message flag as POR not sent yet (0); 1 -> POR sent
unsigned short porValidFlag = 0;                      // Wait for valid time before sending POR time; 1 -> Valid time detected
unsigned short displayStatus = 1;                     // display is enable on powerup
unsigned short tempDisplayStatus = 0;                 // This MUST be set to zero if the display is to stay on
unsigned short mqttEnabled = 1;                       // enable reconnect to mqtt server; 1 -> enable
time_t validTimeDelay = 0;                            // Amount of time the board is up before a valid time is detected
char rawTopicName[32];
char haSensorTopicName[40];
char haMotionTopicName[40];
byte mac[6];                                          // Mac Address
char infoName[5];                                     // Board node name
char friendlyName[FRIENDLY_SIZE];                     // Friendly Name
char mac_string[18];                                  // Buffer to hold MAC address
char daylightLengthChangeMsg[150];                    // Buffer for message indicating daylight gain/loss from previous day
char timeZoneName[2];                                // Name of time zone selected
char timeZoneType[10];                                // Display whether daylight or standard time is in effect for the given time zone
char dstStatusLabel[4];
char ip_string[16];                                   // Buffer to hold IP address
byte testFlag = 0;
byte autoAdd = DEFAULT_SEQUENCE_NUMBER;               // BMEmot sequence number; 0 - disabled: 1-99 order to display sensors
byte controlFlags = DEFAULT_CONTROL;                  // Set control flags to default states
byte haMotionDetectedFlag = 0;                        // Set when motion is detected; reset after timeout
time_t haMotionDetectedTime = 0;                      // Last time motion was detected in mS
unsigned long previousHaMqttMillis = 0;               // initialized in setup...
byte haControlFlags = HA_DEFAULT_CONTROL;             // Set HA control flags to the default
unsigned int haMotionTimeout = HA_DEFAULT_MOTION;     // Set HA motion timeout to the default
char ssid[SSID_SIZE];                                 // WiFi SSID
char pass[PASS_SIZE];                                 // WiFi Password
char haMqttServer[MQTT_SERVER_SIZE];                  // HA server address
char haUser[MQTT_USER_SIZE];                          // HA user
char haPass[MQTT_PASSWORD_SIZE];                      // HA password
char mqttServer[MQTT_SERVER_SIZE];                    // MQTT server address
char timeServer[TIME_SERVER_SIZE];                    // Time server address


union {                                               // Altitude correction factor
    float f;                                          // float representation of correction factor
    unsigned char b[4];                               // byte representation of the floating number
} altCorrection;
union {                                               // Altitude correction factor
    double f;                                         // float representation of longitude (for sunrise/sunset calculations)
    unsigned char b[8];                               // byte representation of the floating number
} longitude;
union {                                               // Altitude correction factor
    double f;                                         // float representation of latitude (for sunrise/sunset calculations)
    unsigned char b[8];                               // byte representation of the floating number
} latitude;


void connectWifiAccessPoint(const char *ssid, const char *pass) {
  Serial.print("Connecting to SSID: ");
  Serial.println(ssid);
  UserFN::displayText("Booting\nConnecting to SSID:\n" + String(ssid));
  WiFi.reconnect();
  WiFi.mode(WIFI_STA);
//  WiFi.setPhyMode(WIFI_PHY_MODE_11G);  
  WiFi.begin(ssid, pass);

  String wifiConnect = "";
  unsigned int countToReset = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    wifiConnect += ".";
    UserFN::displayTextStatus(wifiConnect);
    countToReset++;
    if (countToReset >= 40) ESP.reset();  
  }
  Serial.println("");

  Serial.println("WiFi connected");
  UserFN::displayTextStatus("WiFi connected");
  delay(500);                                                   // Provide time to see display
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (unsigned int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  if(mqttEnabled == 1) {
    UserFN::sendInfoPacket();                                           // Send Info packet before POR packet
    String mqttBuffer = "[POR][" + String(porTime) + "]";       // Send POR packet
    client.publish(rawTopicName, mqttBuffer.c_str(), true);   
   }
}

// Connect to the mqtt server
void reconnect() {
  char clientName[17];
  
  if (!client.connected()) {
    unsigned long currentMillis = millis();
    if (currentMillis - previousMqttMillis >= MQTT_RETRY_TIME) {
      previousMqttMillis = currentMillis;                       // save the last attempt to connect to the MQTT server

      if (mqttEnabled == 1) {
        Serial.print("Attempting MQTT connection...");
        UserFN::displayTextStatus("Attempt MQTT...\n ");
        // Attempt to connect
        snprintf(clientName,17,"BME280Motion%02x%02x",mac[4],mac[5]);
        // If you do not want to use a username and password, change next line to eliminate the user and password
//        if (client.connect(clientName, mqttUser, mqttPassword)) {
        if (client.connect(clientName)) {
          Serial.println("connected");
          UserFN::displayTextStatus("MQTT connected\n");
          client.subscribe(req_topic);
        }
        else {
          Serial.print("failed, rc=");
          Serial.println(client.state());
          UserFN::displayTextStatus("MQTT connect failed\n");
        }
        delay(500);                                 // Provide time to see display
      }
    }
  }
}


// Connect to the home assistant mqtt server
void ha_reconnect() {
  char clientName[17];
  
  if (!haclient.connected()) {
    unsigned long currentMillis = millis();
    if (currentMillis - previousHaMqttMillis >= HA_MQTT_RETRY_TIME) {
      previousHaMqttMillis = currentMillis;        // save the last attempt to connect to the HA MQTT server

      if (haControlFlags != 0) {
        Serial.print("Attempting HA MQTT connection...");
        // Attempt to connect
        snprintf(clientName,16,"BMEmot%02x%02x",mac[4],mac[5]);
        if (haclient.connect(clientName, haUser, haPass)) {
          Serial.println("connected");
        }
        else {
          Serial.print("failed, rc=");
          Serial.println(haclient.state());
        }
      }
    }
  }
}


// Initialize the display
void initDisplay() {
  display.init();
  display.flipScreenVertically();               // The board is flipped for this implementation
}


// Write parameters out to EEPROM
void writeEepromParameters(){
  int i,j;
  
  EEPROM.begin(EEPROM_SIZE);
  for (i = 0; i < EEPROM_SIZE; ++i) EEPROM.write(i,0xFF);                 // Default all bytes in the EEPROM to 0xFF
  i = 0;                                                                  // write friendly name
  while ((i < FRIENDLY_SIZE-1) && (friendlyName[i] != 0)){
    EEPROM.write(offsetFN+i,friendlyName[i]);
    i++;
  }
  EEPROM.write(offsetFN+i,0);                                             // write EOF
  EEPROM.write(offsetTZ,timeZone);                                        // write time zone offset
  for(i = 0; i < 4; ++i) EEPROM.write(offsetAC+i,altCorrection.b[i]);     // write pressure correction
  for(i = 0; i < 8; ++i){
    EEPROM.write(offsetLat+i,latitude.b[i]);                              // write latitude    
    EEPROM.write(offsetLong+i,longitude.b[i]);                            // write longitude
  }
  EEPROM.write(offsetSN,autoAdd);                                         // write BMEmot sequence number
  EEPROM.write(offsetCF,controlFlags);                                    // write control/status flags
  EEPROM.write(offsetHACF,haControlFlags);                                // write HA control flags
  EEPROM.write(offsetMTO,(haMotionTimeout >> 8) & 0xFF);                  // write HA motion timeout value
  EEPROM.write(offsetMTO+1,haMotionTimeout & 0xFF);
  i = 0;                                                                  // write WiFi SSID
  while ((i < SSID_SIZE) && (ssid[i] != 0)){
    EEPROM.write(offsetSSID+i,ssid[i]);
    i++;
  }
  EEPROM.write(offsetSSID+i,0);                                           // write EOF
  i = 0;                                                                  // write WiFi Password
  while ((i < PASS_SIZE) && (pass[i] != 0)){
    EEPROM.write(offsetPASS+i,pass[i]);
    i++;
  }
  EEPROM.write(offsetPASS+i,0);                                           // write EOF
  i = 0;                                                                  // write MQTT Server address
  while ((i < MQTT_SERVER_SIZE) && (mqttServer[i] != 0)){
    EEPROM.write(offsetMQTT+i,mqttServer[i]);
    i++;
  }
  EEPROM.write(offsetMQTT+i,0);                                           // write EOF
  i = 0;                                                                  // write Time Server address
  while ((i < TIME_SERVER_SIZE) && (timeServer[i] != 0)){
    EEPROM.write(offsetTIME+i,timeServer[i]);
    i++;
  }
  EEPROM.write(offsetTIME+i,0);                                           // write EOF
  i = 0;                                                                  // write MQTT Server address
  while ((i < MQTT_SERVER_SIZE) && (mqttServer[i] != 0)){
    EEPROM.write(offsetMQTT+i,mqttServer[i]);
    i++;
  }
  EEPROM.write(offsetMQTT+i,0);                                           // write EOF
  i = 0;                                                                  // write Time Server address
  while ((i < TIME_SERVER_SIZE) && (timeServer[i] != 0)){
    EEPROM.write(offsetTIME+i,timeServer[i]);
    i++;
  }
  EEPROM.write(offsetTIME+i,0);                                           // Write EOF
  i = 0;                                                                  // write HA MQTT Server address
  while ((i < MQTT_SERVER_SIZE) && (haMqttServer[i] != 0)){
  EEPROM.write(offsetHAMQTT+i,haMqttServer[i]);
    i++;
  }
  EEPROM.write(offsetHAMQTT+i,0);                                         // Write EOF
  i = 0;                                                                  // write HA MQTT user name
  while ((i < MQTT_USER_SIZE) && (haUser[i] != 0)){
    EEPROM.write(offsetHAUSER+i,haUser[i]);
    i++;
  }
  EEPROM.write(offsetHAUSER+i,0);                                         // Write EOF
  i = 0;                                                                  // write HA MQTT user password
  while ((i < MQTT_PASSWORD_SIZE) && (haPass[i] != 0)){
    EEPROM.write(offsetHAPASS+i,haPass [i]);
    i++;
  }
  EEPROM.write(offsetHAPASS+i,0);                                         // Write EOF
  int calCRC = UserFN::calculateEEpromCRC(EEPROM_SIZE-2);                         // calculate CRC on EEPROM
  EEPROM.write(EEPROM_SIZE-2,calCRC >> 8);                                // write crc to EEPROM
  EEPROM.write(EEPROM_SIZE-1,calCRC & 0xFF);
  printf("New Calculated CRC = %04x\n",calCRC);
  if (DEBUG_LEVEL >= 5){
    printf("\n");
    for (j = 0; j < (EEPROM_SIZE/32); ++j){
      for (i = 0; i < 31; ++i) printf("%02x ",EEPROM.read(j*32 + i));
      printf("%02x\n",EEPROM.read(j*32 + 31));
    }
  }
  EEPROM.commit();
}// end write eeprom


// Calculate CRC across the EEPROM 
int UserFN::calculateEEpromCRC(int eepromSize){
  int crc;
  short temp;
  int i,j;
  
  // Initialize CRC register to all 1's
  crc = 0xffff;
  
  // Perform CRC calculation
  //  The CRC generator operates on one data byte at a time
  //  rotating the bits within the byte to the right.
  //  Notation: byte wide - rotate right  
  for (i=0; i<eepromSize; ++i) {
    temp = EEPROM.read(i);          // get byte of data and store in temp reg    
    for (j=0; j<8; ++j) {           // loop counter initialized for 8 bits
      if (((temp ^ crc) & 1) != 0) crc=((crc>>1)^0x0408)|0x8000;
      else crc = (crc>>1) & 0x7fff;
      temp = temp >> 1;             // shift input data to the right
      }
    }
  // Complement CRC output
  crc = crc ^ 0xFFFF;
  return crc;
}


// Send information packet to the mqtt server
void UserFN::sendInfoPacket(){
  char infoBuffer[128];                                 // Generic buffer (used for info message)
  
  if(mqttEnabled == 1) {
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("WiFi disconnected");
      connectWifiAccessPoint(ssid,pass);
    }
    snprintf(ip_string,16,"%d.%d.%d.%d",WiFi.localIP()[0],WiFi.localIP()[1],WiFi.localIP()[2],WiFi.localIP()[3]);
    snprintf(infoBuffer,128,"[INF][%s][%s][%s][%s][%5.3f][%d][%d][%d][%d]",
        infoName,friendlyName,mac_string,ip_string,altCorrection.f,timeZone,controlFlags,autoAdd,BMEMotBrd);
    client.publish(info_topic, infoBuffer, true);
  }
}


// ****************************** Setup ******************************
/*
 The following tasks are performed in setup()
 1. Setup a serial port for debugging messages
 2. Setup hardware pin for user switch
 3. Read EEPROM for user settings, if CRC is damaged or corrupt
    set to default values
 4. Set up the OLED display 
 5. Reset WiFi settings and Erase EEPROM if user holds down PB
    for 5 seconds.
 6. Check for valid SSID, if none valid, set up ad-hoc connection
 7. Connect to WiFi
 8. Start web and MQTT servers */
void setup() {
  unsigned short i;
  char apName[FRIENDLY_SIZE];
  Serial.begin(115200);
  delay(1000);
  pinMode(OLED_SWITCH, INPUT_PULLDOWN_16);
  Serial.println();

  WiFi.macAddress(mac);
  snprintf(mac_string,18,"%02x:%02x:%02x:%02x:%02x:%02x",mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);

  snprintf(infoName,5,"%02x%02x",mac[4],mac[5]);
  printf("Info Name: %s\n",infoName);
 
  // Initialize EEPROM to default values or initialize active variables
  EEPROM.begin(EEPROM_SIZE);
  int calCRC = UserFN::calculateEEpromCRC(EEPROM_SIZE-2);
  int storedCRC = (EEPROM.read(EEPROM_SIZE-2) << 8) | EEPROM.read(EEPROM_SIZE-1);
  printf("calCRC = %04x  storedCRC = %04x\n",calCRC, storedCRC);
  if (calCRC != storedCRC) {
    // EEPROM is corrupted or blank.  Initize EEPROM to default values
    printf("Initializing EEPROM with default values...\n");
    snprintf(friendlyName,FRIENDLY_SIZE,"Sensor-%s",infoName);          // Initialize default friendly name
    timeZone = DEFAULT_TIME_ZONE;                                       // Initialize default time zone
    controlFlags = DEFAULT_CONTROL;                                     // Initialize default control/state flags
    autoAdd = DEFAULT_SEQUENCE_NUMBER;                                  // BMEmot default sequence number; 0 - disabled
    altCorrection.f = BP_CORRECTION;                                    // Initialize default pressure correction for altitude
    latitude.f = DEFAULT_LATITUDE;                                      // Initialize default latitude
    longitude.f = DEFAULT_LONGITUDE;                                    // Initialize default longitude
    haControlFlags = HA_DEFAULT_CONTROL;
    haMotionTimeout = HA_DEFAULT_MOTION;
    strncpy(ssid,DEFAULT_SSID,SSID_SIZE-1);
    strncpy(pass,DEFAULT_PASS,PASS_SIZE-1);
    strncpy(mqttServer,DEFAULT_MQTT_SERVER,MQTT_SERVER_SIZE-1);
    strncpy(timeServer,DEFAULT_TIME_SERVER,TIME_SERVER_SIZE-1);
    strncpy(haMqttServer,DEFAULT_HA_MQTT_SERVER,MQTT_SERVER_SIZE-1);
    strncpy(haUser,DEFAULT_HA_MQTT_USER,MQTT_USER_SIZE-1);
    strncpy(haPass,DEFAULT_HA_MQTT_PASSWORD,MQTT_PASSWORD_SIZE-1);
    writeEepromParameters();
    if (DEBUG_LEVEL >= 4){
      printf("\nDefault: Friendly Name = %s\n",friendlyName);
      printf("Default: Time Zone = %d\n",timeZone);
      printf("Default: Control/Status flags = %02x\n",controlFlags);
      printf("Default: BMEmot Sequence Number = %d\n",autoAdd);
      printf("Default: Altitude Correction factor = %5.3f\n",altCorrection.f);
      printf("Default: Latitiude = %11.6f\n",latitude.f);
      printf("Default: Longitiude = %11.6f\n",longitude.f);
      printf("Default: WiFi SSID = %s\n",ssid);
      if (DEBUG_LEVEL >= 5) printf("Default: WiFi Password = %s\n",pass);
      else printf("Default: WiFi Password = ********\n");
      printf("Default: MQTT Server = %s\n",mqttServer);
      printf("Default: Time Server = %s\n",timeServer);
      printf("Default: HA Control flags = %02x\n",haControlFlags);
      printf("Default: HA Motion Timeout (sec) = %5d\n",haMotionTimeout);
      printf("Default: HA Server = %s\n",haMqttServer);
      printf("Default: HA User = %s\n",haUser);
      printf("Default: HA Password = %s\n",haPass);
      printf("Default: HA Password = ********\n");
    }
  }
  else {
    // Load active variables from EEPROM
    i = 0;                                                                    // Initialize friendly name
    while ((i < (FRIENDLY_SIZE-1)) && ((friendlyName[i] = EEPROM.read(offsetFN+i)) != 0)){
      i++;
    }
    for(i = 0; i < 4; ++i) altCorrection.b[i] = EEPROM.read(offsetAC+i);      // Initialize pressure correction for alt
    for(i = 0; i < 8; ++i){
      longitude.b[i] = EEPROM.read(offsetLong+i);                             // Initialize longitude
      latitude.b[i] = EEPROM.read(offsetLat+i);                               // Initialize latitude    
    }
    timeZone = EEPROM.read(offsetTZ);                                         // Initialize time zone
    controlFlags = EEPROM.read(offsetCF);                                     // Initialize control/state flags
    autoAdd = EEPROM.read(offsetSN);                                          // Initialize BMEmot default sequence number
    haControlFlags = EEPROM.read(offsetHACF);
    haMotionTimeout = (EEPROM.read(offsetMTO) << 8) | EEPROM.read(offsetMTO + 1);   // Initialize motion timeout
    i = 0;                                                                    // Initialize WiFi SSID
    while ((i < SSID_SIZE) && ((ssid[i] = EEPROM.read(offsetSSID+i)) != 0)){
      i++;
    }
    i = 0;                                                                    // Initialize WiFi Password
    while ((i < PASS_SIZE) && ((pass[i] = EEPROM.read(offsetPASS+i)) != 0)){
      i++;
    }
    i = 0;                                                                    // Initialize MQTT Server
    while ((i < MQTT_SERVER_SIZE) && ((mqttServer[i] = EEPROM.read(offsetMQTT+i)) != 0)){
      i++;
    }
    i = 0;                                                                    // Initialize WiFi Password
    while ((i < TIME_SERVER_SIZE) && ((timeServer[i] = EEPROM.read(offsetTIME+i)) != 0)){
      i++;
    }
    i = 0;                                                                    // Initialize HA server address
    while ((i < MQTT_SERVER_SIZE) && ((haMqttServer[i] = EEPROM.read(offsetHAMQTT+i)) != 0)){
      i++;
    }
    i = 0;                                                                    // Initialize HA user name
    while ((i < MQTT_USER_SIZE) && ((haUser[i] = EEPROM.read(offsetHAUSER+i)) != 0)){
      i++;
    }
    i = 0;                                                                    // Initialize HA user password
    while ((i < MQTT_PASSWORD_SIZE) && ((haPass[i] = EEPROM.read(offsetHAPASS+i)) != 0)){
      i++;
    }
    if (DEBUG_LEVEL >= 4){
      printf("\nFriendly Name = %s\n",friendlyName);
      printf("Time Zone = %d\n",timeZone);
      printf("Control/Status flags = %02x\n",controlFlags);
      printf("BMEmot Sequence Number = %d\n",autoAdd);
      printf("Altitude Correction factor = %5.3f\n",altCorrection.f);
      printf("Latitiude = %11.6f\n",latitude.f);
      printf("Longitiude = %11.6f\n",longitude.f);
      printf("HA Control flags = %02x\n",haControlFlags);
      printf("HA Motion Timeout (sec) = %5d\n",haMotionTimeout);
      printf("WiFi SSID = %s\n",ssid);
      if (DEBUG_LEVEL >= 5) printf("WiFi Password = %s\n",pass);
      else printf("WiFi Password = ********\n");
      printf("MQTT Server = %s\n",mqttServer);
      printf("Time Server = %s\n",timeServer);
      printf("HA Server = %s\n",haMqttServer);
      printf("HA User = %s\n",haUser);
      if (DEBUG_LEVEL >= 5) printf("HA Password = %s\n",haPass);
      else printf("HA Password = ********\n");
    }
  }

  // Set up the OLED display
  initDisplay();
  display.displayOn();                                                        // Turn on the display through the initialization all the time

  // Check for WiFi Reset and/or EEPROM Erase
  printf("Digital Pin Read = %d\n",digitalRead(OLED_SWITCH));
  if (digitalRead(OLED_SWITCH) == 1) {
    i = 0;
    while((digitalRead(OLED_SWITCH) == 1) && (i < WIFI_RESET_TIME)){
      UserFN::displayText("WiFi Reset in = " + String(WIFI_RESET_TIME-i) + "\n\n\n\n");
      delay(1000);
      i++;
    }
    if (i >= WIFI_RESET_TIME){
      // Reset WiFi Parameters
      ssid[0] = 0;
      pass[0] = 0;
      writeEepromParameters();                                                // Calculate CRC and write buffer to flash
      printf("WiFi Parameters Reset\n");
      while((digitalRead(OLED_SWITCH) == 1) && (i < EEPROM_RESET_TIME)){
        UserFN::displayText("WiFi Parameters Reset\n"
                    "EEPROM Reset in = " + String(EEPROM_RESET_TIME-i) + "\n\n\n");
        delay(1000);
        i++;
      }
    }
    if (i >= EEPROM_RESET_TIME){
      // Erase the EEPROM
      EEPROM.begin(EEPROM_SIZE);
      for (i = 0; i < EEPROM_SIZE; ++i) EEPROM.write(i,0xFF);
      EEPROM.commit();
      printf("EEPROM Parameters Reset\n");
      UserFN::displayText("WiFi Parameters Reset\n"
                  "EEPROM Reset\nWaiting...\n  Release Button\n");
      while(digitalRead(OLED_SWITCH) == 1){                                   // Wait for the button to be released
        delay(100);
      }
    }
    ESP.restart();                                                            // Restart           
  }

  UserFN::displayAll("Booting");  

  // Check for valid SSID.  If no SSID is defined then create an access point
  runningAP = 0;
  if (ssid[0] == 0) {
    Serial.print("Setting soft-AP configuration ... ");
    Serial.println(WiFi.softAPConfig(local_IP, gateway, subnet) ? "Ready" : "Failed!");

    snprintf(apName,FRIENDLY_SIZE,"Sensor-%s",infoName);                        // Initialize default AP name
    Serial.print("Setting soft-AP ... ");
    Serial.println(WiFi.softAP(apName,"BMEmot23") ? "Ready" : "Failed!");

    uint8_t macAddrAP[6];
    WiFi.softAPmacAddress(macAddrAP);
    Serial.printf("MAC address = %02x:%02x:%02x:%02x:%02x:%02x\n", macAddrAP[0], macAddrAP[1], macAddrAP[2], macAddrAP[3], macAddrAP[4], macAddrAP[5]);
    snprintf(mac_string,18,"%02x:%02x:%02x:%02x:%02x:%02x",macAddrAP[0],macAddrAP[1],macAddrAP[2],macAddrAP[3],macAddrAP[4],macAddrAP[5]);

    Serial.print("Soft-AP IP address = ");
    Serial.println(WiFi.softAPIP());

    // Display info on OLED
    UserFN::displayText("AP: " + String(apName) + "\n" +
               String("MAC: ") + String(mac_string) + "\n" +
               "IP: " +  WiFi.softAPIP().toString() + "\n" +
               VERSION);

    // Start the web server for the access point
    server.on("/", HTTP_GET, UserFN::handleWiFi);                       // Call the 'handleWIFi' function when a client requests URI "/"
    server.on("/gotwifi",HTTP_POST, UserFN::handleWiFiAction);
    server.on("/eraseEEPROM",HTTP_GET, UserFN::handleEraseEEPROM);
    server.on("/statusAction",HTTP_POST, UserFN::handleStatusAction);
    server.onNotFound(UserFN::handleNotFound);                          // When a client requests an unknown URI call function "handleNotFound"
    server.begin();
    Serial.println("Web server started");
    runningAP = 1;
    delay(2000);                                                // Give time to see version number
  }
  else
  {
    // We start by connecting to a WiFi network
    connectWifiAccessPoint(ssid,pass);
  
    // Print the network information
    UserFN::displayAll("My HTTP host:\n" +
               String("MAC: ") + String(mac_string) + "\n" +
               "IP: " +  WiFi.localIP().toString() + ":" + String(PORT) + "\n" +
               VERSION);
    delay(2000);                                                                // Give time to see the IP address
  
    // Connect to BME280 with default settings
    if (!bme.begin(0x76)) {
      Serial.println("Could not find a BME280 sensor");
      delay(5000);
      ESP.restart();                                                            // Revisit this...
    }
    
    // Start the web server
    server.on("/", HTTP_GET, UserFN::handleRoot);                       // Call the 'handleRoot' function when a client requests URI "/"
    server.on("/homeAction", HTTP_POST, UserFN::handleHomeAction);      // Call the 'handleRootAction' function when a client presses a button on the home page
    server.on("/reset", HTTP_GET, UserFN::handleReset);                 // Call the 'handleReset' function when a client requests URI "/reset"
    server.on("/displayon", HTTP_GET, UserFN::handleDisplayOn);
    server.on("/displayoff", HTTP_GET, UserFN::handleDisplayOff);
    server.on("/mqttenable", HTTP_GET, UserFN::handleMqttEnable);
    server.on("/mqttdisable", HTTP_GET, UserFN::handleMqttDisable);
    server.on("/config",HTTP_GET, UserFN::handleConfig);                // Call the 'handleConfig' function when a client requests URI "/config"
    server.on("/configAction",HTTP_POST, UserFN::handleConfigAction);
    server.on("/statusAction",HTTP_POST, UserFN::handleStatusAction);
    server.on("/wifi",HTTP_GET, UserFN::handleWiFi);                    // Call the 'handleWiFi' function when a client requests URI "/wifi"
    server.on("/gotwifi",HTTP_POST, UserFN::handleWiFiAction);
    server.on("/eraseEEPROM",HTTP_GET, UserFN::handleEraseEEPROM);
    server.on("/test",HTTP_GET, UserFN::handleTest);                    // Test page to enter a time in (disables future NTP updates after first time submission)
    server.on("/gottime",HTTP_POST, UserFN::handleGotTime);             // Test page processing...
    server.onNotFound(UserFN::handleNotFound);                          // When a client requests an unknown URI call function "handleNotFound"
    server.begin();                                             // Start the web server
    Serial.println("Web server started");
    // Setup MQTT servers
    client.setServer(mqttServer, 1883);
    client.setCallback(callback);
    snprintf(rawTopicName,32,"%s%02x%02x",raw_topic,mac[4],mac[5]);
    printf("MQTT Topic Name: %s\n",rawTopicName);
    if (((1<<mqttPowerup) & controlFlags) != 0) {
      mqttEnabled = 1;                                    // Enable mqtt
      previousMqttMillis = millis() - MQTT_RETRY_TIME;    // Connect to MQTT server first time through main loop
    }
    else mqttEnabled = 0;
  
    haclient.setServer(haMqttServer, 1883);
    snprintf(haSensorTopicName,40,"%s/%02x%02x/sensor",ha_raw_topic,mac[4],mac[5]);
    printf("HA MQTT Sensor Topic Name: %s\n",haSensorTopicName);
    snprintf(haMotionTopicName,40,"%s/%02x%02x/motion",ha_raw_topic,mac[4],mac[5]);
    printf("HA MQTT Motion Topic Name: %s\n",haMotionTopicName);
    if (haControlFlags != 0) {                                // HA MQTT server is enabled if an option is set
      previousHaMqttMillis = millis() - HA_MQTT_RETRY_TIME;   // Connect to HA MQTT server first time through main loop
    }
  
    // Set up Over The Air (OTA)
  //  ArduinoOTA.setHostname("ESP8266");              // Hostname defaults to esp8266-[ChipID] 
  //  ArduinoOTA.setPassword("admin");                // No authentication by default
    ArduinoOTA.onStart([]() {
      Serial.println("OTA Start");
    });
    ArduinoOTA.onEnd([]() {
      Serial.println("\nOTA End");
    });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
      Serial.printf("OTA Progress: %u%%\r", (progress / (total / 100)));
    });
    ArduinoOTA.onError([](ota_error_t error) {
      Serial.printf("OTA Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
      else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
      else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
      else if (error == OTA_END_ERROR) Serial.println("End Failed");
    });
    ArduinoOTA.begin();
    Serial.println("OTA ready");
  
    // Clear the daily buffer
    for (i = 0; i < DAILY_MAX; ++i)
    {
      daily[i].date = 0;
      daily[i].count = 0;
      daily[i].elapsed = 0;
    }
  
    // Clear the weekly buffer
    for (i = 0; i < WEEKLY_MAX; ++i)
    {
      weekly[i].date = 0;
      weekly[i].count = 0;
      weekly[i].elapsed = 0;
    }
  
    // Clear the raw data buffer
    for (i = 0; i < DATA_MAX; ++i)
    {
      rawData[i].startTime = 0;
      rawData[i].endTime = 0;
      rawData[i].count = 0;
      rawData[i].totalElapsed = 0;
      rawData[i].startDelta = 0;
    }

    // Set the display according to the power on selection for the display
    if (((1<<displayPowerup) & controlFlags) != 0) {
      display.displayOn();                                                      // Enable (turn on) the display
      previousDisplayMillis = millis();
      displayStatus = 1;
      tempDisplayStatus = 0;
    }
    else {
      display.displayOff();                                                     // Disable (turn off) the display
      displayStatus = 0;
    }
  
    delay(2000);                                                                // Time to see the display????
  }
  // initialize yesterday's sunrise/sunset times to current day
  // After second running of midnight processor, the sunrise/sunset
  // times for yesterday will be valid.
}// ***************************** End Setup ******************************

// ****************************** Main Loop ******************************
void loop() {
  if (runningAP == 1)
  {
    UserFN::displayTextStatus("Clients connected: " + String(WiFi.softAPgetStationNum()));
    server.handleClient();                                          // Listen for HTTP requests from clients
  }
  else
  {
    UserFN::updateTimeFromServer();                                         // sync internal clock periodically from NTP server
  
    // Connect to mqtt server if not connected
    if(mqttEnabled == 1) {
      if (!client.connected()) {
        reconnect();
      }
      client.loop();
    }
    
    if (haControlFlags != 0) {                                      // HA MQTT server is enabled if an option is set
      if (!haclient.connected()) {
        ha_reconnect();
      }
      haclient.loop();
    }
  
    utcTime = now();                                                // The time is now stored as UTC (gm) time
    currentTime = utcTime - localTimeOffset;                        // Get local time for this time through the loop
    unsigned long currentMillis = millis();                         // Get the number of milliseconds for this time through the loop
  
    // Execute power up functions one time on power up
    if(porFlag == 0) {
      porFlag = 1;                                                  // Disable future power up function calls
      UserFN::sendInfoPacket();                                             // Send Info packet before POR packet
      previousInfoMillis = currentMillis;                           // save the last time the info packet was sent
      delay(100);                                                   // delay for info packet to be processed by the pi (needed?)
    }
  
    if(porValidFlag == 0) {
      if (utcTime > SECS_PER_DAY) {
        porValidFlag = 1;
        porTime = utcTime - validTimeDelay;                           // Store POR time
        if(mqttEnabled == 1) {
          String mqttBuffer = "[POR][" + String(utcTime) + "]";       // Send POR packet
          client.publish(rawTopicName, mqttBuffer.c_str(), true);
        }
        UserFN::runSunRiseSet();                                              // Calculate sunrise and sunset packet - next time will be midnight processsing
      }
      else {
        validTimeDelay = utcTime;
      }
    }
  
    // Send info packet once every INFO_INTERVAL
    if (currentMillis - previousInfoMillis >= INFO_INTERVAL) {
      previousInfoMillis = currentMillis;                             // save the last time the info packet was sent
      UserFN::sendInfoPacket();
    }
  
    // Check for midnight processing and DST change
    UserFN::midnightProcess(currentTime);
  
    // Check to see if the display should be turned on
    if (currentMillis - previousInputMillis >= SAMPLE_INTERVAL) {
      previousInputMillis = currentMillis;
      if ((digitalRead(OLED_SWITCH) == 1) && (displayStatus == 0)) {
        previousDisplayMillis = currentMillis;                        // save the last time the display was turned on
        display.displayOn();
        tempDisplayStatus = 1;
      }
    }  
  
    // Turn off display after timeout
    if (tempDisplayStatus == 1) {
      if (currentMillis - previousDisplayMillis >= DISPLAY_INTERVAL) {
        display.displayOff();
        tempDisplayStatus = 0;
      }
    }   
  
    // Read BME280 at a given interval, not every time in the loop
    if (currentMillis - previousBME280Millis >= ENVIROMENTAL_INTERVAL) {
      previousBME280Millis = currentMillis;                                       // save the last time you read the sensor
      temperature          = (bme.readTemperature() * 9 / 5) + 32;                // In *F
      humidity             = bme.readHumidity();                                  // Percentage
      dewPoint             =  temperature - (9.0/25.0) * (100.0 - humidity);      // dewpoint approximation from wikipedia article
      pressure             = bme.readPressure() * 0.000295299831;                 // In inHg
      adjPressure          = pressure + altCorrection.f;                          // adjusted for altitude
      if (currentMillis - previousEnvMillis >= MQTT_ENVIROMENTAL_INTERVAL) {
        previousEnvMillis = currentMillis;                            // save the last time you read the sensor    
        if (WiFi.status() != WL_CONNECTED) {
          Serial.println("WiFi disconnected");
          connectWifiAccessPoint(ssid,pass);
        }
        if(mqttEnabled == 1) {
          String mqttBuffer = "[ENV][" + String(utcTime) + "][" + String(temperature,1) + "][" + String(humidity,1) + "][" + String(adjPressure,3) + "]";
          client.publish(rawTopicName, mqttBuffer.c_str(), true);
        }
        if ((haControlFlags & (1 << haSensorEnabled)) != 0) {
          String ha_mqttBuffer = "{\"temperature\": " + String(temperature,1) + ",\"humidity\": " + String(humidity,1) + 
                                ",\"pressure\": " + String(pressure,3) + "}";
          haclient.publish(haSensorTopicName, ha_mqttBuffer.c_str(), true);
          if (DEBUG_LEVEL >= 3)Serial.println(ha_mqttBuffer);        
        }
  //      Serial.printf("Heap = %d\n",ESP.getFreeHeap());
      }
    }
  
    // Read Motion Sensor at given interval, not every time in the loop
    if (currentMillis - previousMotionMillis >= MOTION_INTERVAL) {
      previousMotionMillis = currentMillis;                             // save the last time you read the sensor
      sensorValue = analogRead(sensorPin);
      //printf("Analog Sensor Value >>> %i \n", sensorValue);
      currentState = ((sensorValue >= 768) ? 1 : 0);
  //    printf("Current State; %d   Last State: %d\n",currentState,lastState);
      if (currentState != lastState) {
        lastStateChange = utcTime;
        if (currentState == 1) {
          startTime = utcTime;
          if(mqttEnabled == 1) {
            String mqttBuffer = "[MOS][" + String(startTime) + "]";
            client.publish(rawTopicName, mqttBuffer.c_str(), true);
          }
          if ((haControlFlags & (1 << haMotionEnabled)) != 0) {
            haMotionDetectedFlag = 1;
            haMotionDetectedTime = utcTime;
            String haMqttBuffer = "{\"event\": \"Active\"}";
            haclient.publish(haMotionTopicName, haMqttBuffer.c_str(), true);
            if (DEBUG_LEVEL >= 2) Serial.println(haMqttBuffer);
          }       
        }
        else {
          stopTime = utcTime;
          currentElapsed += stopTime - startTime;
          currentCount++;
          if(mqttEnabled == 1) {
            String mqttBuffer = "[MOT][" + String(startTime) + "][" + String(stopTime) + "][" + String(currentCount) + "]";
            client.publish(rawTopicName, mqttBuffer.c_str(), true);
          }
          if (DEBUG_LEVEL >= 2) {
            Serial.print("Values: ");
            Serial.print(startTime, DEC);
            Serial.print("  ");
            Serial.print(stopTime, DEC);
            Serial.print("  ");
            Serial.print(currentCount, DEC);
            Serial.print("  ");
            Serial.println(currentElapsed, DEC);
          }
          rawData[rawPntr].startTime = startTime;
          rawData[rawPntr].endTime = stopTime;
          rawData[rawPntr].count = currentCount;
          rawData[rawPntr].totalElapsed = currentElapsed;
          if (firstDelta == 0) {
            rawData[rawPntr].startDelta = 0;              // First run through so no previous start time so set start delta to 0
            firstDelta = 1;                               
          }
          else {
            if (rawPntr == 0) rawData[rawPntr].startDelta = rawData[rawPntr].startTime - rawData[DATA_MAX-1].startTime;
            else rawData[rawPntr].startDelta = rawData[rawPntr].startTime - rawData[rawPntr-1].startTime;
          }
          rawPntr++;
          if (rawPntr >= DATA_MAX) rawPntr = 0;
        }
        lastState = currentState;
      }
  
      // Digital timeout after motion detected for Home Assistant
      // The hardware will determine the minimum time between detects.  The timeout counter will be reset
      //   on every NEW hardware motion detection
      // Assume the motion option is enabled if the haMotionDetectedFlag is set
      if (((utcTime - haMotionDetectedTime) >= haMotionTimeout) && (haMotionDetectedFlag == 1)){
        haMotionDetectedFlag = 0;
        String haMqttBuffer = "{\"event\": \"Inactive\"}";
        haclient.publish(haMotionTopicName, haMqttBuffer.c_str(), true);
        if (DEBUG_LEVEL >= 2) Serial.println(haMqttBuffer);
      }
  
  //    Serial.print("Sensor Value read: ");
  //    Serial.println(sensorValue, DEC);
      
      // Display stuff to the OLED
      // TODO: Add two sections to the following if-else block that will also display
      // the dewpoint.  Since there isn't enough room on the OLED, Code will have to be added
      // to alternate the barometric pressure reading with that of the dewpoint.  The display
      // of dew point and barometric pressure will alternate at rate that makes these two
      // quantities easy to read. - rschaller
      if (currentState == 1 && displayPressure == 1){// motion dectected
        UserFN::displayText(
        "T = " + String(temperature,1) + "*F   H = " + String(humidity,1) + "%\n" +
        "P = " + String(adjPressure,3) + " inHg\n" +
        "                        " + UserFN::TimeString(utcTime - startTime,0) + "\n" +
        UserFN::ShortDateString(currentTime) + " " + UserFN::TimeString(currentTime,1) + "\n" +
        "");
        delay(1500);
        displayPressure = 0;
        //
      }
      else if(currentState == 1 && displayPressure == 0)
      {
        UserFN::displayText(
        "T = " + String(temperature,1) + "*F   H = " + String(humidity,1) + "%\n" +
        "DP = " + String(dewPoint,2) + " *F\n" +
        "                        " + UserFN::TimeString(utcTime - startTime,0) + "\n" +
        UserFN::ShortDateString(currentTime) + " " + UserFN::TimeString(currentTime,1) + "\n" +
        "");
        delay(1500);
        displayPressure = 1;
      }
      else {// no motion detected
        time_t localStopTime;
        if (stopTime == 0) localStopTime = 0;
        else localStopTime = stopTime - localTimeOffset;
        if(displayPressure == 1)
        {
        UserFN::displayText(
        "T = " + String(temperature,1) + "*F   H = " + String(humidity,1) + "%\n" +
        "P = " + String(adjPressure,3) + " inHg\n" +
        UserFN::TimeString(localStopTime,1) + "   " + UserFN::TimeString(stopTime - startTime,0) + "\n" +
        UserFN::ShortDateString(currentTime) + " " + UserFN::TimeString(currentTime,1) + "\n" +
        "");
        delay(1500);
        displayPressure = 0;
        }
        if(displayPressure == 0)
        {
          UserFN::displayText(
        "T = " + String(temperature,1) + "*F   H = " + String(humidity,1) + "%\n" +
        "DP = " + String(dewPoint,2) + " *F\n" +
        UserFN::TimeString(localStopTime,1) + "   " + UserFN::TimeString(stopTime - startTime,0) + "\n" +
        UserFN::ShortDateString(currentTime) + " " + UserFN::TimeString(currentTime,1) + "\n" +
        "");
        delay(1500);
        displayPressure = 1;
        }
      }
    }
       
    server.handleClient();                        // Listen for HTTP requests from clients
  
    ArduinoOTA.handle();                          // Listen for OTA Updates
  }                                               // End of main loop
}

// ******************** Send HTML Pages ********************
void UserFN::handleRoot() {
  unsigned short didnotlikeindex = weekday(currentTime) - 1;
  time_t localLastStateChange;
  String htmlBuffer;

  UserFN::displayTextStatus("Web page requested\n");
//  Serial.println("\n[Web page requested]");
  if (lastStateChange == 0) localLastStateChange = 0;
  else localLastStateChange = lastStateChange - localTimeOffset;
  htmlBuffer = ""; 
  htmlBuffer += "<head> <title>" + String(friendlyName) + "</title></head>"
        "<form action=\"/homeAction\" method=\"POST\">"
        "<font size='6' color='black'>Enviromental Display and Motion Status</font><br>"
        "<font size='3' color='blue'>" + DateTimeString(currentTime) + " " + String(UserFN::tzName(timeZone)) + String(UserFN::dstName(dstFlag)) + "</font>&emsp;&emsp;"
        "<font size='3' color='red'>" + String(friendlyName) + "</font>&emsp;&emsp;"
        "<font size='3' color='orange'>" + String(infoName) + "</font><br><br>&emsp;&emsp;"
        "<input type=\"submit\" value=\"Config\" name=\"submit\">&emsp;&emsp;"
        "<input type=\"submit\" value=\"WiFi\" name=\"submit\">&emsp;&emsp;";
  if (displayStatus == 0) htmlBuffer += "<input type=\"submit\" value=\"Display On\" name=\"submit\">";
  else htmlBuffer += "<input type=\"submit\" value=\"Display Off\" name=\"submit\">";
  htmlBuffer += "<br><br><font size='3' color='purple'>Latitude: " + String(latitude.f,6) 
        + "&emsp;Longitude: " + String(longitude.f,6)
        + "<br>Today's Sunrise:           " + UserFN::TimeString(sunRise, 0) + "&emsp;Today's Sunset:              " + UserFN::TimeString(sunSet, 0) 
        + "<br> Yesterday's Sunrise: " + UserFN::TimeString(yesterdaySunRise, 0) + "&emsp;Yesterday Sunset: " + UserFN::TimeString(yesterdaySunSet, 0)
        + "<br>" + String(daylightLengthChangeMsg) + "</font><br><br>"
        "<font size='3' color='green'>Temperature = " + String(temperature,1) + " *F<br>"
        + "Humidity    =       " + String(humidity,1) + " %<br>"
        + "Dew Point    =      " + String(dewPoint,1) + " F<br>"
        + "Pressure    =       " + String(adjPressure,3) + " inHg</font><br><br>"
        "<pre><font size='2' color='black' face='Courier'>Current<br>"
        "Date              Count  Elapsed<br>"
        + DateString(currentTime) + "  " + UserFN::zeroPad(currentCount, 5) + "  " + UserFN::TimeString(currentElapsed,0) + "<br>"
        "Current state: " + (lastState ? "running" : "stopped") + " since " + DateTimeString(localLastStateChange) + "<br><br>"
        "Daily<br>"
        "Date              Count  Elapsed<br>" + UserFN::dayStats(didnotlikeindex) + "<br>"
        "Weekly<br>"
        "W/E               Count  Elapsed (days:hours:minutes:seconds)<br>" + UserFN::weekStats(weeklyPntr) + "<br>";
  yield();                                      // Reset software watchdog timer
  endOfDataFlag = 0;
  htmlBuffer += "Raw data (Last 50 points)<br>"
        "Start Date        Start Time  End Time    Start Delta  Elapsed   Count  Total Elapsed<br>" + UserFN::rawStats(rawPntr,25) + "<br>";
  yield();                                      // Reset software watchdog timer
  htmlBuffer += rawStats(((rawPntr+25) % DATA_MAX),25) + "<br><br>"
        "Power On Reset: " + DateTimeString(porTime - localTimeOffset) + "&emsp;&emsp;&emsp;&emsp;Uptime: " 
        + String((time_t)((utcTime - porTime) / SECS_PER_DAY)) + ":" + UserFN::TimeString(utcTime - porTime,0) +
        "</font></pre>"
        "<br><font size='1'>" + String(VERSION) + "</font><br><form>";
  server.send(200, "text/html", htmlBuffer);
  UserFN::displayTextStatus("Web page complete\n");
//  Serial.println("[Web page completed]");
}

void UserFN::handleHomeAction() {
  String htmlBuffer;

  htmlBuffer = "";
    htmlBuffer += "<head> <title>HTML Meta Tag</title>";
  if (server.arg("submit") == String("Config")){
    htmlBuffer += "<meta http-equiv=\"refresh\" content = \"0; url=/config\" /></head>";
    server.send(200, "text/html", htmlBuffer);
  }
  else if (server.arg("submit") == String("WiFi")){
    htmlBuffer += "<meta http-equiv=\"refresh\" content = \"0; url=/wifi\" /></head>";
    server.send(200, "text/html", htmlBuffer);
  }
  else {
    htmlBuffer += "<meta http-equiv=\"refresh\" content = \"0; url=/\" /></head>";
    if (server.arg("submit") == String("Display On")){
      display.displayOn();                                                              // turn the display on
      previousDisplayMillis = millis();                                                 // save the last time the display was turned on
      displayStatus = 1;
     tempDisplayStatus = 0;
    }
    else if (server.arg("submit") == String("Display Off")){
      display.displayOff();                                                             // turn the display off
      displayStatus = 0;
    }
    server.send(200, "text/html", htmlBuffer);
  }
}

void UserFN::handleReset() {
  UserFN::sendStatusPage("Board is resetting");
  UserFN::displayTextStatus("Reset requested\n");
  delay(2000);
  ESP.restart();
}

void UserFN::handleConfig() {
  String htmlBuffer;
  char friendlyNameKludge[FRIENDLY_SIZE];
  char mqttServerKludge[MQTT_SERVER_SIZE];
  char timeServerKludge[TIME_SERVER_SIZE];
  char haMqttServerKludge[MQTT_SERVER_SIZE];
  char haUserKludge[MQTT_USER_SIZE];
  char haPassKludge[MQTT_PASSWORD_SIZE];

  UserFN::kludgeDo(friendlyName,friendlyNameKludge,strlen(friendlyName));                 // Take spaces out of friendly name field
  UserFN::kludgeDo(mqttServer,mqttServerKludge,strlen(mqttServer));                       // Take spaces out of text fields
  UserFN::kludgeDo(timeServer,timeServerKludge,strlen(timeServer));                       //
  UserFN::kludgeDo(haMqttServer,haMqttServerKludge,strlen(haMqttServer));                 //
  UserFN::kludgeDo(haUser,haUserKludge,strlen(haUser));                                   //   spaces should not be in these fields
  UserFN::kludgeDo(haPass,haPassKludge,strlen(haPass));
  htmlBuffer = ""; 
  htmlBuffer += "<head> <title>" + String(friendlyName) + "</title></head>"
        "<font size='6' color='black'>Enviromental Display and Motion Configuration</font><br>"
        "<font size='3' color='blue'>" + UserFN::DateTimeString(currentTime) + "</font>&emsp;&emsp;"
        "<font size='3' color='red'>" + String(friendlyName) + "</font>&emsp;&emsp;"
        "<font size='3' color='orange'>" + String(infoName) + "</font><br><br>"
        "<form action=\"/configAction\" method=\"POST\">"
        "<label for=\"zone\">Select time zone: </label>"
        "<select id=\"zone\" name=\"zone\">"
        + UserFN::printTimeZone("Eastern") + UserFN::printTimeZone("Central") + UserFN::printTimeZone("Mountain") + UserFN::printTimeZone("Pacific") +
        "</select>"
        "<br><br>"
  
        "<label for=""friendlyName"">Enter friendly name: </label>"
        "<input type=""text"" id=""friendlyName"" name=""friendlyName"" maxlength=""16"" value=" + friendlyNameKludge + " required>"
        "<br><br>"

        "<label for=""pressureAdj"">Enter altitude adjustment: </label>"
//        "<input type=""text"" id=""pressureAdj"" name=""pressureAdj"" maxlength=""5"" value=" + String(altCorrection.f,3) + " pattern=""[0-9]{1}.[0-9]{3}"">"
        "<input type=""text"" id=""pressureAdj"" name=""pressureAdj"" maxlength=""5"" value=" + String(altCorrection.f,3) + " pattern=""\\d{1}\\x2E{1}\\d{0,3}"">"
//        "<input type=""text"" id=""pressureAdj"" name=""pressureAdj"" maxlength=""7"" value=" + String(altCorrection.f,3) + " pattern=""\\d{1}\\x2E{1}\\d{0,5}"">"
        "<br><br>"

        "<label for=\"autoAdd\">Enter BMEmot sequence number (0-99): </label>"
        "<input type=\"number\" id=\"autoAdd\" name=\"autoAdd\" maxlength=\"2\" value=\"" + autoAdd + "\" min=\"0\" max=\"99\">"
        "<br><br>"

        "<label for=""latitude"">Enter latitude: </label>"
//        "<input type=""text"" id=""latitude"" name=""latitude"" maxlength=""11"" value=" + String(latitude.f,6) + " pattern=""[-]?[0-9]{0-2}.[0-9]{0-6}"">"
        "<input type=""text"" id=""latitude"" name=""latitude"" maxlength=""11"" value=" + String(latitude.f,6) + " pattern=""-?\\d{0,2}\\x2E{1}\\d{0,6}"">"
        "&emsp;&emsp;"
        "<label for=""longitude"">Enter longitude: </label>"
//        "<input type=""text"" id=""longitude"" name=""longitude"" maxlength=""11"" value=" + String(longitude.f,6) + " pattern=""[-]?[0-9]{0-3}.[0-9]{0-6}"">"
        "<input type=""text"" id=""longitude"" name=""longitude"" maxlength=""11"" value=" + String(longitude.f,6) + " pattern=""-?\\d{0,3}\\x2E{1}[0-9]{0,6}"">"
        "<br><br><br>"

        "<label for=""displayEnable"">Enable display on powerup: </label>"
        "<input type=\"radio\" id=\"on\" name=\"displayEnable\" value=\"on\"";
  if (((1<<displayPowerup) & controlFlags) != 0) htmlBuffer += " checked";
  htmlBuffer += "><label for=\"on\">On</label>&emsp;"
        "<input type=\"radio\" id=\"off\" name=\"displayEnable\" value=\"off\"";
  if (((1<<displayPowerup) & controlFlags) == 0) htmlBuffer += " checked";
  htmlBuffer += "><label for=\"off\">Off</label><br><br>"

        "<label for=""mqttEnable"">Enable mqtt on powerup: </label>"
        "<input type=\"radio\" id=\"on\" name=\"mqttEnable\" value=\"on\"";
  if (((1<<mqttPowerup) & controlFlags) != 0) htmlBuffer += " checked";
  htmlBuffer += "><label for=\"on\">On</label>&emsp;"
        "<input type=\"radio\" id=\"off\" name=\"mqttEnable\" value=\"off\"";
  if (((1<<mqttPowerup) & controlFlags) == 0) htmlBuffer += " checked";
  htmlBuffer += "><label for=\"off\">Off</label><br><br>"

        "<label for=""dstObserved"">Enable DST: </label>"
        "<input type=\"radio\" id=\"on\" name=\"dstObserved\" value=\"on\"";
  if (((1<<dstEnabled) & controlFlags) != 0) htmlBuffer += " checked";
  htmlBuffer += "><label for=\"on\">On</label>&emsp;"
        "<input type=\"radio\" id=\"off\" name=\"dstObserved\" value=\"off\"";
  if (((1<<dstEnabled) & controlFlags) == 0) htmlBuffer += " checked";
  htmlBuffer += "><label for=\"off\">Off</label><br><br>"

        "<label for=""alarmEnable"">Enable Motion Alarm: </label>"
        "<input type=\"radio\" id=\"on\" name=\"alarmEnable\" value=\"on\"";
  if (((1<<alarmEnabled) & controlFlags) != 0) htmlBuffer += " checked";
  htmlBuffer += "><label for=\"on\">On</label>&emsp;"
        "<input type=\"radio\" id=\"off\" name=\"alarmEnable\" value=\"off\"";
  if (((1<<alarmEnabled) & controlFlags) == 0) htmlBuffer += " checked";
  htmlBuffer += "><label for=\"off\">Off</label><br><br>"

        "<label for=""alertEnable"">Enable Motion Alert: </label>"
        "<input type=\"radio\" id=\"on\" name=\"alertEnable\" value=\"on\"";
  if (((1<<alertEnabled) & controlFlags) != 0) htmlBuffer += " checked";
  htmlBuffer += "><label for=\"on\">On</label>&emsp;"
        "<input type=\"radio\" id=\"off\" name=\"alertEnable\" value=\"off\"";
  if (((1<<alertEnabled) & controlFlags) == 0) htmlBuffer += " checked";
  htmlBuffer += "><label for=\"off\">Off</label><br><br>"

        "<label for=""emailEnable"">Enable Motion Email: </label>"
        "<input type=\"radio\" id=\"on\" name=\"emailEnable\" value=\"on\"";
  if (((1<<emailEnabled) & controlFlags) != 0) htmlBuffer += " checked";
  htmlBuffer += "><label for=\"on\">On</label>&emsp;"
        "<input type=\"radio\" id=\"off\" name=\"emailEnable\" value=\"off\"";
  if (((1<<emailEnabled) & controlFlags) == 0) htmlBuffer += " checked";
  htmlBuffer += "><label for=\"off\">Off</label><br><br>"

        "<label for=""porEmail"">Enable POR Email: </label>"
        "<input type=\"radio\" id=\"on\" name=\"porEmail\" value=\"on\"";
  if (((1<<porEmailEnabled) & controlFlags) != 0) htmlBuffer += " checked";
  htmlBuffer += "><label for=\"on\">On</label>&emsp;"
        "<input type=\"radio\" id=\"off\" name=\"porEmail\" value=\"off\"";
  if (((1<<porEmailEnabled) & controlFlags) == 0) htmlBuffer += " checked";
  htmlBuffer += "><label for=\"off\">Off</label><br><br>"

        "<label for=""undefinedEnable"">Enable undefined bit: </label>"
        "<input type=\"radio\" id=\"on\" name=\"undefinedEnable\" value=\"on\"";
  if (((1<<undefinedEnabled) & controlFlags) != 0) htmlBuffer += " checked";
  htmlBuffer += "><label for=\"on\">On</label>&emsp;"
        "<input type=\"radio\" id=\"off\" name=\"undefinedEnable\" value=\"off\"";
  if (((1<<undefinedEnabled) & controlFlags) == 0) htmlBuffer += " checked";
  htmlBuffer += "><label for=\"off\">Off</label><br><br>"

        "<label for=""mqttServer"">Enter MQTT server: </label>"
        "<input type=""text"" id=""mqttServer"" name=""mqttServer"" maxlength=""31"" value=" + String(mqttServerKludge) + " required>"
        "<br><br>"

        "<label for=""timeServer"">Enter Time server: </label>"
        "<input type=""text"" id=""timeServer"" name=""timeServer"" maxlength=""31"" value=" + String(timeServerKludge) + " required>"
        "<br><br>"

        "-------------------------------<br>"
        "Home Assistant (HA) parameters:<br><br>"
        "<label for=""haMotionEnable"">Enable HA motion sensor: </label>"
        "<input type=\"radio\" id=\"on\" name=\"haMotionEnable\" value=\"on\"";
  if (((1<<haMotionEnabled) & haControlFlags) != 0) htmlBuffer += " checked";
  htmlBuffer += "><label for=\"on\">On</label>&emsp;"
        "<input type=\"radio\" id=\"off\" name=\"haMotionEnable\" value=\"off\"";
  if (((1<<haMotionEnabled) & haControlFlags) == 0) htmlBuffer += " checked";
  htmlBuffer += "><label for=\"off\">Off</label>"
        "&emsp;&emsp;"
        "<label for=""haMotionTimeout"">Enter timeout in seconds: </label>"
        "<input type=""text"" id=""haMotionTimeout"" name=""haMotionTimeout"" maxlength=""3"" value=" + String(haMotionTimeout) + " min=\"0\" max=\"600\">"
        "<br><br>"
       
        "<label for=""haSensorEnable"">Enable HA environmental sensor: </label>"
        "<input type=\"radio\" id=\"on\" name=\"haSensorEnable\" value=\"on\"";
  if (((1<<haSensorEnabled) & haControlFlags) != 0) htmlBuffer += " checked";
  htmlBuffer += "><label for=\"on\">On</label>&emsp;"
        "<input type=\"radio\" id=\"off\" name=\"haSensorEnable\" value=\"off\"";
  if (((1<<haSensorEnabled) & haControlFlags) == 0) htmlBuffer += " checked";
  htmlBuffer += "><label for=\"off\">Off</label><br><br>"
  
        "<label for=""haMqttServer"">Enter HA MQTT server: </label>"
        "<input type=""text"" id=""haMqttServer"" name=""haMqttServer"" maxlength=""31"" value=" + String(haMqttServerKludge) + " required>"
        "<br><br>"

        "<label for=""haUser"">Enter HA MQTT user name: </label>"
        "<input type=""text"" id=""haUser"" name=""haUser"" maxlength=""31"" value=" + String(haUserKludge) + ">"
        "<br><br>"

        "<label for=""haPass"">Enter HA user password: </label>"
        "<input type=""password"" id=""haPass"" name=""haPass"" maxlength=""31"" value=" + String(haPassKludge) + ">"
        "<br><br>"

        "<br><br>"
        "<input type=\"submit\" value=\"Submit\" name=\"submit\">&emsp;"
        "<input type=\"reset\">&emsp;&emsp;"
        "<input type=\"submit\" value=\"Cancel\" name=\"submit\">"
        "</form>";
  server.send(200, "text/html", htmlBuffer);

  if (DEBUG_LEVEL >= 4){
    int i;
    printf("\nhandleConfig: Friendly name kludge = %s    ",friendlyNameKludge);
    for (i = 0; i < 16; ++i) printf("%02x ",friendlyNameKludge[i]);
    printf("%02x   %d\n",friendlyNameKludge[16],strlen(friendlyNameKludge));
    printf("handleConfig: Time zone = %d\n",timeZone);
    printf("handleConfig: Control/Status flags (Hex) = %02x\n",controlFlags);
    printf("handleConfig: autoAdd = %d\n",autoAdd);
    printf("handleConfig: altCorrection: f = %5.3f   d = %02x %02x %02x %02x\n",altCorrection.f,
              altCorrection.b[0],altCorrection.b[1],altCorrection.b[2],altCorrection.b[3]);
    printf("handleConfig: latitude: f = %11.6f\n",latitude.f);
    printf("handleConfig: longitude: f = %11.6f\n",longitude.f);
    printf("handleConfig: MQTT Server = %s\n",mqttServer);
    printf("handleConfig: Time Server = %s\n",timeServer);
    printf("handleConfig: HA Control flags = %02x\n",haControlFlags);
    printf("handleConfig: HA Motion Timeout (sec) = %5d\n",haMotionTimeout);
    printf("handleConfig: HA Server = %s\n",haMqttServer);
    printf("handleConfig: HA User = %s\n",haUser);
    if (DEBUG_LEVEL >= 5) printf("handleConfig: HA Password = %s\n",haPass);
    else printf("handleConfig: HA Password = ********\n");
  }
}

void UserFN::handleConfigAction() {
  String htmlBuffer;
  char friendlyNameKludge[FRIENDLY_SIZE];
  char mqttServerKludge[MQTT_SERVER_SIZE];
  char timeServerKludge[TIME_SERVER_SIZE];
  char haMqttServerKludge[MQTT_SERVER_SIZE];
  char haUserKludge[MQTT_USER_SIZE];
  char haPassKludge[MQTT_PASSWORD_SIZE];

  if (server.arg("submit") == String("Cancel")){
    htmlBuffer = "";                                                                  // Return to home page on cancel
    htmlBuffer += "<head> <title>HTML Meta Tag</title>"
        "<meta http-equiv=\"refresh\" content = \"0; url=/\" />"
        "</head>";
    server.send(200, "text/html", htmlBuffer);
  }
  else{
    if(!strncmp(server.arg("zone").c_str(),"Eastern",7)) timeZone = 5;                // Determine time zone offset
    else if(!strncmp(server.arg("zone").c_str(),"Central",7)) timeZone = 6;
    else if(!strncmp(server.arg("zone").c_str(),"Mountain",8)) timeZone = 7;
    else if(!strncmp(server.arg("zone").c_str(),"Pacific",7)) timeZone = 8;
    String str_value = server.arg("friendlyName");                                    // get friendly name from form
    int str_len = str_value.length() + 1;
    if (str_len <= FRIENDLY_SIZE) {
      str_value.toCharArray(friendlyNameKludge, str_len);
      UserFN::kludgeUndo(friendlyNameKludge,friendlyName,strlen(friendlyNameKludge));         // Replace spaces back into friendly name field
    }
    str_value = server.arg("pressureAdj");                                            // Get altitude pressure adjustment
    altCorrection.f = atof(str_value.c_str());

    str_value = server.arg("displayEnable");                                          // Get display enable on powerup selection
    if(str_value == "on") controlFlags |= (1<<displayPowerup);
    else controlFlags &= (0xFF ^(1<<displayPowerup));
    str_value = server.arg("mqttEnable");                                             // Get mqtt enable on powerup selection
    if(str_value == "on") controlFlags |= (1<<mqttPowerup);
    else controlFlags &= (0xFF ^(1<<mqttPowerup));
    str_value = server.arg("dstObserved");                                            // Get DST observed selection
    if(str_value == "on") controlFlags |= (1<<dstEnabled);
    else controlFlags &= (0xFF ^(1<<dstEnabled));
    str_value = server.arg("alarmEnable");                                            // Get alarm enable selection
    if(str_value == "on") controlFlags |= (1<<alarmEnabled);
    else controlFlags &= (0xFF ^(1<<alarmEnabled));
    str_value = server.arg("alertEnable");                                            // Get alert enable selection
    if(str_value == "on") controlFlags |= (1<<alertEnabled);
    else controlFlags &= (0xFF ^(1<<alertEnabled));
    str_value = server.arg("emailEnable");                                            // Get motion email enable selection
    if(str_value == "on") controlFlags |= (1<<emailEnabled);
    else controlFlags &= (0xFF ^(1<<emailEnabled));
    str_value = server.arg("porEmail");                                               // Get send email on POR selection
    if(str_value == "on") controlFlags |= (1<<porEmailEnabled);
    else controlFlags &= (0xFF ^(1<<porEmailEnabled));
    str_value = server.arg("undefinedEnable");                                        // Get undefined selection
    if(str_value == "on") controlFlags |= (1<<undefinedEnabled);
    else controlFlags &= (0xFF ^(1<<undefinedEnabled));

    str_value = server.arg("autoAdd");                                                // Get BMEmot sequence number
    autoAdd = atoi(str_value.c_str());
    str_value = server.arg("latitude");                                               // Get latitude
    latitude.f = atof(str_value.c_str());
    str_value = server.arg("longitude");                                              // Get longitude
    longitude.f = atof(str_value.c_str());  

    str_value = server.arg("mqttServer");                                             // get MQTT Server address from form
    str_len = str_value.length() + 1;
    if (str_len <= MQTT_SERVER_SIZE) {
      str_value.toCharArray(mqttServerKludge, str_len);
      UserFN::kludgeUndo(mqttServerKludge,mqttServer,strlen(mqttServerKludge));               // Replace spaces back into MQTT Server address
    }
    str_value = server.arg("timeServer");                                             // get Time Server address from form
    str_len = str_value.length() + 1;
    if (str_len <= TIME_SERVER_SIZE) {
      str_value.toCharArray(timeServerKludge, str_len);
      UserFN::kludgeUndo(timeServerKludge,timeServer,strlen(timeServerKludge));               // Replace spaces back into Time Server address
    }

    str_value = server.arg("haMqttServer");                                           // get HA MQTT Server address from form
    str_len = str_value.length() + 1;
    if (str_len <= MQTT_SERVER_SIZE) {
      str_value.toCharArray(haMqttServerKludge, str_len);
      UserFN::kludgeUndo(haMqttServerKludge,haMqttServer,strlen(haMqttServerKludge));         // Replace spaces back into HA MQTT Server address
    }
    str_value = server.arg("haUser");                                                 // get HA User name from form
    str_len = str_value.length() + 1;
    if (str_len <= MQTT_USER_SIZE) {
      str_value.toCharArray(haUserKludge, str_len);
      UserFN::kludgeUndo(haUserKludge,haUser,strlen(haUserKludge));                           // Replace spaces back into HA User name
    }
    str_value = server.arg("haPass");                                                 // get HA User password from form
    str_len = str_value.length() + 1;
    if (str_len <= MQTT_PASSWORD_SIZE) {
      str_value.toCharArray(haPassKludge, str_len);
      UserFN::kludgeUndo(haPassKludge,haPass,strlen(haPassKludge));                           // Replace spaces back into HA User password
    }
    str_value = server.arg("haMotionEnable");                                         // Get HA Motion enable selection
    if(str_value == "on") haControlFlags |= (1<<haMotionEnabled);
    else haControlFlags &= (0xFF ^(1<<haMotionEnabled));
    str_value = server.arg("haSensorEnable");                                         // Get Ha Sensor selection
    if(str_value == "on") haControlFlags |= (1<<haSensorEnabled);
    else haControlFlags &= (0xFF ^(1<<haSensorEnabled));
    str_value = server.arg("haMotionTimeout");                                        // Get Motion timeout value
    haMotionTimeout = atoi(str_value.c_str());

    writeEepromParameters();                                                          // EEPROM will only be writen if something changed
    localTimeOffset = UserFN::dstUSA(utcTime);                                                // Update current local time offset
    UserFN::sendInfoPacket();                                                                 // Send updated info packet
    
    if (DEBUG_LEVEL >= 4){
      int i;
      printf("\nhandleAction: Friendly name = %s    ",friendlyName);
      for (i = 0; i < 16; ++i) printf("%02x ",friendlyName[i]);
      printf("%02x   %d\n",friendlyName[16],strlen(friendlyName));
      printf("handleAction: Time zone = %d\n",timeZone);
      printf("handleAction: Control/Status flags (Hex) = %02x\n",controlFlags);
      printf("handleAction: autoAdd = %d\n",autoAdd);
      printf("handleAction: altCorrection: f = %5.3f   d = %02x %02x %02x %02x\n",altCorrection.f,
                altCorrection.b[0],altCorrection.b[1],altCorrection.b[2],altCorrection.b[3]);
      printf("handleAction: latitude: f = %11.6f\n",latitude.f);
      printf("handleAction: longitude: f = %11.6f\n",longitude.f);
      printf("handleAction: MQTT Server = %s\n",mqttServer);
      printf("handleAction: Time Server = %s\n",timeServer);
      printf("handleAction: HA Control flags = %02x\n",haControlFlags);
      printf("handleAction: HA Motion Timeout (sec) = %5d\n",haMotionTimeout);
      printf("handleAction: HA Server = %s\n",haMqttServer);
      printf("handleAction: HA User = %s\n",haUser);
      if(DEBUG_LEVEL >= 5) printf("handleAction: HA Password = %s\n",haPass);
      else printf("handleAction: HA Password = ********\n");
    }
 
    htmlBuffer = "Parameters saved"; 
    UserFN::sendStatusPage(htmlBuffer);
  }
}

// Create pop up menu for selecting the time zone
String UserFN::printTimeZone(String zone){
  String menu = "<option value=\"" + zone + "\"";
  if (((timeZone == 5) && (!strncmp(zone.c_str(),"Eastern",7))) ||
      ((timeZone == 6) && (!strncmp(zone.c_str(),"Central",7))) ||
      ((timeZone == 7) && (!strncmp(zone.c_str(),"Mountain",8))) ||
      ((timeZone == 8) && (!strncmp(zone.c_str(),"Pacific",7)))) menu += " selected";
  menu += ">" + zone + "</option>";
  return menu;
}


void UserFN::handleWiFi() {
  String htmlBuffer;
  char ssidKludge[SSID_SIZE];
  char passKludge[PASS_SIZE];

  UserFN::kludgeDo(ssid,ssidKludge,strlen(ssid));                                         // Take spaces out of text fields
  UserFN::kludgeDo(pass,passKludge,strlen(pass));
  htmlBuffer = ""; 
  htmlBuffer += "<head> <title>" + String(friendlyName) + "</title></head>"
        "<font size='6' color='black'>WiFi Parameters Configuration</font><br>";
  if(runningAP == 0) htmlBuffer += "<font size='3' color='blue'>" + UserFN::DateTimeString(currentTime) + "</font>&emsp;&emsp;";
  htmlBuffer += "<font size='3' color='red'>" + String(friendlyName) + "</font>&emsp;&emsp;"
        "<font size='3' color='orange'>" + String(infoName) + "</font><br><br>"
        "<form action=\"/gotwifi\" method=\"POST\">"
        "<label for=""ssid"">Enter WiFi SSID: </label>"
        "<input type=""text"" id=""ssid"" name=""ssid"" maxlength=""31"" value=" + String(ssidKludge) + ">"
        "<br><br>"

        "<label for=""pass"">Enter WiFi Password: </label>"
        "<input type=""password"" id=""pass"" name=""pass"" maxlength=""31"" value=" + String(passKludge) + ">"
        "<br><br>"

        "<br><br>"
        "<input type=\"submit\" value=\"Submit\" name=\"submit\">&emsp;"
        "<input type=\"reset\">&emsp;&emsp;"
        "<input type=\"submit\" value=\"Cancel\" name=\"submit\">&emsp;&emsp;"
//        "<input type=\"submit\" value=\"Restart\" name=\"submit\">&emsp;&emsp;"
        "</form>";
  server.send(200, "text/html", htmlBuffer);

  if (DEBUG_LEVEL >= 4){
    printf("\nUserFN::handleWiFi: WiFi SSID = %s\n",ssid);
    if (DEBUG_LEVEL >= 5) printf("UserFN::handleWiFi: WiFi Password = %s\n",pass);
    else printf("UserFN::handleWiFi: WiFi Password = ********\n");
  }
}

void UserFN::handleWiFiAction() {
  String htmlBuffer;
  String str_value;
  int str_len;
  char ssidKludge[SSID_SIZE];
  char passKludge[PASS_SIZE];

  if (server.arg("submit") == String("Cancel")){
    htmlBuffer = "";                                                                  // Return to home page on cancel
    htmlBuffer += "<head> <title>HTML Meta Tag</title>"
        "<meta http-equiv=\"refresh\" content = \"0; url=/\" />"
        "</head>";
    server.send(200, "text/html", htmlBuffer);
  }
  else{
    str_value = server.arg("ssid");                                                   // get WiFi SSID from form
    str_len = str_value.length() + 1;
    if (str_len <= SSID_SIZE) {
      str_value.toCharArray(ssidKludge, str_len);
      UserFN::kludgeUndo(ssidKludge,ssid,strlen(ssidKludge));                                 // Replace spaces back into WiFi SSID
    }
    str_value = server.arg("pass");                                                   // get WiFi password from form
    str_len = str_value.length() + 1;
    if (str_len <= PASS_SIZE) {
      str_value.toCharArray(passKludge, str_len);
      UserFN::kludgeUndo(passKludge,pass,strlen(passKludge));                                 // Replace spaces back into HA User password
    }
    writeEepromParameters();                                                          // EEPROM will only be writen if something changed
    
    if (DEBUG_LEVEL >= 4){
      printf("\nhandleWiFiAction: WiFi SSID = %s\n",ssid);
      if (DEBUG_LEVEL >= 5) printf("handleWiFiAction: WiFi Password = %s\n",pass);
      else printf("handleWiFiAction: WiFi Password = ********\n");
    }
    
    htmlBuffer = "Parameters saved"; 
    UserFN::sendStatusPage(htmlBuffer);
  }
}

// This is a kludge to handle a server error that does not handle spaces (correctly) in the initial value option.
// The server apparently sees the spaces in the value field as a delimiter between options even though the value
// field is enclosed by quotes. Therfore the kludgeDo routine replaces spaces with a non breaking space ($nbsp; or
// &#160; ==> equivalent to a byte value of 0xA0) where the server will consider it another character and the browsers
// will display it as a space.  kludgeUndo just reverses the process...
void UserFN::kludgeDo(char *friendlyName, char *friendlyNameKludge, int fieldSize){
  int i;
  memset(friendlyNameKludge,0xFF,FRIENDLY_SIZE);
  for (i = 0; i < fieldSize+1; ++i){
    if(friendlyName[i] == ' ') friendlyNameKludge[i] = 0xA0;
    else friendlyNameKludge[i] = friendlyName[i];
  }
}

void UserFN::kludgeUndo(char *friendlyNameKludge, char *friendlyName, int fieldSize){
  int i;
  memset(friendlyName,0xFF,FRIENDLY_SIZE);
  for (i = 0; i < fieldSize+1; ++i){
    if(friendlyNameKludge[i] == 0xA0) friendlyName[i] = ' ';
    else friendlyName[i] = friendlyNameKludge[i];
  }
}

void UserFN::handleDisplayOff() {
  // Disable the display
  display.displayOff();
  displayStatus = 0;

  // Send status to web page
  UserFN::sendStatusPage("Display turned off");
}

void UserFN::handleDisplayOn() {
  // Enable the display
  display.displayOn();
  previousDisplayMillis = millis();          // save the last time the display was turned on
  displayStatus = 1;
  tempDisplayStatus = 0;

  // Send status to web page
  UserFN::sendStatusPage("Display turned on");
}

void UserFN::handleMqttDisable() {
  // enable mqtt server
  mqttEnabled = 0;

  // Send status to web page
  UserFN::sendStatusPage("Disable reconnect attempts to MQTT server");
}

void UserFN::handleMqttEnable() {
  // enable mqtt server
  mqttEnabled = 1;
  previousMqttMillis = millis() - MQTT_RETRY_TIME;    // Connect to MQTT server next time through main loop

  // Send status to web page
  UserFN::sendStatusPage("Enabled MQTT connection");
}

void UserFN::handleNotFound(){
  // Send HTTP status 404 (Not Found) when there's no handler for the URI in the request
  server.send(404, "text/plain", "404: Not found");
}

void UserFN::handleEraseEEPROM() {
  int i;
  
  // Erase the EEPROM
  EEPROM.begin(EEPROM_SIZE);
  for (i = 0; i < EEPROM_SIZE; ++i) EEPROM.write(i,0xFF);
  EEPROM.commit();

  // Send status to web page
  UserFN::sendStatusPage("Erasing the EEPROM.  Resetting...");
  delay(1000);
  ESP.restart();
}

// Helper to send status on action requests in the url
void UserFN::sendStatusPage(String text) {
  String htmlBuffer;

  // Send status to web page
  htmlBuffer = ""; 
  htmlBuffer += "<head> <title>" + String(friendlyName) + "</title></head>"
        "<font size='6' color='black'>Enviromental Display and Motion Status</font><br>"
        "<font size='3' color='blue'>" + DateTimeString(currentTime) + "</font>&emsp;&emsp;"
        "<font size='3' color='red'>" + String(friendlyName) + "</font>&emsp;&emsp;"
        "<font size='3' color='orange'>" + String(infoName) + "</font><br><br>"
        "<form action=\"/statusAction\" method=\"POST\">"
        "<pre><font size='2' color='black' face='Courier'>" + text + "<br></font></pre>"

        "<br>"
        "<input type=\"submit\" value=\"Home\" name=\"submit\">&emsp;&emsp;"
        "<input type=\"submit\" value=\"Restart\" name=\"submit\">"
        "</form>";
  server.send(200, "text/html", htmlBuffer);
}

void UserFN::handleStatusAction()
{
  String htmlBuffer;

  if (server.arg("submit") == String("Restart")){
    ESP.restart();                                                                      // Restart
  }
  else if (server.arg("submit") == String("Home")){
    htmlBuffer = "";                                                                    // Return to home page
    htmlBuffer += "<head> <title>HTML Meta Tag</title>"
                  "<meta http-equiv=\"refresh\" content = \"0; url=/\" />"
                  "</head>";
    server.send(200, "text/html", htmlBuffer);
  }
  else server.send(404, "text/plain", "Oops -- Server Error");
}

// ******************** Midnight Processor ********************
void UserFN::midnightProcess(time_t currentTime) {
  tmElements_t currentTime_tm;
  unsigned short index;
  unsigned short i;
  
  breakTime(currentTime, currentTime_tm);                         // Convert seconds to Date/Time format
  if (currentTime_tm.Hour == 0)
  {
    if(midnightFlag == 1)
    {
      // Do midnight processing
      index = currentTime_tm.Wday-1;
      if (index == 0) index = DAILY_MAX;
      index--;
      daily[index].date = previousMidnight(currentTime) - SECS_PER_DAY;
      daily[index].count = currentCount;
      daily[index].elapsed = currentElapsed;
//      Serial.print("Daily values: ");
//      Serial.print(index, DEC);
//      Serial.print("  ");
//      Serial.print(daily[index].date, DEC);
//      Serial.print("  ");
//      Serial.print(daily[index].count, DEC);
//      Serial.print("  ");
//      Serial.println(daily[index].elapsed, DEC);
      currentCount = 0;
      currentElapsed = 0;

      // Do weekly processing on Sunday
      if (currentTime_tm.Wday == 1)
      {
//        time_t skipWeek = 0;
//        for (i = 0; i < DAILY_MAX; ++i) skipWeek += weekly[weeklyPntr].date;
//        if (skipWeek != 0)
//        {
          weekly[weeklyPntr].date = previousMidnight(currentTime) - SECS_PER_DAY;
          weekly[weeklyPntr].count = 0;
          weekly[weeklyPntr].elapsed = 0;
          for (i = 0; i < DAILY_MAX; ++i)
          {
            weekly[weeklyPntr].count += daily[i].count;
            weekly[weeklyPntr].elapsed += daily[i].elapsed;
          }
          weeklyPntr++;
          if (weeklyPntr >= WEEKLY_MAX) weeklyPntr = 0;
//        }
      }

      byte tempDstFlag = dstFlag;                   // Store current DST state
      UserFN::dstUSA(utcTime + (2 * SECS_PER_HOUR));         // Kludge to handle days that have a time change 
      runSunRiseSet();                              // Calculate sunrise and sunset times
      dstFlag = tempDstFlag;                        // Restore current DST state
      
      midnightFlag = 0;                             // Disable midnight processing again until tomorrow
    }
  }
  else midnightFlag = 1;                            // Enable midnight processing

  // Do 2AM processing to test for DST change
  if (currentTime_tm.Hour == 2)
  {
    if(dstCheckFlag == 1) {
      localTimeOffset = UserFN::dstUSA(utcTime);            // GMT time is required 
      dstCheckFlag = 0;                             // Disable DST check until tomorrow
    }
  }
  else dstCheckFlag = 1;                            // Enable DST check
}// END midnight processor


// ******************** NPT time server ********************
//const long timeInterval = 10 * 60 * 1000;             // TEST interval at which to read time webpage (10 minutes)
const long timeInterval = 60 * 60 * 1000;             // interval at which to read time webpage (hourly)
//const long timeInterval = 24 * 60 * 60 * 1000;        // interval at which to read time webpage (daily)
unsigned long previousTimeMillis = timeInterval;      // will store last time was read
bool firstTimeGot = false;

void UserFN::updateTimeFromServer() {
  unsigned long currentMillis = millis();
  if (currentMillis - previousTimeMillis >= timeInterval) {
    previousTimeMillis = currentMillis;               // save the last time you read the server time
    if (UserFN::setNTPtime() || firstTimeGot) {
      previousTimeMillis = currentMillis;
      firstTimeGot = true;
    } else {
      previousTimeMillis = currentMillis - timeInterval + (30 * 1000);      // if failed, try again in 30 seconds
    }
  }
}

// -------------------- Retrieve time from NPT server --------------------
WiFiUDP udp;
unsigned int localPort = 2390;                            // local port to listen for UDP packets
const int NTP_PACKET_SIZE = 48;                           // NTP time stamp is in the first 48 bytes of the message
byte packetBuffer[NTP_PACKET_SIZE];                       //buffer to hold incoming and outgoing packets

bool UserFN::setNTPtime() {
  time_t epoch = 0UL;
  if (testFlag != 0) return false;
  if ((epoch = UserFN::getFromNTP(timeServer)) != 0) {            // get time from NTP server
    epoch -= 2208988800UL;                                // Unix time starts in 1970 (substract 70 years)
    setTime(epoch);                                       // Set UTC time for the main clock
    localTimeOffset = UserFN::dstUSA(epoch);                      // Get current local time offset
    return true;
  }
  return false;
}

unsigned long UserFN::getFromNTP(const char* server) {
  udp.begin(localPort);
  UserFN::sendNTPpacket(server);                                  // send an NTP packet to a time server
  delay(1000);                                            // wait to see if a reply is available
  int cb = udp.parsePacket();
  if (!cb) {
    Serial.println("No packet yet");
    return 0UL;
  }
  Serial.print("packet received, length=");
  Serial.println(cb);
  // We've received a packet, read the data from it
  udp.read(packetBuffer, NTP_PACKET_SIZE);                // read the packet into the buffer

  // The timestamp starts at byte 40 of the received packet and is four bytes,
  // or two words, long. First, extract the two words:
  unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
  unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);
  // combine the four bytes (two words) into a long integer
  // this is NTP time (seconds since Jan 1 1900):
  udp.stop();
  return (unsigned long) highWord << 16 | lowWord;
}

// send an NTP request to the time server at the given address
void UserFN::sendNTPpacket(const char  *address) {
  Serial.print("Sending NTP packet...");
  memset(packetBuffer, 0, NTP_PACKET_SIZE);         // set all bytes in the buffer to 0
  // Initialize values needed to form NTP request (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;                     // LI, Version, Mode
  packetBuffer[1] = 0;                              // Stratum, or type of clock
  packetBuffer[2] = 6;                              // Polling Interval
  packetBuffer[3] = 0xEC;                           // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12] = 49;
  packetBuffer[13] = 0x4E;
  packetBuffer[14] = 49;
  packetBuffer[15] = 52;

  // all NTP fields have been given values, now send a packet requesting a timestamp
  udp.beginPacket(address, 123);                    // NTP requests are to port 123
  udp.write(packetBuffer, NTP_PACKET_SIZE);
  udp.endPacket();
}

// Calculate if summertime in USA (2nd Sunday in Mar, first Sunday in Nov)
// The time to be passed to this routine is UTC time.  The local time offset including DST will be returned.
time_t UserFN::dstUSA (time_t t)
{
  tmElements_t te;
  time_t dstStart, dstEnd;
  time_t timeOffset = timeZone * SECS_PER_HOUR;

  if (((1<<dstEnabled) & controlFlags) == 0){
    dstFlag = 0;
    return(timeOffset);
  }

  t -= timeOffset;
  te.Year = year(t) - 1970;
  te.Month = 3;
  te.Day = 7;
  te.Hour = 0;                                        // The time changes at 2AM
  te.Minute = 0;
  te.Second = 0;
  dstStart = makeTime(te) + timeOffset;               // Calculate first Sunday on or after March 8th
  dstStart = nextSunday(dstStart) + (2 * SECS_PER_HOUR);
  te.Month = 11;
  te.Day = 1;
  dstEnd = makeTime(te) - SECS_PER_DAY;                // Calculate first Sunday on or after November 1st
  dstEnd = nextSunday(dstEnd) + SECS_PER_HOUR;
  if (t >= dstStart && t < dstEnd) {
    dstFlag = 1;                                      // DST is in effect
    return (timeOffset - SECS_PER_HOUR);              // DST is in effect - Add in one hours worth of seconds
  } else {
    dstFlag = 0;                                      // DST is not in effect
    return (timeOffset);
  }
}

// ******************** Date format helpers ********************
String UserFN::DateTimeString(time_t currentTime)
{
  return  String(dayStr(weekday(currentTime))) + " " + String(monthStr(month(currentTime))) + " " + String(day(currentTime)) + 
    ", " + String(year(currentTime)) + "&emsp;" + UserFN::spacePad(hourFormat12(currentTime), 2) + ":" + UserFN::zeroPad(minute(currentTime), 2) + 
    ":" + UserFN::zeroPad(second(currentTime), 2) + (isAM(currentTime) ? " AM" : " PM");
}

String UserFN::DateString(time_t currentTime)
{
  return String(dayShortStr(weekday(currentTime))) + " " + String(monthShortStr(month(currentTime))) + " " + 
    UserFN::UserFN::spacePad(day(currentTime),2) + ", " + String(year(currentTime));
}

String UserFN::ShortDateString(time_t currentTime)
{
  return String(dayShortStr(weekday(currentTime))) + " " + String(monthShortStr(month(currentTime))) + " " + 
    UserFN::spacePad(day(currentTime),2);
}

String UserFN::TimeString(time_t currentTime, short ampmFlag)
{
  return (ampmFlag ? UserFN::spacePad(hourFormat12(currentTime), 2) : UserFN::zeroPad(hour(currentTime), 2)) + ":" + 
    UserFN::zeroPad(minute(currentTime), 2) + ":" + UserFN::zeroPad(second(currentTime), 2) + 
    (ampmFlag ? (isAM(currentTime) ? "AM" : "PM") : "");
}

// ******************** String/Display Helpers ********************

// Function that returns a string representing the name of the selected 
// time zone
String UserFN::tzName(byte timeZone)
{
  if(timeZone == 5)           strcpy(timeZoneName, "E");
  else if(timeZone == 6)      strcpy(timeZoneName, "C");
  else if(timeZone == 7)      strcpy(timeZoneName, "M");
  else if(timeZone == 8)      strcpy(timeZoneName, "P");
  return String(timeZoneName);
}
// Function that returns a string denoting whether daylight saving
// time is active or not.
String UserFN::dstName(byte dstFlag)
{
  if(dstFlag == 0)  strcpy(dstStatusLabel, "ST");
  else if(dstFlag == 1) strcpy(dstStatusLabel, "DST");
  return String(dstStatusLabel);
}
// Helper to left zero pad a value to a given string size
String UserFN::zeroPad(int value, int digits) {
  String s = String(value);
  while (s.length() < (unsigned int)digits) {
    s = "0" + s;
  }
  return s;
}

// Helper to left space pad a value to a given string size
String UserFN::spacePad(int value, int digits) {
  String s = String(value);
  while (s.length() < (unsigned int)digits) {
    s = " " + s;
  }
  return s;
}

// Helper string to display to both serial port and OLED - use only for initial startup or unusual (i.e. error) conditons,
// so you don't flash stuff to OLED in normal use
void UserFN::displayAll(String text) {
  Serial.println(text);
  UserFN::displayText(text);
}

// display text to OLED - every \n newline character advances to next line (4 lines available)
void UserFN::displayText(String lines) {
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_LEFT);
//  display.setFont(ArialMT_Plain_10);
  display.setFont(Open_Sans_Regular_12);

  int lineY = 0;
  int ptr = 0;
  int len = lines.length();
  while (ptr < len) {
    int newLine = lines.indexOf("\n", ptr);
    if (newLine == -1) {
      newLine = len;
    }
    String line = lines.substring(ptr, newLine);
    if (line.length() > 0) {
      display.drawString(0, lineY, line);
    }
    lineY += 16;
    ptr = newLine + 1;
  }
  display.display();
}

// updates just the bottom line of the display
void UserFN::displayTextStatus(String line) {
  display.setColor(BLACK);
  display.fillRect(0, 16 * 3, 128, 16);
  display.setColor(WHITE);
  display.setTextAlignment(TEXT_ALIGN_LEFT);
//  display.setFont(ArialMT_Plain_10);
  display.setFont(Open_Sans_Regular_12);
  display.drawString(0, 16 * 3, line);
  display.display();
}

String UserFN::dayStats(short index)
{
  unsigned short i;
  String payload = "";
  for (i = 0; i < DAILY_MAX; ++i)
  {
    if (index == 0) index = DAILY_MAX;
    index--;
    if (daily[index].date == 0)
    {
      payload = payload + "*** End of available daily statistics ***<br>";
      break;
    }
    payload = payload + UserFN::DateString(daily[index].date) + "  " + UserFN::zeroPad(daily[index].count , 5) + "  " + UserFN::TimeString(daily[index].elapsed,0) + "<br>";
//    Serial.print(index,DEC);
//    Serial.print("  ");
//    Serial.println(payload);
//    yield();
  }
  return payload;
}

String UserFN::weekStats(short index)
{
  unsigned short i;
  String payload = "";
//  Serial.println(index,DEC);
  for (i = 0; i < WEEKLY_MAX; ++i)
  {
    if (index == 0) index = WEEKLY_MAX;
    index--;
    if (weekly[index].date == 0)
    {
      payload = payload + "*** End of available weekly statistics ***<br>";
      break;
    }
    payload = payload + UserFN::DateString(weekly[index].date) + "  " + UserFN::zeroPad(weekly[index].count , 5) + "  " + 
      UserFN::zeroPad(day(weekly[index].elapsed) - 1,2) + ":" + UserFN::TimeString(weekly[index].elapsed,0) + "<br>";
  }
  return payload;
}

String UserFN::rawStats(short index,short limit)
{
  unsigned short i;
  String payload = "";
  if(endOfDataFlag == 0) {
    for(i = 0; i < limit; ++i)
    {
      if (index == 0) index = DATA_MAX;
      index--;
      if (rawData[index].startTime == 0)
      {
        payload = payload + "*** End of available raw data statistics ***<br>";
        endOfDataFlag = 1;
        break;
      }
      payload = payload + UserFN::DateString(rawData[index].startTime-localTimeOffset) + "  " + 
        UserFN::TimeString(rawData[index].startTime-localTimeOffset,1) + "  " + 
        UserFN::TimeString(rawData[index].endTime-localTimeOffset,1) + "  " + 
        UserFN::zeroPad(day(rawData[index].startDelta) - 1,2) + ":" + UserFN::TimeString(rawData[index].startDelta,0) + "  " +
        UserFN::TimeString(rawData[index].endTime - rawData[index].startTime,0) + "  " +
        UserFN::zeroPad(rawData[index].count , 5) + "  " + UserFN::TimeString(rawData[index].totalElapsed,0) + "<br>";
//      Serial.print(index,DEC);
//      Serial.print("  ");
//      Serial.println(payload);
//      yield();
    }
  }
  return payload;
}


void UserFN::handleTest() {
  String htmlBuffer;
  //char friendlyNameKludge[FRIENDLY_SIZE];

  htmlBuffer = ""; 
  htmlBuffer += "<font size='6' color='black'>Enviromental Display and Motion Configuration</font><br>"
        "<font size='3' color='blue'>" + DateTimeString(currentTime) + "</font><br><br>"
        "<form action=\"/gottime\" method=\"POST\">"
        "<label for=""testTime"">Enter time: </label>"
        "<input type=""text"" id=""tsetTime"" name=""testTime"" maxlength=""10"">"
        "<br><br>"

        "<input type=""submit"" value=""Submit"" name=""submit"">&emsp;"
        "</form>";
  server.send(200, "text/html", htmlBuffer);
}

void UserFN::handleGotTime() {
  String str_value = server.arg("testTime");
  time_t enteredTime = atol(str_value.c_str());
  setTime(enteredTime);
  localTimeOffset = UserFN::dstUSA(enteredTime);
  testFlag = 1;
  printf("Test: Local time offset = %lld\n",localTimeOffset);

  server.send(200, "text/html", "Time has been changed");
}

// Calculate both Sunrise and Sunset times
void UserFN::runSunRiseSet(){
  tmElements_t utcTime_tm;
  tmElements_t sun_tm;
    
  breakTime(utcTime, utcTime_tm);                                 // Convert seconds to Date/Time format
  sunRise          = calculateSunRiseSet(utcTime_tm.Year,utcTime_tm.Month,utcTime_tm.Day,latitude.f,longitude.f,timeZone*(-1),dstFlag,0);  // sunrise
  yesterdaySunRise = calculateSunRiseSet(utcTime_tm.Year,utcTime_tm.Month,utcTime_tm.Day-1,latitude.f,longitude.f,timeZone*(-1),dstFlag,0);  // yesterday's sunrise
  sunSet           = calculateSunRiseSet(utcTime_tm.Year,utcTime_tm.Month,utcTime_tm.Day,latitude.f,longitude.f,timeZone*(-1),dstFlag,1);   // sunset
  yesterdaySunSet  = calculateSunRiseSet(utcTime_tm.Year,utcTime_tm.Month,utcTime_tm.Day-1,latitude.f,longitude.f,timeZone*(-1),dstFlag,1);   // yesterday's sunset
  double daylight  = difftime(sunSet, sunRise);
  // Save sunset/sunrise to compare today's value to yesterday's value
  // Compute the time gain/loss from the previous day in minutes and seconds
  // The current implementation might cause problems on the first of the month
  // Yesterday's (utcTime_tm.day - 1) expression may evaluate to zero which will
  // cause problems. In this case, each month of the year will need to be considered
  // For example, when current day is April 1, the previous day will need to be the 31
  // and when current day is July 1, the previous day will need to be June 30.
  // Leap year produces a special case for the current day being March 1 and on 
  // New Years day, the previous day will be in the previous year.
  breakTime(sunRise, sun_tm);
  printf("Sunrise = %2d:%02d:%02d   ",sun_tm.Hour,sun_tm.Minute,sun_tm.Second);
  breakTime(yesterdaySunRise, sun_tm);
  printf("yesterday's Sunrise = %2d:%02d:%02d\n    ",sun_tm.Hour,sun_tm.Minute,sun_tm.Second);
  breakTime(sunSet, sun_tm);
  printf("Sunset  = %2d:%02d:%02d\n",sun_tm.Hour,sun_tm.Minute,sun_tm.Second);
  breakTime(yesterdaySunSet, sun_tm);
  printf("yesterday's Sunset = %2d:%02d:%02d\n    ",sun_tm.Hour,sun_tm.Minute,sun_tm.Second);
  double yesterdayDayLight            = difftime(yesterdaySunSet, yesterdaySunRise);
  double deltaDayLight                = difftime(daylight, yesterdayDayLight);
  double deltaMinutes                 = deltaDayLight/60;
  int    wholeMinutes                 = (int)deltaMinutes;
  double decDeltaMinutes              = deltaMinutes - wholeMinutes;
  double seconds                      = decDeltaMinutes * 60;
  strcpy(daylightLengthChangeMsg, "NOT AVAILABLE");
  if (deltaDayLight > 0)
  {
    // Passed the Winter Solstice and gaining daylight
    printf("Today is %i minutes and %i seconds longer than yesterday\n", wholeMinutes, (int)seconds);
    snprintf(daylightLengthChangeMsg,200,"Today is %i minutes and %3.0f seconds longer than yesterday\n", wholeMinutes, seconds);
  }
  else if (deltaDayLight < 0)
  {
    // Passed the Summer Solstice and loosing daylight
    printf("Today is %i minutes and %i seconds shorter than yesterday\n", abs(wholeMinutes), (int)abs(seconds));
    snprintf(daylightLengthChangeMsg,200,"Today is %i minutes and %i seconds shorter than yesterday\n", abs(wholeMinutes), (int)abs(seconds));
  }
  else
  {
    // unlikely this will happen mathematically but handle it just in case
    printf("No change from yesterday\n");
    strcpy(daylightLengthChangeMsg,"No change from yesterday\n");
  }
  return;
}

// Calculate sunrise and sunset function
//#define PI 3.1415926        // defined in C:\Users\cmb002.MBLB\AppData\Local\Arduino15\packages\esp8266\hardware\esp8266\3.0.2\cores\esp8266/Arduino.h:62
#define ZENITH -.83
time_t UserFN::calculateSunRiseSet(int year,int month,int day,float lat,float lng,int localOffset,int daylightSavings,int riseSet) {
  /*
  localOffset will be <0 for western hemisphere and >0 for eastern hemisphere
  daylightSavings should be 1 if it is in effect during the summer otherwise it should be 0
  riseSet will be 0 to calculate sunrise time or 1 to calculate sunset
  */
  // 1. first calculate the day of the year
  year += 1970;
  float N1 = floor(275 * month / 9);
  float N2 = floor((month + 9) / 12);
  float N3 = (1 + floor((year - 4 * floor(year / 4) + 2) / 3));
  float N = N1 - (N2 * N3) + day - 30;

  // 2. convert the longitude to hour value and calculate an approximate time
  float lngHour = lng / 15.0;      
  float t = N + ((6 - lngHour) / 24);   //if rising time is desired:

  // 3. calculate the Sun's mean anomaly   
  float M = (0.9856 * t) - 3.289;

  // 4. calculate the Sun's true longitude
  float L = fmod(M + (1.916 * sin((PI/180)*M)) + (0.020 * sin(2 *(PI/180) * M)) + 282.634,360.0);

  // 5a. calculate the Sun's right ascension      
  float RA = fmod(180/PI*atan(0.91764 * tan((PI/180)*L)),360.0);

  // 5b. right ascension value needs to be in the same quadrant as L   
  float Lquadrant  = floor( L/90) * 90;
  float RAquadrant = floor(RA/90) * 90;
  RA = RA + (Lquadrant - RAquadrant);

  //5c. right ascension value needs to be converted into hours   
  RA = RA / 15;

  // 6. calculate the Sun's declination
  float sinDec = 0.39782 * sin((PI/180)*L);
  float cosDec = cos(asin(sinDec));

  // 7a. calculate the Sun's local hour angle
  float cosH = (sin((PI/180)*ZENITH) - (sinDec * sin((PI/180)*lat))) / (cosDec * cos((PI/180)*lat));
  // if (cosH >  1) the sun never rises on this location (on the specified date)
  // if (cosH < -1) the sun never sets on this location (on the specified date)

  // 7b. finish calculating H and convert into hours
  float H;
  if (riseSet == 0) H = 360 - (180/PI)*acos(cosH);                      // if rising time is desired:
  else H = (180/PI)*acos(cosH);                                         // if setting time is desired:      
  H = H / 15;

  // 8. calculate local mean time of rising/setting      
  float T = H + RA - (0.06571 * t) - 6.622;

  // 9. adjust back to UTC
  float UT = fmod(T - lngHour,24.0);

  // 10. convert UT value to local time zone of latitude/longitude
  float localT = (UT + (float)localOffset + (float)daylightSavings);    // sunrise
  if (riseSet != 0) localT = fmod(24+localT,24.0);                      // sunset
  
  double sunHours,sunMinutes,sunSeconds;
  double sunPartial;
  sunPartial = modf(localT,&sunHours) * 60;                             // get hours
  sunPartial = modf(sunPartial,&sunMinutes) * 60;                       // get minutes
  modf(sunPartial,&sunSeconds);                                         // get seconds
  return (((((time_t)sunHours * 60) + (time_t)sunMinutes) * 60) + (time_t)sunSeconds);      // return time_t value of the time
}
