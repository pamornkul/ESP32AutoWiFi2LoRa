/*
 *  esp32
 *  Lora 433
 *  autoconnect  Wifi
 *  Server buttons
 * V1.0.0 DEC 10, 2023 pakapong : append response switch from Lara to home page  
 */
#include "WiFi.h"
#include <WebServer.h>
#include <SPI.h>              
#include <LoRa.h>
#include <AutoConnect.h>
/*  ==========begin for ESP32=========*/
#define nss 5   //  nss 5
#define rst 14  //   reset 14
#define dio0 2 //  d0  interup recieve pin 2 
/*  ==========end for ESP8266=========*/

#define RELAY_NO    true
#define NUM_RELAYS  2  // Set number of relays

// Assign each GPIO to a relay
int relayGPIOs[NUM_RELAYS] = {12, 13 }; //{2, 0 , 14, 12, 13};
WebServer Server;
AutoConnect       Portal(Server);
AutoConnectConfig Config;

const char* PARAM_INPUT_1 = "relay";  
const char* PARAM_INPUT_2 = "state";
String LEDstatus = "0";

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    html {font-family: Arial; display: inline-block; text-align: center;}
    h2 {font-size: 3.0rem;}
    p {font-size: 3.0rem;}
    body {max-width: 600px; margin:0px auto; padding-bottom: 25px;}
    .switch {position: relative; display: inline-block; width: 120px; height: 68px} 
    .switch input {display: none}
    .slider {position: absolute; top: 0; left: 0; right: 0; bottom: 0; background-color: #ccc; border-radius: 34px}
    .slider:before {position: absolute; content: ""; height: 52px; width: 52px; left: 8px; bottom: 8px; background-color: #fff; -webkit-transition: .4s; transition: .4s; border-radius: 68px}
    input:checked+.slider {background-color: #2196F3}
    input:checked+.slider:before {-webkit-transform: translateX(52px); -ms-transform: translateX(52px); transform: translateX(52px)}
  </style>
</head>
<body>
  <h2>ESP Web Server BB Control AA</h2>
  %BUTTONPLACEHOLDER%
<script>function toggleCheckbox(element) {
  var xhr = new XMLHttpRequest();
  if(element.checked){ xhr.open("GET", "/update?relay="+element.id+"&state=1", true); }
  else { xhr.open("GET", "/update?relay="+element.id+"&state=0", true); }
  xhr.send();
}</script>
 <p>
  %TEXT%
</body>
</html>
)rawliteral";

// Replaces placeholder with button section in your web page
String processor1(const String& var){
  //Serial.println(var);
  if(var == "BUTTONPLACEHOLDER"){
    String buttons ="";
    for(int i=1; i<=NUM_RELAYS; i++){
      String relayStateValue = relayState(i);
      buttons+= "<h4>Relay #" + String(i) + " - GPIO " + relayGPIOs[i-1] + "</h4><label class=\"switch\"><input type=\"checkbox\" onchange=\"toggleCheckbox(this)\" id=\"" + String(i) + "\" "+ relayStateValue +"><span class=\"slider\"></span></label>";
    }
    return buttons;
  }
  return String();
}
String processor2(const String& var){
  if(var == "TEXT"){
    String Pump ="";
      Pump+= "<h4>Relay :" + String(LEDstatus) + "</h4>";
    return Pump;
  }
  return String();
}
String relayState(int numRelay){
  if(RELAY_NO){
    if(digitalRead(relayGPIOs[numRelay-1])){
      return "0";
    }
    else {
      return "1";
    }
  }
  else {
    if(digitalRead(relayGPIOs[numRelay-1])){
      return "1";
    }
    else {
      return "0";
    }
  }
  return "";
}
void rootPage() {
   String content = String(index_html); 
   content.replace("%BUTTONPLACEHOLDER%", String(processor1("BUTTONPLACEHOLDER")));
   content.replace("%TEXT%", String(processor2("TEXT")));
   Server.send(200, "text/html", content);
}
void sendRedirect(String uri) {
  //WebServerClass& server = Portal.host();  // esp8266
  Server.sendHeader("Location", uri, true);
  Server.send(302, "text/plain", "");
  Server.client().stop();
}
void updatepage() {
   //WebServerClass& server = Portal.host();// esp8266
    String inputMessage;
    String inputParam;
    String inputMessage2;
    inputMessage = Server.arg("relay");
    inputMessage2 = Server.arg("state");

    if (Server.arg("relay")!=0 && Server.arg("state")!=0) {
      if(RELAY_NO){
        digitalWrite(relayGPIOs[inputMessage.toInt()-1], inputMessage2.toInt());
        String message = "pump:"+String(inputMessage.toInt()-1)+":"+String(inputMessage2.toInt());   // send a message
        Serial.println(message);
        sendMessage(message);
      }
      else{
        digitalWrite(relayGPIOs[inputMessage.toInt()-1], !inputMessage2.toInt());
        String message = "pump:"+String(inputMessage.toInt()-1)+":"+String(!inputMessage2.toInt());   // send a message 
        Serial.println(message);
        sendMessage(message);
      }
    }
    else {
      inputMessage = "No message sent";
      inputParam = "none";
    }
  sendRedirect("/");
  LoRa.receive();
}

String outgoing;             
byte msgCount = 0;           
byte localAddress = 0xBB;    
byte destination = 0xAA; 
long lastSendTime = 0;   
int interval = 2000;  

void setup() {
 for(int i=1; i<=NUM_RELAYS; i++){
    pinMode(relayGPIOs[i-1], OUTPUT);
    if(RELAY_NO){
      digitalWrite(relayGPIOs[i-1], HIGH);
    }
    else{
      digitalWrite(relayGPIOs[i-1], LOW);
    }
  }
  Serial.begin(115200);                   // initialize serial
  while (!Serial);
  LoRa.setPins(nss, rst, dio0);
  LoRa.setGain(6); 
  while (!LoRa.begin(433E6)) 
      {
         Serial.println("Starting LoRa failed!");
         delay(500);
      }
  Config.autoReconnect = true;
  Portal.config(Config);
  Server.on("/", rootPage);
  Server.on("/update",updatepage);
  if (Portal.begin()) {
    Serial.println("WiFi connected: " + WiFi.localIP().toString());
    LoRa.onReceive(onReceive);
    LoRa.receive();
    Serial.println("LoRa init succeeded.");
  }
}

void loop() {
  Portal.handleClient();
}

void sendMessage(String outgoing) {
  LoRa.beginPacket();                   // start packet
  LoRa.write(destination);              // add destination address
  LoRa.write(localAddress);             // add sender address
  LoRa.write(msgCount);                 // add message ID
  LoRa.write(outgoing.length());        // add payload length
  LoRa.print(outgoing);                 // add payload
  LoRa.endPacket();                     // finish packet and send it
  msgCount++;                           // increment message ID
}

void onReceive(int packetSize) {
  if (packetSize == 0) return;          // if there's no packet, return
  // read packet header bytes:
  int recipient = LoRa.read();          // recipient address
  byte sender = LoRa.read();            // sender address
  byte incomingMsgId = LoRa.read();     // incoming msg ID
  byte incomingLength = LoRa.read();    // incoming msg length
  String incoming = "";                 // payload of packet

  while (LoRa.available()) {            // can't use readString() in callback, so
    incoming += (char)LoRa.read();      // add bytes one by one
  }

  if (incomingLength != incoming.length()) {   // check length for error
    Serial.println("error: message length does not match length");
    return;                             // skip rest of function
  }
  // if the recipient isn't this device or broadcast,
  if (recipient != localAddress && recipient != 0xFF) {
    Serial.println("This message is not for me.");
    return;                             // skip rest of function
  }

  String RelayNumber=incoming.substring(5);
  String XStatus=RelayNumber.substring(2,7);
    if(XStatus!=LEDstatus ){
      digitalWrite(relayGPIOs[RelayNumber.toInt()],XStatus.toInt()); //paka ok but should send pin to tell 
      LEDstatus=XStatus;
      Server.on("/", rootPage);
      Server.on("/update",updatepage);
    }
}
