#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266mDNS.h>
#include <ESP8266WebServer.h>
#include <NTPClient.h>
#include <string.h>
#include <Adafruit_NeoPixel.h>

ESP8266WebServer server(80);  // Create a webserver object that listens for HTTP request on port 80
WiFiUDP ntpUDP;

/********** PLEASE CHANGE THIS *************************/
#define ssid "<ENTER YOUR WIFI SSID>"          // The SSID (name) of the Wi-Fi network you want to connect to
#define password "<ENTER YOUR PASSWORD>"       // The password of the Wi-Fi network   
#define Lights 4                               // GPIO4 D2 (SDA), use this link ti see the pinout, https://randomnerdtutorials.com/esp8266-pinout-reference-gpios/
#define LED_PIN 3
#define LED_COUNT  300                         // How many NeoPixels are attached to the Arduino?
#define BRIGHTNESS 50                          // Set BRIGHTNESS to about 1/5 (max = 255)

const long utcOffsetInSeconds = 3600 * 2;      //DK tids-zone, Use this link to se you timezone, https://lastminuteengineers.com/esp8266-ntp-server-date-time-tutorial/ , https://en.wikipedia.org/wiki/List_of_UTC_offsets
static const char* const daysOfTheWeek[7] = { "sunday", "monday", "tuesday", "wednesday", "thursday", "friday", "saturday" };
NTPClient timeClient(ntpUDP, "pool.ntp.org", utcOffsetInSeconds);

// Declare our NeoPixel strip object:
Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRBW + NEO_KHZ800);

// Handle the webpages hosted
//////////////////////////////////////////////////////////////////////////////////
void handleRoot();
void handleON();
void handleOFF();
void handleConfig();
void handlesetTimeDate();
void handleledConfig();
void handleledShuffle();
void handleNotFound();

// Variables
//////////////////////////////////////////////////////////////////////////////////

bool lightsOverride = false;
bool OFF = false;
bool shuffle = false;
bool ledset = false;

enum weekDays { sunday, monday, tuesday, wednesday, thursday, friday, saturday, NUM_DAYS } weekDay;

struct dayInfo {
  unsigned int start1, end1, start2, end2;
  bool enable;
};

struct led {
  unsigned int red, green, blue, white;
};

dayInfo weekdayInfo[7];

unsigned int hm = 0;                            // Hours and minutes

//setup
//////////////////////////////////////////////////////////////////////////////////

void setup() {
  Serial.begin(115200);
  delay(10);

  pinMode(Lights, OUTPUT);
  pinMode(LED_PIN, OUTPUT);

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
  server.on("/ledConfig", HTTP_POST, handleledConfig);
  server.on("/ledShuffle", HTTP_POST, handleledShuffle);
  server.onNotFound(handleNotFound);              // When a client requests an unknown URI (i.e. something other than "/"), call function "handleNotFound"

  // Setup Default Time For Start And Stop
  for (int i = 0; i < NUM_DAYS; i++) {
    weekdayInfo[i].enable = true;
    weekdayInfo[i].start1 = getMinFromHour(6, 30);
    weekdayInfo[i].end1 = getMinFromHour(8, 00);
    weekdayInfo[i].start2 = getMinFromHour(16, 00);
    weekdayInfo[i].end2 = getMinFromHour(22, 30);
  }
  ledSetup();
}

