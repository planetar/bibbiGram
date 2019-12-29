
/*
  *   bibberBot
  
     sammlung auf dem wege zu Luises Geburtstagsgeschenk
  : ein kleines device, das brüllt, wenn das Fenster im Bad offen blieb :

  * yo, geht erst mal. example by Brian Lough 
  * 
  * am ende von setup 
  * 
  * client.setInsecure();                                errrm!
  * alternative: Arduino-core Version zurück auf 2.4.2 
  * 
  * die grossen libraries Wifimanager, UniversalTelegrambot und ArduinoJson (in version 5.x)
  * liegen direkt im prg-Ordner, um die versionitis hinzubekommen
  * 
  * auf #102 kommt jetzt die Integration mit sensor, display
  * 
  * 
  * 
  */


#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>

//#include <i2cdetect.h>
#include "Average.h"
#include <Streaming.h>
#include "CharStream.h"
#include <U8g2lib.h>
#include "xbm_images.h"
#include "chatid.h"

  
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include "UniversalTelegramBot.h"

#include <EEPROM.h>

#include <DNSServer.h>            //Local DNS Server used for redirecting all rs to the configuration portal
#include <ESP8266WebServer.h>     //Local WebServer used to serve the configuration portal
#include "WiFiManager.h"          //https://github.com/tzapu/WiFiManager WiFi Configuration Magic




const char* version = "0.06.4";
const char* APname  = "bibbiGram_bot";
const char* SENSORNAME = APname;

/**************************** PIN **************************************************/

const int clkPin = D3;
const int sdaPin = D4;
const int ledPin = D5;
const int resetConfigPin = D6; //When low will reset the wifi manager config


/**************************** time & vars **************************************************/

const char* on_cmd = "ON";
const char* off_cmd = "OFF";


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
//// debug messages
//const int numMsg=20;
//int msgCount=0;
//String msg=".";
//String arrMessages[numMsg];


// display
int currPage = 4;  //0;
int maxPage=7;
int maxLoopPage = 2;
int maxLoopPageDefault = 2;
int loopStep = 1;

int skip1Loop = 0;
int countDown = -1;

CharStream<30> strBuffer;


/**************************************** obsolete ***************************************/

int ledStatus = 0;


/*********************************** sensor & trigger ********************************/


// measurement results
long rssi =0;
float humidity, temperature, pressure, rPressure,altitude_measured;
float altitude = 56; //54,06 +2m schrank   // für r48

uint8_t shortCnt=5;

// für rising humidity ~ Wasserkessel
//int triggerList[]={8,13,20};
// für falling temp ~ Fenster offen:
int triggerList[]={3,6,9};

uint8_t alarmCount=0;
uint8_t maxAlarm=3;
uint8_t ignoreTemp=17;

Average<float> aveLongList(240);
Average<float> aveShortList(shortCnt);

float avgShort;
float avgLong;


// beide false ist: auf sinkende Temperaturen triggern
// rising, else falling
boolean check_rising=false;
// humidity, else temperature
boolean check_humidity=false;

boolean sendDebug=false;

// wird hochgezählt, Idee ist, anfangs bei kleinen werten hier wg unvollkommener Basis noch nichts zu tun
long cntMeasures=0; 
long cntMinutes=0;
int  cntAlarms=0;


float tempOffset=1.8;

/*

  wenn die kiste gerade anläuft, cntMeasures < 100 ist, ist der Einfluss der neuen Werte recht gross
  wenn die checkMeasurement zu oft gerufen wird, besteht die Gefahr, dass die Temp-Veränderung kleiner der Fehler-Streubreite ist und nicht steti erkannt wird, obwohl es immer kälter wird
  
 
 */


/*********************************** telegram bot ********************************/

#define BOT_TOKEN_LENGTH 46

char botToken[BOT_TOKEN_LENGTH] = "";



//flag for saving data
bool shouldSaveConfig = false;



/*********************************** instances  ********************************/

WiFiClientSecure client;
UniversalTelegramBot *bot;


U8G2_SH1106_128X64_NONAME_F_HW_I2C display(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);

//WiFiClient espClient;
//PubSubClient mqClient(espClient);
Adafruit_BME280 bme;



/*********************************** SETUP  ********************************/

void setup() {
  Serial.begin(115200);

  setupBotMagic();
  //setupOta();

  // Initialize the I2C bus (BH1750 library doesn't do this automatically)
  // On esp8266 you can select SCL and SDA pins using Wire.begin(D4, D3);
  Wire.begin(D4, D3);

  //i2cdetect();
  
  setupBme();
  display.begin();
  showLogoPage();
  debug( String(SENSORNAME)+" "+version,true);

  // pinMode led, resetPin
  pinMode(resetConfigPin, INPUT);
  pinMode(ledPin, OUTPUT); // initialize digital ledPin as an output.
  delay(10);
  digitalWrite(ledPin, LOW); // initialize pin as off
  
  alarmLine("Hallo sagt W27, dein bibbiBot!");
}




