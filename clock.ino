#include "config.h"

// https://github.com/plapointe6/EspMQTTClient
#include "EspMQTTClient.h"
#include <FastLED.h>

// next lines are for time - from https://github.com/arduino-libraries/NTPClient
#include <NTPClient.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>

// TODO only include if AWTRIX is enabled
// used to parse JSON from AWTRIX example
#include <ArduinoJson.h>

// debug mode enables speedup of 300x (5 minutes are passing per second)
// also additional output is shown in the serial console
#define DEBUG       false

#define LED_PIN     2
#define NUM_LEDS    78
#define BRIGHTNESS  64
#define LED_TYPE    WS2811
#define COLOR_ORDER GRB
#define TIME_OFFSET 3600    // 3600 is UTC+1 - Berlin normal time (non DST)

CRGB leds[NUM_LEDS];
bool ledsActivated[NUM_LEDS];

// was initially 100, which looks good for full screen
#define UPDATES_PER_SECOND 10

const TProgmemPalette16 RedOrangeColorPalette_p PROGMEM =
{
  0xFF0000, 0xD52A00, 0xAB5500, 0xAB7F00,
  0xFF0000, 0xD52A00, 0xAB5500, 0xAB7F00,
  0xFF0000, 0xD52A00, 0xAB5500, 0xAB7F00,
  0xFF0000, 0xD52A00, 0xAB5500, 0xAB7F00
};
const TProgmemPalette16 GreenYellowColorPalette_p PROGMEM =
{
  0xABAB00, 0x56D500, 0x00FF00, 0x00D52A,
  0xABAB00, 0x56D500, 0x00FF00, 0x00D52A,
  0xABAB00, 0x56D500, 0x00FF00, 0x00D52A,
  0xABAB00, 0x56D500, 0x00FF00, 0x00D52A
};
const TProgmemPalette16 BlueColorPalette_p PROGMEM =
{
  0x00AB55, 0x0056AA, 0x0000FF, 0x2A00D5,
  0x00AB55, 0x0056AA, 0x0000FF, 0x2A00D5,
  0x00AB55, 0x0056AA, 0x0000FF, 0x2A00D5,
  0x00AB55, 0x0056AA, 0x0000FF, 0x2A00D5
};
const TProgmemPalette16 PurpleColorPalette_p PROGMEM =
{
  0x5500AB, 0x7F0081, 0xAB0055, 0xD5002B,
  0x5500AB, 0x7F0081, 0xAB0055, 0xD5002B,
  0x5500AB, 0x7F0081, 0xAB0055, 0xD5002B,
  0x5500AB, 0x7F0081, 0xAB0055, 0xD5002B
};

CRGBPalette16 currentPalette;
TBlendType    currentBlending;
bool          showTime = true;

// this automatically connects to Wifi and MQTT
EspMQTTClient client(
  SSID_NAME,
  SSID_PASSWORD,
  MQTT_SERVER,
  MQTT_CLIENT_ID
);

// time related
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, TIME_OFFSET); // 3600 is UTC+1 - Berlin normal time (non DST)
// used to speed up time in debug mode
int debugModeOffset = TIME_OFFSET;

