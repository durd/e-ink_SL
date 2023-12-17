/*
   Inkplate6_Black_And_White example for Soldered Inkplate 6
   For this example you will need only USB cable and Inkplate 6.
   Select "e-radionica Inkplate6" or "Soldered Inkplate6" from Tools -> Board menu.
   Don't have "e-radionica Inkplate6" or "Soldered Inkplate6" option? Follow our tutorial and add it:
   https://soldered.com/learn/add-inkplate-6-board-definition-to-arduino-ide/

   This example will show you how you can draw some simple graphics using
   Adafruit GFX functions. Yes, Inkplate library is 100% compatible with GFX lib!
   Learn more about Adafruit GFX: https://learn.adafruit.com/adafruit-gfx-graphics-library )

   Want to learn more about Inkplate? Visit www.inkplate.io
   Looking to get support? Write on our forums: https://forum.soldered.com/
   1 December 2022 by Soldered
*/

// Next 3 lines are a precaution, you can ignore those, and the example would also work without them
#if !defined(ARDUINO_ESP32_DEV) && !defined(ARDUINO_INKPLATE6V2)
#error "Wrong board selection for this example, please select e-radionica Inkplate6 or Soldered Inkplate6 in the boards menu."
#endif

#include "Inkplate.h"             //Include Inkplate library to the sketch
#include "ArduinoJson.h"
#include "driver/rtc_io.h" //ESP32 library used for deep sleep and RTC wake up pins

// Create objects from included libraries
WiFiClientSecure client;
HTTPClient http;
Inkplate display(INKPLATE_1BIT);  // Create object on Inkplate library and set library to work in monochorme mode
DynamicJsonDocument doc(50 * 1024);

// Delay in milliseconds between screen refresh. Refreshing e-paper screens more often than 5s is not recommended.
// Want to refresh faster? Use partial update! Find example in "Inkplate6_Partial_Update"
#define DELAY_MS 5000
// Specify the delay time between 2 POST requests in milliseconds
#define DELAY_BETWEEN_REQUESTS 10000
#define uS_TO_S_FACTOR 1000000 // Conversion factor for micro seconds to seconds
#define TIME_TO_SLEEP 60      // How long ESP32 will be in deep sleep (in seconds)

// Enter your WiFi credentials
const char *ssid = "ssid";
const char *pass = "pass";
void setIOExpanderForLowPower();

// Enter your API key for SL Platsuppslag here (if you don't need SL Platsuppslag just set fetchStopInfo to false):
const String PUAPIKey       = "key";
// Enter your API key for SL Realtidsinformation 4 here:
const String RTD4APIKey     = "key";
// Enter site ID (e.g. 9001 for T-centralen, 9192 for Slussen):
const String RTD4siteID     = "9001";
// Enter time window (60 minutes is max):
const String RTD4timeWindow = "60";

const bool fetchStopInfo = false; // Set to false if not using the SL Platsuppslag API.
const String lookForMetros = "false";
const String lookForBuses  = "true";
const String lookForTrains = "true";
const String lookForTrams  = "false";
const String lookForShips  = "false";
const String enablePrediction = "true";
const bool lookForStopPointDeviations  = false;

// Specify the API URL to send a POST request
const String apiUrl = "https://api.sl.se/api2/realtimedeparturesV4.json";
// const String apiUrl = "/api2/realtimedeparturesV4.json";
const char* apiHostname = "api.sl.se";

void setup() {
    // Init serial communication
    Serial.begin(115200);

    // Connect to WiFi
    WiFi.begin(ssid, pass);
    Serial.println("Connecting to WiFi");
    int waited = 0;
    while (WiFi.status() != WL_CONNECTED) {
        Serial.print(".");
        delay(500);
        if (waited == 20) {
          ESP.restart();
        }
        waited++;
    }
    Serial.println();
    Serial.print("Connected to WiFi with IP address ");
    Serial.println(WiFi.localIP());

    // Use https but don't use a certificate, because the ESP does not know of any root CA's
    client.setInsecure();
    delay(1000);
    while (!get_RTD4()) {
      Serial.println("Retrying get_RTD4() in 5 seconds...");
      delay(5000);
    }
    // while (!do_screen()) {
    //   Serial.println("Retrying do_screen()");
    //   do_screen();
    //   delay(5000);
    // }
    // get_RTD4();
    do_screen();
    Serial.print("Battery voltage: ");
    Serial.println(display.readBattery());
    Serial.print("Sleeping for ");
    Serial.print(TIME_TO_SLEEP);
    Serial.println(" seconds...");
    rtc_gpio_isolate(GPIO_NUM_12);
    esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR); // Activate wake-up timer -- wake up after 20s here
    esp_deep_sleep_start();                                        // Put ESP32 into deep sleep. Program stops here.
}

