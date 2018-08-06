/* ESP8266 plus MAX7219 LED Matrix that displays messages revecioved via a WiFi connection using a Web Server
 Provides an automous display of messages
 The MIT License (MIT) Copyright (c) 2017 by David Bird.
 Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, but not sub-license and/or 
 to sell copies of the Software or to permit persons to whom the Software is furnished to do so, subject to the following conditions: 
   The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
See more at http://dsbird.org.uk
*/

//################# LIBRARIES ##########################
#include <TimeLib.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WebServer.h>
#include <WiFiUdp.h>
//#include <WiFiClient.h>
//#include <WiFiManager.h>     // https://github.com/tzapu/WiFiManager
#include <DNSServer.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Max72xxPanel.h>


int pinCS = D4; // Attach CS to this pin, DIN to MOSI and CLK to SCK (cf http://arduino.cc/en/Reference/SPI )
int numberOfHorizontalDisplays = 4;
int numberOfVerticalDisplays   = 1;
char time_value[20];
String message, webpage;
bool msgstatic = false;

// WIFI
const char* ssid = "REPLACE_WITH_YOUR_SSID"; //Enter SSID
const char* password = "REPLACE_WITH_YOUR_WIFI_PWD"; //Enter Password

// NTP
IPAddress timeServer(132, 163, 4, 101); // time-a.timefreq.bldrdoc.gov
// IPAddress timeServer(132, 163, 4, 102); // time-b.timefreq.bldrdoc.gov
// IPAddress timeServer(132, 163, 4, 103); // time-c.timefreq.bldrdoc.gov
// or your local NTP server
int timeZone = 1; 
WiFiUDP Udp;
unsigned int localPort = 8888;  // local port to listen for UDP packets


//################# DISPLAY CONNECTIONS ################
// LED Matrix Pin -> ESP8266 Pin
// Vcc            -> 3v  (3V on NodeMCU 3V3 on WEMOS)
// Gnd            -> Gnd (G on NodeMCU)
// DIN            -> D7  (Same Pin for WEMOS)
// CS             -> D4  (Same Pin for WEMOS)
// CLK            -> D5  (Same Pin for WEMOS)

//################ PROGRAM SETTINGS ####################
String version = "v2.1";       // Version of this program
ESP8266WebServer server(80); // Start server on port 80 (default for a web-browser, change to your requirements, e.g. 8080 if your Router uses port 80
                               // To access server from the outside of a WiFi network e.g. ESP8266WebServer server(8266) add a rule on your Router that forwards a
                               // connection request to http://your_network_ip_address:8266 to port 8266 and view your ESP server from anywhere.
                               // Example http://yourhome.ip:8266 will be directed to http://192.168.0.40:8266 or whatever IP address your router gives to this server
Max72xxPanel matrix = Max72xxPanel(pinCS, numberOfHorizontalDisplays, numberOfVerticalDisplays);
int wait   = 75; // In milliseconds between scroll movements
int spacer = 1;
int width  = 5 + spacer; // The font width is 5 pixels
String SITE_WIDTH =  "1000";

long previousMillis = 0;        // will store last time Message was updated
 
// the follow variables is a long because the time, measured in miliseconds,
// will quickly become a bigger number than can be stored in an int.
long interval = 30000;  

