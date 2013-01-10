/* timerino -- web-controlled relay timer
visit https://github.com/legkodymov/timerino
for more information
*/


#include "SPI.h"
#include "Ethernet.h"
#include "WebServer.h"
#include "EEPROM.h"
#include <string.h>
#include <avr/pgmspace.h>

// no-cost stream operator as described at 
// http://sundial.org/arduino/?page_id=119
template<class T>
inline Print &operator <<(Print &obj, T arg)
{ 
  obj.print(arg); 
  return obj; 
}


// CHANGE THIS TO YOUR OWN UNIQUE VALUE
static uint8_t mac[] = { 
  0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };

// CHANGE THIS TO MATCH YOUR HOST NETWORK
static uint8_t ip[] = {
  192, 168, 0, 77};

/* Relays description
 
 1. Argon
 2. Azot
 3. Acetilen
 4. Air
 5. Not Used
 6. Not Used
 7. Vacuum pump
 8. HV relay
 
 */
 
prog_char string_0[] PROGMEM = "Аргон";
prog_char string_1[] PROGMEM = "Азот";
prog_char string_2[] PROGMEM = "Ацетилен";
prog_char string_3[] PROGMEM = "Воздух";
prog_char string_4[] PROGMEM = "";
prog_char string_5[] PROGMEM = "";
prog_char string_6[] PROGMEM = "";
prog_char string_7[] PROGMEM = "";


// Then set up a table to refer to your strings.

PROGMEM const char *gaz_table[] =
{   
  string_0,
  string_1,
  string_2,
  string_3,
  string_4,
  string_5,
  string_6,
  string_7 
};
 
#define SHOT_QUANT 100
#define SHOTS_IN_SECOND 1000/SHOT_QUANT
// relays
byte numPins = 8;
byte numPeriods = 6; // Only first 6 relays used with gases/timer
byte pins[] = {
  9, 4, 2, 3, 5, 6, 7, 8};
byte pinState[] = {
  0, 0, 0, 0, 0, 0, 0, 0};
int periods[] = { 
  10, 30, 40, 70, 110, 180}; //in seconds
int shots[] = {
  6, 5, 4, 3, 2, 1}; // in 0.1 seconds
int cperiods[] = { 
  0, 0, 0, 0, 0, 0}; // counter periods
int cshots[] = {
  0, 0, 0, 0, 0, 0}; // counter shots
int voltage;
int voltage_enabled;
int voltage_thres = 512;
int voltage_gist = 32;
int timer = 0;


#define PREFIX ""

#define NAMELEN 32
#define VALUELEN 32

WebServer webserver(PREFIX, 80);

P(default_head) = "<meta http-equiv=\"Content-Type\" content=\"text/html; charset=UTF-8\" />\n"
"<style type=\"text/css\">"
"* {"
"margin:0;"
"padding:0;"
"color:#999"
"text-align:center;"
"}\n"
"body {"
"background:#393939;"
"font: 16px Helvetica, sans-serif;"
"text-align:center;"
"color:#999;"
"}\n"
"span {"
"background:#393939;"
"font: 16px Helvetica, sans-serif;"
"text-align:center;"
"color:#fff;"
"}\n"
"h3 {"
"color:#FFF;"
"text-align:center;"
"}\n"
"input[type=text] {"
"width:60px;"
"font:bold 20px Helvetica, sans-serif;"
"padding:3px 0 0;"
"}\n"
"#submit {"
"border:1px solid #000;"
"background:#fff;"
"margin:15px 0 0 175px;"
"padding:5px 10px 3px;"
"}\n"
"</style>\n"
"<script type=\"text/javascript\">\n"
"    function incDec(element, Delta) {\n"
"        var element_value = parseFloat(document.getElementById(element).value) + Delta;\n"
"        if (element_value > 0 ) {\n"
"            if (parseInt(element_value * 10) % 10 == 0) {\n"
"                element_value = parseInt(element_value);\n"
"            } else {\n"
"                element_value = element_value.toFixed(1);\n"
"            }\n"
"            document.getElementById(element).value = element_value;\n"
"        }\n"
"    }\n"
"</script>\n"
"<form id=\"forms\" method=\"get\" action=\"\">"
"<input style=\"background:#0f0\" type=\"submit\" value=\"Обновить\" id=\"submit\" />"
"</form>"
"<form id=\"forms\" method=\"post\" action=\"update\">\n"
"<input type=\"submit\" value=\"Запомнить\" id=\"submit\" style=\"background:#f00\" />\n"
;

