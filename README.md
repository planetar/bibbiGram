# bibbiGram

Sketch to drive an ESP8266 with BME280 sensor and .96" I2C display and connect it to Telegram for control and report. 

The basic idea is to connect an ESP8266 mcu with a sensor and enable it to not only measure the current temperature and humidity but to compare these to past measurements so that it can discover sudden drastic changes. Think of a bathroom with a forgotten open window in winter. Or a kettle with water boiling along in the kitchen.

In a smartHome setup it would suffice that the sensor box reports the measurements to some backend event bus and a smartHome controller (i.e. an openHab installation) has the rules to react on this event and notify the inhabitants or do the necessary.

BibbiGram however aims to do this standalone by sending Telegram messages.
It needs 5V from a wall plug, access to a Wlan and a Telegram-Bot token.

## Hardware:
- Nodemcu v2 'Amica' ESP8266 the narrow type that fits nicely on a breadboard. Other types of ESP8266 will work as well but the case would need adaption.
- BME280 sensor which reads temperature, humidity and air pressure.
- 0,96" OLED display with a 4Pin I2C interface.

## Case:
https://www.thingiverse.com/thing:4068963 has been designed for it. The version v2 is recommended. 

## Telegram-Bot:
To enable telegram communication you have to set up a bot. Go to the telegram app on your phone and search for the “botfather” telegram bot (he’s the one that’ll assist you with creating and managing your bot). Send "/help" to the bot for a menu of available commands. "/newbot" get's you going, you have to set a name and a username for it and if you succeed you get a token in a reply. Copy and save that.

While we're at it let's create 2 telegram groups for the bot to post updates: 
- botName_alarm is the destination to post to when an alarm occurs,
- botName_debug receives all sorts of debug messages

Add the new bot to both of these groups and it will listen on commands posted therein.
Later, when everything is up and running, all the family can join the botname_alarm - group and get notifications...

## Libraries:
In order to join a Wlan which we may know nothing about the thing should go in Access Point modus at first start and offer a setup page for clients connecting to the initial AP. There are several libraries which offer this, WiFiManager by tzapu a well known among them. 

The next building block is the telegram communications and I chose the Universal-Arduino-Telegram-Bot for this. This library has an example UsingWiFiManager which was my starting point for this sketch.

The forementioned library depend on the ArduinoJson library in version 5.x which may conflict with dependencies of other libraries (it did for me). The solution is to provide the libraries in the same folder and include them in quotes. To make this work I edited UniversalTelegramBot.h, line 26 to read #include "ArduinoJson.h" (instead of #include <ArduinoJson.h>)

Average.h supplies an easy way to build and get long- and shorttime averages of the measurements and CharStream.h (together with Streaming lirary) allows much nicer formatting of complex output lines to Serial.

Besides, U8g2lib cares for the display and Adafruit_BME280 does the sensor. 
ESP8266WiFi, WiFiClientSecure, EEPROM, DNSServer,ESP8266WebServer are requirements of WiFiManager or UniversalTelegramBot.

mTypes.h defines a data structure for saving the Telegram token and other settings in the emulated Eeprom.

And then there is xbm_images.h which I introduced to separate private content (pictures) from published code. This lib is in its published version just a placeholder where you could include own pics if you want that.

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

I want to thank all the authors for these libraries.

### install/folder contents:
Basically, download the files from here as .zip or by git clone. Then make all the required libraries available, either by using the Arduino library Manager or by having them present in the project folder. You may have to edit some of the include statements to fit your situation.

_My_ project folder includes:
- ArduinoJson.h
- Average.h      
- bibbiGram.ino
- CharStream.h 
- mTypes.h
- README.md        
- UniversalTelegramBot.cpp 
- UniversalTelegramBot.h         
- WiFiManager.cpp    
- WiFiManager.h    
- xbm_images.h  