void setup() {
     delay(2000);
  
  Serial.begin(115200); // initialize serial communications

  message = "Smart Led Messenger (C) 2018";
  matrix.setIntensity(2);    // Use a value between 0 and 15 for brightness
  matrix.setRotation(0, 1);  // The first display is position upside down
  matrix.setRotation(1, 1);  // The first display is position upside down
  matrix.setRotation(2, 1);  // The first display is position upside down
  matrix.setRotation(3, 1);  // The first display is position upside down
  display_message(message); // Display the message

  // Connect to WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) 
  {
     delay(500);
     Serial.print("*");
  }

  Udp.begin(localPort);
  setSyncProvider(getNtpTime);
  
  Serial.println("");
  Serial.println("WiFi connection Successful");
  Serial.print("The IP Address of ESP8266 Module is: ");
  Serial.print(WiFi.localIP());// Print the IP address
  // At this stage the WiFi manager will have successfully connected to a network, or if not will try again in 180-seconds
  Serial.println(F("WiFi connected..."));
  display_message(WiFi.localIP().toString());
  //----------------------------------------------------------------------
  server.begin(); Serial.println(F("Webserver started...")); // Start the webserver  configTime(0 * 3600, 0, "pool.ntp.org", "time.nist.gov");

  server.on("/", GetMessage);
  wait    = 40;
  message = "Smart Led Messenger (C) 2018";
  display_message(message); // Display the message
  wait    = 75;
  String IPaddress = WiFi.localIP().toString();
  message = "Welcome... Configure me on http://"+IPaddress;
}

void loop() {
  if (msgstatic) {
    display_static_message(message);
  } else {
    display_message(message); // Display the message
  }
}

String padzero(int num) {
  if (num < 10)
    return '0'+String(num);
  return String(num);
}

String parseCodes(String str) {
  String remotemessage = String(str);
  remotemessage.replace("{{time}}",  padzero(hour()) +":"+ padzero(minute()));
  return remotemessage;
}

void display_static_message(String passedmsg) {
  String remotemessage = parseCodes(passedmsg);
  int letter = 0;
  int x = 0;
  int y = (matrix.height() - 8) / 2; // center the text vertically
  while ( letter < remotemessage.length() ) {
    matrix.drawChar(x, y, remotemessage[letter], HIGH, LOW, 1); // HIGH LOW means foreground ON, background OFF, reverse these to invert the display!
    letter++;
    x += width;
  }
  matrix.write(); // Send bitmap to display
  delay(wait);
  server.handleClient();
  if (message != passedmsg) {
    matrix.fillScreen(0);
  }
}

void display_message(String passedmsg){
  String remotemessage = parseCodes(passedmsg);
  Serial.println(remotemessage);
   for ( int i = 0 ; i < width * remotemessage.length() + matrix.width() - spacer; i++ ) {
    //matrix.fillScreen(LOW);
    int letter = i / width;
    int x = (matrix.width() - 1) - i % width;
    int y = (matrix.height() - 8) / 2; // center the text vertically
    while ( x + width - spacer >= 0 && letter >= 0 ) {
      if ( letter < remotemessage.length() ) {        
        matrix.drawChar(x, y, remotemessage[letter], HIGH, LOW, 1); // HIGH LOW means foreground ON, background OFF, reverse these to invert the display!
      }
      letter--;
      x -= width;
    }
    matrix.write(); // Send bitmap to display
    delay(wait/2);
    server.handleClient(); // dans la boucle pour traitement rapide
    if (message != passedmsg) {
      matrix.fillScreen(0);
      break;
    }
  }
}

// not used for now
void GetServerMessage() {  
    HTTPClient http;  //Declare an object of class HTTPClient 
    http.begin("URL_TO_GET_THE_STRING_TO_DISPLAY");  // http://www.smartledmessenger.com/slm.ashx?key=2 
    int httpCode = http.GET();                                                                   
    if (httpCode > 0) {  
      message = http.getString();   
      Serial.println(message);             
    }
    http.end(); 
}

void GetMessage() {
  webpage = ""; // don't delete this command, it ensures the server works reliably!
  append_page_header();
  String IPaddress = WiFi.localIP().toString();
  webpage += "<form action=\"http://"+IPaddress+"\" method='post'>";
  webpage += "<P>Enter text for display: <INPUT type='text' size ='60' name='message' value='"+message+"'></P></form><br>";
  append_page_footer();
  server.send(200, "text/html", webpage); // Send a response to the client to enter their inputs, if needed, Enter=defaults
  if (server.args() > 0 ) { // Arguments were received
    for ( uint8_t i = 0; i  <= server.args(); i++ ) {
      String Argument_Name   = server.argName(i);
      String client_response = server.arg(i);
      if (Argument_Name == "message") message = client_response;
      if (Argument_Name == "static") msgstatic = client_response == "1";
      if (Argument_Name == "timezone") {
        timeZone = client_response.toInt();
        Serial.println(client_response);
        setTime(getNtpTime());
      }
      if (Argument_Name == "wait") wait = client_response.toInt();
    }
  }
}

