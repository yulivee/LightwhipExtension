#include "Globals.h"
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include "DoubleResetDetector.h" // https://github.com/datacute/DoubleResetDetector
#include <ArtnetWifi.h>
#include "WiFiManagerKT.h"
#include "config.h"
#include <ArduinoJson.h> //https://github.com/bblanchon/ArduinoJson
#include <FS.h>          //this needs to be first
#include <LittleFS.h>

// Art-Net settings
ArtnetWifi artnet;

int pwmPinRed = D7;
int pwmPinGreen = D6;
int pwmPinBlue = D5;

bool shouldSaveConfig = false;
DoubleResetDetector drd(DRD_TIMEOUT, DRD_ADDRESS);
iData whipConfigData;

// connect to wifi – returns true if successful or false if not
boolean ConnectWifi(void)
{
  boolean state = true;
  int i = 0;

  WiFi.begin(ssid, password);
  Serial.println("");
  Serial.println("Connecting to WiFi");

  // Wait for connection
  Serial.print("Connecting");
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
    if (i > 20)
    {
      state = false;
      break;
    }
    i++;
  }
  if (state)
  {
    Serial.println("");
    Serial.print("Connected to ");
    Serial.println(ssid);
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
  }
  else
  {
    Serial.println("");
    Serial.println("Connection failed.");
  }

  return state;
}

bool formatLittleFS()
{
  CONSOLE(F("\nneed to format LittleFS: "));
  LittleFS.end();
  LittleFS.begin();
  CONSOLELN(LittleFS.format());
  return LittleFS.begin();
}

bool saveConfig()
{
  CONSOLE(F("saving config...\n"));

  // if LittleFS is not usable
  if (!LittleFS.begin())
  {
    Serial.println("Failed to mount file system");
    if (!formatLittleFS())
    {
      Serial.println("Failed to format file system - hardware issues!");
      return false;
    }
  }

  DynamicJsonDocument doc(2048);

  doc["Name"] = whipConfigData.name;
  doc["universe"] = whipConfigData.universe;
  // first reboot is for test
  doc["SSID"] = WiFi.SSID();
  doc["PSK"] = WiFi.psk();

  File configFile = LittleFS.open(CFGFILE, "w");
  if (!configFile)
  {
    CONSOLELN(F("failed to open config file for writing"));
    LittleFS.end();
    return false;
  }
  else
  {
    serializeJson(doc, configFile);
#ifdef DEBUG
    serializeJson(doc, Serial);
#endif
    configFile.flush();
    configFile.close();
    LittleFS.gc();
    LittleFS.end();
    CONSOLELN(F("\nsaved successfully"));
    return true;
  }
}
// callback notifying us of the need to save config
void saveConfigCallback()
{
  shouldSaveConfig = true;
}

String htmlencode(String str)
{
  String encodedstr = "";
  char c;

  for (uint16_t i = 0; i < str.length(); i++)
  {
    c = str.charAt(i);

    if (isalnum(c))
    {
      encodedstr += c;
    }
    else
    {
      encodedstr += "&#";
      encodedstr += String((uint8_t)c);
      encodedstr += ';';
    }
  }
  return encodedstr;
}

void validateInput(const char *input, char *output)
{
  String tmp = input;
  tmp.trim();
  tmp.replace(' ', '_');
  tmp.toCharArray(output, tmp.length() + 1);
}

