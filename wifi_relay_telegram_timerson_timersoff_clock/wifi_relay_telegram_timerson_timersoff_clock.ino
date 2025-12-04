/* WiFi STA Telegram */

#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <Ticker.h>
#include <time.h>
#include <EEPROM.h>
#include "secrets.h"
#define RELAY_1 D5
#define RELAY_2 D6
#define EEPROM_ADDR 0

volatile bool flag_i = false;
volatile bool flag_ip = false;
bool timer1_on_active = false;
bool timer2_on_active = false;
bool timer1_off_active = false;
bool timer2_off_active = false;
volatile uint32_t clock_sec = 0;
uint32_t timer1_on = 0;
uint32_t timer2_on = 0;
uint32_t timer1_off = 0;
uint32_t timer2_off = 0;
uint32_t timer1_off_target = 0;
uint32_t timer2_off_target = 0;
uint32_t timer1_on_target = 0;
uint32_t timer2_on_target = 0;
volatile uint16_t count = 0;
volatile uint8_t clock_div = 0;
uint8_t t_s = 100;
String chat_id  = "";
String IP = "";
Ticker timer;

// Telegram bot client
WiFiClientSecure client_telegram;
UniversalTelegramBot bot(BOT_TOKEN, client_telegram);



// --------------------------
// Telegram
// --------------------------
void send_ReplyKeyboard() {
  String keyboard ="[[\"POWER ON 1\",\"POWER ON 2\"],[\"TIMER 1 ON\",\"TIMER 2 ON\"],[\"TIMER 1 OFF\",\"TIMER 2 OFF\"],[\"POWER OFF 1\",\"POWER OFF 2\"],[\"STATUS\"]]";
  bot.sendMessageWithReplyKeyboard(chat_id, "Control panel", "", keyboard, true, false, false);
}

void send_Timer1OffKeyboard() {
  String kb = "[[\"+1 MIN (T1 OFF)\",\"+10 MIN (T1 OFF)\"],[\"+1 HOUR (T1 OFF)\",\"TURN OFF (T1 OFF)\"],[\"+10 HOURS (T1 OFF)\",\"BACK\"]]";
  bot.sendMessageWithReplyKeyboard(chat_id, "Timer panel", "", kb, true, false, false);
}

void send_Timer2OffKeyboard() {
  String kb = "[[\"+1 MIN (T2 OFF)\",\"+10 MIN (T2 OFF)\"],[\"+1 HOUR (T2 OFF)\",\"TURN OFF (T2 OFF)\"],[\"+10 HOURS (T2 OFF)\",\"BACK\"]]";
  bot.sendMessageWithReplyKeyboard(chat_id, "Timer panel", "", kb, true, false, false);
}

void send_Timer1OnKeyboard() {
  String kb = "[[\"+1 MIN (T1 ON)\",\"+10 MIN (T1 ON)\"],[\"+1 HOUR (T1 ON)\",\"TURN OFF (T1 ON)\"],[\"+10 HOURS (T1 ON)\",\"BACK\"]]";
  bot.sendMessageWithReplyKeyboard(chat_id, "Timer panel", "", kb, true, false, false);
}

void send_Timer2OnKeyboard() {
  String kb = "[[\"+1 MIN (T2 ON)\",\"+10 MIN (T2 ON)\"],[\"+1 HOUR (T2 ON)\",\"TURN OFF (T2 ON)\"],[\"+10 HOURS (T2 ON)\",\"BACK\"]]";
  bot.sendMessageWithReplyKeyboard(chat_id, "Timer panel", "", kb, true, false, false);
}

void addTimer(bool &active, uint32_t &timer_sec, uint32_t &target, uint32_t amount_sec, void (*action)()) {
  if(!active) timer_sec = amount_sec;
  else       timer_sec += amount_sec;
  saveTargets();
  
  target = timer_sec%86400;
  active = true;
  action();
}

void stopTimer(bool &active, uint32_t &timer_sec, const char* msg) {
  active    = false;
  timer_sec = 0;
  bot.sendMessage(chat_id, msg, "");
}

void checkTimer(bool &active, uint32_t &timer_sec, uint32_t &target, void (*action)(), const char* msg) {
  if (!active) return;
  
  if (clock_sec >= target && clock_sec < target+15) {
    action();
    bot.sendMessage(chat_id, msg, "");
  }
}

String formatTime(uint32_t target) {
  if (!target) return "--:--";

  uint32_t h = target/3600;
  uint32_t m = (target%3600)/60;

  char buffer[6];
  sprintf(buffer, "%02u:%02u", h, m);
  return String(buffer);
}

