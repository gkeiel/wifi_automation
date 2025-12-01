/* Create a WiFi access point (AP) and web server. */
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include "secrets.h"

#ifndef APSSID
#define APSSID "NodeMCU"
#define APPSK "aaabbb111"
#endif

#define RELAY_1 D5
#define RELAY_2 D6

/* Credentials from secret.h */
ESP8266WebServer server(80);

/* http://192.168.4.1 in a web browser connected to this AP to see it. */
void handleRoot() {
  String html = "<html><body style='font-family:Arial;text-align:center;'>";
  html += "<h2>Controle WiFi NodeMCU</h2>";
  html += "<p><a href='/ligar_1'><button style='font-size:20px;padding:10px 20px;'>Ligar 1</button></a></p>";
  html += "<p><a href='/ligar_2'><button style='font-size:20px;padding:10px 20px;'>Ligar 2</button></a></p>";
  html += "<p><a href='/desligar'><button style='font-size:20px;padding:10px 20px;'>Desligar</button></a></p>";
  html += "<p>Estado do rele 1: " + String(digitalRead(RELAY_1)) + "</p>";
  html += "<p>Estado do rele 2: " + String(digitalRead(RELAY_2)) + "</p>";
  html += "</body></html>";
  server.send(200, "text/html", html);
}

void handleLigar_1() {
  digitalWrite(RELAY_1, HIGH);
  digitalWrite(RELAY_2, LOW);
  handleRoot();  // mostra a página atualizada
}

void handleLigar_2() {
  digitalWrite(RELAY_1, LOW);
  digitalWrite(RELAY_2, HIGH);
  handleRoot();  // mostra a página atualizada
}

void handleDesligar() {
  digitalWrite(RELAY_1, LOW);
  digitalWrite(RELAY_2, LOW);
  handleRoot();  // mostra a página atualizada
}

void setup() {
  delay(1000);
  Serial.begin(115200);
  Serial.println();
  Serial.print("Configuring access point...");

	WiFi.mode(WIFI_OFF);
	delay(100);
	WiFi.mode(WIFI_AP);
	delay(100);
  WiFi.softAP(AP_SSID, AP_PASSWORD, 1, false, 2);

  IPAddress myIP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(myIP);

	pinMode(RELAY_1, OUTPUT);
  pinMode(RELAY_2, OUTPUT);
  digitalWrite(RELAY_1, LOW);
  digitalWrite(RELAY_2, LOW);

  server.on("/", handleRoot);
  server.on("/ligar_1", handleLigar_1);
  server.on("/ligar_2", handleLigar_2);
  server.on("/desligar", handleDesligar);
	
	server.begin();
  Serial.println("HTTP server started");
}

void loop() {
  server.handleClient();
}