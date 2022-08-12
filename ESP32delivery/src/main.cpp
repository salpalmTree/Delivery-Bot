#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include "SPIFFS.h"
#include <SoftwareSerial.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <SPI.h>
#include <Adafruit_I2CDevice.h>
#include <Wire.h>

#define LED1 2
#define RX 16
#define TX 17
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET); 


SoftwareSerial mySer; 

String message = ""; //message received from server
String romiMessage = ""; //message to be sent to romi
int theNumber = 9; 

const char* ssid = "SSID"; 
const char* password = "PASSWORD"; 

const char* ssid1 = "SSID2"; 
const char* password1 = "PASSWORD2"; 

const char* ssid2 = "SSID3"; 
const char* password2 = "PASSWORD3"; 

AsyncWebServer server(80); 
AsyncWebSocket ws("/ws"); 

String numRoutes;
//String thestate = "notcalib"; 
String routes; 

bool newRequest = false; 


//SPIFFS init
void initSPIFFS()
{
  if(!SPIFFS.begin(true))
  {
    Serial.println("Error occured on the SPIFFS"); 
  }
  else
  {
    Serial.println("SPIFFS mounted Successfully"); 
  }
}

//Wifi init
void initWiFi()
{
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password); 
  Serial.println("Connecting to Wifi..."); 
  while(WiFi.status() != WL_CONNECTED)
  {
    Serial.print("-"); 
    delay(1000); 
  }
  Serial.println(WiFi.localIP());
  digitalWrite(LED1, HIGH); 
}

void notifyClients(String state)
{
  ws.textAll(state); 
}

void handleWebSocketMessage(void *arg, uint8_t *data, size_t len)
{
  AwsFrameInfo *info = (AwsFrameInfo*)arg;
  if(info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT)
  {
    data[len] = 0; 
    message = (char*)data; 
    numRoutes = message.substring(0, message.indexOf("$")); 
    routes = message.substring(message.indexOf("$")+1, message.length()); 
    Serial.print("number of routes: "); 
    Serial.println(numRoutes); 
    Serial.print("routes: "); 
    Serial.println(routes); 
    newRequest = true; 
  }
}

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len)
{
  switch (type)
  {
    case WS_EVT_CONNECT:
      Serial.printf("Client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str()); 
      //notifyClients(numRoutes); 
      break; 
    case WS_EVT_DISCONNECT:
      Serial.printf("Client #%u disconnected\n", client->id());
      break; 
    case WS_EVT_DATA:
      handleWebSocketMessage(arg, data, len); 
      break; 
    case WS_EVT_PONG:
    case WS_EVT_ERROR:
      break; 
  }
}

void initWebSocket()
{
  ws.onEvent(onEvent); 
  server.addHandler(&ws); 
}
//function to handle uart messages
void handleUart()
{
  while(mySer.available() > 0)
  {
    static char umessage[5]; 
    static uint8_t pos = 0; 
    char byte = mySer.read(); 
    if(byte != '\n')
    {
      umessage[pos] = byte; 
      pos++; 
    }
    else
    {
      umessage[pos] = '\0'; 
      pos = 0;  
      Serial.println(umessage); 
      if(umessage[0] == '0')
      {
        notifyClients("notcalib");
      }
      else if(umessage[0] == '1') //Ready
      {
        notifyClients("calib"); 
      }
      else if(umessage[0] == '3')
      {
        notifyClients("path"); 
      }
      else if(umessage[0] == '4')
      {
        notifyClients("delivery"); 
      }
      else if(umessage[0] == '5')
      {
        notifyClients("pick"); 
      }
      else if(umessage[0] == '6')
      {
        notifyClients("drop"); 
      }
    }
  }
}

void setup() 
{
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS))
  {
    Serial.println("SSD1306 allocation failed"); 
    for(;;); 
  }
  delay(2000); 
  display.clearDisplay();
  display.setTextSize(1); 
  display.setTextColor(WHITE); 
  display.setCursor(0, 16); 
  display.println("Wubba lubba dub dub!");   
  display.display(); 
  display.startscrollleft(0x00, 0x0F);
  delay(1000); 
  display.stopscroll(); 


  pinMode(LED1, OUTPUT); 
  digitalWrite(LED1, LOW); 

  Serial.begin(115200); 
  mySer.begin(14400, SWSERIAL_8N1, RX, TX);
  mySer.enableIntTx(false); 

  initSPIFFS(); 
  initWiFi();

  display.clearDisplay(); 
  display.setCursor(0, 16); 
  display.print("IP address is...");
  display.display();  
  delay(2500); 
  display.clearDisplay(); 
  display.setCursor(30, 16);   
  display.display(); 
  display.println(WiFi.localIP()); 
  display.display();   
  initWebSocket();   
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){request->send(SPIFFS, "/myindex.html", "text/html");}); 
  server.serveStatic("/", SPIFFS, "/"); 
  server.begin(); 
}

void loop() 
{ 
  handleUart();
  if(newRequest)
  {
    //send info to ROMI
    char number = numRoutes.charAt(0);
    romiMessage = routes; 
    Serial.println(number + romiMessage); 
    mySer.println(number + romiMessage); 
  }
  newRequest = false; 


  ws.cleanupClients(); 
}