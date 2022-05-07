#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266mDNS.h>
#include <ESP8266WebServer.h>
#include <NTPClient.h>

const char* ssid = "****";                        // The SSID (name) of the Wi-Fi network you want to connect to
const char* password = "****";                    // The password of the Wi-Fi network

ESP8266WebServer server(80);                         // Create a webserver object that listens for HTTP request on port 80
WiFiUDP ntpUDP;


const long utcOffsetInSeconds = 3600 * 2;            //DK tids-zone, Use this link to se you timezone, https://lastminuteengineers.com/esp8266-ntp-server-date-time-tutorial/ , https://en.wikipedia.org/wiki/List_of_UTC_offsets
char daysOfTheWeek[7][12] = { "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday" };
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

const int Lights = 4;                                 // GPIO4 D2 (SDA), use this link ti see the pinout, https://randomnerdtutorials.com/esp8266-pinout-reference-gpios/

bool lightsOverride = false;
bool OFF = false;
bool mondayEnable = true;                                  
bool tuesdayEnable = true;
bool wednesdayEnable = true;
bool thursdayEnable = true;
bool fridayEnable = true;
bool saturdayEnable = true;
bool sundayEnable = true;

const int monday = 1;
const int tuesday = 2;
const int wednesday = 3;
const int thursday = 4;
const int friday = 5;
const int saturday = 6;
const int sunday = 7;

int weekDay = 0;
float hours = 0;
float minutes = 0;
float hm = 0;

float mondaystart = 6.30;
float mondayend = 22.30;
float tuesdaystart = 6.30;
float tuesdayend = 22.30;
float wednesdaystart = 6.30;
float wednesdayend = 22.30;
float thursdaystart = 6.30;
float thursdayend = 22.30;
float fridaystart = 6.30;
float fridayend = 22.30;
float saturdaystart = 6.30;
float saturdayend = 22.30;
float sundaystart = 6.30;
float sundayend = 22.30;

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
  server.onNotFound(handleNotFound);              // When a client requests an unknown URI (i.e. something other than "/"), call function "handleNotFound"
}

//The loop
//Checking for overrides, and enabling the light. 
//////////////////////////////////////////////////////////////////////////////////
void loop() {
  updateDateTime();                               // Updates the time, over the web. web RTC. 
  server.handleClient();                          // Listen for HTTP requests from clients

  // Checks for overrides, ON or OFF, else just do the regualr lights.
  if (lightsOverride) {
    Serial.println("LIGHTS ON: override");
    digitalWrite(Lights, HIGH);
    delay(1000);
  } else if (OFF) {
    Serial.println("LIGHTS OFF: override");
    digitalWrite(Lights, LOW);
    delay(1000);
  } else {
    updateLights();
  }
}

//////////////////////////////////////////////////////////////////////////////////

void wifiConSetup() {
  delay(10);
  Serial.println('\n');

  WiFi.begin(ssid, password);                      // Connect to the network
  Serial.print("Connecting to ");
  Serial.print(ssid);
  Serial.println(" ...");

  int i = 0;
  while (WiFi.status() != WL_CONNECTED) {          // Wait for the Wi-Fi to connect
    delay(1000);
    Serial.print(++i);
    Serial.print(' ');
  }
  Serial.println();

  Serial.print("IP address:\t");
  Serial.println(WiFi.localIP());                  // Send the IP address of the ESP8266 to the computer

  if (MDNS.begin("esp8266")) {                     // Start the mDNS responder for esp8266.local
    Serial.println("mDNS responder started");
  } else {
    Serial.println("Error setting up MDNS responder!");
  }
}

