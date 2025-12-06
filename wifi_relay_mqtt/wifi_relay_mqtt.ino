/* WiFi STA, web server, Telegram bot and MQTT */

#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
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
String IP = "";
Ticker timer;

WiFiClientSecure client_mqtt;
PubSubClient mqtt(client_mqtt);



// --------------------------
// MQTT
// --------------------------
void reconnectMQTT() {
  while (!mqtt.connected()) {
    if (mqtt.connect("NodeMCU_Client", MQTT_USER, MQTT_PASSWORD)) {
      Serial.println("[MQTT] Connected. ✔");
      mqtt.subscribe("esp8266/cmd");
      publishStatus();
    } else {
      Serial.print("[MQTT] Failed to connect. ❌");
      delay(t_s*1000);
    }
  }
}

void publishStatus() {
  String msg = "{";
  msg += "\"relay1\":" +String(digitalRead(RELAY_1) == LOW ? 1 : 0) +",";
  msg += "\"relay2\":" +String(digitalRead(RELAY_2) == LOW ? 1 : 0) +",";
  msg += "\"t1_on\":\""  +formatTime(timer1_on_target)  +"\",";
  msg += "\"t2_on\":\""  +formatTime(timer2_on_target)  +"\",";
  msg += "\"t1_off\":\"" +formatTime(timer1_off_target) +"\",";
  msg += "\"t2_off\":\"" +formatTime(timer2_off_target) +"\"";
  msg += "}";
  mqtt.publish("esp8266/status", msg.c_str());
}

void check_mqtt(char* topic, byte* payload, unsigned int length) {
  String cmd;
  for (unsigned int i=0; i<length; i++) cmd += (char)payload[i];
  handleCommand(cmd);  
}

bool parseHHMM(const String &s, uint32_t &outSec) {
  if (s.length() != 5) return false;
  if (s.charAt(2) != ':') return false;

  int hh = (s.charAt(0)-'0')*10 +(s.charAt(1)-'0');
  int mm = (s.charAt(3)-'0')*10 +(s.charAt(4)-'0');
  if (hh < 0 || hh > 23 || mm < 0 || mm > 59) return false;

  outSec = hh*3600 + mm*60;
  return true;
}

void handleCommand(String cmd) {
  cmd.trim();   
  
  // ====== MAIN CONTROL ======
  if (cmd == "POWER ON 1")  activateRelay1();
  if (cmd == "POWER ON 2")  activateRelay2();
  if (cmd == "POWER OFF 1") disableRelay1();
  if (cmd == "POWER OFF 2") disableRelay2();
  if (cmd == "STATUS") statusRelays();

  // ====== STOP TIMERS ======
  if (cmd == "STOP T1 ON")  stopTimer(timer1_on_active, timer1_on, "Timer 1 ON stopped");
  if (cmd == "STOP T1 OFF") stopTimer(timer1_off_active, timer1_off, "Timer 1 OFF stopped");
  if (cmd == "STOP T2 ON")  stopTimer(timer2_on_active, timer2_on, "Timer 2 ON stopped");
  if (cmd == "STOP T2 OFF") stopTimer(timer2_off_active, timer2_off, "Timer 2 OFF stopped");

  // ====== SET TIMERS ======
  // SET T1 ON 00:00
  if (cmd.startsWith("SET ")) {
    int p1 = cmd.indexOf(' ');
    int p2 = cmd.indexOf(' ', p1+1);
    int p3 = cmd.indexOf(' ', p2+1);

    String timer = cmd.substring(p1+1, p2);   // T1 / T2
    String mode  = cmd.substring(p2+1, p3);   // ON / OFF
    String hm    = cmd.substring(p3+1);       // hh:mm

    uint32_t sec;
    if (!parseHHMM(hm, sec)) return;
    if (timer=="T1" && mode=="ON")  setTimer(timer1_on_active, timer1_on, timer1_on_target, sec, activateRelay1);
    if (timer=="T1" && mode=="OFF") setTimer(timer1_off_active, timer1_off, timer1_off_target, sec, disableRelay1);
    if (timer=="T2" && mode=="ON")  setTimer(timer2_on_active, timer2_on, timer2_on_target, sec, activateRelay2);
    if (timer=="T2" && mode=="OFF") setTimer(timer2_off_active, timer2_off, timer2_off_target, sec, disableRelay2);
  }
  publishStatus();
}



