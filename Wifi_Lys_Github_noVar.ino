#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266mDNS.h>
#include <ESP8266WebServer.h>
#include <NTPClient.h>
#include <string.h>

ESP8266WebServer server(80);  // Create a webserver object that listens for HTTP request on port 80
WiFiUDP ntpUDP;

/********** PLEASE CHANGE THIS *************************/
#define ssid "TP-Link_8144"                    // The SSID (name) of the Wi-Fi network you want to connect to
#define password "HjcMg9uf5Go2KHvjm9gKfuHR5Z"  // The password of the Wi-Fi network
#define Lights 4                               // GPIO4 D2 (SDA), use this link ti see the pinout, https://randomnerdtutorials.com/esp8266-pinout-reference-gpios/

const long utcOffsetInSeconds = 3600 * 2;      //DK tids-zone, Use this link to se you timezone, https://lastminuteengineers.com/esp8266-ntp-server-date-time-tutorial/ , https://en.wikipedia.org/wiki/List_of_UTC_offsets
static const char* const daysOfTheWeek[7] = { "sunday", "monday", "tuesday", "wednesday", "thursday", "friday", "saturday" };
NTPClient timeClient(ntpUDP, "pool.ntp.org", utcOffsetInSeconds);

// Handle the webpages hosted
//////////////////////////////////////////////////////////////////////////////////
void handleRoot();
void handleON();
void handleOFF();
void handleConfig();
void handlesetTimeDate();
void handleNotFound();

// Variables
//////////////////////////////////////////////////////////////////////////////////

bool lightsOverride = false;
bool OFF = false;

enum weekDays { sunday, monday, tuesday, wednesday, thursday, friday, saturday, NUM_DAYS } weekDay;

struct dayInfo {
  unsigned int start, end;
  bool enable;
};

dayInfo weekdayInfo[7];

unsigned int hm = 0;                            // Hours and minutes

//setup
//////////////////////////////////////////////////////////////////////////////////

void setup() {
  Serial.begin(115200);
  delay(10);

  pinMode(Lights, OUTPUT);

  // Actually start the server for the time and wifi toggle
  wifiConSetup();
  server.begin();
  timeClient.begin();
  Serial.println("HTTP server started");

  //Setup for the web-interface.
  server.on("/", HTTP_GET, handleRoot);           // Call the 'handleRoot' function when a client requests URI "/"
  server.on("/ON", HTTP_POST, handleON);          // Call the 'handleON' function when a POST request is made to URI "/ON"
  server.on("/OFF", HTTP_POST, handleOFF);        // Call the 'handleOFF' function when a POST request is made to URI "/OFF"
  server.on("/Config", HTTP_POST, handleConfig);  // Call the 'handleREST' function when a post request is made to URI "/Config"
  server.on("/setTimeDate", HTTP_POST, handlesetTimeDate);
  server.onNotFound(handleNotFound);  // When a client requests an unknown URI (i.e. something other than "/"), call function "handleNotFound"

  // Setup Default Time For Start And Stop
  for (int i = 0; i < NUM_DAYS; i++) {
    weekdayInfo[i].enable = true;
    weekdayInfo[i].start = getMinFromHour(6, 30);
    weekdayInfo[i].end = getMinFromHour(22, 30);
  }
}

//Checking for overrides, and enabling the light.
//////////////////////////////////////////////////////////////////////////////////
void loop() {
  updateDateTime();       // Updates the time, over the web. web RTC.
  server.handleClient();  // Listen for HTTP requests from clients

  // Checks for overrides, ON or OFF, else just do the regualr lights.
  if (lightsOverride) {
    Serial.println("LIGHTS ON: override");
    digitalWrite(Lights, HIGH);
  } else if (OFF) {
    Serial.println("LIGHTS OFF: override");
    digitalWrite(Lights, LOW);
  } else {
    updateLights();
  }
  delay(1000);
}

