# Word clock

## Features

* show the time in rainbow color schema
* show a color schema based on a CO2 PPM value that needs to be handed in via MQTT topic (see API for reference)
* light up all LEDs to have a beautiful panel of letters instead the time
* adjust the brightness via API and via an existing [AWTRIX that has a LUX sensor](https://awtrixdocs.blueforcer.de/#/de-de/hardware?id=lichtsensor-zur-helligkeitsregelung-optional) and MQTT API enabled (it then asks the AWTRIX API once a minute for the current LUX value and adjusts it's brightness accordingly)
* connect to Wifi to fetch the current time and to connect to an MQTT to provide an API

## Requirements

* install following dependencies via the Arduino IDE:
  * EspMQTTClient (https://github.com/plapointe6/EspMQTTClient)
  * NTPClient (https://github.com/arduino-libraries/NTPClient)
  * ArduinoJSON (https://arduinojson.org/)
  * PubSubClient (https://pubsubclient.knolleary.net)
* copy `config.h.dist` to `config.h` and adjust the config options
* adjust the `MQTT_MAX_PACKET_SIZE` option in the `PubSubClient` library to 250 to allow proper AWTRIX support

## Known limitations

* DST is not yet respected (plan is to announce this via MQTT)
* max MQTT packet size needs to be raised for the feature to fetch the lux value from an AWTRIX:

> From PubSubClient: "The maximum message size, including header, is 128 bytes by default. This is configurable via MQTT_MAX_PACKET_SIZE in PubSubClient.h"

* the AWTRIX part is not yet optional. One can remove the code segments that start with `// TODO only include if AWTRIX is enabled`

## API

There is an MQTT based API available. The topics can be specified in the `config.h`.

### Color schema

* `MQTT_SUBSCRIBE_COLOR_PPM_TOPIC`: send an integer to this topic and the clock will adjust it's color schema based on the value. Thresholds are 800, 1000 and 1400. It will use a purple, a blue, a green-yellow and an orange-red color schema for the ranges between those thresholds. If something is send there it will stay in this color schema until the next reboot or `MQTT_SUBSCRIBE_COLOR_RAINBOW_TOPIC` is triggered.
* `MQTT_SUBSCRIBE_COLOR_RAINBOW_TOPIC`: send anything there and the clock will move back to the classic rainbow color schema.

### Clock mode vs full mode

* `MQTT_SUBSCRIBE_VIEW_FULL_TOPIC`: send anything there and the clock will enable all LEDs making it a colorfull panel instead of a clock
* `MQTT_SUBSCRIBE_VIEW_TIME_TOPIC`: send anything there and the clock will show the time again

### Brightness

* `MQTT_SUBSCRIBE_BRIGHTNESS_TOPIC`: send a value between 0 and 255 there and the brightness will be adjusted to that value

### Time offset

* `MQTT_SUBSCRIBE_OFFSET_TOPIC`: send the offset in seconds to this topic and the clock will change it's display time to UTC + offset. This can be used to work around the non-existing DST option.

## Related projects

* [Sleek Word Clock](http://www.instructables.com/id/Sleek-word-clock/)
* [The Word Clock - Arduino Version](http://www.instructables.com/id/The-Word-Clock-Arduino-version/?ALLSTEPS)
* [AWTRIX 2.0](http://www.instructables.com/id/The-Word-Clock-Arduino-version/?ALLSTEPS)