P(default_end) = "</form>\n";

void defaultCmd(WebServer &server, WebServer::ConnectionType type, char *url_tail, bool tail_complete);

// commands are functions that get called by the webserver framework
// they can read any posted data from client, and they output to server

void updateCmd(WebServer &server, WebServer::ConnectionType type, char *url_tail, bool tail_complete)
{
  URLPARAM_RESULT rc;                                                           
  char name[NAMELEN];                                                           
  char value[VALUELEN];                                                         
  bool state;
  String name_s;
  int pin;
  bool data_changed = false;

//  server.httpSuccess("text/html");
//  server.print("<!DOCTYPE html><html><head><title>Relay</title></head><body><p>OK!</p>\n");
  if (type == WebServer::POST)
  {
    bool result;
    do {
      result = server.readPOSTparam(name, NAMELEN, value, VALUELEN);
      name_s = String(name);
//      server << "result: " << result << ", name: " << name << ", value: " << value << "<br>";
      if (name_s.startsWith("relay") && name_s.length() > 5) {
        state = (strcmp(value, "0") == 0);
        pin = name_s.charAt(5) - '0';
        digitalWrite(pins[pin], state);  
      } 
      if (name_s.startsWith("period") && name_s.length() > 6) {
        pin = name_s.charAt(6) - '0';
        periods[pin] = atoi(value);
        data_changed = true;
      } 
      if (name_s.startsWith("shot") && name_s.length() > 4) {
        pin = name_s.charAt(4) - '0';
        shots[pin] = int(atof(value) * 10);
        data_changed = true;
      } 
      if (name_s == "voltage_thres") {
        voltage_thres = atoi(value);
        data_changed = true;
      } 
      if (name_s == "voltage_gist") {
        voltage_gist = atoi(value);
        data_changed = true;
      }
    } 
    while (result);
    if (data_changed)
      write_eeprom();
  }
//  server.print("  </body></html>");
  defaultCmd(server, type, url_tail, tail_complete);
  return;
}

void jsonCmd(WebServer &server, WebServer::ConnectionType type, char *url_tail, bool tail_complete) {
  URLPARAM_RESULT rc;                                                           
  char name[NAMELEN];                                                           
  char value[VALUELEN];                                                         
  bool state;
  String name_s;
  int pin;

  server.httpSuccess("application/json");

  if (type == WebServer::HEAD)
    return;

  int i;    
  server << "{ ";
  server << "\"relays\": { ";
  for (i = 0; i < numPins; ++i)
  {
    // ignore the pins we use to talk to the Ethernet chip
    int val = !digitalRead(pins[i]);
    server << "\n\t\"relay" << i << "\": " << val << ", ";
  }
  server << "}, \n";

  server << "\"analog\": { ";
  for (i = 0; i <= 5; ++i)
  {
    int val = analogRead(i);
    server << "\n\t\"a" << i << "\": " << val;
    if (i != 5)
      server << ",";
  }
  server << "} \n";

  server << "\"periods\": { ";
  for (i = 0; i < numPeriods; i++) {
    // ignore the pins we use to talk to the Ethernet chip
    int val = periods[i];
    server << "\n\t\"period" << i << "\": " << val << ", ";
  }
  server << "}, \n";

  server << "\"shots\": { ";
  for (i = 0; i < numPeriods; i++) {
    // ignore the pins we use to talk to the Ethernet chip
    int val = shots[i];
    server << "\n\t\"shot" << i << "\": " << val << ", ";
  }
  server << "}, \n";

  server << "\"cperiods\": { ";
  for (i = 0; i < numPeriods; i++) {
    // ignore the pins we use to talk to the Ethernet chip
    int val = cperiods[i];
    server << "\n\t\"cperiod" << i << "\": " << val << ", ";
  }
  server << "}, \n";

  server << "\"cshots\": { ";
  for (i = 0; i < numPeriods; i++) {
    // ignore the pins we use to talk to the Ethernet chip
    int val = cshots[i];
    server << "\n\t\"cshot" << i << "\": " << val << ", ";
  }
  server << "}, \n";
  server << "\"voltage_thres\":  " << voltage_thres << ", \n";
  server << "\"voltage_gist\":  " << voltage_gist << ", \n";

  server << " }";
}