//////////////////////////////////////////////////////////////////////////////////
void wifiConSetup() {

  delay(10);
  Serial.println('\n');

  WiFi.begin(ssid, password);  // Connect to the network
  Serial.print("Connecting to ");
  Serial.print(ssid);
  Serial.println(" ...");

  int i = 0;
  while (WiFi.status() != WL_CONNECTED) {  // Wait for the Wi-Fi to connect
    delay(1000);
    Serial.print(++i);
    Serial.print(' ');
  }
  Serial.println();

  Serial.print("IP address:\t");
  Serial.println(WiFi.localIP());  // Send the IP address of the ESP8266 to the computer

  if (MDNS.begin("esp8266")) {  // Start the mDNS responder for esp8266.local
    Serial.println("mDNS responder started");
  } else {
    Serial.println("Error setting up MDNS responder!");
  }
}

//Calculate the total amount of minutes
inline constexpr unsigned int getMinFromHour(const int hour, const int minute) {
  return hour * 60 + minute;
}

void updateDateTime() {
  timeClient.update();
  weekDay = (weekDays)timeClient.getDay();
  const int hours = timeClient.getHours();
  const int minutes = timeClient.getMinutes();
  hm = getMinFromHour(hours, minutes);
}

// Handle the Internet-Interface
//////////////////////////////////////////////////////////////////////////////////
void handleRoot() {                    // When URI / is requested, send a web page with a buttons to toggle the Lights
  server.send(200, "text/html", "<h1>Hi and welcome to my Website!</h1><p>press butten to override the lights</p><form action=\"/ON\" method=\"POST\"><input type=\"submit\" value=\"Turn On Lights\"></form><form action=\"/OFF\" method=\"POST\"><input type=\"submit\" value=\"Turn OFF Lights\"></form><form action=\"/Config\" method=\"POST\"><input type=\"submit\" value=\"Light Config\"></form>");
}

void handleON() {                      // If a POST request is made to URI /ON
  OFF = false;
  lightsOverride = !lightsOverride;    // Change the state of the Lights, so double press revers the handle.
  server.sendHeader("Location", "/");  // Add a header to respond with a new location for the browser to go to the home page again
  server.send(303);                    // Send it back to the browser with an HTTP status 303 (See Other) to redirect
}

void handleOFF() {                     // If a POST request is made to URI /OFF
  OFF = !OFF;                          // Turns off the state of OFF so double press revers the handle.
  lightsOverride = false;
  server.sendHeader("Location", "/");  // Add a header to respond with a new location for the browser to go to the home page again
  server.send(303);                    // Send it back to the browser with an HTTP status 303 (See Other) to redirect
}

void handleConfig() {
  OFF = false;
  lightsOverride = false;
  server.send(200, "text/html", prepareHTML());
}

