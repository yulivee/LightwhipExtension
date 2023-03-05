#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <ArtnetWifi.h>
#include "config.h"

// Art-Net settings
ArtnetWifi artnet;

int pwmPinRed = D7;
int pwmPinGreen = D6;
int pwmPinBlue = D5;

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

void setup()
{
  Serial.begin(115200);
  ESP.wdtDisable();
  ESP.wdtEnable(1000);
  ConnectWifi();

  pinMode(pwmPinRed, OUTPUT);
  pinMode(pwmPinGreen, OUTPUT);
  pinMode(pwmPinBlue, OUTPUT);

  artnet.begin();
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