void write_eeprom() {
  int addr = 0;
  int i;

  for (i = 0; i < numPeriods; i++) {
    EEPROM.write(addr, periods[i]); 
    addr += 2;
  }
  for (i = 0; i < numPeriods; i++) {
    EEPROM.write(addr, shots[i]); 
    addr += 2;
  }
  EEPROM.write(addr, voltage_thres); 
  addr++;
  EEPROM.write(addr, voltage_thres >> 8); 
  addr++;
  EEPROM.write(addr, voltage_gist); 
  addr++;
  EEPROM.write(addr, voltage_gist >> 8); 
  addr++;
}

void read_eeprom() {
  int addr = 0;
  int i;

  for (i = 0; i < numPeriods; i++) {
    periods[i] = EEPROM.read(addr); 
    addr += 2;
  }
  for (i = 0; i < numPeriods; i++) {
    shots[i] = EEPROM.read(addr); 
    addr += 2;
  }
  voltage_thres = EEPROM.read(addr); 
  addr++;
  voltage_thres |= EEPROM.read(addr) << 8; 
  addr++;
  voltage_gist = EEPROM.read(addr); 
  addr++;
  voltage_gist |= EEPROM.read(addr) << 8; 
  addr++;
}

P(t1) = "<span style=\"font:bold 35px Helvetica, sans-serif;\" onclick=\"incDec('period";
P(t2) = "',1)\">&nbsp+&nbsp</span>\n";
P(t3) = "<span style=\"font:bold 35px Helvetica, sans-serif;\" onclick=\"incDec('period";
P(t4) = "<span style=\"font:bold 35px Helvetica, sans-serif;\" onclick=\"incDec('voltage_thres', 8)\">&nbsp+&nbsp</span>\n";
P(t5) = "<span style=\"font:bold 35px Helvetica, sans-serif;\" onclick=\"incDec('voltage_thres', -8)\">&nbsp-&nbsp</span>\n";
P(t6) = "<span style=\"font:bold 35px Helvetica, sans-serif;\" onclick=\"incDec('voltage_gist', 4)\">&nbsp+&nbsp</span>\n";
P(t7) = "<span style=\"font:bold 35px Helvetica, sans-serif;\" onclick=\"incDec('voltage_gist', -4)\">&nbsp-&nbsp</span>\n";
P(t8) = "<h3>Реле 7. Датчик давления </h3>\n<p>Текущее значение: ";
P(t9) = "<input type=\"text\" id=\"voltage_thres\" name=\"voltage_thres\" value=\"";
P(t10) = "Порог срабатывания, попугаи (0-1023)&nbsp&nbsp&nbsp&nbsp\n";
P(t11) = "<input type=\"text\" id=\"voltage_gist\" name=\"voltage_gist\" value=\"";
P(t12) = "Гистерезис, попугаи&nbsp&nbsp&nbsp&nbsp\n";


