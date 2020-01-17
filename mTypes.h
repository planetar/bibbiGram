#ifndef mTypes_h
#define mTypes_h


// Types 'byte' und 'word' don't work!
typedef struct {
  char botToken[46];                // Token of Telegram bot
  char chatId_debug[11];            // telegram group for debug messages
  char chatId_alarm[11];            // telegram group for alarm messages
  int  detectModus;             // "0"=falling , "1"=rising 
  int  detectType;              // "0"=Temp, "1"= Humidity
  int  offsetTenth;             // (int) tempOffset*10

} configData_t;

#endif