//Checking for overrides, and enabling the light.
//////////////////////////////////////////////////////////////////////////////////
void loop() {
  updateDateTime();       // Updates the time, over the web. web RTC.
  server.handleClient();  // Listen for HTTP requests from clients

  // Checks for overrides, ON or OFF, else just do the regualr lights.
  if (lightsOverride) {
    //Serial.println("LIGHTS ON: override");
    digitalWrite(Lights, HIGH);
    updateLED();
  } else if (OFF) {
    //Serial.println("LIGHTS OFF: override");
    digitalWrite(Lights, LOW);
  } else {
    updateLights();
    updateLED();
  }
  delay(50);
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

void ledSetup() {
    // Setup LED
  strip.begin();           // INITIALIZE NeoPixel strip object (REQUIRED)
  strip.setBrightness(BRIGHTNESS);
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

String prepare_handleConfig_HTML() {
  const String HTML = "<form action=\"/setTimeDate\" method=\"POST\">\n"
                      "<input type=\"checkbox\" name=\"monday\" value=\"monday\" checked>\n"
                      "<label for=\"monday\"> Monday </label>\n"
                      "<br>\n"
                      "<label for=\"appt\">What time shuld the lights turn on in the morning? :</label>\n"
                      "<input type=\"time\" name=\"mondaySTART1\" value=\"06:30\">\n"
                      "<label for=\"appt\">And what time shuld it turn off in the morning? :</label>\n"
                      "<input type=\"time\" name=\"mondayEND1\" value=\"07:30\">\n"
                      "<br>\n"
                      "<label for=\"appt\">What time shuld the lights turn on in the evening? :</label>\n"
                      "<input type=\"time\" name=\"mondaySTART2\" value=\"16:00\">\n"
                      "<label for=\"appt\">And what time shuld it turn off in the evening? :</label>\n"
                      "<input type=\"time\" name=\"mondayEND2\" value=\"22:30\">\n"
                      "<br>\n"
                      "<input type=\"checkbox\" name=\"tuesday\" value=\"tuesday\" checked>\n"
                      "<label for=\"tuesday\"> Tuesday </label>\n"
                      "<br>\n"
                      "<label for=\"appt\">What time shuld the lights turn on in the morning? :</label>\n"
                      "<input type=\"time\" name=\"tuesdaySTART1\" value=\"06:30\">\n"
                      "<label for=\"appt\">And what time shuld it turn off in the morning? :</label>\n"
                      "<input type=\"time\" name=\"tuesdayEND1\" value=\"07:30\">\n"
                      "<br>\n"
                      "<label for=\"appt\">What time shuld the lights turn on in the evening? :</label>\n"
                      "<input type=\"time\" name=\"tuesdaySTART2\" value=\"16:00\">\n"
                      "<label for=\"appt\">And what time shuld it turn off in the evening? :</label>\n"
                      "<input type=\"time\" name=\"tuesdayEND2\" value=\"22:30\">\n"
                      "<br>\n"
                      "<input type=\"checkbox\" name=\"wednesday\" value=\"wednesday\" checked>\n"
                      "<label for=\"wednesday\"> Wednesday</label>\n"
                      "<br>\n"
                      "<label for=\"appt\">What time shuld the lights turn on in the morning? :</label>\n"
                      "<input type=\"time\" name=\"wednesdaySTART1\" value=\"06:30\">\n"
                      "<label for=\"appt\">And what time shuld it turn off in the morning? :</label>\n"
                      "<input type=\"time\" name=\"wednesdayEND1\" value=\"07:30\">\n"
                      "<br>\n"
                      "<label for=\"appt\">What time shuld the lights turn on in the evening? :</label>\n"
                      "<input type=\"time\" name=\"wednesdaySTART2\" value=\"16:00\">\n"
                      "<label for=\"appt\">And what time shuld it turn off in the evening? :</label>\n"
                      "<input type=\"time\" name=\"wednesdayEND2\" value=\"22:30\">\n"
                      "<br>\n"
                      "<input type=\"checkbox\" name=\"thursday\" value=\"thursday\" checked>\n"
                      "<label for=\"thursday\"> Thursday</label>\n"
                      "<br>\n"
                      "<label for=\"appt\">What time shuld the lights turn on in the morning? :</label>\n"
                      "<input type=\"time\" name=\"thursdaySTART1\" value=\"06:30\">\n"
                      "<label for=\"appt\">And what time shuld it turn off in the morning? :</label>\n"
                      "<input type=\"time\" name=\"thursdayEND1\" value=\"07:30\">\n"
                      "<br>\n"
                      "<label for=\"appt\">What time shuld the lights turn on in the evening? :</label>\n"
                      "<input type=\"time\" name=\"thursdaySTART2\" value=\"16:00\">\n"
                      "<label for=\"appt\">And what time shuld it turn off in the evening? :</label>\n"
                      "<input type=\"time\" name=\"thursdayEND2\" value=\"22:30\">\n"
                      "<br>\n"
                      "<input type=\"checkbox\" name=\"friday\" value=\"friday\" checked>\n"
                      "<label for=\"friday\"> Friday</label>\n"
                      "<br>\n"
                      "<label for=\"appt\">What time shuld the lights turn on in the morning? :</label>\n"
                      "<input type=\"time\" name=\"fridaySTART1\" value=\"06:30\">\n"
                      "<label for=\"appt\">And what time shuld it turn off in the morning? :</label>\n"
                      "<input type=\"time\" name=\"fridayEND1\" value=\"07:30\">\n"
                      "<br>\n"
                      "<label for=\"appt\">What time shuld the lights turn on in the evening? :</label>\n"
                      "<input type=\"time\" name=\"fridaySTART2\" value=\"16:00\">\n"
                      "<label for=\"appt\">And what time shuld it turn off in the evening? :</label>\n"
                      "<input type=\"time\" name=\"fridayEND2\" value=\"22:30\">\n"
                      "<br>\n"
                      "<input type=\"checkbox\" name=\"saturday\" value=\"saturday\" checked>\n"
                      "<label for=\"saturday\"> Saturday</label>\n"
                      "<br>\n"
                      "<label for=\"appt\">What time shuld the lights turn on in the morning? :</label>\n"
                      "<input type=\"time\" name=\"saturdaySTART1\" value=\"06:30\">\n"
                      "<label for=\"appt\">And what time shuld it turn off in the morning? :</label>\n"
                      "<input type=\"time\" name=\"saturdayEND1\" value=\"07:30\">\n"
                      "<br>\n"
                      "<label for=\"appt\">What time shuld the lights turn on in the evening? :</label>\n"
                      "<input type=\"time\" name=\"saturdaySTART2\" value=\"16:00\">\n"
                      "<label for=\"appt\">And what time shuld it turn off in the evening? :</label>\n"
                      "<input type=\"time\" name=\"saturdayEND2\" value=\"22:30\">\n"
                      "<br>\n"
                      "<input type=\"checkbox\" name=\"sunday\" value=\"sunday\" checked>\n"
                      "<label for=\"sunday\"> Sunday</label>\n"
                      "<br>\n"
                      "<label for=\"appt\">What time shuld the lights turn on in the morning? :</label>\n"
                      "<input type=\"time\" name=\"sundaySTART1\" value=\"06:30\">\n"
                      "<label for=\"appt\">And what time shuld it turn off in the morning? :</label>\n"
                      "<input type=\"time\" name=\"sundayEND1\" value=\"07:30\">\n"
                      "<br>\n"
                      "<label for=\"appt\">What time shuld the lights turn on in the evening? :</label>\n"
                      "<input type=\"time\" name=\"sundaySTART2\" value=\"16:00\">\n"
                      "<label for=\"appt\">And what time shuld it turn off in the evening? :</label>\n"
                      "<input type=\"time\" name=\"sundayEND2\" value=\"22:30\">\n"
                      "<br>\n"
                      "<input type=\"submit\" value=\"Submit\"></form>\n";
  return HTML;
}

String prepare_root_HTML() {
  const String HTML = "<h1>Hi and welcome to my Website!</h1>\n"
                      "<p>press butten to override the lights</p>\n"
                      "<form action=\"/ON\" method=\"POST\">\n"
                      "<input type=\"submit\" value=\"Turn On Lights\"></form>\n"
                      "<form action=\"/OFF\" method=\"POST\">\n"
                      "<input type=\"submit\" value=\"Turn OFF Lights\"></form>\n"
                      "<form action=\"/Config\" method=\"POST\">\n"
                      "<input type=\"submit\" value=\"Light Config\"></form>\n"
                      "<form action=\"/ledConfig\" method=\"POST\">\n"
                      "<label for=\"ledColor\">Select a color for the LED:</label>\n"
                      "<input type=\"color\" name=\"ledColor\" value=\"#ffffff\">\n"
                      "<input type=\"submit\" value=\"Choose\"></form>\n"
                      "<form action =\"/ledShuffle\" method=\"POST\">\n"
                      "<input type=\"submit\" value=\"Shuffle LED Lights\">\n";
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

void handleRoot() {                    // When URI / is requested, send a web page with a buttons to toggle the Lights
  server.send(200, "text/html", prepare_root_HTML());
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

uint32_t hexStringToInt(const char* hex) {
  uint32_t ledColor = 0;

  for (int i = 0; hex[i]; i++) {
    char c = hex[i];
    ledColor <<= 4;

    if (c > '9') {
      ledColor += c - 'a' + 10;
    }
    else {
      ledColor += c - '0';
    }
    
  }
  return ledColor;
}

bool whiteLedOn(uint32_t color) {
  return (color & 0xFF) == ((color >> 8) & 0xFF) && (color & 0xFF) == ((color >> 16) & 0xFF);
}

void handleledConfig() {
  lightsOverride = true;
  ledset = true;
  shuffle = false;

  const char* webfarve = server.arg("ledColor").c_str();
  uint32_t color = hexStringToInt(webfarve + 1);
  if (whiteLedOn(color)) {
    strip.fill(strip.Color(0,0,0,color&0xFF));
  }
  else {
    strip.fill(color);
  }
  strip.show();

  delay(1000);
  
  server.sendHeader("Location", "/");  // Add a header to respond with a new location for the browser to go to the home page again
  server.send(303);                    // Send it back to the browser with an HTTP status 303 (See Other) to redirect
}

void handleledShuffle() {
  ledset = false;
  shuffle = !shuffle;

  server.sendHeader("Location", "/");  // Add a header to respond with a new location for the browser to go to the home page again
  server.send(303);                    // Send it back to the browser with an HTTP status 303 (See Other) to redirect
}

void theaterChase(uint32_t color, int wait) {
  for(int a=0; a<10; a++) {  // Repeat 10 times...
    for(int b=0; b<3; b++) { //  'b' counts from 0 to 2...
      strip.clear();         //   Set all pixels in RAM to 0 (off)
      // 'c' counts up from 'b' to end of strip in steps of 3...
      for(int c=b; c<strip.numPixels(); c += 3) {
        strip.setPixelColor(c, color); // Set pixel 'c' to value 'color'
      }
      strip.show(); // Update strip with new contents
      delay(wait);  // Pause for a moment
    }
  }
}
void rainbow(int wait) {
  // Hue of first pixel runs 5 complete loops through the color wheel.
  // Color wheel has a range of 65536 but it's OK if we roll over, so
  // just count from 0 to 5*65536. Adding 256 to firstPixelHue each time
  // means we'll make 5*65536/256 = 1280 passes through this outer loop:
  for(long firstPixelHue = 0; firstPixelHue < 5*65536; firstPixelHue += 256) {
    for(int i=0; i<strip.numPixels(); i++) { // For each pixel in strip...
      // Offset pixel hue by an amount to make one full revolution of the
      // color wheel (range of 65536) along the length of the strip
      // (strip.numPixels() steps):
      int pixelHue = firstPixelHue + (i * 65536L / strip.numPixels());
      // strip.ColorHSV() can take 1 or 3 arguments: a hue (0 to 65535) or
      // optionally add saturation and value (brightness) (each 0 to 255).
      // Here we're using just the single-argument hue variant. The result
      // is passed through strip.gamma32() to provide 'truer' colors
      // before assigning to each pixel:
      strip.setPixelColor(i, strip.gamma32(strip.ColorHSV(pixelHue)));
    }
    strip.show(); // Update strip with new contents
    delay(wait);  // Pause for a moment
  }
}
void theaterChaseRainbow(int wait) {
  int firstPixelHue = 0;     // First pixel starts at red (hue 0)
  for(int a=0; a<30; a++) {  // Repeat 30 times...
    for(int b=0; b<3; b++) { //  'b' counts from 0 to 2...
      strip.clear();         //   Set all pixels in RAM to 0 (off)
      // 'c' counts up from 'b' to end of strip in increments of 3...
      for(int c=b; c<strip.numPixels(); c += 3) {
        // hue of pixel 'c' is offset by an amount to make one full
        // revolution of the color wheel (range 65536) along the length
        // of the strip (strip.numPixels() steps):
        int      hue   = firstPixelHue + c * 65536L / strip.numPixels();
        uint32_t color = strip.gamma32(strip.ColorHSV(hue)); // hue -> RGB
        strip.setPixelColor(c, color); // Set pixel 'c' to value 'color'
      }
      strip.show();                // Update strip with new contents
      delay(wait);                 // Pause for a moment
      firstPixelHue += 65536 / 90; // One cycle of color wheel over 90 frames
    }
  }
}

void handleConfig() {
  OFF = false;
  lightsOverride = false;
  server.send(200, "text/html", prepare_handleConfig_HTML());
}

// Checks the Web Responce and sets the weekdayInfo corresponding.
void handlesetTimeDate() {
  for (int i = 0; i < 7; i++) {
    if (!strcmp(server.arg(daysOfTheWeek[i]).c_str(), daysOfTheWeek[i])) {

      //Start1
      char start1[strlen(daysOfTheWeek[i]) + 6];
      strcpy(start1, daysOfTheWeek[i]);
      strcpy(start1 + strlen(daysOfTheWeek[i]), "START1");
      weekdayInfo[i].start1 = strTimeToMin(server.arg(start1).c_str());

      //End1
      char end1[strlen(daysOfTheWeek[i]) + 4];
      strcpy(end1, daysOfTheWeek[i]);
      strcpy(end1 + strlen(daysOfTheWeek[i]), "END1");
      weekdayInfo[i].end1 = strTimeToMin(server.arg(end1).c_str());

      //Start2
      char start2[strlen(daysOfTheWeek[i]) + 6];
      strcpy(start2, daysOfTheWeek[i]);
      strcpy(start2 + strlen(daysOfTheWeek[i]), "START2");
      weekdayInfo[i].start2 = strTimeToMin(server.arg(start2).c_str());

      //End2
      char end2[strlen(daysOfTheWeek[i]) + 4];
      strcpy(end2, daysOfTheWeek[i]);
      strcpy(end2 + strlen(daysOfTheWeek[i]), "END2");
      weekdayInfo[i].end2 = strTimeToMin(server.arg(end2).c_str());

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

  if (weekdayInfo[weekDay].enable && weekdayInfo[weekDay].start1 < hm && weekdayInfo[weekDay].end1 > hm || weekdayInfo[weekDay].enable && weekdayInfo[weekDay].start2 < hm && weekdayInfo[weekDay].end2 > hm) {
    digitalWrite(Lights, HIGH);
    //Serial.print("Lights ON: ");
    //Serial.println(daysOfTheWeek[weekDay]);
  } else {
    digitalWrite(Lights, LOW);
    //Serial.println("LIGHTS OFF");
  }
}

void updateLED() {
  if (shuffle) {
    // Do a theater marquee effect in various colors...
    theaterChase(strip.Color(127, 127, 127), 50); // White, half brightness
    theaterChase(strip.Color(127,   0,   0), 50); // Red, half brightness
    theaterChase(strip.Color(  0,   0, 127), 50); // Blue, half brightness
    rainbow(10);             // Flowing rainbow cycle along the whole strip
    theaterChaseRainbow(50); // Rainbow-enhanced theaterChase variant
  }
  else if (!ledset) {
    strip.fill(strip.Color(0,0,0,200));
    strip.show();
  }
}