void onConnectionEstablished() {
  Serial.println("MQTT connection established");
  client.subscribe(MQTT_SUBSCRIBE_COLOR_PPM_TOPIC, [] (const String & payload)  {
    Serial.print("CO2 ppm: ");
    int ppm = atoi(payload.c_str());
    Serial.println(ppm);
    if (ppm <= 800) {
      currentPalette = PurpleColorPalette_p;
    } else if (ppm <= 1000) {
      currentPalette = BlueColorPalette_p;
    } else if (ppm <= 1400) {
      currentPalette = GreenYellowColorPalette_p;
    } else {
      currentPalette = RedOrangeColorPalette_p;
    }
  });
  client.subscribe(MQTT_SUBSCRIBE_COLOR_RAINBOW_TOPIC, [] (const String & payload)  {
    currentPalette = RainbowColors_p;
  });
  client.subscribe(MQTT_SUBSCRIBE_VIEW_FULL_TOPIC, [] (const String & payload)  {
    showTime = false;
  });
  client.subscribe(MQTT_SUBSCRIBE_VIEW_TIME_TOPIC, [] (const String & payload)  {
    showTime = true;
  });

  // TODO set brightness automatically - photo sensor?
  client.subscribe(MQTT_SUBSCRIBE_BRIGHTNESS_TOPIC, [] (const String & payload)  {
    int brightness = atoi(payload.c_str());
    Serial.print("brightness: ");
    Serial.println(brightness);
    if (brightness >= 0 && brightness < 256) {
      FastLED.setBrightness(brightness);
    }
  });

  // TODO set offset automatically - DST ;)
  client.subscribe(MQTT_SUBSCRIBE_OFFSET_TOPIC, [] (const String & payload)  {
    int offset = atoi(payload.c_str());
    Serial.print("offset: ");
    Serial.println(offset);
    timeClient.setTimeOffset(offset);
    CalculateEnabledLEDsForTime();
  });

  // TODO only include if AWTRIX is enabled
  client.subscribe(MQTT_AWTRIX_PREFIX "/response", [] (const String & payload) {
     StaticJsonDocument<250> doc;
     DeserializationError error = deserializeJson(doc, payload.c_str());
     if (error) {
      Serial.print(F("deserializeJson() failed: "));
      Serial.println(error.c_str());
      return;
    }

    int lux = (int)doc["LUX"];
    Serial.print("lux: ");
    Serial.println(lux);

    if (lux >= BRIGHTNESS_MAX_LUX) {
      Serial.println("set maximum brightness");
      FastLED.setBrightness(BRIGHTNESS_MAX_VALUE);
    } else if (lux <= BRIGHTNESS_MIN_LUX) {
      Serial.println("set minimum brightness");
      FastLED.setBrightness(BRIGHTNESS_MIN_VALUE);
    } else {
      Serial.println("calculate brightness");
      int brightnessSteps = BRIGHTNESS_MAX_VALUE - BRIGHTNESS_MIN_VALUE;
      Serial.print("brightness steps: ");
      Serial.println(brightnessSteps);
      float luxPerBrightnessStep = (BRIGHTNESS_MAX_LUX - BRIGHTNESS_MIN_LUX + .0) / brightnessSteps;
      Serial.print("lux per brightness step: ");
      Serial.println(luxPerBrightnessStep);

      // level lux to minimum value
      lux = lux - BRIGHTNESS_MIN_LUX;
      Serial.print("leveled lux: ");
      Serial.println(lux);
      float steps = lux / luxPerBrightnessStep;
      Serial.print("steps: ");
      Serial.println(steps);

      Serial.print("calculated brightness: ");
      Serial.println(BRIGHTNESS_MIN_VALUE + steps);
    }
  });
}


void setup() {
  Serial.begin(115200);

  // configure time
  timeClient.begin();

  delay(3000); // power-up safety delay


  FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection( TypicalLEDStrip );
  FastLED.setBrightness(BRIGHTNESS);

  // FastLED provides several 'preset' palettes: RainbowColors_p, RainbowStripeColors_p,
  // OceanColors_p, CloudColors_p, LavaColors_p, ForestColors_p, and PartyColors_p.
  currentPalette = RainbowColors_p;
  currentBlending = LINEARBLEND;

  timeClient.update();

  // initial set of the time
  CalculateEnabledLEDsForTime();
}


void loop()
{
  client.loop();
  timeClient.update();

  TimeUpdateLoop();
  static uint8_t startIndex = 0;
  startIndex = startIndex + 1; /* motion speed */

  FillLEDsFromPaletteColors( startIndex);

  FastLED.show();
  FastLED.delay(1000 / UPDATES_PER_SECOND);
}

void FillLEDsFromPaletteColors( uint8_t colorIndex)
{
  uint8_t brightness = 255;

  for (int i = 0; i < NUM_LEDS; i++) {
    if (!showTime || ledsActivated[i]) {
      leds[i] = ColorFromPalette(currentPalette, colorIndex, brightness, currentBlending);
      colorIndex += 3;
    } else {
      leds[i] = CRGB::Black;
    }
  }
}

void TimeUpdateLoop()
{
  uint8_t secondHand = (millis() / 1000) % 60;
  static uint8_t lastSecond = 99;

  if (lastSecond != secondHand) {
    lastSecond = secondHand;
    Serial.println(timeClient.getFormattedTime());
    if (DEBUG) {
      Serial.print("Connected: ");
      Serial.println(client.isConnected());
      Serial.print("WiFi connected: ");
      Serial.println(client.isWifiConnected());
      Serial.print("MQTT connected: ");
      Serial.println(client.isMqttConnected());
    }

    // as the clock can only show the time in 5 minute steps the check will
    // only be done if it is right in the middle between two slots - at 2
    // minutes and 31 seconds into the slot
    int seconds = timeClient.getSeconds();
    if (seconds == 31) {
      int minutes = timeClient.getMinutes();
      if (minutes % 5 == 2) {
        CalculateEnabledLEDsForTime();
      }

      // only do this once a minute to trigger the update of the brightness
      // TODO only include if AWTRIX is enabled
      client.publish(MQTT_AWTRIX_PREFIX "/basics", "{\"get\":\"matrixInfo\"}");
    }

    /*
       test code

       runs at higher speed: 5 minutes per second via offset to test the time code
    */
    if (DEBUG) {
      CalculateEnabledLEDsForTime();
      debugModeOffset += 300;
      timeClient.setTimeOffset(debugModeOffset);
    }
  }
}