void check_telegram(){
  uint8_t num = bot.getUpdates(bot.last_message_received +1);
  if (num == 0) return;
  
  for (uint8_t i = 0; i < num; i++) {
    chat_id              = bot.messages[i].chat_id;
    String telegram_data = bot.messages[i].text;

    if (!flag_ip) {
      bot.sendMessage(chat_id, "WiFi connected IP " +IP, "");
      flag_ip = true;
    }

    // ====== MAIN KEYBOARD ======
    if (telegram_data == "/start") send_ReplyKeyboard();
    if (telegram_data == "POWER ON 1") activateRelay1();
    if (telegram_data == "POWER ON 2") activateRelay2();
    if (telegram_data == "POWER OFF 1") disableRelay1();
    if (telegram_data == "POWER OFF 2") disableRelay2();
    if (telegram_data == "STATUS") statusRelays();

    // ====== TIMER KEYBOARDS ======
    if (telegram_data == "TIMER 1 OFF") send_Timer1OffKeyboard();
    if (telegram_data == "TIMER 2 OFF") send_Timer2OffKeyboard();
    if (telegram_data == "TIMER 1 ON") send_Timer1OnKeyboard();
    if (telegram_data == "TIMER 2 ON") send_Timer2OnKeyboard();
    if (telegram_data == "BACK") send_ReplyKeyboard();

    // ====== TIMER 1 OFF ======
    if (telegram_data == "+1 MIN (T1 OFF)")  addTimer(timer1_off_active, timer1_off, timer1_off_target, 60, activateRelay1);
    if (telegram_data == "+10 MIN (T1 OFF)") addTimer(timer1_off_active, timer1_off, timer1_off_target, 600, activateRelay1);
    if (telegram_data == "+1 HOUR (T1 OFF)") addTimer(timer1_off_active, timer1_off, timer1_off_target, 3600, activateRelay1);
    if (telegram_data == "+10 HOURS (T1 OFF)") addTimer(timer1_off_active, timer1_off, timer1_off_target, 36000, activateRelay1);
    if (telegram_data == "TURN OFF (T1 OFF)") stopTimer(timer1_off_active, timer1_off, "Timer 1 OFF stopped.");

    // ====== TIMER 2 OFF ======
    if (telegram_data == "+1 MIN (T2 OFF)")  addTimer(timer2_off_active, timer2_off, timer2_off_target, 60, activateRelay2);
    if (telegram_data == "+10 MIN (T2 OFF)") addTimer(timer2_off_active, timer2_off, timer2_off_target, 600, activateRelay2);
    if (telegram_data == "+1 HOUR (T2 OFF)") addTimer(timer2_off_active, timer2_off, timer2_off_target, 3600, activateRelay2);
    if (telegram_data == "+10 HOURS (T2 OFF)") addTimer(timer2_off_active, timer2_off, timer2_off_target, 36000, activateRelay2);
    if (telegram_data == "TURN OFF (T2 OFF)") stopTimer(timer2_off_active, timer2_off, "Timer 2 OFF stopped.");

    // ====== TIMER 1 ON ======
    if (telegram_data == "+1 MIN (T1 ON)") addTimer(timer1_on_active, timer1_on, timer1_on_target, 60, disableRelay1);
    if (telegram_data == "+10 MIN (T1 ON)") addTimer(timer1_on_active, timer1_on, timer1_on_target, 600, disableRelay1);
    if (telegram_data == "+1 HOUR (T1 ON)") addTimer(timer1_on_active, timer1_on, timer1_on_target, 3600, disableRelay1);
    if (telegram_data == "+10 HOURS (T1 ON)") addTimer(timer1_on_active, timer1_on, timer1_on_target, 36000, disableRelay1);
    if (telegram_data == "TURN OFF (T1 ON)") stopTimer(timer1_on_active, timer1_on, "Timer 1 ON stopped");

    // ====== TIMER 2 ON ======
    if (telegram_data == "+1 MIN (T2 ON)") addTimer(timer2_on_active, timer2_on, timer2_on_target, 60, disableRelay2);
    if (telegram_data == "+10 MIN (T2 ON)") addTimer(timer2_on_active, timer2_on, timer2_on_target, 600, disableRelay2);
    if (telegram_data == "+1 HOUR (T2 ON)") addTimer(timer2_on_active, timer2_on, timer2_on_target, 3600, disableRelay2);
    if (telegram_data == "+10 HOURS (T2 ON)") addTimer(timer2_on_active, timer2_on, timer2_on_target, 36000, disableRelay2);
    if (telegram_data == "TURN OFF (T2 ON)") stopTimer(timer2_on_active, timer2_on, "Timer 2 ON stopped");
  }
}



