#include "Globals.h"
extern "C"
{
#include <RGBColor.h>
}
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include "DoubleResetDetector.h" // https://github.com/datacute/DoubleResetDetector
#include <ArtnetWifi.h>
#include "WiFiManagerKT.h"
// #include "config.h"
#include <ArduinoJson.h> //https://github.com/bblanchon/ArduinoJson
#include <FS.h>          //this needs to be first
#include <LittleFS.h>

// Art-Net settings
ArtnetWifi artnet;

//                   R,  G,  B
rgb_pin led_pins = {D7, D6, D5};

bool shouldSaveConfig = false;
DoubleResetDetector drd(DRD_TIMEOUT, DRD_ADDRESS);
iData whipConfigData;
bool wifiDisconnected = true;

void printMac(byte *mac)
{
  Serial.print("MAC: ");
  Serial.print(mac[0], HEX);
  Serial.print(":");
  Serial.print(mac[1], HEX);
  Serial.print(":");
  Serial.print(mac[2], HEX);
  Serial.print(":");
  Serial.print(mac[3], HEX);
  Serial.print(":");
  Serial.print(mac[4], HEX);
  Serial.print(":");
  Serial.println(mac[5], HEX);
}

void printWifiInformation()
{
  byte mac[6];
  WiFiMode_t mode = WiFi.getMode();
  Serial.println();
  Serial.print(F("Mode: "));
  Serial.print(mode);
  Serial.print(", ");

  WiFiPhyMode_t phyMode = WiFi.getPhyMode();
  Serial.print(F("PhyMode: "));
  Serial.print(phyMode);
  Serial.print(", ");

  wl_status_t status = WiFi.status();
  Serial.print(F("Status: "));
  Serial.print(status);
  Serial.print(", ");

  WiFiSleepType_t sleepMode = WiFi.getSleepMode();
  Serial.print(F("SleepMode: "));
  Serial.print(sleepMode);
  Serial.print(", ");

  WiFi.macAddress(mac);
  printMac(mac);
}

void writeColor(rgb_color color)
{
  analogWrite(led_pins.red, color.red);
  analogWrite(led_pins.green, color.green);
  analogWrite(led_pins.blue, color.blue);
}

void flashLED(led_color color, int flashes)
{

  rgb_color rgbcolor = lookup_color(color);
  // rgb_color rgb_black = lookup_color(black);
  rgb_color rgb_black = {0, 0, 0};

  for (int i = 0; i < flashes; i++)
  {
    writeColor(rgbcolor);
    delay(300);
    writeColor(rgb_black);
    delay(200);
  }
}

void lightUpLED(led_color color)
{
  rgb_color rgbcolor = lookup_color(color);
  writeColor(rgbcolor);
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
  CONSOLELN(F("saving config..."));

  // if LittleFS is not usable
  if (!LittleFS.begin())
  {
    CONSOLELN(F("Failed to mount file system"));
    if (!formatLittleFS())
    {

      CONSOLELN(F("Failed to format file system - hardware issues!"));
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
    flashLED(green, 3);
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

  // WiFiManagerParameter custom_name("name", "Lightwhip Name", htmlencode(whipConfigData.name).c_str(), TKIDSIZE);
  WiFiManagerParameter custom_universe("universe", "ArtNet Universe Number", String(whipConfigData.universe).c_str(), 6, TYPE_NUMBER);
  // wifiManager.addParameter(&custom_name);
  wifiManager.addParameter(&custom_universe);

  wifiManager.setConfHostname(htmlencode(whipConfigData.name));
  wifiManager.setConfSSID(htmlencode(whipConfigData.ssid));
  wifiManager.setConfPSK(htmlencode(whipConfigData.psk));

  flashLED(blue, 3);
  lightUpLED(pink);
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
  lightUpLED(black);

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
  //                         R,       G,        B
  rgb_color artnet_color = {data[0], data[1], data[2]};
#if DEBUG
  Serial.print("Received artnet frame: R: ");
  Serial.print(artnet_color.red);
  Serial.print(" G: ");
  Serial.print(artnet_color.green);
  Serial.print(" B: ");
  Serial.println(artnet_color.blue);
#endif
  writeColor(artnet_color);
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
  flashLED(blue, 2);

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

void SetWifiParameters()
{
  WiFi.mode(WIFI_STA);
  WiFi.setSleepMode(WIFI_NONE_SLEEP);
  WiFi.setPhyMode(WIFI_PHY_MODE_11G);
  WiFi.persistent(false);
}

bool connectBackupCredentials()
{
  WiFi.disconnect();
  SetWifiParameters();
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
    flashLED(red, 1);
    if (wait > 50)
      break;
  }
  CONSOLELN();
  auto waited = (millis() - startedAt);
  CONSOLE(F("   -> waited for "));
  CONSOLE(waited);
  CONSOLE(F("ms, result "));
  CONSOLELN(WiFi.status());

  if (WiFi.status() == WL_DISCONNECTED || WiFi.status() == WL_CONNECT_FAILED)
  {
    return false;
  }
  else
  {
    CONSOLE(F("IP: "));
    CONSOLELN(WiFi.localIP());
    return true;
  }
}

void setup()
{
  Serial.begin(115200);

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
  SetWifiParameters();

  WiFi.hostname(whipConfigData.name);
  WiFi.begin(whipConfigData.ssid.c_str(), whipConfigData.psk.c_str());

  printWifiInformation();

  while (wifiDisconnected)
  {

    if (!WiFi.isConnected())
    {
      unsigned long startedAt = millis();
      uint8_t wait = 0;
      while (!WiFi.isConnected())
      {
        delay(300);
        wait++;
        CONSOLE(F("."));
        flashLED(red, 1);
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
      wifiDisconnected = false;
    }
    else
    {
      CONSOLELN(F("Failed to connect -> trying to restore connection..."));
      if (connectBackupCredentials())
      {
        CONSOLE(F("   -> Connection restored!"));
        wifiDisconnected = false;
      }
      else
      {
        CONSOLE(F("   -> Failed to restore connection..."));
        WiFi.disconnect();
        wifiDisconnected = true;
      }
    }
    printWifiInformation();
  }

  CONSOLELN();

  ESP.wdtDisable();
  ESP.wdtEnable(1000);

  pinMode(led_pins.red, OUTPUT);
  pinMode(led_pins.green, OUTPUT);
  pinMode(led_pins.blue, OUTPUT);

  // artnet.begin();
  // this will be called for each packet received
  CONSOLELN(F("Starting ArtNet"));
  flashLED(green, 4);

  artnet.setArtDmxCallback(onDmxFrame);
  artnet.begin();
}

void loop()
{
  ESP.wdtFeed();
  // we call the read function inside the loop
  if (artnet.read() == 0)
  {
    delay(15); // on NOP small delay
  }
}