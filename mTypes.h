#ifndef mTypes_h
#define mTypes_h


// Types 'byte' und 'word' don't work!
typedef struct {
  char botToken[46];                // Token of Telegram bot
  char nada[1];                     // kludge to isolate botToken from chatId_debug
  char chatId_debug[15];            // telegram group for debug messages
  char chatId_alarm[15];            // telegram group for alarm messages
  int  detectModus;             // "0"=falling , "1"=rising 
  int  detectType;              // "0"=Temp, "1"= Humidity
  int  offsetTenth;             // (int) tempOffset*10
  char passwd[20];

} configData_t;

#endif