bool get_RTD4() {
  if (!client.connect(apiHostname, 443)) {
    Serial.print("Connection failed to: ");
    Serial.println(apiHostname);
  } else {
    Serial.print("Connected to server: ");
    Serial.println(apiHostname);
    // https://api.sl.se/api2/realtimedeparturesV4.json?key=key&timewindow=15&siteid=9529&bus=true&train=true&tram=false&ship=false&enableprediction=true
    client.print("GET " + apiUrl);
    client.print("?key=" + RTD4APIKey);
    client.print("&timewindow=" + RTD4timeWindow);
    client.print("&siteid=" + RTD4siteID);
    client.print("&metro=" + lookForMetros);
    client.print("&bus=" + lookForBuses);
    client.print("&train=" + lookForTrains);
    client.print("&ship=" + lookForShips);
    client.print("&tram=" + lookForTrams);
    client.print("&enableprediction=" + enablePrediction);
    client.println(" HTTP/1.1");
    client.print("Host: ");
    client.println(apiHostname);
    client.println("Connection: close");
    if (client.println() == 0) {
      Serial.println(F("Failed to send request"));
      client.stop();
       return 0;
    }
    // Check HTTP status
    char status[32] = {0};
    client.readBytesUntil('\r', status, sizeof(status));
    if (strcmp(status, "HTTP/1.1 200 OK") != 0) {
      Serial.print("Unexpected response: ");
      Serial.println(status);
      client.stop();
      return 0;
    }
    // Skip HTTP headers
    char endOfHeaders[] = "\r\n\r\n";
    if (!client.find(endOfHeaders)) {
      Serial.println("Invalid response");
      client.stop();
      return 0;
    }
    DeserializationError error = deserializeJson(doc, client);
    if (error) {
      Serial.print("deserializeJson() failed: ");
      Serial.println(error.c_str());
      return 0;
    }
    // serializeJsonPretty(doc, Serial);
    client.stop();
    if (doc["StatusCode"] != 0) {
      Serial.println(strdup(doc["StatusCode"]));
      return 0;
    }
    return 1;
  }
  return 0;
}