// --------------------------
// Timers
// --------------------------
void setTimer(bool &active, uint32_t &timer_sec, uint32_t &target, uint32_t amount_sec, void (*action)()) {
  timer_sec = amount_sec;
  target    = amount_sec;
  active = true;
  saveTargets();
}

void stopTimer(bool &active, uint32_t &timer_sec, const char* msg) {
  timer_sec = 0;
  active    = false;
  saveTargets();
}

void checkTimer(bool &active, uint32_t &timer_sec, uint32_t &target, void (*action)(), const char* msg) {
  if (!active) return;
  
  if (clock_sec == target) {
    action();
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



// --------------------------
// Relays
// --------------------------
void activateRelay1() {
  //if (digitalRead(RELAY_1) == LOW) return;
  digitalWrite(RELAY_1, LOW);
}

void activateRelay2() {
  //if (digitalRead(RELAY_2) == LOW) return;
  digitalWrite(RELAY_2, LOW);
}

void disableRelay1() {
  //if (digitalRead(RELAY_1) == HIGH) return;
  digitalWrite(RELAY_1, HIGH);
}

void disableRelay2() {
  //if (digitalRead(RELAY_2) == HIGH) return;
  digitalWrite(RELAY_2, HIGH);
}

void statusRelays() {
  String msg = "Relay 1 " +String(digitalRead(RELAY_1) == LOW ? "ON" : "OFF") +" | Timer ON at " +formatTime(timer1_on_target) +" | Timer OFF at "+formatTime(timer1_off_target);
  msg     += "\nRelay 2 " +String(digitalRead(RELAY_2) == LOW ? "ON" : "OFF") +" | Timer ON at " +formatTime(timer2_on_target) +" | Timer OFF at "+formatTime(timer2_off_target);
}



// --------------------------
// Clock and EEPROM
// --------------------------
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

void ICACHE_RAM_ATTR Timer_ISR() {
  flag_i = true;

  clock_div++;
  if (clock_div >= 10){
    clock_div = 0;
    clock_sec ++;
  }
  if(clock_sec >= 86400) clock_sec = 0;
}

void saveTargets() {
  EEPROM.put(0,  timer1_on_target);
  EEPROM.put(4,  timer2_on_target);
  EEPROM.put(8,  timer1_off_target);
  EEPROM.put(12, timer2_off_target);
  EEPROM.put(16, timer1_on_active);
  EEPROM.put(17, timer2_on_active);
  EEPROM.put(18, timer1_off_active);
  EEPROM.put(19, timer2_off_active);
  EEPROM.commit();
}

void loadTargets() {
  EEPROM.get(0,  timer1_on_target);
  EEPROM.get(4,  timer2_on_target);
  EEPROM.get(8,  timer1_off_target);
  EEPROM.get(12, timer2_off_target);
  EEPROM.get(16, timer1_on_active);
  EEPROM.get(17, timer2_on_active);
  EEPROM.get(18, timer1_off_active);
  EEPROM.get(19, timer2_off_active);
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
  
  // MQTT client secure
  client_mqtt.setInsecure();
  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  mqtt.setCallback(check_mqtt);

  // NTP time
  syncClock();
}

void loop() {
  mqtt.loop();
  
  if (flag_i) {
    flag_i = false;
    count ++;
  
    if (!mqtt.connected()) reconnectMQTT();
    if (count >= 5){
      count = 0;
      //Serial.print("[TICK] clock_sec = ");
      //Serial.println(clock_sec);

      // ===== CHECK TIMERS =====
      checkTimer(timer1_off_active, timer1_off, timer1_off_target, disableRelay1, "Timer 1 OFF finished");
      checkTimer(timer2_off_active, timer2_off, timer2_off_target, disableRelay2, "Timer 2 OFF finished");
      checkTimer(timer1_on_active,  timer1_on,  timer1_on_target,  activateRelay1, "Timer 1 ON finished");
      checkTimer(timer2_on_active,  timer2_on,  timer2_on_target,  activateRelay2, "Timer 2 ON finished");
    }
  }
}