void defaultCmd(WebServer &server, WebServer::ConnectionType type, char *url_tail, bool tail_complete)
{
  URLPARAM_RESULT rc;                                                           
  char name[NAMELEN];                                                           
  char value[VALUELEN];                                                         
  char buf[8];
  bool state;
  String name_s;
  int pin;
  const char * voltage_enabled_str;
  bool data_changed = false;

  server.httpSuccess("text/html");

  if (type == WebServer::HEAD)
    return;

  server.printP(default_head);

  server.printP(t8);


  voltage_enabled_str = voltage_enabled ? "<span style='color: #0f0'><b>ВКЛ</b></span>" :
  "<span style='color: #777'>ВЫКЛ</span>";

  server << voltage_enabled_str << "(" << voltage << ")</p>";
  server.printP(t9);
  server << voltage_thres << "\" style=\"text-align: center\"/>\n";
  server.printP(t4);
  server.printP(t5);
  server.printP(t10);
  server.printP(t11);
  server << voltage_gist << "\" style=\"text-align: center\"/>\n";
  server.printP(t6);
  server.printP(t7);
  server.printP(t12);

  for (int i = 0; i < numPeriods; i++) {
    server << "<h3>Реле " << i << ". ";
    server.printP((char*)pgm_read_word(&(gaz_table[i])));
    server << "</h3>\n";
    server << "<input type=\"text\" id=\"period" << i << "\" name=\"period" << i << "\" value=\"" << periods[i] << "\" style=\"text-align: center\">\n";
    server.printP(t1);
    server << i;
    server.printP(t2);
    server.printP(t3);
    server << i << "',-1)\">&nbsp-&nbsp</span>\n Период, сек&nbsp&nbsp&nbsp&nbsp\n";

    sprintf(buf, "%d.%d", shots[i] / 10, shots[i] % 10);
    server << "<input type=\"text\" id=\"shot" << i << "\" name=\"shot" << i << "\" value=\"" << buf << "\" style=\"text-align: center\"/>\n";
    server << "<span style=\"font:bold 35px Helvetica, sans-serif;\" onclick=\"incDec('shot" << i << "',0.1)\">&nbsp+&nbsp</span>\n";
    server << "<span style=\"font:bold 35px Helvetica, sans-serif;\" onclick=\"incDec('shot" << i << "',-0.1)\">&nbsp-&nbsp</span>\n";
    server << "Длительность, сек&nbsp&nbsp&nbsp&nbsp\n";
  }
  server.printP(default_end); 
}

void setup()
{
  for (int i = 0; i < numPins; i++)
  {
    pinMode(pins[i], OUTPUT);
    digitalWrite(pins[i], !pinState[i]);
  }
  read_eeprom();
  for (int i = 0; i < numPeriods; i++) {
    cperiods[i] = periods[i];
  }

  Ethernet.begin(mac, ip);
  webserver.begin();

  webserver.setDefaultCommand(&defaultCmd);
  webserver.addCommand("json", &jsonCmd);
  webserver.addCommand("update", &updateCmd);
}

void loop()
{
  // process incoming connections one at a time forever
  webserver.processConnection();

  // if you wanted to do other work based on a connecton, it would go here
  int i;
  for (i = 0; i < numPeriods; i++) {
    if (cshots[i]) {
      cshots[i]--;
      if (!cshots[i])
        digitalWrite(pins[i], HIGH);
    }
  }
  if (!timer) {
    for (i = 0; i < numPeriods; i++) {
      if (cperiods[i]) {
        cperiods[i]--;
        if (!cperiods[i]) {
                    digitalWrite(pins[i], LOW);
          cperiods[i] = periods[i];
          cshots[i] = shots[i];
        }
      }
    }
    voltage = analogRead(0);
    if (voltage >= voltage_thres) {
      digitalWrite(pins[7], LOW);
      voltage_enabled = true;
    }
    if (voltage < (voltage_thres - voltage_gist)) {
      digitalWrite(pins[7], HIGH);
      voltage_enabled = false;
    }

    timer = SHOTS_IN_SECOND;
  }

  timer--;
  delay(SHOT_QUANT);
}




