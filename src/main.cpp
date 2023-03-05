/*
  This example will receive multiple universes via Art-Net and control a strip of
  WS2812 LEDs via the FastLED library: https://github.com/FastLED/FastLED
  This example may be copied under the terms of the MIT license, see the LICENSE file for details
*/
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <ArtnetWifi.h>

// include some libraries
#include <NeoPixelBus.h>
//#include <NeoPixelAnimator.h>



// Wifi settings 
const char* ssid = "";
const char* password = ""; 

// LED settings
const int numLeds = 1; // CHANGE FOR YOUR SETUP
const int numberOfChannels = numLeds * 3; // Total number of channels you want to receive (1 led = 3 channels)
const byte dataPin = 12;
//NeoPixelBus<NeoGrbFeature, NeoEsp8266DmaWs2812xMethod> *strip;
//NeoPixelBus<NeoGrbFeature, NeoEsp8266Dma800KbpsMethod> *strip;
NeoPixelBus<NeoGrbFeature, Neo800KbpsMethod> *strip;
// Art-Net settings
ArtnetWifi artnet;
const int startUniverse = 0; // CHANGE FOR YOUR SETUP most software this is 1, some software send out artnet first universe as 0.

// Check if we got all universes
const int maxUniverses = numberOfChannels / 512 + ((numberOfChannels % 512) ? 1 : 0);
bool universesReceived[maxUniverses];
bool sendFrame = 1;
int previousDataLength = 0;


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
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    if (i > 20){
      state = false;
      break;
    }
    i++;
  }
  if (state) {
    Serial.println("");
    Serial.print("Connected to ");
    Serial.println(ssid);
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("");
    Serial.println("Connection failed.");
  }
  
  return state;
}

void onDmxFrame(uint16_t universe, uint16_t length, uint8_t sequence, uint8_t* data)
{
  sendFrame = 1;
  // set brightness of the whole strip
  if (universe == 15)
  {
    //FastLED.setBrightness(data[0]);
    strip->Show();
  }

  // Store which universe has got in
  if ((universe - startUniverse) < maxUniverses) {
    universesReceived[universe - startUniverse] = 1;
  }

  for (int i = 0 ; i < maxUniverses ; i++)
  {
    if (universesReceived[i] == 0)
    {
      //Serial.println("Broke");
      sendFrame = 0;
      break;
    }
  }

  // read universe and put into the right part of the display buffer
  for (int i = 0; i < length / 3; i++)
  {
    int led = i + (universe - startUniverse) * (previousDataLength / 3);
    if (led < numLeds)
      strip->SetPixelColor(led, RgbColor(data[i * 3], data[i * 3 + 1], data[i * 3 + 2]));
  }
  previousDataLength = length;

  if (sendFrame)
  {
    strip->Show();
    // Reset universeReceived to 0
    memset(universesReceived, 0, maxUniverses);
  }
}


void setup()
{
  Serial.begin(115200);
  ESP.wdtDisable();
  ESP.wdtEnable(1000);
  ConnectWifi();
  artnet.begin();
 // strip = new NeoPixelBus<NeoGrbFeature, NeoEsp8266DmaWs2812xMethod>(numLeds, dataPin);
 // strip = new NeoPixelBus<NeoGrbFeature, NeoEsp8266Dma800KbpsMethod>(numLeds, dataPin);
    strip = new NeoPixelBus<NeoGrbFeature, Neo800KbpsMethod>(numLeds, dataPin);
  strip->Begin();

  // this will be called for each packet received
  artnet.setArtDmxCallback(onDmxFrame);
  artnet.begin();
}

void loop()
{
  ESP.wdtFeed();
  // we call the read function inside the loop
  artnet.read();
}