/********************************** START MAIN LOOP*****************************************/

void loop() {
  //ArduinoOTA.handle();
  checkResetPin();
  timed_loop();

}


void timed_loop() {
  
   if(millis() > time_0 + INTERVAL_0){
    time_0 = millis();
 
  } 
  
  if(millis() > time_1 + INTERVAL_1){
    time_1 = millis();
    
      //checkDebug();  
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
    if (countDown>-1){
      countDown--;
      showCountDownPage();
    }
        
  }

  if(millis() > time_4 + INTERVAL_4){
    time_4 = millis();
      
         //sendState();
      
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
  
  Serial.println("");

  // bot token
  EEPROM.begin(BOT_TOKEN_LENGTH);
  Serial.println("read bot token");
  readBotTokenFromEeprom(0);
  Serial.println(botToken);

  WiFiManager wifiManager;
  wifiManager.setSaveConfigCallback(saveConfigCallback);
  //Adding an additional config on the WIFI manager webpage for the bot token
  WiFiManagerParameter custom_bot_id("botid", "Bot Token", botToken, 50);
  wifiManager.addParameter(&custom_bot_id);
  //If it fails to connect it will create a TELEGRAM-BOT access point
  wifiManager.autoConnect(APname);

  strcpy(botToken, custom_bot_id.getValue());
  if (shouldSaveConfig) {
    writeBotTokenToEeprom(0);
  }

  bot = new UniversalTelegramBot(botToken, client);
  //bot->_debug=true;
  Serial.println("");
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

void readBotTokenFromEeprom(int offset){
  for(int i = offset; i<BOT_TOKEN_LENGTH; i++ ){
    botToken[i] = EEPROM.read(i);
  }
  EEPROM.commit();
}

void writeBotTokenToEeprom(int offset){
  for(int i = offset; i<BOT_TOKEN_LENGTH; i++ ){
    EEPROM.write(i, botToken[i]);
  }
  EEPROM.commit();
}


/********************************** bot chat  *****************************************/


void handleNewMessages(int numNewMessages) {
  Serial.println("handleNewMessages");
  Serial.println(String(numNewMessages));
  for(int i=0; i<numNewMessages; i++) {
    String chat_id = String(bot->messages[i].chat_id);
    String text = bot->messages[i].text;
    text.toLowerCase();

    if (text.startsWith("/hallo")||text.startsWith("/sagauswahl")) {
      String msg = "Worauf ich so höre:\n";
      msg += "/sagWerte : nennt aktuelle Meßwerte\n";
      msg += "/zeigLuise : zeigt Luise auf display \n";
      msg += "/zeigAnton : zeigt Anton auf display \n";
      msg += "/zeigAbend : zeigt Bild auf display \n";
      msg += "/zeigMsg   : zeigt Kurztext auf display \n";
      msg += "/zeigCountDown n : zeigt Countdown\n";
      msg += "/clearCountDown   : stoppt Countdown\n";
      
      msg += "/sagMenu   : nennt weitere Optionen\n";
      bot->sendMessage(chat_id, msg, "Markdown");
    }

    else if (text.startsWith("/sagmenu")) {
      String msg = "Mehr Aufrufe für BibbiBot:\n";
      msg += "/sagNetz : listet Verbindungsdaten\n";
      msg += "/sagId : nennt die effektive chatId\n";
      msg += "/sagMittel : nennt Mittelwerte\n";
      msg += "/sagStatus : Selbstauskunft\n";
      msg += "/setzModus : check Kälte oder Dampf?\n";
      msg += "/setzDebug : debug-Msg senden?\n";
      msg += "/sagAuswahl: nennt weitere Optionen\n";
      bot->sendMessage(chat_id, msg, "Markdown");
    }
        
    else if (text.startsWith("/sagid")) {
      String msg="deine id ist: "+String(chat_id);
      bot->sendMessage(chat_id, msg, "");
    }
    else if (text.startsWith("/sagwerte")) {
      String msg="die aktuellen Messwerte sind: \n";
      msg += "Temperatur:  "+String(temperature)+" °C\n";
      msg += "Luftfeuchte: "+String(humidity)+" %\n";
      msg += "r.Luftdruck: "+String(rPressure)+" hP\n";
      bot->sendMessage(chat_id, msg, "");
    }    
    else if (text.startsWith("/sagmittel")) {
      String msg="kurz- und langfristige Mittelwerte sind: \n";
      msg += "kurzfr.:   "+String(avgShort)+" °C\n";
      msg += "langfr.:   "+String(avgLong) +" °C\n";
      msg += "Differenz: "+String(avgLong-avgShort) +" °C\n"; // cntMeasures  "Differenz: "+
      msg += String(cntMeasures) +" Messungen\n";
      msg += "Trigger 1: "+String(triggerList[0])+"\n";
      msg += "Trigger 2: "+String(triggerList[1])+"\n";
      msg += "Trigger 3: "+String(triggerList[2])+"\n";
      bot->sendMessage(chat_id, msg, "");
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
      msg += "Gruppe für Alerts: "+ alarmGroupName+" \n";
      msg += "Gruppe für debug: "+  debugGroupName+" \n";
      //bot.sendMessage(update.message.chat_id, str(update.message.from_user.username)) 
      bot->sendMessage(chat_id, msg, "");
    }    

   else if (text.startsWith("/zeigcountdown")) {
      String msg;
      int amount;     
      // wie lange?
      String tmp = text.substring(14);
      //debug ("|"+tmp+"|",false);

      tmp.trim();
      amount = (int) tmp.toInt(); //tmp,toFloat();
      //debug (String(amount),false);

      if (amount > 0){
               // feedback geben
        msg="die Zeit läuft... \n";
      } else {
        msg="Countdown von was? Ich nehm' mal 3... \n";
        amount = 3;
      }
      
      bot->sendMessage(chat_id, msg, "");
 
      // countDown setzen
      countDown=amount;
      
      // stopUhr stellen
      time_3 = millis();
      
      // ersten Schritt zeigen
      showCountDownPage();
        
 
            
   }  
   else if (text.startsWith("/clearcountdown")) {
    countDown=-1;
    String msg="na gut! \n";
    bot->sendMessage(chat_id, msg, "");   
   }



    else if (text == "/setzdebug") {  
      sendDebug = !sendDebug;
      String msg="mein Status ist: \n";
      msg += "debug: "+  String(sendDebug)+" \n";    
      bot->sendMessage(chat_id, msg, "");
      bot->sendMessage(chat_id_debug, msg, "");
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


/********************************** sensor  *****************************************/
/*
String chat_id_debug_default = "-372509778";
String chat_id_alarm_default = "-356847558";

String chat_id_debug = "";
String chat_id_alarm = "";
 */
void debug(String msg,boolean toSerial){

  if (chat_id_debug == ""){ chat_id_debug = chat_id_debug_default;}

  // chat away
  bot->sendMessage(chat_id_debug, msg, "");

  if (toSerial){
    Serial << msg << "\n";
  }
}
void alarmLine(String msg){

  // chat away
  bot->sendMessage(chat_id_alarm, msg, "");

}


void doAlarm(){
  String msg;
  if (chat_id_alarm == ""){ chat_id_alarm = chat_id_alarm_default;}
     
  msg = "Alarm #" + String(alarmCount)+"\n";
  msg += String(temperature) + " °C\n\n";
  msg += "Fenster vergessen?\n\n";
  msg += "/sagMittel gibt Details";
  
 // chat away
  bot->sendMessage(chat_id_alarm, msg, "");

  
  
  debug(msg,true);
}




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


/*
 check_humidity
 check_rising
 
 * */

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
  
  // zeitl. Beschraenkung? in r48 5 min

  aveShortList.push(newVal);
  avgShort=aveShortList.mean();

  cntMeasures++;

  // only care for low temps when searching for falling temp
  if (!check_humidity && (temperature > ignoreTemp)){return;}
    
  
    
    String msg= "temp: "+String(temperature)+" humi: "+String(humidity)+" newVal: "+String(newVal)+" avgLong: "+String(avgLong)+" avgShort: "+String(avgShort)+" alarmCount: "+String(alarmCount)+" trigger: "+String(alarmTrigger)+" cnt: "+String(cntMeasures) ;
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



////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////


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


///////////////
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

// automatisch geht es auf kleiner Runde
void loopDisplayStep(void) {
  //if (flagAlert){ return; }
  
  if (skip1Loop>0 ){
    
      skip1Loop -= 1;
      return;
  }

  if (countDown>-1 ){
    
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



void showMsgPage(String alertMsg){
    
  display.clearBuffer();         // clear the internal memory
  display.setFont( u8g2_font_fub11_tf);  // choose a suitable font

  // TBD:
  // pruefe Laenge der msg und teile ggf in 2 oder gar drei
  
  strBuffer.start() << alertMsg;
  display.drawStr (3, 32, strBuffer);  
  
 
  display.sendBuffer();
}

void showCountDownPage(){
  String value;
  display.clearBuffer();         // clear the internal memory
  display.setFont( u8g2_font_logisoso32_tf);  // choose a suitable font

  if (countDown>0){
    value = String(countDown);
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