void updateDateTime() {
  timeClient.update();
  weekDay = timeClient.getDay();
  hours = timeClient.getHours();
  minutes = timeClient.getMinutes() / (float)100;
  hm = hours + minutes;
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
  String HTML = "<form action=\"/setTimeDate\" method=\"POST\">\n"
                "<input type=\"checkbox\" name=\"MONDAY\" value=\"MONDAY\">\n"
                "<label for=\"MONDAY\"> Monday </label>\n"
                "<label for=\"appt\">What time shuld the lights turn on? :</label>\n"
                "<input type=\"time\" name=\"mondaySTART\">\n"
                "<label for=\"appt\">And what time shuld it turn off? :</label>\n"
                "<input type=\"time\" name=\"mondayEND\">\n"
                "<br>\n"
                "<input type=\"checkbox\" name=\"TUESDAY\" value=\"TUESDAY\">\n"
                "<label for=\"TUESDAY\"> Tuesday </label>\n"
                "<label for=\"appt\">What time shuld the lights turn on? :</label>\n"
                "<input type=\"time\" name=\"tuesdaySTART\">\n"
                "<label for=\"appt\">And what time shuld it turn off? :</label>\n"
                "<input type=\"time\" name=\"tuesdayEND\">\n"
                "<br>\n"
                "<input type=\"checkbox\" name=\"WEDNESDAY\" value=\"WEDNESDAY\">\n"
                "<label for=\"WEDNESDAY\"> Wednesday</label>\n"
                "<label for=\"appt\">What time shuld the lights turn on? :</label>\n"
                "<input type=\"time\" name=\"wednesdaySTART\">\n"
                "<label for=\"appt\">And what time shuld it turn off? :</label>\n"
                "<input type=\"time\" name=\"wednesdayEND\">\n"
                "<br>\n"
                "<input type=\"checkbox\" name=\"THURSDAY\" value=\"THURSDAY\">\n"
                "<label for=\"THURSDAY\"> Thursday</label>\n"
                "<label for=\"appt\">What time shuld the lights turn on? :</label>\n"
                "<input type=\"time\" name=\"thursdaySTART\">\n"
                "<label for=\"appt\">And what time shuld it turn off? :</label>\n"
                "<input type=\"time\" name=\"thursdayEND\">\n"
                "<br>\n"
                "<input type=\"checkbox\" name=\"FRIDAY\" value=\"FRIDAY\">\n"
                "<label for=\"FRIDAY\"> Friday</label>\n"
                "<label for=\"appt\">What time shuld the lights turn on? :</label>\n"
                "<input type=\"time\" name=\"fridaySTART\">\n"
                "<label for=\"appt\">And what time shuld it turn off? :</label>\n"
                "<input type=\"time\" name=\"fridayEND\">\n"
                "<br>\n"
                "<input type=\"checkbox\" name=\"SATURDAY\" value=\"SATURDAY\">\n"
                "<label for=\"SATURDAY\"> Saturday</label>\n"
                "<label for=\"appt\">What time shuld the lights turn on? :</label>\n"
                "<input type=\"time\" name=\"saturdaySTART\">\n"
                "<label for=\"appt\">And what time shuld it turn off? :</label>\n"
                "<input type=\"time\" name=\"saturdayEND\">\n"
                "<br>\n"
                "<input type=\"checkbox\" name=\"SUNDAY\" value=\"SUNDAY\">\n"
                "<label for=\"SUNDAY\"> Sunday</label>\n"
                "<label for=\"appt\">What time shuld the lights turn on? :</label>\n"
                "<input type=\"time\" name=\"sundaySTART\">\n"
                "<label for=\"appt\">And what time shuld it turn off? :</label>\n"
                "<input type=\"time\" name=\"sundayEND\">\n"
                "<br>\n"
                "<input type=\"submit\" value=\"Submit\"></form>\n";
  return HTML;
}