## Usage:
Once the sketch loads and starts it checks if it has valid config data available (nope on first run) and then it switches to AP mode. Take your phone, scan wifi access points and join to the new bibbiGram AP. It will ask you to tap again to connect to the new network and once you do your browser will open with a start screen. 
From a list of available networks select your own. Type in or paste the password.
Below there is another input field for the telegram_bot token. 45 chars long, you may want to copy/paste it in.
The last input is for an device password which is required when setting message recipient groups for the bot. It can take up to 20 chars.
Click 'save' and wait for the 'dw' logo getting shown. The thing needs a restart now (power cycle) and hopefully connects to the wifi network. 

### Security
First of all, telegram as a platform is not very secure and the data this thing handles are not very private. Should a three-letter-agency desire to learn the temperature of your bathroom they will find a way. But there is a need to keep others from kidnapping access to the commands or send unwanted texts to it. 

This thing sends messages to one of two telegram groups, '_ alarm' and '_ debug'. And membership in one of those groups limits who will be able to call a command on the bot as well. Most commands will be filtered if they do not come frome one of these groups, with the exception of commands opening a menu (/hallo, /sagmenu1, /sagmenu2) and the commands to establish the id of those 2 groups (/setzAlarmGroup, /setzDebugGroup). The latter two commands require that the device password is given.

### Initial Config
After the first config and reboot we need to teach the device where to send alarm and status messages. 

Open your Telegram client and enter the botName_alarm - Group you created before, make sure you already invited the bot to this group. 

- Type "/hallo" (w/o the quotes) and after some seconds the bot replies with links to 2 menus and an info introduction.
- Type "/sagMenu2" and you will see a list of commands to see or change config and status values.
- Type "/setzAlarmGroup *devicePassword*" to teach the bot to direct alarm messages to this group.

Change over to the botName_debug - Group you created before, again please check you already invited the bot to this group or do so now.
- Type "/setzDebugGroup *devicePassword*" to teach the bot to direct debug messages to this group. (Not much for now, you can toggle measurement reports with "/setDebug")

With all of the above parts put in place we have a thingee that can report to telegram groups but it also listens for messages and can react to them.

### Commands
/hallo, /sagMenu1 and /sagMenu2 will reply with lists of commands.

Those commands are /verbObject:
- a slash starts the command
- sag (say) will invoke an output to the chat while 
- zeig (show) directs an output to the display.
- setz (set) changes a setting

Available commands include:
- /sagWerte (say Values)  reports the current measurements in the telegram chat
- /sagMittel reports the short- and longterm median of measurements, the difference of those, the number of measurements and the list of triggerpoints
- /sagStatus summarizes the status of the thing: does it check for decline (or rise) of temperature (or humidity)
uptime in minutes, number of allarms it raised, name of the group it posts debug or alarm messages to
- /zeigMsg msg show message msg on the display. 12 or 13 chars will be visible, i.e. /zeigMsg hello world
- /zeigCountDown n shows a minutely countdown from n down to 0, 3 - 2 - 1 - #
- /clearCountDown allows to interrupt a running countDown
- /sagNetz reports SSID, RSSI, IP 
- /sagId reports the effective chatId of the current chat
- /setzModus switch between modus 1: check for decline of temperature and modus 2  check for raise of humidity. Modus 1 is the use case of a forgotten window in winter, modus 2 the boiling kettle in the kitchen.
- /sagOffset reports the current temp. calibration. (no magic involved: tempMeasured-tempOffset=tempShown)
- /setzOffset n.n (i.e.: /setOffset 1.4) allows to set the offset, decimal separator is a dot
- /setDebug toggle the debug flag (which doesn't change much for the current version... TBD)
- /zeigLuise, /zeigAnton, /zeigAbend show some tiny pics on the display (pics have been replaced with dummies, you may want to rename or even delete those...)

### Calibration
The ESP8266 doesnt take much energy but it still creates exhaust heat. The box has been updated to allow better airflow and more exposure to outside air for the sensor, but still... 
A rough estimate says the tmeperature measurements will be off by about 2.5°C but it is recommended to actually measure it with another thermometer and adapt the offset accordingly. Then again, even if the temp is a little off changes of temperature and humidity will be detected, and the box acts on changes.

### i8n
TBD, currently, the interface is in German. 
