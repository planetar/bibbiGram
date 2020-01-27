# development targets
a list of desiderata

## Telegram is a moving target (?!)
- ~~looks like the format and length of group - chatid s in telegram have changed on 2020/01/18. What used to look like -387654321 is now like -1001234567890. 4 more chars to save and restore to/from 'Eeprom'. 
Advancing an account to admin changes the type of group of  the telegram group. It get's a different id then. 

## UI and feedback at the very first start
- ~~when setting up a _bot in a new environment (new wifi) it is somewhat irritating that there is no initial visual helloWorld on the display. Me myself thought the thing was broken.~~
fixed with 0.07.2
- ~~a related issue: after entering the wifi config data and botToken and Save the thing needs a reboot. But it doesnt say so. It really should.~~
the readme.md says it now

## i8n and de - personalisation
need to replace /zeigAnton, /zeigAbend sort of private entries. This thing has been designed as a birthday present and there will always be a 'private edition' but I need to hide that even better. And other languages...
- Currently, commands, menu entries, feedback phrases, report wordings  and /sagInfo help texts are hardcoded. 
- I need something that stores phrases separate from code, 

adresses phrases by 
- short 
- flexible 
- symbolic
- paths

has
- ISO language codes ( de || en || es || fr || ru || zh ) at the root
- has a style indicator close to the root ( private || colloquial || formal )
- knows how to default when a selected language / style is not available

This needs research, input appreciated

## ideas & nice to have
### display
- emojis on display?
- utf8 on display?
- pics, gis, stickers anything graphic from telegram to the display?
- just a click for sound

### connex
- Alexa 
- Go / home 
- ITTT

### add your own...