void CalculateEnabledLEDsForTime() {
  int hour = timeClient.getHours() % 12;
  if (hour == 0) {
    hour = 12;
  }
  int nextHour = (hour + 1) % 12;
  if (nextHour == 0) {
    nextHour = 12;
  }
  int minutes = timeClient.getMinutes();
  int seconds = timeClient.getSeconds();
  int secondsIntoTheHour = minutes * 60 + seconds;

  ClearMap();

  if (secondsIntoTheHour < 2 * 60 + 30) {
    Serial.print("It is ");
    Serial.print(hour);
    Serial.println(" oclock");

    Enable_IT_IS();
    TranslateHourIntoLED(hour);
    Enable_OCLOCK();
    return;
  }
  if (secondsIntoTheHour < 7 * 60 + 30) {
    Serial.print("It is five minutes past ");
    Serial.println(hour);

    Enable_IT_IS();
    Enable_FIVE_M();
    Enable_MINUTES();
    Enable_PAST();
    TranslateHourIntoLED(hour);
    return;
  }
  if (secondsIntoTheHour < 12 * 60 + 30) {
    Serial.print("It is ten minutes past ");
    Serial.println(hour);

    Enable_IT_IS();
    Enable_TEN_M();
    Enable_MINUTES();
    Enable_PAST();
    TranslateHourIntoLED(hour);
    return;
  }
  if (secondsIntoTheHour < 17 * 60 + 30) {
    Serial.print("It is quarter past ");
    Serial.println(hour);

    Enable_IT_IS();
    Enable_QUARTER();
    Enable_PAST();
    TranslateHourIntoLED(hour);
    return;
  }
  if (secondsIntoTheHour < 22 * 60 + 30) {
    Serial.print("It is twenty minutes past ");
    Serial.println(hour);

    Enable_IT_IS();
    Enable_TWENTY_M();
    Enable_MINUTES();
    Enable_PAST();
    TranslateHourIntoLED(hour);
    return;
  }
  if (secondsIntoTheHour < 27 * 60 + 30) {
    Serial.print("It is twenty five minutes past ");
    Serial.println(hour);

    Enable_IT_IS();
    Enable_TWENTY_M();
    Enable_FIVE_M();
    Enable_MINUTES();
    Enable_PAST();
    TranslateHourIntoLED(hour);
    return;
  }
  if (secondsIntoTheHour < 32 * 60 + 30) {
    Serial.print("It is half past ");
    Serial.println(hour);

    Enable_IT_IS();
    Enable_HALF();
    Enable_PAST();
    TranslateHourIntoLED(hour);
    return;
  }
  if (secondsIntoTheHour < 37 * 60 + 30) {
    Serial.print("It is twenty five minutes to ");
    Serial.println(nextHour);

    Enable_IT_IS();
    Enable_TWENTY_M();
    Enable_FIVE_M();
    Enable_MINUTES();
    Enable_TO();
    TranslateHourIntoLED(nextHour);
    return;
  }
  if (secondsIntoTheHour < 42 * 60 + 30) {
    Serial.print("It is twenty minutes to ");
    Serial.println(nextHour);

    Enable_IT_IS();
    Enable_TWENTY_M();
    Enable_MINUTES();
    Enable_TO();
    TranslateHourIntoLED(nextHour);
    return;
  }
  if (secondsIntoTheHour < 47 * 60 + 30) {
    Serial.print("It is quarter to ");
    Serial.println(nextHour);

    Enable_IT_IS();
    Enable_QUARTER();
    Enable_TO();
    TranslateHourIntoLED(nextHour);
    return;
  }
  if (secondsIntoTheHour < 52 * 60 + 30) {
    Serial.print("It is ten minutes to ");
    Serial.println(nextHour);

    Enable_IT_IS();
    Enable_TEN_M();
    Enable_MINUTES();
    Enable_TO();
    TranslateHourIntoLED(nextHour);
    return;
  }
  if (secondsIntoTheHour < 57 * 60 + 30) {
    Serial.print("It is five minutes to ");
    Serial.println(nextHour);

    Enable_IT_IS();
    Enable_FIVE_M();
    Enable_MINUTES();
    Enable_TO();
    TranslateHourIntoLED(nextHour);
    return;
  }
  if (secondsIntoTheHour >= 57 * 60 + 30) {
    Serial.print("It is ");
    Serial.print(nextHour);
    Serial.println(" oclock");

    Enable_IT_IS();
    TranslateHourIntoLED(nextHour);
    Enable_OCLOCK();
    return;
  }
}