bool readConfig()
{
  CONSOLE(F("mounting FS..."));

  if (!LittleFS.begin())
  {
    CONSOLELN(F(" ERROR: failed to mount FS!"));
    return false;
  }
  else
  {
    CONSOLELN(F(" mounted!"));
    if (!LittleFS.exists(CFGFILE))
    {
      CONSOLELN(F("ERROR: failed to load json config"));
      return false;
    }
    else
    {
      // file exists, reading and loading
      CONSOLELN(F("reading config file"));
      File configFile = LittleFS.open(CFGFILE, "r");
      if (!configFile)
      {
        CONSOLELN(F("ERROR: unable to open config file"));
      }
      else
      {
        size_t size = configFile.size();
        DynamicJsonDocument doc(size * 3);
        DeserializationError error = deserializeJson(doc, configFile);
        if (error)
        {
          CONSOLE(F("deserializeJson() failed: "));
          CONSOLELN(error.c_str());
        }
        else
        {
          if (doc.containsKey("Name"))
            strcpy(whipConfigData.name, doc["Name"]);
          if (doc.containsKey("SSID"))
            whipConfigData.ssid = (const char *)doc["SSID"];
          if (doc.containsKey("PSK"))
            whipConfigData.psk = (const char *)doc["PSK"];
          if (doc.containsKey("universe"))
            whipConfigData.universe = doc["universe"];
          CONSOLELN(F("parsed config:"));
#ifdef DEBUG
          serializeJson(doc, Serial);
          CONSOLELN();
#endif
        }
      }
    }
  }
  return true;
}

bool startConfiguration()
{

  WiFiManager wifiManager;

  wifiManager.setConfigPortalTimeout(PORTALTIMEOUT);
  wifiManager.setSaveConfigCallback(saveConfigCallback);
  wifiManager.setBreakAfterConfig(true);

  //WiFiManagerParameter custom_name("name", "Lightwhip Name", htmlencode(whipConfigData.name).c_str(), TKIDSIZE);
  WiFiManagerParameter custom_universe("universe", "ArtNet Universe Number", String(whipConfigData.universe).c_str(), 6, TYPE_NUMBER);
  //wifiManager.addParameter(&custom_name);
  wifiManager.addParameter(&custom_universe);

  wifiManager.setConfHostname(htmlencode(whipConfigData.name));
  wifiManager.setConfSSID(htmlencode(whipConfigData.ssid));
  wifiManager.setConfPSK(htmlencode(whipConfigData.psk));

  CONSOLELN(F("started Portal"));
  static char ssid[33] = {0}; // 32 char max for SSIDs
  if (strlen(whipConfigData.name) == 0)
  {
    snprintf(ssid, sizeof ssid, "Lightwhip_%06X", ESP.getChipId());
    snprintf(whipConfigData.name, sizeof ssid, "Lightwhip_%06X", ESP.getChipId());
  }
  else
  {
    snprintf(ssid, sizeof ssid, "Lightwhip_%s", whipConfigData.name);
    WiFi.hostname(whipConfigData.name); // Set DNS hostname
  }

  wifiManager.startConfigPortal(ssid);
  whipConfigData.universe = String(custom_universe.getValue()).toInt();

  // save the custom parameters to FS
  if (shouldSaveConfig)
  {
    // Wifi config
    WiFi.setAutoConnect(true);
    WiFi.setAutoReconnect(true);

    return saveConfig();
  }
  return false;
}

void onDmxFrame(uint16_t universe, uint16_t length, uint8_t sequence, uint8_t *data)
{
  // read universe and put into the right part of the display buffer
  uint8 red = data[0];
  uint8 green = data[1];
  uint8 blue = data[2];

  Serial.print("Received artnet frame: R: ");
  Serial.print(red);
  Serial.print(" G: ");
  Serial.print(green);
  Serial.print(" B: ");
  Serial.println(blue);

  analogWrite(pwmPinRed, red);
  analogWrite(pwmPinGreen, green);
  analogWrite(pwmPinBlue, blue);
}

bool shouldStartConfig(bool validConf)
{

  // we make sure that configuration is properly set and we are not woken by
  // RESET button
  // ensure this was called

  rst_info *_reset_info = ESP.getResetInfoPtr();
  uint8_t _reset_reason = _reset_info->reason;

  // The ESP reset info is sill buggy. see http://www.esp8266.com/viewtopic.php?f=32&t=8411
  // The reset reason is "5" (woken from deep-sleep) in most cases (also after a power-cycle)
  // I added a single reset detection as workaround to enter the config-mode easier
  CONSOLE(F("Boot-Mode: "));
  CONSOLELN(ESP.getResetReason());
  bool _poweredOnOffOn = _reset_reason == REASON_DEFAULT_RST || _reset_reason == REASON_EXT_SYS_RST;
  if (_poweredOnOffOn)
    CONSOLELN(F("power-cycle or reset detected, config mode"));

  bool _dblreset = drd.detectDoubleReset();
  if (_dblreset)
    CONSOLELN(F("\nDouble Reset detected"));

  bool _wifiCred = !(whipConfigData.ssid.isEmpty() || whipConfigData.psk.isEmpty());

  if (!_wifiCred)
    CONSOLELN(F("\nERROR no Wifi credentials"));

  if (validConf && !_dblreset && _wifiCred)
  {
    CONSOLELN(F("\nno reset detected, normal mode"));
    return false;
  }
  // config mode
  else
  {
    CONSOLELN(F("\ngoing to Config Mode"));
    return true;
  }
}

