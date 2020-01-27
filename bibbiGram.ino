
/*
  *   bibberBot
      Sketch to drive an ESP8266 with BME280 sensor and .96" I2C display and 
      connect it to Telegram for control and report

  * 0.7.2 : security. We need a device passWd that is set in the initial config screen 
  *         /setz*Group requires the passwd to match
  *         all other commands only work from one of the configured groups
  * 
  */



#include <U8g2lib.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <i2cdetect.h>

#include <Streaming.h>
#include <EEPROM.h>

#include <DNSServer.h>            //Local DNS Server used for redirecting all rs to the configuration portal
#include <ESP8266WebServer.h>     //Local WebServer used to serve the configuration portal
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>

#include "UniversalTelegramBot.h"
#include "WiFiManager.h"          //https://github.com/tzapu/WiFiManager WiFi Configuration Magic

#include "CharStream.h"
#include "Average.h"

#include "xbm_images.h"
#include "mTypes.h"




const char* version = "0.07.3";
const char* APname  = "bibbiGram_bot";
const char* SENSORNAME = APname;

/**************************** PIN **************************************************/

// resetPin (and ledPin) is an option found in the demo for UniversalTelegramBot that has been left included while not actually used
// clkPin and sdaPin are just mentioned here for info
const int clkPin = D3;
const int sdaPin = D4;
const int ledPin = D5;
const int resetConfigPin = D6; //When low will reset the wifi manager config


/**************************** time & vars **************************************************/


// timed_loop
#define INTERVAL_0     60
#define INTERVAL_1   1000
#define INTERVAL_2  10000
#define INTERVAL_3  60000
#define INTERVAL_4 180000
unsigned long time_0 = 0;
unsigned long time_1 = 0;
unsigned long time_2 = 0;
unsigned long time_3 = 0;
unsigned long time_4 = 0;

//


// display
int currPage = 4;  //0;
int maxPage=7;
int maxLoopPage = 2;
int maxLoopPageDefault = 2;
int loopStep = 1;

int skip1Loop = 0;

CharStream<30> strBuffer;


/**************************************** obsolete ***************************************/

int ledStatus = 0;


/*********************************** sensor & trigger ********************************/


// measurement results
long rssi =0;
float humidity, temperature, pressure, rPressure,altitude_measured;
float altitude = 56; //54,06 +2m schrank   // für r48

uint8_t shortCnt=5;

// für rising humidity ~ boiling kettle
//int triggerList[]={8,13,20};
// für falling temp ~ open window:
int triggerList[]={3,6,9};

uint8_t alarmCount=0;
uint8_t maxAlarm=3;
uint8_t ignoreTemp=17;

Average<float> aveLongList(360);
Average<float> aveShortList(shortCnt);

float avgShort;
float avgLong;


// both false is: trigger on temperature doing down
// rising, else falling
boolean check_rising=false;
// humidity, else temperature
boolean check_humidity=false;

// just a reminder of the format this is saved in Eeprom/flash
//int  detectModus;                 // 0=falling , 1=rising 
//int  detectType;                  // 0=Temp, 1= Humidity

// currently only msg from checkMeasurements every 30 s 
boolean sendDebug=false;

// cntMeasures gets reported in /sagMittel and informs about the stability of long time average
// cntMinutes is uptime
// cntDown used for countDown feature
// cntAlarms helps distinguish between 3 different levels of deviation
long cntMeasures=0; 
long cntMinutes=0;

int cntAlarms=0;
int cntDown = -1;
int cntSilentMinutes=0;

// you can use this offset to calibrate the shown temperature in an effort to
// counter the heat that is created by the mcu
float tempOffset=2.6;




/*********************************** telegram bot ********************************/

#define BOT_TOKEN_LENGTH 46
char botToken[BOT_TOKEN_LENGTH] = "";

#define cfgLength 110
configData_t cfg;



//flag for saving data
bool shouldSaveConfig = false;

bool botDebugState = false;

/*********************************** instances  ********************************/

WiFiClientSecure client;
UniversalTelegramBot *bot;


U8G2_SH1106_128X64_NONAME_F_HW_I2C display(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);

Adafruit_BME280 bme;



/*********************************** SETUP  ********************************/

void setup() {
  Serial.begin(115200);
  Serial << F("Setup\n");

 // Initialize the I2C bus 
  // On esp8266 you can select SCL and SDA pins using Wire.begin(D4, D3);
  Wire.begin(D4, D3); 

  display.begin();
  //display.enableUTF8Print();  
  //showUtfPage("");
  //delay(200);
  showCnxPage();
  // show waves to indicate setup/connexion

  // read or at first start, allow config of SSID, passwd, token
  setupBotMagic();

  setupAverageing();
      
  showLogoPage();  
  delay(100); 
  
  setupBme();

  debug( String(SENSORNAME)+" "+version,true);

  // pinMode led, resetPin
  pinMode(resetConfigPin, INPUT);
  pinMode(ledPin, OUTPUT); // initialize digital ledPin as an output.
  delay(10);
  digitalWrite(ledPin, LOW); // initialize pin as off
  
  alarmLine("Hallo sagt dein bibbiBot!");
}