void handlesetTimeDate() {

  String x = "00:00";

  if (String(server.arg("MONDAY")) == "MONDAY") {
    mondayEnable = true;
    for (int i = 0; i <= 23; i++) {
      for (int j = 0; j <= 59; j++) {
        x = String(i) + ":" + String(j);
        if (server.arg("mondaySTART") == x) {
          mondaystart = (float)i + (float)j / (float)100;
        }
        if (server.arg("mondayEND") == x) {
          mondayend = (float)i + (float)j / (float)100;
        }
      }
    }

  } else {
    mondayEnable = false;
  }

  if (String(server.arg("TUESDAY")) == "TUESDAY") {
    tuesdayEnable = true;
    for (int i = 0; i <= 23; i++) {
      for (int j = 0; j <= 59; j++) {
        x = String(i) + ":" + String(j);
        if (server.arg("tuesdaySTART") == x) {
          tuesdaystart = (float)i + (float)j / (float)100;
        }
        if (server.arg("tuesdayEND") == x) {
          tuesdayend = (float)i + (float)j / (float)100;
        }
      }
    }
  } else {
    tuesdayEnable = false;
  }

  if (String(server.arg("WEDNESDAY")) == "WEDNESDAY") {
    wednesdayEnable = true;
    for (int i = 0; i <= 23; i++) {
      for (int j = 0; j <= 59; j++) {
        x = String(i) + ":" + String(j);
        if (server.arg("wednesdaySTART") == x) {
          wednesdaystart = (float)i + (float)j / (float)100;
        }
        if (server.arg("wednesdayEND") == x) {
          wednesdayend = (float)i + (float)j / (float)100;
        }
      }
    }
  } else {
    wednesdayEnable = false;
  }

  if (String(server.arg("THURSDAY")) == "THURSDAY") {
    thursdayEnable = true;
    for (int i = 0; i <= 23; i++) {
      for (int j = 0; j <= 59; j++) {
        x = String(i) + ":" + String(j);
        if (server.arg("thursdaySTART") == x) {
          thursdaystart = (float)i + (float)j / (float)100;
        }
        if (server.arg("thursdayEND") == x) {
          thursdayend = (float)i + (float)j / (float)100;
        }
      }
    }
  } else {
    thursdayEnable = false;
  }

  if (String(server.arg("FRIDAY")) == "FRIDAY") {
    fridayEnable = true;
    for (int i = 0; i <= 23; i++) {
      for (int j = 0; j <= 59; j++) {
        x = String(i) + ":" + String(j);
        if (server.arg("fridaySTART") == x) {
          fridaystart = (float)i + (float)j / (float)100;
          ;
        }
        if (server.arg("fridayEND") == x) {
          fridayend = (float)i + (float)j / (float)100;
        }
      }
    }
  } else {
    fridayEnable = false;
  }

  if (String(server.arg("SATURDAY")) == "SATURDAY") {
    saturdayEnable = true;
    for (int i = 0; i <= 23; i++) {
      for (int j = 0; j <= 59; j++) {
        x = String(i) + ":" + String(j);
        if (server.arg("saturdaySTART") == x) {
          saturdaystart = (float)i + (float)j / (float)100;
        }
        if (server.arg("saturdayEND") == x) {
          saturdayend = (float)i + (float)j / (float)100;
        }
      }
    }
  } else {
    saturdayEnable = false;
  }

  if (String(server.arg("SUNDAY")) == "SUNDAY") {
    sundayEnable = true;
    for (int i = 0; i <= 23; i++) {
      for (int j = 0; j <= 59; j++) {
        x = String(i) + ":" + String(j);
        if (server.arg("sundaySTART") == x) {
          sundaystart = (float)i + (float)j / (float)100;
        }
        if (server.arg("sundayEND") == x) {
          sundayend = (float)i + (float)j / (float)100;
        }
      }
    }
  } else {
    sundayEnable = false;
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

  if (mondayEnable && weekDay == monday && hm > (float)mondaystart && hm < (float)mondayend) {
    Serial.println("LIGHTS ON: monday");
    digitalWrite(Lights, HIGH);
  } else if (tuesdayEnable && weekDay == tuesday && hm > (float)tuesdaystart && hm < (float)tuesdayend) {
    Serial.println("LIGHTS ON: tuesday");
    digitalWrite(Lights, HIGH);
  } else if (wednesdayEnable && weekDay == wednesday && hm > (float)wednesdaystart && hm < (float)wednesdayend) {
    Serial.println("LIGHTS ON: wednesday");
    digitalWrite(Lights, HIGH);
  } else if (thursdayEnable && weekDay == thursday && hm > (float)thursdaystart && hm < (float)thursdayend) {
    Serial.println("LIGHTS ON: thursday");
    digitalWrite(Lights, HIGH);
  } else if (fridayEnable && weekDay == friday && hm > (float)fridaystart && hm < (float)fridayend) {
    Serial.println("LIGHTS ON: friday");
    digitalWrite(Lights, HIGH);
  } else if (saturdayEnable && weekDay == saturday && hm > (float)saturdaystart && hm < (float)saturdayend) {
    Serial.println("LIGHTS ON: saturday");
    digitalWrite(Lights, HIGH);
  } else if (sundayEnable && weekDay == sunday && hm > (float)sundaystart && hm < (float)sundayend) {
    Serial.println("LIGHTS ON: sunday");
    digitalWrite(Lights, HIGH);
  } else {
    Serial.println("LIGHTS OFF");
    digitalWrite(Lights, LOW);
  }

  delay(1000);
}