// --------------------------
// Relays
// --------------------------
void activateRelay1() {
  if (digitalRead(RELAY_1) == LOW) return;
  digitalWrite(RELAY_1, LOW);
  bot.sendMessage(chat_id, "Relay 1 ON", "");
}

void activateRelay2() {
  if (digitalRead(RELAY_2) == LOW) return;
  digitalWrite(RELAY_2, LOW);
  bot.sendMessage(chat_id, "Relay 2 ON", "");
}

void disableRelay1() {
  if (digitalRead(RELAY_1) == HIGH) return;
  digitalWrite(RELAY_1, HIGH);
  bot.sendMessage(chat_id, "Relay 1 OFF", "");
}

void disableRelay2() {
  if (digitalRead(RELAY_2) == HIGH) return;
  digitalWrite(RELAY_2, HIGH);
  bot.sendMessage(chat_id, "Relay 2 OFF", "");
}

void statusRelays() {
  String msg = "Relay 1 " +String(digitalRead(RELAY_1) == LOW ? "ON" : "OFF") +" | Timer ON at " +formatTime(timer1_on_target) +" | Timer OFF at "+formatTime(timer1_off_target);
  msg     += "\nRelay 2 " +String(digitalRead(RELAY_2) == LOW ? "ON" : "OFF") +" | Timer ON at " +formatTime(timer2_on_target) +" | Timer OFF at "+formatTime(timer2_off_target);
  bot.sendMessage(chat_id, msg, "");
}

void ICACHE_RAM_ATTR Timer_ISR() {
  flag_i = true;

  clock_div++;
  if (clock_div >= 10){
    clock_div = 0;
    clock_sec ++;
  }
  if(clock_sec >= 86400) clock_sec = 0;
}

void syncClock() {
  // UTC-3
  configTime(-3*3600, 0, "pool.ntp.org");

  time_t now = time(nullptr);
  while (now < 100000) {  
    delay(200);
    now = time(nullptr);
  }

  struct tm*t = localtime(&now);
  clock_sec = t->tm_hour*3600 +t->tm_min*60 +t->tm_sec;
  Serial.println("[Clock] NTP synchronized. ✔");
}

void saveTargets() {
  EEPROM.put(0,  timer1_on_target);
  EEPROM.put(4,  timer2_on_target);
  EEPROM.put(8,  timer1_off_target);
  EEPROM.put(12, timer2_off_target);
  EEPROM.commit();
}

void loadTargets() {
  EEPROM.get(0,  timer1_on_target);
  EEPROM.get(4,  timer2_on_target);
  EEPROM.get(8,  timer1_off_target);
  EEPROM.get(12, timer2_off_target);
}



// --------------------------
// Setup and Main
// --------------------------
void setup() {
  EEPROM.begin(64);
  loadTargets();

  Serial.begin(115200);
  timer.attach_ms(t_s, Timer_ISR);
	
  pinMode(RELAY_1, OUTPUT);
  pinMode(RELAY_2, OUTPUT);
  digitalWrite(RELAY_1, HIGH);
  digitalWrite(RELAY_2, HIGH);

  // connect to WiFi
  WiFi.begin(HOME_SSID, HOME_PASSWORD);
  WiFi.setAutoReconnect(true);
  while (WiFi.status() != WL_CONNECTED){
    delay(1000);
  }
  Serial.println("[WiFi] Connected. ✔");
  IP = WiFi.localIP().toString();

  // Telegram client
  client_telegram.setInsecure();
  client_telegram.setBufferSizes(4096, 4096);
  if (client_telegram.connect("api.telegram.org", 443)) {
    Serial.println("[Telegram] Connected. ✔");
    bot.getUpdates(-1);
    bot.last_message_received = 0;
  } else {
    Serial.println("[Telegram] Failed to connect. ❌");
  }

  // NTP time
  syncClock();
}

void loop() {
  if (flag_i) {
    flag_i = false;
    count ++;

    if (!client_telegram.connected()) client_telegram.connect("api.telegram.org", 443);

    if (count >= 5){
      count = 0;

      // ===== CHECK TIMERS =====
      checkTimer(timer1_off_active, timer1_off, timer1_off_target, disableRelay1, "Timer 1 OFF finished");
      checkTimer(timer2_off_active, timer2_off, timer2_off_target, disableRelay2, "Timer 2 OFF finished");
      checkTimer(timer1_on_active, timer1_on, timer1_on_target, activateRelay1, "Timer 1 ON finished");
      checkTimer(timer2_on_active, timer2_on, timer2_on_target, activateRelay2, "Timer 2 ON finished");

      // ===== CHECK TELEGRAM =====
      check_telegram();
    }
  }
}