/********************************** START MAIN LOOP*****************************************/

void loop() {
  checkResetPin();
  timed_loop();
}


void timed_loop() {
  
   if(millis() > time_0 + INTERVAL_0){
    time_0 = millis();
 
  } 
  
  if(millis() > time_1 + INTERVAL_1){
    time_1 = millis();
    
    checkBotNewMessages();
  }
   
  if(millis() > time_2 + INTERVAL_2){
    time_2 = millis();

    measureBme(); 
    checkMeasurement();    
    loopDisplayStep();  
  }
 
  if(millis() > time_3 + INTERVAL_3){
    // every minute
    time_3 = millis();
      
    cntMinutes++;
    if (cntDown>-1){
      cntDown--;
      showCountDownPage();
    }

    if (cntSilentMinutes > 0){
      cntSilentMinutes--;
    }
        
  }

  if(millis() > time_4 + INTERVAL_4){
    time_4 = millis();
      
  }
  
 

}

void checkResetPin(){
    // RESET ueber PIN  auf low
  if ( digitalRead(resetConfigPin) == LOW ) {
    Serial.println("Reset");
    WiFi.disconnect();
    Serial.println("Dq");
    delay(500);
    ESP.reset();
    delay(5000);
  }
}

// timed_loop, ~1000
void checkBotNewMessages(){
    int numNewMessages = bot->getUpdates(bot->last_message_received + 1);
    while(numNewMessages) {
      Serial.println("got response");
      handleNewMessages(numNewMessages);
      numNewMessages = bot->getUpdates(bot->last_message_received + 1);
    }
}


void setupBotMagic(){
  
  Serial.println("setupBotMagic");
  
  EEPROM.begin(cfgLength);
  Serial << F("read configData:\n");
  readConfigFromEeprom(0);
  
  Serial << F("cfg.botToken:     ") << (cfg.botToken)      << F("\n");
  Serial << F("cfg.chatId_debug: ") << (cfg.chatId_debug)  << F("\n");
  Serial << F("cfg.chatId_alarm: ") << (cfg.chatId_alarm)  << F("\n");
  Serial << F("cfg.detectModus:  ") << (cfg.detectModus)   << F("\n");
  Serial << F("cfg.detectType:   ") << (cfg.detectType)    << F("\n");
  Serial << F("cfg.offsetTenth:  ") << (cfg.offsetTenth)   << F("\n");
  Serial << F("cfg.passwd:  ")      << (cfg.passwd)        << F("\n");


  WiFiManager wifiManager;
  wifiManager.setSaveConfigCallback(saveConfigCallback);
  
  //Adding an additional config on the WIFI manager webpage for the bot token
  WiFiManagerParameter custom_bot_id("botid", "Bot Token", cfg.botToken, 46); // 50
  wifiManager.addParameter(&custom_bot_id);
  
  //Adding an additional config on the WIFI manager webpage for the device passwd
  WiFiManagerParameter custom_bot_pw("devpass", "Device Password", cfg.passwd, 20);
  wifiManager.addParameter(&custom_bot_pw);

  //If it fails to connect it will create a TELEGRAM-BOT access point
  wifiManager.autoConnect(APname);

  // inserting cfg.nada helps to workaround the issue 
  //Serial << F("cfg.chatId_debug: ") << cfg.chatId_debug << F("\n");      // the value is still intact here  
  //Serial << F("custom_bot_id: ") << custom_bot_id.getValue() << F("\n"); // comes out as expected
  strcpy(cfg.botToken, custom_bot_id.getValue());                        // this call destroys the neighbouring field
  //Serial << F("cfg.botToken: ") << cfg.botToken << F("\n");            // comes out as expected
  Serial << F("cfg.chatId_debug: ") << cfg.chatId_debug << F("\n");      // the value is empty here unless cfg.nada isolates the fields


  // hash the passwd: https://github.com/tzikis/ArduinoMD5
  // but is overkill as you only get to the passwd when you have physical access and then you own it anyway
  strcpy(cfg.passwd, custom_bot_pw.getValue());
  
  if (shouldSaveConfig) {
    writeConfigToEeprom(0);
  }
  
  //Serial << F("cfg.chatId_debug: ") << cfg.chatId_debug << F("\n");

  Serial << F("cfg.passwd: ") << cfg.passwd << F("\n");

  bot = new UniversalTelegramBot(cfg.botToken, client);
  //bot->_debug=true;
  Serial.println("------------------------------------");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  IPAddress ip = WiFi.localIP();
  Serial.println(ip);
    
  client.setInsecure();
}

/********************************** callbacks  *****************************************/


//callback notifying us of the need to save config
void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}