bool connectBackupCredentials()
{
  WiFi.disconnect();
  WiFi.mode(WIFI_STA); // suggestion that MQTT connection failures can happen if WIFI mode isn't STA.
  if (strlen(whipConfigData.name) != 0)
    WiFi.hostname(whipConfigData.name); // Set DNS hostname

  WiFi.begin(whipConfigData.ssid.c_str(), whipConfigData.psk.c_str());
  unsigned long startedAt = millis();
  uint8_t wait = 0;
  while (!WiFi.isConnected())
  {
    delay(300);
    wait++;
    CONSOLE(F("."));
    if (wait > 50)
      break;
  }
  CONSOLELN();
  auto waited = (millis() - startedAt);
  CONSOLE(F("   -> waited for "));
  CONSOLE(waited);
  CONSOLE(F("ms, result "));
  CONSOLELN(WiFi.status());

  if (WiFi.status() == WL_DISCONNECTED)
    return false;
  else
    return true;
}

void setup()
{
  Serial.begin(115200);

  delay(5000);

  bool validConf = readConfig();
  if (!validConf)
    CONSOLELN(F("\nERROR config corrupted"));

  if (shouldStartConfig(validConf))
  {
    // rescue if wifi credentials lost because of power loss
    if (!startConfiguration())
    {
      // test if ssid exists
      if (WiFi.SSID() == "" && whipConfigData.ssid != "" && whipConfigData.psk != "")
      {
        connectBackupCredentials();
      }
    }
  }

  // to make sure we wake up with STA but AP
  WiFi.mode(WIFI_STA);
  WiFi.persistent(false);
  WiFi.hostname(whipConfigData.name); 
  WiFi.begin(whipConfigData.ssid.c_str(), whipConfigData.psk.c_str());

  if (!WiFi.isConnected())
  {
    unsigned long startedAt = millis();
    uint8_t wait = 0;
    while (!WiFi.isConnected())
    {
      delay(300);
      wait++;
      CONSOLE(F("."));
      if (wait > 50)
        break;
    }
    CONSOLELN();
    auto waited = (millis() - startedAt);
    CONSOLE(F("After waiting "));
    CONSOLE(waited);
    CONSOLE(F("ms, result "));
    CONSOLELN(WiFi.status());
  }
  if (WiFi.status() == WL_CONNECTED)
  {
    CONSOLE(F("IP: "));
    CONSOLELN(WiFi.localIP());
  }
  else
  {
    CONSOLELN(F("Failed to connect -> trying to restore connection..."));
    if (connectBackupCredentials())
      CONSOLE(F("   -> Connection restored!"));
    else
    {
      CONSOLE(F("   -> Failed to restore connection..."));
      WiFi.disconnect();
    }
  }

  CONSOLELN();

  ESP.wdtDisable();
  ESP.wdtEnable(1000);
  // ConnectWifi();

  pinMode(pwmPinRed, OUTPUT);
  pinMode(pwmPinGreen, OUTPUT);
  pinMode(pwmPinBlue, OUTPUT);

  // artnet.begin();
  // this will be called for each packet received
  CONSOLELN(F("Starting ArtNet"));
  artnet.setArtDmxCallback(onDmxFrame);
  artnet.begin();
}

void loop()
{
  ESP.wdtFeed();
  // we call the read function inside the loop
  artnet.read();
}