String prepareHTML() {
  const String HTML = "<form action=\"/setTimeDate\" method=\"POST\">\n"
                      "<input type=\"checkbox\" name=\"monday\" value=\"monday\" checked>\n"
                      "<label for=\"monday\"> Monday </label>\n"
                      "<label for=\"appt\">What time shuld the lights turn on? :</label>\n"
                      "<input type=\"time\" name=\"mondaySTART\" value=\"07:30\">\n"
                      "<label for=\"appt\">And what time shuld it turn off? :</label>\n"
                      "<input type=\"time\" name=\"mondayEND\" value=\"22:30\">\n"
                      "<br>\n"
                      "<input type=\"checkbox\" name=\"tuesday\" value=\"tuesday\" checked>\n"
                      "<label for=\"tuesday\"> Tuesday </label>\n"
                      "<label for=\"appt\">What time shuld the lights turn on? :</label>\n"
                      "<input type=\"time\" name=\"tuesdaySTART\" value=\"07:30\">\n"
                      "<label for=\"appt\">And what time shuld it turn off? :</label>\n"
                      "<input type=\"time\" name=\"tuesdayEND\" value=\"22:30\">\n"
                      "<br>\n"
                      "<input type=\"checkbox\" name=\"wednesday\" value=\"wednesday\" checked>\n"
                      "<label for=\"wednesday\"> Wednesday</label>\n"
                      "<label for=\"appt\">What time shuld the lights turn on? :</label>\n"
                      "<input type=\"time\" name=\"wednesdaySTART\" value=\"07:30\">\n"
                      "<label for=\"appt\">And what time shuld it turn off? :</label>\n"
                      "<input type=\"time\" name=\"wednesdayEND\" value=\"22:30\">\n"
                      "<br>\n"
                      "<input type=\"checkbox\" name=\"thursday\" value=\"thursday\" checked>\n"
                      "<label for=\"thursday\"> Thursday</label>\n"
                      "<label for=\"appt\">What time shuld the lights turn on? :</label>\n"
                      "<input type=\"time\" name=\"thursdaySTART\" value=\"07:30\">\n"
                      "<label for=\"appt\">And what time shuld it turn off? :</label>\n"
                      "<input type=\"time\" name=\"thursdayEND\" value=\"22:30\">\n"
                      "<br>\n"
                      "<input type=\"checkbox\" name=\"friday\" value=\"friday\" checked>\n"
                      "<label for=\"friday\"> Friday</label>\n"
                      "<label for=\"appt\">What time shuld the lights turn on? :</label>\n"
                      "<input type=\"time\" name=\"fridaySTART\" value=\"07:30\">\n"
                      "<label for=\"appt\">And what time shuld it turn off? :</label>\n"
                      "<input type=\"time\" name=\"fridayEND\" value=\"22:30\">\n"
                      "<br>\n"
                      "<input type=\"checkbox\" name=\"saturday\" value=\"saturday\" checked>\n"
                      "<label for=\"saturday\"> Saturday</label>\n"
                      "<label for=\"appt\">What time shuld the lights turn on? :</label>\n"
                      "<input type=\"time\" name=\"saturdaySTART\" value=\"07:30\">\n"
                      "<label for=\"appt\">And what time shuld it turn off? :</label>\n"
                      "<input type=\"time\" name=\"saturdayEND\" value=\"22:30\">\n"
                      "<br>\n"
                      "<input type=\"checkbox\" name=\"sunday\" value=\"sunday\" checked>\n"
                      "<label for=\"sunday\"> Sunday</label>\n"
                      "<label for=\"appt\">What time shuld the lights turn on? :</label>\n"
                      "<input type=\"time\" name=\"sundaySTART\" value=\"07:30\">\n"
                      "<label for=\"appt\">And what time shuld it turn off? :</label>\n"
                      "<input type=\"time\" name=\"sundayEND\" value=\"22:30\">\n"
                      "<br>\n"
                      "<input type=\"submit\" value=\"Submit\"></form>\n";
  return HTML;
}

//converts Char* String to Int
unsigned int strTimeToMin(const char* str) {
  unsigned int acc = 0;
  unsigned int hours;
  for (int i = 0; str[i]; i++) {
    if (str[i] == ':') {
      hours = acc;
      acc = 0;
      continue;
    }
    acc *= 10;
    acc += str[i] - '0';
  }
  return getMinFromHour(hours, acc);
}

// Checks the Web Responce and sets the weekdayInfo corresponding.
void handlesetTimeDate() {
  for (int i = 0; i < 7; i++) {
    if (!strcmp(server.arg(daysOfTheWeek[i]).c_str(), daysOfTheWeek[i])) {

      //Start
      char start[strlen(daysOfTheWeek[i]) + 5];
      strcpy(start, daysOfTheWeek[i]);
      strcpy(start + strlen(daysOfTheWeek[i]), "START");
      weekdayInfo[i].start = strTimeToMin(server.arg(start).c_str());

      //End
      char end[strlen(daysOfTheWeek[i]) + 3];
      strcpy(end, daysOfTheWeek[i]);
      strcpy(end + strlen(daysOfTheWeek[i]), "END");
      weekdayInfo[i].end = strTimeToMin(server.arg(end).c_str());

      weekdayInfo[i].enable = true;
    } else {
      weekdayInfo[i].enable = false;
    }
  }

  delay(1000);
  server.sendHeader("Location", "/");
  server.send(303);
}

void handleNotFound() {
  server.send(404, "text/plain", "404: Not found");  // Send HTTP status 404 (Not Found) when there's no handler for the URI in the request
}

// Handle the Lights
//////////////////////////////////////////////////////////////////////////////////
void updateLights() {

  if (weekdayInfo[weekDay].enable && weekdayInfo[weekDay].start < hm && weekdayInfo[weekDay].end > hm) {
    digitalWrite(Lights, HIGH);
    Serial.print("Lights ON: ");
    Serial.println(daysOfTheWeek[weekDay]);
  } else {
    digitalWrite(Lights, LOW);
    Serial.println("LIGHTS OFF");
  }
}