void readConfigFromEeprom(int offset){  
  EEPROM.get( offset, cfg );
  // set to defaults if unconfigured
  if (cfg.detectModus==-1){(cfg.detectModus=check_rising);}
  if (cfg.detectType==-1){(cfg.detectType=check_humidity);}
  if (cfg.offsetTenth==-1){
    float t = tempOffset * 10;
    (cfg.offsetTenth=(int) t);
  }
  // after format (erase all flash) every byte is 255. We want to empty then
  // but it is possible that the first byte is 255 simply cause this is the configured value
  // it should stay untouched then. So instead of one-by-one let's check them all together
  int test0 = (int)cfg.botToken[0];
  int test1 = (int)cfg.chatId_debug[0];
  int test2 = (int)cfg.chatId_alarm[0];
  int test3 = (int)cfg.passwd[0];

  if (test0==255 && test1==255 && test2==255 && test3==255){
    cfg.botToken[0]=0;
    cfg.nada[0]=0;
    cfg.chatId_debug[0]=0;
    cfg.chatId_alarm[0]=0;
    cfg.passwd[0]=0;
  }

  check_rising = (cfg.detectModus==1);
  check_humidity = (cfg.detectType == 1);
  
  float amount = (float) cfg.offsetTenth;
  amount = amount/10;
  if (amount > -6 && amount < 6){tempOffset = amount ;};
  
}

void writeConfigToEeprom(int offset){
  
  Serial << F("writeConfigToEeprom ") << offset << F("\n");
  
  // update possibly changed values
  cfg.detectModus = check_rising ? 1 : 0;
  cfg.detectType = check_humidity ? 1 : 0;
  float t = tempOffset * 10;
  int t1 = (int) t;
  cfg.offsetTenth = t1;
  
  EEPROM.put( offset, cfg );
  delay(200);
  EEPROM.commit();    // Needed for ESP8266 to get data written
  
}

void setDebugGroup(String chat_id){
      int bufLen=15;
      char charBuf[bufLen];
      chat_id.toCharArray(charBuf,bufLen);
      for (int i = 0; i<bufLen; i++ ){
        cfg.chatId_debug[i]=charBuf[i]; }   
      writeConfigToEeprom(0);
}

void setAlarmGroup(String chat_id){
      int bufLen=15;
      char charBuf[bufLen];
      chat_id.toCharArray(charBuf,bufLen);
      for (int i = 0; i<bufLen; i++ ){
        cfg.chatId_alarm[i]=charBuf[i]; }
      writeConfigToEeprom(0);
}


/********************************** bot chat  *****************************************/


