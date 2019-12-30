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

## Libraries:
In order to join a Wlan which we may know nothing about the thing should go in Access Point modus at first start and offer a setup page for clients connecting to the initial AP. There a several libraries which offer this, WiFiManager by tzapu a well known among them. The next building block is the telegram communications and I chose the Universal-Arduino-Telegram-Bot for this. This library has an example UsingWiFiManager which pretty much formed the basis of this sketch.
The forementioned library depend on the ArduinoJson library in version 5.x which may conflict with dependencies of other libraries (it did for me). The solution is to provide the libraries in the same folder and include them in quotes. To make this work I edited UniversalTelegramBot.h, line 26 to read #include "ArduinoJson.h" (instead of #include <ArduinoJson.h>)

Average.h supplies an easy way to build and get long- and shorttime averages of the measurements and CharStream.h (together with Streaming lirary) allows much nicer formatting of complex output lines to Serial.

Besides, U8g2lib cares for the display and Adafruit_BME280 does the sensor. 
ESP8266WiFi, WiFiClientSecure, EEPROM, DNSServer,ESP8266WebServer are requirements of WiFiManager or UniversalTelegramBot.

And then there are xbm_images.h and chatid.h which I introduced to separate private content (pictures, name of telegram groups) from published code. Those 2 libs currently compromise the concept of an agnostic bot that gets all the config at startup. This is work to be done.

### 3rd party libraries and their repositories:
- Adafruit_BME280 https://github.com/adafruit/Adafruit_BME280_Library
- Adafruit_Sensor https://github.com/adafruit/Adafruit_Sensor
- Average.h       https://github.com/MajenkoLibraries/Average
- ArduinoJson.h 	https://github.com/bblanchon/ArduinoJson/tree/5.x
- CharStream.h    https://bitbucket.org/gundolf_/thermometergraph/src/master/
- Streaming       https://github.com/janelia-arduino/Streaming
- UniversalTelegramBot.h   https://github.com/witnessmenow/Universal-Arduino-Telegram-Bot
- UniversalTelegramBot.cpp https://github.com/witnessmenow/Universal-Arduino-Telegram-Bot
- WiFiManager.h   https://github.com/tzapu/WiFiManager
- WiFiManager.cpp https://github.com/tzapu/WiFiManager

### install/folder contents:
While all the relevant info has been written above it may still be confusing. Basically, download the files from here as .zip or by git clone. Then make all the required libraries available, either by using the Arduino library Manager or by having them present in the project folder. You may have to edit some of the include statements to fit your situation.

_My_ project folder includes:
- ArduinoJson.h
- Average.h      
- bibbiGram.ino
- CharStream.h 
- chatid.h     
- README.md        
- UniversalTelegramBot.cpp 
- UniversalTelegramBot.h         
- WiFiManager.cpp    
- WiFiManager.h    
- xbm_images.h  

## Usage:
Once the sketch loads and starts it checks if it has valid config data available (nope on first run) and then it switches to AP mode. Take your phone, scan wifi access points and join to the new bibbiGram AP. It will ask you to tap again to connect to the new network and once you do your browser will open with a start screen. 
From a list of available networks select your own. Type in or paste the password.
Below there is another input field for the telegram_bot token. 46 chars long, you may want to copy/paste it in.
Click 'save' and the thing restarts and hopefully connects to the wifi network. 
Have a look now at the botName_debug group, a first message should appear there. 
With all of the above parts put in place we have a thingee that can report to telegram groups but it also listens for messages and can react to them.
Write a message... /hallo
If all went well the bot wil answer with a list of commands it is ready to act on.
/sagMenu will make it reply with a second list of even more commands.
Those commands are /verbObject:
sag (say) will invoke an output to the chat while zeig (show) directs an output to the display.
- /sagWerte (say Values)  reports the current measurements in the telegram chat
- /zeigLuise, /zeigAnton, /zeigAbend shows some tiny pics on the display (pics have been replaced with dummies, you may want to rename or even delete those...)
- zeigMsg msg show message msg on the display. 12 or 13 chars will be visible, i.e. /zeigMsg hello world
- /zeigCountDown n shows a minutely countdown from n down to 0, 3 - 2 - 1 - #
- /clearCountDown allows to interrupt a running countDown
- sagNetz reports SSID, RSSI, IP 
- sagId reports the effective chatId of the current chat
- sagMittel reports the short- and longterm median of measurements, the difference of those, the number of measurements and the list of triggerpoints
- sagStatus summarizes the status of the thing: does it check for decline (or rise) of temperature (or humidity)
uptime in minutes, number of allarms it raised, name of the group it posts debug or alarm messages to
- /setzModus switch between modus 1: check for decline of temperature and modus 2  check for raise of humidity. Modus 1 is the use case of a forgotten window in winter, modus 2 the boiling kettle in the kitchen.
-setDebug toggle the debug flag (which doesn't change much for the current version... TBD)

