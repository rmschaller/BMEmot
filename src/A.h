// Add any helper or user defined function here
#include <WString.h> // used for String type
#include <ESP8266WebServer.h>
class UserFN
{
    public:
      static void displayText(String lines);
      static void displayTextStatus(String line);
      static void displayAll(String text);
      static String TimeString(time_t currentTime, short ampmFlag);
      static String tzName(byte timeZone);
      static String printTimeZone(String zone);
      static String dstName(byte dstFlag);
      static time_t calculateSunRiseSet(int year,int month,int day,float lat,float lng,int localOffset,int daylightSavings,int riseSet);
      static String ShortDateString(time_t currentTime);
      static String dayStats(short index);
      static String weekStats(short index);
      static String rawStats(short index,short limit);
      static void sendNTPpacket(const char  *address);
      static void kludgeDo(char *friendlyName, char *friendlyNameKludge, int fieldSize);
      static void kludgeUndo(char *friendlyNameKludge, char *friendlyName, int fieldSize);
      static bool setNTPtime();
      static void handleEraseEEPROM();
      static int calculateEEpromCRC(int eepromSize);
      static void handleRoot();
      static void handleHomeAction();
      static void handleMqttEnable();
      static void handleMqttDisable();
      static void handleDisplayOn();
      static void handleDisplayOff();
      static void handleGotTime();
      static void handleReset();
      static void handleConfig();
      static void handleTest();
      static void handleConfigAction();
      static void handleNotFound();
      static void sendStatusPage(String text);
      static String DateString(time_t currentTime);
      static String DateTimeString(time_t currentTime);
      static time_t dstUSA (time_t t);
      static String spacePad(int value, int digits);
      static String zeroPad(int value, int digits);
      static void sendInfoPacket();
      static void updateTimeFromServer();
      static void runSunRiseSet();
      static void midnightProcess(time_t currentTime);
      static void handleWiFi();
      static void handleWiFiAction();
      static unsigned long getFromNTP(const char* server);
      static void handleStatusAction();
};