void handleNewMessages(int numNewMessages) {
  Serial << F("handleNewMessages (") << numNewMessages << F(")\n");
  for(int i=0; i<numNewMessages; i++) {
    String chat_id = String(bot->messages[i].chat_id);
    String text = bot->messages[i].text;
    String from_name = bot->messages[i].from_name;
    String chat_title =  bot->messages[i].chat_title;
    text.toLowerCase();
    Serial << F("from_name: ") << from_name << F("\n");
    Serial << F("chat_title: ") << chat_title << F("\n");
    Serial << F("text: |") << text << F("|\n");

    // check if chat_id is among the configured groupIds
    bool isValidGroup = (chat_id == String(cfg.chatId_debug) || chat_id == String(cfg.chatId_alarm));
    bool isValidCmd   = (isValidGroup || (text=="/hallo"||text=="/sagmenu1"||text=="/sagmenu2"||text=="/sagmenu3"||text.startsWith("/setzdebuggroup")||text.startsWith("/setzalarmgroup")));

    if (!isValidCmd) {
      String msg = "Hallo "+ from_name +"!\n\n";
      msg += "Schön, von dir zu hören, aber es fühlt sich gerade nicht richtig an. \n";
      msg += "(restricted access) \n";
      bot->sendMessage(chat_id, msg, "Markdown");
      return;
    }
    
    if (text.startsWith("/hallo")) {
      Serial << F("if: ") << text << F("\n\n");
      String msg = "Hallo "+ from_name +"!\n\n";
      msg += "was hier geht: \n";
      msg += "/sagMenu1   : HauptMenü\n";
      msg += "/sagMenu2   : Status & Config\n";
      msg += "/sagInfo    : erklär mal\n";
      bot->sendMessage(chat_id, msg, "Markdown");
    }

    else if (text.startsWith("/sagauswahl")||text.startsWith("/sagmenu1")) {
      String msg = "Worauf ich so höre:\n\n";
      msg += "/sagWerte : nennt akt. Meßwerte\n";
      msg += "/sagMittel : nennt Mittelwerte\n";
      msg += "/sagStatus : Selbstauskunft\n";
      msg += "/zeigMsg   : zeigt Kurztext \n";
      msg += "/zeigCountDown n : zeigt Countdown\n";
      msg += "/clearCountDown   : stoppt Countdown\n";
      msg += "/gibRuhe n : n Minuten kein Alarm\n";
      msg += "/zeigLuise : zeigt Luise auf display \n";
      msg += "/zeigAnton : zeigt Anton auf display \n";
      msg += "/zeigAbend : zeigt Bild auf display \n";
      msg += "/hallo     : sag Hallo\n";
      msg += "/sagMenu2  : Status & Config\n";
      bot->sendMessage(chat_id, msg, "Markdown");
    }

    else if (text.startsWith("/sagmenu2")) {
      String msg = "Mehr Aufrufe für BibbiBot:\n\n";
      msg += "/sagNetz : listet Verbindungsdaten\n";
      msg += "/sagId : nennt die effektive chatId\n";
      msg += "/setzModus : check Kälte oder Dampf?\n";
      msg += "/sagOffset : nennt Temperatur-Offset\n";
      msg += "/setzOffset n.n : setze Offset \n";
      
      msg += "/setzDebug : debug-Msg senden?\n";
      msg += "/setzDebugGroup passwd: aktuelle Gruppe bekommt debug-Msg\n";
      msg += "/setzAlarmGroup passwd: aktuelle Gruppe bekommt Alarm-Msg\n";
      msg += "/hallo     : sag Hallo \u2764\ufe0f \n";
      msg += "/sagMenu1: Hauptmenü\n";
      bot->sendMessage(chat_id, msg, "Markdown");
    }

    // this is 'undocumented' and the commands are only useful for debug
    // and while a Serial monitor is open 
    else if (text.startsWith("/sagmenu3")) {
      String msg = "Weitere Aufrufe für BibbiBot:\n\n";
      msg += "/gibAlarm : tu, als sei ein Alarm-Event\n";
      msg += "/gibDebug : tu, als sei ein Debug-Event\n";
      msg += "/gib12c   : ruf 12cdetect auf Serial\n";
      msg += "/setzBotDebugState : debug bot to Serial\n";
      msg += "/checkPasswd n : check, ob das Passwort stimmt \n";
//      

      bot->sendMessage(chat_id, msg, "Markdown");
    }

    else if (text.startsWith("/gibalarm")) {
      String msg = "gebe Alarm an "+String(cfg.chatId_alarm);
      
      bot->sendMessage(chat_id, msg, "Markdown");

      alarmLine("dummy-Alarm alarmLine");
      doAlarm();
   }

    else if (text.startsWith("/gibdebug")) {
      String msg = "gebe Debug an "+String(cfg.chatId_debug);
      
      bot->sendMessage(chat_id, msg, "Markdown");
      debug("dummy-debug",true);

   }

    else if (text.startsWith("/gibi2c")) {
      String msg = "gebe i2cdetect() \n";
      
      bot->sendMessage(chat_id, msg, "Markdown");

      i2cdetect();

   }

    else if (text.startsWith("/checkpw")) {
      String tmp = text.substring(9);
      tmp.trim();
      
      String msg = "Eingabe: " + tmp +" \n";
      msg += (String(cfg.passwd) == tmp)?" passt!\n":" passt nicht\n";
      
      bot->sendMessage(chat_id, msg, "Markdown");

   }
    // botDebugState
    
    else if (text.startsWith("/setzbotdebugstate")) {
      //Serial << F("if: ") << text << F("\n\n");
      botDebugState = !botDebugState;
      bot->_debug=botDebugState;
      String msg="botDebugState ist jetzt "+String(botDebugState);
  
      bot->sendMessage(chat_id, msg, "Markdown");

    }   
                    
        
    else if (text.startsWith("/saginfo")) {
      Serial << F("if: ") << text << F("\n\n");
      String msg="BibbiBot ist ein Sensor mit Anzeige und Telegram-Verbindung über WLAN.\n";
      msg += "Es braucht ein WLAN, wo es mit Kennwort angemeldet ist. Wenn das WLAN aus ist, wird die Kiste selbst zum Accesspoint. Du kannst dich mit dem Handi dort anmelden und das Netz, Passwort und Token eingeben.\n\n";
      msg += "Telegram dient zur Interaktion mit der Kiste, damit alles funktioniert, braucht BibbiBot ein botToken und zwei Telegram-Gruppen. \n";
      msg += "Eine Gruppe für Statusmeldungen (debug) und eine Gruppe für Alarme.\n";
      
      msg += "/hallo     : sag Hallo\n";
      msg += "/sagMehrInfo: genau, mehr Info\n";
      bot->sendMessage(chat_id, msg, "Markdown");
    }
        
    else if (text.startsWith("/sagmehrinfo")) {
      Serial << F("if: ") << text << F("\n\n");
      String msg="";   
      msg += "So ein Alarm kann sein: die Temperatur sinkt auf einmal stetig. (zB.: Fenster steht offen). Oder auch: Die Feuchtigkeit steigt stetig (Wasserkessel verkocht in der Küche).\n\n";
      msg += "Der Sensor misst Temperatur und Luftfeuchte und vergleicht langfristige und kurzfristige Mittelwerte. Und er hat Schwellwerte, wenn aktueller und langfristiger Mittelwerte mehr als die abweichen, gibt es einen Alarm.\n";
      msg += "Alarm heisst: eine Nachricht in die Gruppe für Alarme. Und wer in dieser Gruppe ist, bekommt den Alarm per Telegram auf's Handi.\n\n";
      
      msg += "/hallo     : sag Hallo\n";
      msg += "/sagNochMehrInfo: genau, noch mehr Info\n";
      bot->sendMessage(chat_id, msg, "Markdown");
    }

    else if (text.startsWith("/sagnochmehrinfo")) {
      String msg="Wer misst, misst Mist\n";
      msg += "Die gemessene Temperatur ist evtl. zu hoch, weil die Kiste selbst Abwärme erzeugt. Es gibt einen Offset zur Kalibrierung. Und für die Alarme ist die Änderung wichtig, und die ist genau.\n";
      msg += "BibbiBot kann noch mehr: Kurznachrichten (ca 12 Zeichen). Countdown in Minuten. Kleine Bildchen.\n\n";
      msg += "und und und.\n\n";
      
      
      bot->sendMessage(chat_id, msg, "Markdown");
    }
        
    else if (text.startsWith("/sagid")) {
      String msg="deine id ist: "+String(chat_id);
      
      bot->sendMessage(chat_id, msg, "Markdown");
    }       
    
    else if (text.startsWith("/sagoffset")) {
      String msg="der Temperatur-Offset ist: "+String(tempOffset)+"\n";
      
      bot->sendMessage(chat_id, msg, "Markdown");
    }

  
    else if (text.startsWith("/setzoffset")) {
      String msg;
      String tmp = text.substring(11);
      tmp.trim();
      float amount = (float) tmp.toFloat(); 
      if (amount<-6||amount>6){msg="off limit!";}else{
      Serial << F("neuer Offset: ") << amount << F("\n");
      tempOffset=amount;
      msg="der neue Temperatur-Offset ist: "+String(tempOffset)+"\n";

      writeConfigToEeprom(0);
      
      }
      bot->sendMessage(chat_id, msg, "Markdown");

    }
    
    else if (text.startsWith("/setzdebuggroup")) {
      String msg;
      String tmp = text.substring(16);
      tmp.trim();
      if (String(cfg.passwd) == tmp){
        msg="debugMsg gehen an die Gruppe "+chat_title+" mit der id: "+chat_id;
        bot->sendMessage(chat_id, msg, "Markdown");  
        setDebugGroup(chat_id);
      } else {
        msg = "das Passwort war jetzt nicht ganz richtig";
        bot->sendMessage(chat_id, msg, "Markdown");  
      }
    }
    
    else if (text.startsWith("/setzalarmgroup")) {
      String msg;
      String tmp = text.substring(16);
      tmp.trim();
      if (String(cfg.passwd) == tmp){
        msg="alarmMsg gehen an die Gruppe "+chat_title+" mit der id: "+chat_id;
        bot->sendMessage(chat_id, msg, "Markdown");  
        setAlarmGroup(chat_id);
      } else {
        msg = "das Passwort war leider nicht ganz richtig";
        bot->sendMessage(chat_id, msg, "Markdown");  
      }
      
    }

    
    else if (text.startsWith("/sagwerte")) {
      String msg="die aktuellen Messwerte sind: \n";
      msg += "Temperatur:  "+String(temperature)+" °C\n";
      msg += "Luftfeuchte: "+String(humidity)+" %\n";
      msg += "r.Luftdruck: "+String(rPressure)+" hP\n";
      bot->sendMessage(chat_id, msg, "Markdown");
    }    
    else if (text.startsWith("/sagmittel")) {
      
      float diff = avgLong-avgShort;
      //diff = abs(diff);
      if (diff < 0) {diff = diff * -1;}
      
      String diffLine="Differenz: "+String(diff);
      
      if (check_humidity){
        diffLine += " %\n\n";
      } else {
        diffLine += " °C\n\n";
      }
      
      String msg="kurz- und langfristige Mittelwerte sind: \n";
      msg += "kurzfr.:   "+String(avgShort)+" °C\n";
      msg += "langfr.:   "+String(avgLong) +" °C\n";
      msg += diffLine; 
      
      msg += String(cntMeasures) +" Messungen\n";
      msg += "Trigger 1: "+String(triggerList[0])+"\n";
      msg += "Trigger 2: "+String(triggerList[1])+"\n";
      msg += "Trigger 3: "+String(triggerList[2])+"\n\n";
      
      if (check_rising){
        msg += "achte auf Anstieg der\n";
      } else {
        msg += "achte auf Abfallen der\n";
      }   
      if (check_humidity){
        msg += "Luftfeuchte\n";
      } else {
        msg += "Temperatur\n";
      }  

            
      bot->sendMessage(chat_id, msg, "Markdown");
    }    
    
    else if (text.startsWith("/sagnetz")) {
      String msg="die aktuellen Netzwerte sind: \n";
      msg += "Wlan:  "+WiFi.SSID()+"\n";
      msg += "IP: "+WiFi.localIP().toString()+"\n";
      msg += "Signal: "+String(WiFi.RSSI())+" dB\n";
      bot->sendMessage(chat_id, msg, "");
    }    

    else if (text.startsWith("/sagstatus")) {
      String msg="mein Status ist: \n";
      if (check_rising){
        msg += "achte auf Anstieg der\n";
      } else {
        msg += "achte auf Abfallen der\n";
      }   
      if (check_humidity){
        msg += "Luftfeuchte\n";
      } else {
        msg += "Temperatur\n";
      }  
      msg += "ich laufe seit:  "+String(cntMinutes)+" Minuten\n";
      msg += "ich habe "+String(cntAlarms) +" Alerts ausgelöst\n";
      //msg += "Gruppe für Alerts: "+cfg.chatId_alarm +" \n";
      //msg += "Gruppe für debug:  "+cfg.chatId_debug+" \n";

      if (cntSilentMinutes>0){
        msg += "\nich schweige noch "+String(cntSilentMinutes) +" Minuten\n";
      }

     if (cntDown>0){
        msg += "\nCountDown: "+String(cntDown) +" Minuten\n";
      }

      
      msg += "firmware: "+  String(SENSORNAME)+" "+version+" \n";
      
      bot->sendMessage(chat_id, msg, "");
    }    

   else if (text.startsWith("/zeigcountdown")) {
      String msg;
      int amount;     
      // wie lange?
      String tmp = text.substring(14);
      
      tmp.trim();
      amount = (int) tmp.toInt(); 

      if (amount > 0){
               // feedback geben
        msg="die Zeit läuft... \n";
        msg+="beenden mit /clearCountdown \n";
      } else {
        msg="Countdown von was? Ich nehm' mal 3... \n";
        msg+="beenden mit /clearCountdown \n";
        amount = 3;
      }
      
      bot->sendMessage(chat_id, msg, "");
 
      // countDown setzen
      cntDown=amount;
      
      // stopUhr stellen
      time_3 = millis();
      
      // ersten Schritt zeigen
      showCountDownPage();
              
   }  
   
   else if (text.startsWith("/clearcountdown")) {
    cntDown=-1;
    String msg="na gut! \n";
    bot->sendMessage(chat_id, msg, "");   
   }

//cntSilentMinutes
   else if (text.startsWith("/gibruhe")) {
      String msg;
      int amount;     
      // wie lange?
      String tmp = text.substring(9);
      
      tmp.trim();
      if (tmp==""){
        // cmd w/o parameter, default = 60
        msg="wielange kein Alarm? Ich nehm' mal 60 min... \n";
        msg+="beenden mit /gibRuhe 0 \n";
        amount = 60;               
        
        // SilentMinutes setzen
        cntSilentMinutes=amount;  
      }
      
      else {
        // we got a parameter and read it as a number
        amount = (int) tmp.toInt();
        
        if (amount == 0){
          // they want to cancel
           
          // feedback geben
          msg="ok, alles wieder normal! \n";
          
          // SilentMinutes setzen
          cntSilentMinutes=amount; 
                   
        } else if (amount > 0){
          // set silence for amount minutes
          
          // feedback geben
          msg="na gut, ich schweige... \n";
          msg+=String(amount)+" Minuten kein Alarm\n";
          msg+="beenden mit /gibRuhe 0 \n";

 
          // SilentMinutes setzen
          cntSilentMinutes=amount;          
        
        } else {
          // param is a surprise, do nothing special
          msg="(hüstel)... \n";
        }

      }

      bot->sendMessage(chat_id, msg, "");
                    
   }  
   

    else if (text == "/setzdebug") {  
      sendDebug = !sendDebug;
      String msg="mein Status ist: \n";
      msg += "debug: "+  String(sendDebug)+" \n";    
      bot->sendMessage(chat_id, msg, "");
      bot->sendMessage(String(cfg.chatId_debug), msg, "");
    }

    else if (text == "/setzmodus") {  

      switchModus();
      
      String msg="mein Modus ist: \n";
      if (check_rising){
        msg += "achte auf Anstieg der\n";
      } else {
        msg += "achte auf Abfallen der\n";
      }   
      if (check_humidity){
        msg += "Luftfeuchte\n";
      } else {
        msg += "Temperatur\n";
      }  

    bot->sendMessage(chat_id, msg, "");
    }

    else if (text == "/sagcfg") {  

 
      
      String msg="cfg enthält: \n";

        //msg += "botToken: |"+String(cfg.botToken)+"|\n";
        msg += "chatId_debug: |"+String(cfg.chatId_debug)+"|\n";
        msg += "chatId_alarm: |"+String(cfg.chatId_alarm)+"|\n";
        msg += "detectModus: |"+String(cfg.detectModus)+"|\n";
        msg += "detectType: |"+String(cfg.detectType)+"|\n";
        msg += "offsetTenth: |"+String(cfg.offsetTenth)+"|\n";
  

    bot->sendMessage(chat_id, msg, "");
    }

    
    
    else if (text == "/zeigluise") {
      String msg="gerne \n";

      bot->sendMessage(chat_id, msg, "");
      skip1Loop = 5;
      showXBM1Page();
    }    
    
    else if (text == "/zeiganton") {
      String msg="sofort \n";

      bot->sendMessage(chat_id, msg, "");
      skip1Loop = 3;
      showXBM2Page();
    }    

    else if (text == "/zeigabend") {
      String msg="mmnnhh \n";

      bot->sendMessage(chat_id, msg, "");
      skip1Loop = 2;
      showXBM3Page();
    }    


    else if (text.startsWith("/zeigmsg")) {
      String msg=text.substring(9);

      bot->sendMessage(chat_id, "mach ich", "");
      skip1Loop = 3;
      showMsgPage(msg);
    }       
    else {
      String msg="oh, war was? \n";
      bot->sendMessage(chat_id, msg, "");
      debug(text,false);
    }
    
    
  }
}