void do_screen() {
  int16_t x, y;
  uint16_t w, h;
  // display.getTextBounds(bleh, 0, 0, &x, &y, &w, &h);
  display.begin();         // Init library (you should call this function ONLY ONCE)
  display.clearDisplay();  // Clear any data that may have been in (software) frame buffer.
  //(NOTE! This does not clean image on screen, it only clears it in the frame buffer inside ESP32).
  display.display();  // Clear everything that has previously been on a screen

  JsonObject ResponseData = doc["ResponseData"];
  display.println(strdup(ResponseData["LatestUpdate"]));

  // char* orig = strdup(ResponseData["Trains"][0]["StopAreaName"]);
  // display.printlnUTF8(orig);
  // display.drawThickLine(272, 65, 468, 65, BLACK, 3);

  display.setTextSize(2);
  char prBuffer[100];
  char* line = "Linje";
  char* destination = "Destination";
  char* track = "Spår";
  char* time = "Tid";
  char* issues = "#I";
  char* prediction = "?";
  
  // sprintf(prBuffer, " %-5s %-29s %-3s %07s", line, destination, track, time);
  // sprintf(prBuffer, " %-4s %5s %-5s %-29s", track, time, line, destination);
  sprintf(prBuffer, " %-4s %7s %2s %2s %-29s", track, time, issues, prediction, destination);
  display.printlnUTF8(prBuffer);
  Serial.println(prBuffer);
  for (JsonObject ResponseData_Train : ResponseData["Trains"].as<JsonArray>()) {
    int cancelled = 0;
    char* line = strdup(ResponseData_Train["LineNumber"]);
    char* destination = strdup(ResponseData_Train["Destination"]);
    char buf[40];
    char* track = strdup(ResponseData_Train["StopPointDesignation"]);
    char* time = strdup(ResponseData_Train["DisplayTime"]);
    size_t issues = ResponseData_Train["Deviations"].size();
    if (ResponseData_Train["Deviations"]) {
      for (int i = 0; i < ResponseData_Train["Deviations"].size(); i++) {
        if (ResponseData_Train["Deviations"][i]["Consequence"] == "CANCELLED") {
          strcpy(buf, "--- ");
          strcat(buf, line);
          strcat(buf, " ");
          strcat(buf, destination);
          strcat(buf, " ---");
          Serial.println("Train cancelled, continuing loop");
          cancelled = 1;
        } else {
          strcpy(buf, line);
          strcat(buf, " ");
          strcat(buf, destination);
        }
      }
    } else {
      strcpy(buf, line);
      strcat(buf, " ");
      strcat(buf, destination);
    }
    if (cancelled) {
      // continue;
    }
    if (ResponseData_Train["PredictionState"] == "NORMAL") {
      prediction = "-";
    } else {
      prediction = "!";
    }
    // if (ResponseData_Train["Deviations"]) {
    //   for (JsonObject ResponseData_Train_Deviation : ResponseData["Deviations"].as<JsonArray>()) {
    //     issues = sizeof(ResponseData_Train_Deviation) / sizeof(ResponseData_Train_Deviation[0]);
    // }

    // sprintf(prBuffer, " %-5s %-29s %-3s %010s", line, destination, track, time);
    // sprintf(prBuffer, " %-4s %5s %-5s %-29s", track, time, line, destination);
    sprintf(prBuffer, " %-4s %7s %2i %2s %-29s", track, time, issues, prediction, buf);
    display.printlnUTF8(prBuffer);
    Serial.println(prBuffer);
  }
  // display.setCursor(time_width, display.getCursorY());
  // display.printlnUTF8(time);


  // Serial.println(strdup(orig));
  display.display();  // Write hello message
  // delay(5000);       // Wait a little bit

  // Write some text on screen with different sizes
  // display.clearDisplay();
  // for (int i = 0; i < 6; i++)
  // {
  //     display.setTextSize(i +
  //                         1); // textSize parameter starts at 0 and goes up to 10 (larger won't fit Inkplate 6 screen)
  //     display.setCursor(200, (i * i * 8)); // setCursor works as same as on LCD displays - sets "the cursor" at the
  //                                          // place you want to write someting next
  //     display.print("INKPLATE 6!");        // The actual text you want to show on e-paper as String
  // }
  // displayCurrentAction("Text in different sizes and shadings");

  // Write same text on different location, but now invert colors (text is white, text background is black), without
  // cleaning the previous text
  // display.setTextColor(
  //     WHITE, BLACK); // First argument is text color, while second argument is background color. In BW, there are
  // for (int i = 0; i < 6; i++)
  // { // only two options: BLACK & WHITE
  //     display.setTextSize(i + 1);
  //     display.setCursor(200, 300 + (i * i * 8));
  //     display.print("INKPLATE 6!");
  // }
  // display.display();
}

String asciirize(String text) {
  text.replace("Å", "AA");
  text.replace("Ä", "AE");
  text.replace("Ö", "OE");
  text.replace("å", "aa");
  text.replace("ä", "ae");
  text.replace("ö", "oe");
  return text;
}

void loop() {
  // // Write some text on screen with different sizes
  // display.clearDisplay();
  // for (int i = 0; i < 6; i++)
  // {
  //     display.setTextSize(i +
  //                         1); // textSize parameter starts at 0 and goes up to 10 (larger won't fit Inkplate 6 screen)
  //     display.setCursor(200, (i * i * 8)); // setCursor works as same as on LCD displays - sets "the cursor" at the
  //                                          // place you want to write someting next
  //     display.print("INKPLATE 6!");        // The actual text you want to show on e-paper as String
  // }
  // displayCurrentAction("Text in different sizes and shadings");
  // display.display(); // To show stuff on screen, you always need to call display.display();
  // delay(DELAY_MS);

  // // Write same text on different location, but now invert colors (text is white, text background is black), without
  // // cleaning the previous text
  // display.setTextColor(
  //     WHITE, BLACK); // First argument is text color, while second argument is background color. In BW, there are
  // for (int i = 0; i < 6; i++)
  // { // only two options: BLACK & WHITE
  //     display.setTextSize(i + 1);
  //     display.setCursor(200, 300 + (i * i * 8));
  //     display.print("INKPLATE 6!");
  // }
  // display.display();
  // delay(DELAY_MS);


  // Write text and rotate it by 90 deg. forever
  // int r = 0;
  // display.setTextSize(8);
  // display.setTextColor(WHITE, BLACK);
  // while (true)
  // {
  //     display.setCursor(100, 100);
  //     display.clearDisplay();
  //     display.setRotation(
  //         r); // Set rotation will sent rotation for the entire display, so you can use it sideways or upside-down
  //     display.print("INKPLATE6");
  //     display.display();
  //     r++;
  //     delay(DELAY_MS);
  // }
}

// Small function that will write on the screen what function is currently in demonstration.
void displayCurrentAction(String text) {
  display.setTextSize(2);
  display.setCursor(2, 580);
  display.print(text);
}
