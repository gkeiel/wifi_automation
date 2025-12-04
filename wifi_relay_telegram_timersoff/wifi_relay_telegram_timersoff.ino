/* WiFi STA Telegram */

#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <Ticker.h>
#include "secrets.h"
#define RELAY_1 D5
#define RELAY_2 D6

volatile bool flag_i = false;
volatile bool flag_ip = false;
bool relay1 = true;
bool relay2 = true;
bool timer1_off_active = false;
bool timer2_off_active = false;
bool timer_update = false;
uint32_t timer1_off = 0;
uint32_t timer2_off = 0;
uint32_t timer1_off_end = 0;
uint32_t timer2_off_end = 0;
uint32_t next_timer_update = 0;
volatile uint16_t count = 0;
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
  String keyboard ="[[\"POWER ON 1\",\"POWER ON 2\"],[\"TIMER 1 OFF\",\"TIMER 2 OFF\"],[\"POWER OFF 1\",\"POWER OFF 2\"],[\"STATUS\"]]";
  bot.sendMessageWithReplyKeyboard(chat_id, "Control panel loaded.", "", keyboard, true, false, false);
}

void send_Timer1OffKeyboard() {
  String kb = "[[\"+1 MIN (T1 OFF)\",\"+10 MIN (T1 OFF)\"],[\"+1 HOUR (T1 OFF)\",\"TURN OFF (T1 OFF)\"],[\"BACK\"]]";
  bot.sendMessageWithReplyKeyboard(chat_id, "Timer 1 panel.", "", kb, true, false, false);
}

void send_Timer2OffKeyboard() {
  String kb = "[[\"+1 MIN (T2 OFF)\",\"+10 MIN (T2 OFF)\"],[\"+1 HOUR (T2 OFF)\",\"TURN OFF (T2 OFF)\"],[\"BACK\"]]";
  bot.sendMessageWithReplyKeyboard(chat_id, "Timer 2 panel.", "", kb, true, false, false);
}

void addTimer(bool &active, uint32_t &timer_var, uint32_t &timer_end, uint32_t amount, void (*action)()) {
  if(!active) timer_var = amount;
  else       timer_var += amount;

  timer_end = millis() +timer_var;
  active = true;
  action();

  timer_update = true;
  next_timer_update = millis() +5000;
  //bot.sendMessage(chat_id, "Timer OFF remaining " +formatTime(timer_end), "");
}

void stopTimer(bool &active, uint32_t &timer_ms, const char* msg) {
  active = false;
  timer_ms = 0;
  bot.sendMessage(chat_id, msg, "");
}

void checkTimer(bool &active, uint32_t &timer_ms, uint32_t &end, void (*action)(), const char* msg) {
  if (!active){
    timer_ms = 0;
    end      = 0;
    return;
  }
  
  if (millis() >= end) {
    active = false;
    timer_ms = 0;
    action();
    bot.sendMessage(chat_id, msg, "");
  }
}

String formatTime(uint32_t timer_end) {
  if (timer_end == 0 || millis() >= timer_end) return "00:00";
  uint32_t remaining_ms = timer_end -millis();
  uint32_t minutes      = remaining_ms/60000;
  uint32_t seconds      = (remaining_ms %60000)/1000;
  return String(minutes) + ":" + (seconds < 10 ? "0" : "") +String(seconds);
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
    if (telegram_data == "BACK") send_ReplyKeyboard();

    // ====== TIMER 1 OFF ======
    if (telegram_data == "+1 MIN (T1 OFF)")  addTimer(timer1_off_active, timer1_off, timer1_off_end, 60UL*1000UL, activateRelay1);
    if (telegram_data == "+10 MIN (T1 OFF)") addTimer(timer1_off_active, timer1_off, timer1_off_end, 10UL*60UL*1000UL, activateRelay1);
    if (telegram_data == "+1 HOUR (T1 OFF)") addTimer(timer1_off_active, timer1_off, timer1_off_end, 60UL*60UL*1000UL, activateRelay1);
    if (telegram_data == "TURN OFF (T1 OFF)") stopTimer(timer1_off_active, timer1_off, "Timer 1 OFF stopped.");

    // ====== TIMER 2 OFF ======
    if (telegram_data == "+1 MIN (T2 OFF)")  addTimer(timer2_off_active, timer2_off, timer2_off_end, 60UL*1000UL, activateRelay2);
    if (telegram_data == "+10 MIN (T2 OFF)") addTimer(timer2_off_active, timer2_off, timer2_off_end, 10UL*60UL*1000UL, activateRelay2);
    if (telegram_data == "+1 HOUR (T2 OFF)") addTimer(timer2_off_active, timer2_off, timer2_off_end, 60UL*60UL*1000UL, activateRelay2);
    if (telegram_data == "TURN OFF (T2 OFF)") stopTimer(timer2_off_active, timer2_off, "Timer 2 OFF stopped.");
  }
  
  // ===== CHECK TIMERS =====
  checkTimer(timer1_off_active, timer1_off, timer1_off_end, disableRelay1, "Timer 1 OFF finished.");
  checkTimer(timer2_off_active, timer2_off, timer2_off_end, disableRelay2, "Timer 2 OFF finished.");
}



// --------------------------
// Relays
// --------------------------
void activateRelay1() {
  if (digitalRead(RELAY_1) == LOW) return;
  digitalWrite(RELAY_1, LOW);
  relay1 = false;
  bot.sendMessage(chat_id, "Relay 1 ON", "");
}

void activateRelay2() {
  if (digitalRead(RELAY_2) == LOW) return;
  digitalWrite(RELAY_2, LOW);
  relay2 = false;
  bot.sendMessage(chat_id, "Relay 2 ON", "");
}

void disableRelay1() {
  digitalWrite(RELAY_1, HIGH);
  relay1 = true;
  bot.sendMessage(chat_id, "Relay 1 OFF", "");
}

void disableRelay2() {
  digitalWrite(RELAY_2, HIGH);
  relay2 = true;
  bot.sendMessage(chat_id, "Relay 2 OFF", "");
}

void statusRelays() {
  String msg = "Relay 1 " +String(digitalRead(RELAY_1) == LOW ? "ON" : "OFF") +" | Timer OFF remaining " +formatTime(timer1_off_end);
  msg     += "\nRelay 2 " +String(digitalRead(RELAY_2) == LOW ? "ON" : "OFF") +" | Timer OFF remaining " +formatTime(timer2_off_end);
  bot.sendMessage(chat_id, msg, "");
}

void ICACHE_RAM_ATTR Timer_ISR() {
  flag_i = true;
}



// --------------------------
// Setup and Main
// --------------------------
void setup() {
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
}

void loop() {
  if (flag_i) {
    flag_i = false;
    count ++;

    if (!client_telegram.connected()) client_telegram.connect("api.telegram.org", 443);

    if (count >= 5){
      // Telegram
      count = 0;

      if (timer_update && millis() >= next_timer_update) {
        timer_update = false;
        statusRelays();
      }
      
      check_telegram();
    }
  }
}