/********************************** messaging  *****************************************/

void debug(String msg,boolean toSerial){
  // toSerial sets whether to send msg to Serial
  // but at the same time it signals a msg that should be send to debugGroup regardless of sendDebug True or False
  // sendDebug ?
  
  if (toSerial||sendDebug){
    // chat away
    bot->sendMessage(String(cfg.chatId_debug), msg, "");
  }

  if (toSerial){
    Serial << msg << "\n";
  }
}

void alarmLine(String msg){

  // chat away
  String id=String(cfg.chatId_alarm);
  bot->_debug=true;
  bot->sendMessage(id, msg, "");
  bot->_debug=false;
  Serial << F("alarmLine: |") << id << F("|") << msg << "\n";

}

void doAlarm(){
  String msg;
  
  msg = "Alarm #" + String(alarmCount)+"\n";
  
  if (check_humidity){
    msg += String(humidity) + " %\n\n";
    msg += "Wasserkessel vergessen?\n\n";   
  } else {    
    msg += String(temperature) + " °C\n\n";
    msg += "Fenster vergessen?\n\n";
  }
  
  msg += "/sagMittel gibt Details";
  
 // chat away
  bot->sendMessage(String(cfg.chatId_alarm), msg, "");

  
  
  debug(msg,true);
}



/********************************** sensor  *****************************************/


