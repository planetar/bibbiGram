# bibbiGram

The basic idea is to connect an ESP8266 mcu with a sensor and enable it to not only measure the current temperature and humidity but to compare these to past measurements so that it can discover sudden drastic changes. Think of a bathroom with a forgotten open window in winter. Or a kettle with water boiling along in the kitchen.
In a smartHome setup it would suffice that the sensor box reports the measurements to some backend event bus and a smartHome controller (i.e. an openHab installation) has the rules to react on this event and notify the inhabitants. BibbiGram however aims to do this standalone by sending Telegram messages.
It needs 5V from a wall plug, access to a Wlan and a Telegram-Bot token.

## Hardware:
- Nodemcu v2 'Amica' ESP8266 the narrow type that fits nicely on a breadboard. Other types of ESP8266 will work as well but the case would need adaption.
- BME280 sensor which reads temperature, humidity and air pressure.
- 0,96" OLED display with a 4Pin I2C interface.

## Case:
https://www.thingiverse.com/thing:4068963 has been designed for it. Or simply create your own.

## Telegram-Bot:
To enable telegram communication you have to set up a bot. You don’t need to write any code for this. In fact, you don’t even need your computer! Go to the telegram app on your phone and search for the “botfather” telegram bot (he’s the one that’ll assist you with creating and managing your bot). Send "/help" to the bot for a menu of available commands. "/newbot" get's you going, you have to set a name and a username for it and if you succeed you get a token in a reply. Copy and save that.
While we're at it let's create 2 telegram groups for the bot to post updates: 
- botName_alarm is the destination to post to when an alarm occurs,
- botName_debug receives all sorts of debug messages
Add the new bot to both of these groups and it will listen on commands posted therein.
Later, when everything is up and running, all the family can join the botname_alarm - group and get notifications...

## Software:
In order to join a Wlan which we may know nothing about the thing should go in Access Point modus at first start and offer a setup page for clients connecting to the initial AP. 


## libraries:

CharStream.h    https://bitbucket.org/gundolf_/thermometergraph/src/master/
WiFiManager.h   https://github.com/tzapu/WiFiManager
WiFiManager.cpp https://github.com/tzapu/WiFiManager
UniversalTelegramBot.h  https://github.com/witnessmenow/Universal-Arduino-Telegram-Bot
UniversalTelegramBot.cpp    https://github.com/witnessmenow/Universal-Arduino-Telegram-Bot
Average.h   https://github.com/MajenkoLibraries/Average
ArduinoJson.h 	    https://github.com/bblanchon/ArduinoJson/tree/5.x
