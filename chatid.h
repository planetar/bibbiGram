/*
 *  Once you've created your telegram bot and copied it's token 
 *  proceed to create 2 groups, aptly named (someGroupName)_alarm and _debug
 *  the _alarm group and enter their respective chat_id here.
 *  the bot expects to have them available and will send messages with debug info or alarms to 
 *  these chat_id
 * 
 *  The names of these groups are stored for informational purposes only. 
 *  The bot uses those data only to tell users which groups to look for
 *  The separation into _default  and actually used chat_id_XY is in preparation of a 
 *  planned feature to let users change the groups on the fly
 * 
 */

String chat_id_debug_default = "-3db809778";
String chat_id_alarm_default = "-32b847558";

String alarmGroupName = "botgroup_alarm";
String debugGroupName = "botgroup_debug";

String chat_id_debug = chat_id_debug_default;
String chat_id_alarm = chat_id_alarm_default;