void measureBme(void)
{
  bme.takeForcedMeasurement();
  humidity    = bme.readHumidity();
  temperature = bme.readTemperature();
  pressure    = bme.readPressure()/100.0F;
  rPressure   = bme.seaLevelForAltitude(altitude, pressure);

  // ahemm, the case inflates the temperature values
  temperature -= tempOffset;
}


void checkMeasurement(){
  // called immediately after taking a new measurement
  // humidity, temperature are taken from the globals

  boolean stetig = true;
  uint8_t iter = 0;

  int alarmTrigger = triggerList[alarmCount];
  float newVal;
  
  if (check_humidity){newVal = humidity;} else {newVal = temperature;}

  aveLongList.push(newVal);
  avgLong = aveLongList.mean();
  
  aveShortList.push(newVal);
  avgShort=aveShortList.mean();

  // test if avgLong and avgShort are valid
  if (isnan(avgLong) || isnan(avgShort)){
    debug("average fail: clear lists",true);
    setupAverageing();
    // try it again
    aveLongList.push(newVal);
    avgLong = aveLongList.mean();
    
    aveShortList.push(newVal);
    avgShort=aveShortList.mean();   
  }


  cntMeasures++;
  
  // if there were any limiting factors, this wd be the place to check them
  if (cntSilentMinutes>0){return;}
  
  // only care for low temps when searching for falling temp
  if (!check_humidity && (temperature > ignoreTemp)){return;}
    
  float diff = avgLong - avgShort;
  //diff = abs(diff);
  if (diff < 0){diff=diff*-1;}
    
    String msg= "temp: "+String(temperature)+" humi: "+String(humidity)+"\n newVal: "+String(newVal)+"\n avgLong: "+String(avgLong)+" avgShort: "+String(avgShort)+"\ndiff: "+String(diff)+" trigger: "+String(alarmTrigger)+" cnt: "+String(cntMeasures)+" alarmCount: "+String(alarmCount);
    debug(msg,false);
    
    for (int i = 0; i < shortCnt; i++) {
          if (i>0){
  
            // we search rising values but the last was less than the one before  or
            // we search falling values but the last was more than the one before
            if (check_rising && (aveShortList.get(i-1)> aveShortList.get(i))|| !check_rising && (aveShortList.get(i-1)< aveShortList.get(i)) ){
                stetig=false;
            }
            
          }
          
    }
    
    if (stetig){
      debug("stetig",false);
      // we search rising values and now the short even after deducting trigger is still larger than long average  or
      // we search falling values and now the short even after adding trigger is less then long average    
      if ( check_rising && (avgShort - alarmTrigger > avgLong) || !check_rising && (avgShort + alarmTrigger < avgLong) ){
        if (alarmCount<maxAlarm){
          alarmCount++;
          doAlarm();
        }
      }
  
    }
    else {
      alarmCount=0;
      debug("nicht stetig",false);
    }
  

  
  
}



