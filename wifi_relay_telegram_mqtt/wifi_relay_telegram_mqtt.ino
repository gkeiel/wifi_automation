/* WiFi STA, web server, Telegram bot and MQTT */

#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <ESP8266WebServer.h>
#include <PubSubClient.h>
#include <Ticker.h>
#include <EEPROM.h>
#include "secrets.h"
#define RELAY_1 D5
#define RELAY_2 D6

Ticker timer;
volatile bool flag_i = false;
uint8_t t_s = 5;
unsigned long lastTimeBotRan;

// Telegram bot
String BOTtoken = BOT_TOKEN;
WiFiClientSecure client_telegram;
UniversalTelegramBot bot(BOTtoken, client_telegram);

// MQTT
WiFiClientSecure client_mqtt;
PubSubClient mqtt(client_mqtt);

// web server
ESP8266WebServer server(80);



// --------------------------
// MQTT
// --------------------------
// publish status MQTT
void publishStatus() {
  String msg = "{";
  msg += "\"relay1\":" + String(digitalRead(RELAY_1)) + ",";
  msg += "\"relay2\":" + String(digitalRead(RELAY_2));
  msg += "}";
  mqtt.publish("esp8266/status", msg.c_str());
}

// reconnect to MQTT
void reconnectMQTT() {
  while (!mqtt.connected()) {
    Serial.println("[MQTT] Connecting to MQTT...");
    if (mqtt.connect("NodeMCU_Client", MQTT_USER, MQTT_PASSWORD)) {
      Serial.println("[MQTT] Connected. ✔");
      mqtt.subscribe("esp8266/cmd");
      publishStatus();
    } else {
      Serial.print("[MQTT] Failed to connect. ❌");
      //Serial.println(mqtt.state());
      delay(t_s*1000);
    }
  }
}

// execute MQTT
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String cmd = "";
  for (unsigned int i=0; i<length; i++) cmd += (char)payload[i];

  if (cmd == "activate_1") {
    digitalWrite(RELAY_1, LOW);
    digitalWrite(RELAY_2, HIGH);
  }
  if (cmd == "activate_2") {
    digitalWrite(RELAY_1, HIGH);
    digitalWrite(RELAY_2, LOW);
  }
  if (cmd == "disable") {
    digitalWrite(RELAY_1, HIGH);
    digitalWrite(RELAY_2, HIGH);
  }
  publishStatus();
}



// --------------------------
// Telegram
// --------------------------
void handleCallbackQuery(int i) {
  String query_id = bot.messages[i].query_id;
  String chat_id  = bot.messages[i].chat_id;
  String data     = bot.messages[i].text;

  if (data == "activate_1") {
    digitalWrite(RELAY_1, LOW);
    digitalWrite(RELAY_2, HIGH);
    bot.sendMessage(chat_id, "Relay 1 ON", "");
  }

  if (data == "activate_2") {
    digitalWrite(RELAY_1, HIGH);
    digitalWrite(RELAY_2, LOW);
    bot.sendMessage(chat_id, "Relay 2 ON", "");
  }

  if (data == "disable") {
    digitalWrite(RELAY_1, HIGH);
    digitalWrite(RELAY_2, HIGH);
    bot.sendMessage(chat_id, "Relays OFF", "");
  }

  if (data == "status") {
    String msg = "Relay 1: " + String(digitalRead(RELAY_1)) + "\n";
    msg +=       "Relay 2: " + String(digitalRead(RELAY_2));
    bot.sendMessage(chat_id, msg, "");
  }

  bot.answerCallbackQuery(query_id, "");
}

void check_telegram(){
  Serial.println("[Telegram] Checking new commands...");
  String keyboardJson = "["
  "[{\"text\":\"POWER ON 1\",\"callback_data\":\"activate_1\"},"
  "{\"text\":\"POWER ON 2\",\"callback_data\":\"activate_2\"}],"
  "[{\"text\":\"POWER OFF\",\"callback_data\":\"disable\"}],"
  "[{\"text\":\"STATUS\",\"callback_data\":\"status\"}]"
  "]";

  int num = bot.getUpdates(bot.last_message_received +1);
  if (num == 0) return;
  
  for (int i = 0; i < num; i++) {
    if (bot.messages[i].type == "callback_query"){
      handleCallbackQuery(i);
      continue;
    }

    String chat_id = bot.messages[i].chat_id;
    bot.sendMessageWithInlineKeyboard(chat_id, "Options:", "", keyboardJson);
  }
}



// --------------------------
// Web server
// --------------------------
void handleRoot() {
  String html = "<html><body style='font-family:Arial;text-align:center;'>";
  html += "<h2>Controle WiFi NodeMCU</h2>";
  html += "<p><a href='/activate_1'><button style='font-size:20px;padding:10px 20px;'>Power ON 1</button></a></p>";
  html += "<p><a href='/activate_2'><button style='font-size:20px;padding:10px 20px;'>Power ON 2</button></a></p>";
  html += "<p><a href='/disable'><button style='font-size:20px;padding:10px 20px;'>Power OFF</button></a></p>";
  html += "<p>Relay 1: " + String(digitalRead(RELAY_1)) + "</p>";
  html += "<p>Relay 2: " + String(digitalRead(RELAY_2)) + "</p>";
  html += "</body></html>";
  server.send(200, "text/html", html);
}

void handleON_1() {
  digitalWrite(RELAY_1, LOW);
  digitalWrite(RELAY_2, HIGH);
  publishStatus();
  handleRoot();
}

void handleON_2() {
  digitalWrite(RELAY_1, HIGH);
  digitalWrite(RELAY_2, LOW);
  publishStatus();
  handleRoot();
}

void handleOFF() {
  digitalWrite(RELAY_1, HIGH);
  digitalWrite(RELAY_2, HIGH);
  publishStatus();
  handleRoot();
}


// --------------------------
// Setup and Main
// --------------------------
void ICACHE_RAM_ATTR Timer_ISR() {
  flag_i = true;
}

void setup() {
  Serial.begin(115200);
  timer.attach(t_s, Timer_ISR);
	
  pinMode(RELAY_1, OUTPUT);
  pinMode(RELAY_2, OUTPUT);
  digitalWrite(RELAY_1, HIGH);
  digitalWrite(RELAY_2, HIGH);

  // connect to WiFi
  WiFi.begin(HOME_SSID, HOME_PASSWORD);
  Serial.print("Connecting to home network.");
  while (WiFi.status() != WL_CONNECTED){
    delay(1000);
    Serial.print(".");
  }
  Serial.println("\nConnected.");
  Serial.println(WiFi.localIP());

  // Telegram client
  client_telegram.setInsecure();
  client_telegram.setBufferSizes(4096, 4096);
  
  if (client_telegram.connect("api.telegram.org", 443)) {
    Serial.println("[Telegram] Connected. ✔");
    client_telegram.stop();
    bot.getUpdates(-1);
    bot.last_message_received = 0;
  } else {
    Serial.println("[Telegram] Failed to connect. ❌");
  }
  
  // MQTT client secure
  client_mqtt.setInsecure();
  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  mqtt.setCallback(mqttCallback);

  // create web server
  server.on("/", handleRoot);
  server.on("/activate_1", handleON_1);
  server.on("/activate_2", handleON_2);
  server.on("/disable", handleOFF);
	server.begin();
}

void loop() {

  // MQTT
  if (!mqtt.connected()) reconnectMQTT();
  mqtt.loop();

  // web server
  server.handleClient();

  if (flag_i) {
    flag_i = false;
    
    // Telegram
    check_telegram();
  }
}