void append_page_header() {
  webpage  = F("<!DOCTYPE HTML><html lang='en'><head>"); // Change language (en) as required
  webpage += "<META HTTP-EQUIV='Content-Type' CONTENT='text/html; charset=utf-8'>";
  webpage += F("<title>Smart Led Messenger</title><style>");
  webpage += F("body {width:");
  webpage += SITE_WIDTH;
  webpage += F("px;margin:0 auto;font-family:arial;font-size:14px;text-align:center;color:#cc66ff;background-color:#F7F2Fd;}");
  webpage += F("ul{list-style-type:circle; margin:0;padding:0;overflow:hidden;background-color:#d8d8d8;font-size:14px;}");
  webpage += F("li{float:left;border-right:1px solid #bbb;}last-child {border-right:none;}");
  webpage += F("li a{display: block;padding:2px 12px;text-decoration:none;}");
  webpage += F("li a:hover{background-color:#FFFFFF;}");
  webpage += F("section {font-size:16px;}");
  webpage += F("p {background-color:#E3D1E2;font-size:16px;}");
  webpage += F("div.header,div.footer{padding:0.5em;color:white;background-color:gray;clear:left;}");
  webpage += F("h1{background-color:#d8d8d8;font-size:26px;}");
  webpage += F("h2{color:#9370DB;font-size:22px;line-height:65%;}");
  webpage += F("h3{color:#9370DB;font-size:16px;line-height:55%;}");
  webpage += F("</style></head><body><h1>Message Display Board ");
  webpage += version+"</h1>";
}

void append_page_footer(){ // Saves repeating many lines of code for HTML page footers
  webpage += F("<ul><li><a href='/'>Enter Message</a></li></ul>");
  webpage += "&copy; Smart Led Messenger";
  webpage += F("</body></html>");
}



//////////////////////////////////
/*-------- NTP code ----------*/

const int NTP_PACKET_SIZE = 48; // NTP time is in the first 48 bytes of message
byte packetBuffer[NTP_PACKET_SIZE]; //buffer to hold incoming & outgoing packets

time_t getNtpTime()
{
  while (Udp.parsePacket() > 0) ; // discard any previously received packets
  Serial.println("Transmit NTP Request");
  sendNTPpacket(timeServer);
  uint32_t beginWait = millis();
  while (millis() - beginWait < 1500) {
    int size = Udp.parsePacket();
    if (size >= NTP_PACKET_SIZE) {
      Serial.println("Receive NTP Response");
      Udp.read(packetBuffer, NTP_PACKET_SIZE);  // read packet into the buffer
      unsigned long secsSince1900;
      // convert four bytes starting at location 40 to a long integer
      secsSince1900 =  (unsigned long)packetBuffer[40] << 24;
      secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
      secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
      secsSince1900 |= (unsigned long)packetBuffer[43];
      return secsSince1900 - 2208988800UL + timeZone * SECS_PER_HOUR;
    }
  }
  Serial.println("No NTP Response :-(");
  return 0; // return 0 if unable to get the time
}

// send an NTP request to the time server at the given address
void sendNTPpacket(IPAddress &address)
{
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12]  = 49;
  packetBuffer[13]  = 0x4E;
  packetBuffer[14]  = 49;
  packetBuffer[15]  = 52;
  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:                 
  Udp.beginPacket(address, 123); //NTP requests are to port 123
  Udp.write(packetBuffer, NTP_PACKET_SIZE);
  Udp.endPacket();
}