void switchModus(){
      check_humidity = !check_humidity;
      check_rising = !check_rising;

      setupAverageing();
      
      writeConfigToEeprom(0);
  
}

void setupAverageing(){
      aveLongList.clear();
      aveShortList.clear();

      avgLong     = 0;
      avgShort    = 0;
      cntMeasures = 0;
      alarmCount=0;  
}



float seaLevelForAltitude(float altitude, float atmospheric)
{
    // Equation taken from BMP180 datasheet (page 17):
    //  http://www.adafruit.com/datasheets/BST-BMP180-DS000-09.pdf

    // Note that using the equation from wikipedia can give bad results
    // at high altitude. See this thread for more information:
    //  http://forums.adafruit.com/viewtopic.php?f=22&t=58064

    return atmospheric / pow(1.0 - (altitude/44330.0), 5.255);
}



/********************************** sentup stuff  *****************************************/



void setupBme(){

  //Initialize BME280
  bme.begin(); 

  // weather monitoring
    bme.setSampling(Adafruit_BME280::MODE_FORCED,
                    Adafruit_BME280::SAMPLING_X1, // temperature
                    Adafruit_BME280::SAMPLING_X1, // pressure
                    Adafruit_BME280::SAMPLING_X1, // humidity
                    Adafruit_BME280::FILTER_OFF   );
                      
    // suggested rate is 1/60Hz (1m)
    //delayTime = 60000; // in milliseconds

}