/*
   Below here is basically mappers of a word to a given amount of
   LED bits and to have proper names
*/

void ClearMap() {
  for (int i = 0; i < NUM_LEDS; i++) {
    ledsActivated[i] = false;
  }
}

void TranslateHourIntoLED(int hour) {
  switch (hour) {
    case 1:
      Enable_ONE();
      break;
    case 2:
      Enable_TWO();
      break;
    case 3:
      Enable_THREE();
      break;
    case 4:
      Enable_FOUR();
      break;
    case 5:
      Enable_FIVE();
      break;
    case 6:
      Enable_SIX();
      break;
    case 7:
      Enable_SEVEN();
      break;
    case 8:
      Enable_EIGHT();
      break;
    case 9:
      Enable_NINE();
      break;
    case 10:
      Enable_TEN();
      break;
    case 11:
      Enable_ELEVEN();
      break;
    case 12:
      Enable_TWELVE();
      break;
  }
}

void Enable_IT_IS() {
  ledsActivated[68] = true;
  ledsActivated[69] = true;
  ledsActivated[70] = true;
  ledsActivated[71] = true;
}
void Enable_TEN_M() {
  ledsActivated[72] = true;
  ledsActivated[73] = true;
  ledsActivated[74] = true;
}
void Enable_HALF() {
  ledsActivated[75] = true;
  ledsActivated[76] = true;
  ledsActivated[77] = true;
}
void Enable_QUARTER() {
  ledsActivated[63] = true;
  ledsActivated[64] = true;
  ledsActivated[65] = true;
  ledsActivated[66] = true;
  ledsActivated[67] = true;
}
void Enable_TWENTY_M() {
  ledsActivated[58] = true;
  ledsActivated[59] = true;
  ledsActivated[60] = true;
  ledsActivated[61] = true;
  ledsActivated[62] = true;
}
void Enable_FIVE_M() {
  ledsActivated[48] = true;
  ledsActivated[49] = true;
  ledsActivated[50] = true;
}
void Enable_MINUTES() {
  ledsActivated[52] = true;
  ledsActivated[53] = true;
  ledsActivated[54] = true;
  ledsActivated[55] = true;
  ledsActivated[56] = true;
}
void Enable_PAST() {
  ledsActivated[45] = true;
  ledsActivated[46] = true;
  ledsActivated[47] = true;
}
void Enable_TO() {
  ledsActivated[44] = true;
}
void Enable_ONE() {
  ledsActivated[41] = true;
  ledsActivated[42] = true;
}
void Enable_TWO() {
  ledsActivated[38] = true;
  ledsActivated[39] = true;
  ledsActivated[40] = true;
}
void Enable_THREE() {
  ledsActivated[28] = true;
  ledsActivated[29] = true;
  ledsActivated[30] = true;
  ledsActivated[31] = true;
}
void Enable_FOUR() {
  ledsActivated[32] = true;
  ledsActivated[33] = true;
  ledsActivated[34] = true;
}
void Enable_FIVE() {
  ledsActivated[35] = true;
  ledsActivated[36] = true;
  ledsActivated[37] = true;
}
void Enable_SIX() {
  ledsActivated[26] = true;
  ledsActivated[27] = true;
}
void Enable_SEVEN() {
  ledsActivated[22] = true;
  ledsActivated[23] = true;
  ledsActivated[24] = true;
  ledsActivated[25] = true;
}
void Enable_EIGHT() {
  ledsActivated[19] = true;
  ledsActivated[20] = true;
  ledsActivated[21] = true;
}
void Enable_NINE() {
  ledsActivated[10] = true;
  ledsActivated[11] = true;
}
void Enable_TEN() {
  ledsActivated[12] = true;
  ledsActivated[13] = true;
  ledsActivated[14] = true;
}
void Enable_ELEVEN() {
  ledsActivated[15] = true;
  ledsActivated[16] = true;
  ledsActivated[17] = true;
  ledsActivated[18] = true;
}
void Enable_TWELVE() {
  ledsActivated[6] = true;
  ledsActivated[7] = true;
  ledsActivated[8] = true;
  ledsActivated[9] = true;
}
void Enable_OCLOCK() {
  ledsActivated[0] = true;
  ledsActivated[1] = true;
  ledsActivated[2] = true;
  ledsActivated[3] = true;
  ledsActivated[4] = true;
}
void Enable_C() {
  ledsActivated[51] = true;
}
void Enable_H() {
  ledsActivated[57] = true;
}
void Enable_E() {
  ledsActivated[43] = true;
}
void Enable_L() {
  ledsActivated[5] = true;
}