/********************************** display stuff  *****************************************/

void showPage(void){

  switch (currPage){
    case 0:
      showTempPage();
      break;      
    case 1:
      showHumPage();
      break;
    case 2:
      showPressPage();
      break;
    case 3:
      showXBM1Page();
      break;
    case 4:
      showXBM2Page();
      break;
    case 5:
     showXBM3Page();
      break;
    case 6:
//      showBabyYodaPage();
      break;
    case 7:
//      showDanielPage();
      break;
    case 9:
      showLogoPage();
      break;
      }
}

// loop display of pages
void loopDisplayStep(void) {
  //if (flagAlert){ return; }
  
  if (skip1Loop>0 ){
    
      skip1Loop -= 1;
      return;
  }

  if (cntDown>-1 ){
    
      return;
  }

  
  currPage += loopStep;
  if (currPage > maxLoopPage){currPage=0;};
  if (currPage < 0){currPage=maxLoopPage;};
  
  showPage();
}



void showTempPage(){
  
  display.clearBuffer();         // clear the internal memory
  display.setFont( u8g2_font_logisoso32_tf);  // choose a suitable font

  // erste dezimal in kleinem font
  int temp = (int) temperature;
  int dez = (temperature -temp)*10;
   
  String value = String(temp);
  strBuffer.start() << value << F(" *C");
  display.drawStr (10, 40, strBuffer);

  value = String(dez);
  display.setFont( u8g2_font_logisoso16_tf);
  strBuffer.start() << value;
  display.drawStr (52, 40, strBuffer);
   
  display.sendBuffer();
}


void showHumPage(){
  
  display.clearBuffer();         // clear the internal memory
  display.setFont( u8g2_font_logisoso32_tf);  // choose a suitable font

  // erste dezimal in kleinem font
  int temp = (int) humidity;
  int dez = (humidity -temp)*10;
  
  String value = String(temp);
  strBuffer.start() << value << F(" %") ;
  display.drawStr (10, 40, strBuffer);

  value = String(dez);
  display.setFont( u8g2_font_logisoso16_tf);
  strBuffer.start() << value;
  display.drawStr (52, 40, strBuffer);
     
  display.sendBuffer();
}


void showPressPage(){
  
  display.clearBuffer();         // clear the internal memory
  display.setFont( u8g2_font_logisoso22_tf);  // choose a suitable font

  String value = String(int(rPressure));
    
  strBuffer.start() << value << F(" hPa") ;
  display.drawStr (1, 35, strBuffer);
   
  display.sendBuffer();
}


void showUtfPage(String alertMsg){
  // does not work yet 
  display.clearBuffer();         // clear the internal memory
  display.setFont( u8g2_font_fub11_tf);  // choose a suitable font
  alertMsg = "\u2764\ufe0f";

  
  strBuffer.start() << alertMsg;
  display.drawUTF8 (3, 32, strBuffer);  
  
 
  display.sendBuffer();
}


void showMsgPage(String alertMsg){
    
  display.clearBuffer();         // clear the internal memory
  display.setFont( u8g2_font_fub11_tf);  // choose a suitable font

  // TBD:
  // pruefe Laenge der msg und teile ggf in 2 oder gar drei
  
  strBuffer.start() << alertMsg;
  display.drawUTF8 (3, 32, strBuffer);  
  
 
  display.sendBuffer();
}

void showCountDownPage(){
  String value;
  display.clearBuffer();         // clear the internal memory
  display.setFont( u8g2_font_logisoso32_tf);  // choose a suitable font

  if (cntDown>0){
    value = String(cntDown);
  } else {
    value = "#💥!!#";
  }
    
  strBuffer.start() << value;
  display.drawStr (51, 50, strBuffer);
   
  display.sendBuffer();
  skip1Loop = 2;
}


void showLogoPage(void){
  
  display.clearBuffer();
  display.drawXBM( 23, 11, 82, 42, logo_xbm);
  display.sendBuffer();
}


void showCnxPage(void){

  display.clearBuffer();
  display.drawXBM( 32, 2, 60, 60, cnx_xbm);
  display.sendBuffer();
}


void showXBM1Page(void){

  display.clearBuffer();
  display.drawXBM( 32, 2, 60, 60, pic1_xbm);
  display.sendBuffer();
}

void showXBM2Page(void){

    
  display.clearBuffer();
  display.drawXBM( 32, 2, 60, 60, pic2_xbm);
  display.sendBuffer();
}


void showXBM3Page(void){

  display.clearBuffer();
  display.drawXBM( 32, 2, 60, 60, pic3_xbm);
  display.sendBuffer();
}
