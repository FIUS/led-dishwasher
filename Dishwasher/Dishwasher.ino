#include <FastLED.h>
#include <WiFi.h>
#include "WLAN_CONF.hpp"

String debugInfo;

struct HttpResponse {
  String httpCode;
  String html;
};

const int NUM_LEDS = 4*7;
const int ENDPOINT_COUNT = 3;

const int DATA_PIN_UP = 2;
const int DATA_PIN_DOWN = 4;

CRGB leds[2][NUM_LEDS];

String endpoints[ENDPOINT_COUNT];

int status = WL_IDLE_STATUS;

WiFiServer server(80);

//Status variables


//End variables


void setup() {

  /*Defining endpoints*/
  endpoints[0] = "changeWasherState";
  endpoints[1] = "doorOpen";
  endpoints[2] = "doorClosed";

  /*Initializing LEDs and serial port*/

  FastLED.addLeds<NEOPIXEL, DATA_PIN_UP>(leds[0], NUM_LEDS);
  FastLED.addLeds<NEOPIXEL, DATA_PIN_DOWN>(leds[1], NUM_LEDS);
  Serial.begin(9600);
  FastLED.show();

  /*Connecting to Wifi or starting accesspoint*/
  setupWlan();

}

void loop() {
  refreshPage();
  refreshLED();
}

void setProgress(int character, int startIndex, int color) {

  CHSV hsv( color, 255, 100);
  CRGB rgb;
  hsv2rgb_rainbow( hsv, rgb);

  leds[0][startIndex] = rgb;
  leds[1][startIndex] = rgb;

  FastLED.show();
}

void setupWlan() {

  /*Trying to connect to wifi*/
  for (int i = 0; i < 5 && status != WL_CONNECTED; i++) {
    Serial.print("Trying to connect to SSID: ");
    Serial.println(ssid);

    status = WiFi.begin(ssid, pass);

    Serial.println(status);
    Serial.println(WiFi.localIP());

    int startindex = 0;
    setProgress('o', startindex + i, 100);


    delay(5000);
  }

  /*Clear leds*/
  for (int j = 0; j < NUM_LEDS; j++) {
    leds[0][j] = CRGB::Black;
    leds[1][j] = CRGB::Black;
  }
  FastLED.show();

  /*If could'nt connect to wifi then open acesspoint*/
  if (status != WL_CONNECTED) {
    WiFi.softAP("chroma", "PeterDerWolf");
    Serial.println("A");
    setProgress('A', 0, 240);
  } else {
    Serial.println("W");
    setProgress('W', 0, 160);
  }
  Serial.println("started");
  /*Start webserver*/
  server.begin();
  Serial.println("started2");
  delay(5000);
}

void refreshPage() {

  WiFiClient client = server.available();

  bool check = false;
  HttpResponse resp;
  if (client) {
    
    /*Client connected to webserver*/
    String parsingString = "";


    bool currentLineIsBlank = true;
    HttpResponse httpResponse;
    httpResponse.httpCode = "HTTP/1.1 405 METHOD NOT ALLOWED";
    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        parsingString += c;
        int startIndex = parsingString.indexOf("POST");


        if (parsingString.lastIndexOf("HTTP/1.1") != -1 && !check) {
          /*We received the whole url*/
          check = true;

          int endIndex = parsingString.lastIndexOf("HTTP/1.1");
          parsingString = parsingString.substring(startIndex, endIndex);
          parsingString = parsingString.substring(7, parsingString.length() - 1);
          if (parsingString.indexOf("favicon.ico") == -1) {
            httpResponse = reactOnHTTPCall(parsingString);

          }

        }

        if (c == '\n' && currentLineIsBlank) {
          // send a standard http response header

          client.println(httpResponse.httpCode);
          client.println("Content-Type: text/html");
          client.println("Connection: close");  // the connection will be closed after completion of the response
          client.println();

          client.println(httpResponse.html);

          break;
        }
        if (c == '\n') {
          currentLineIsBlank = true;
        } else if (c != '\r') {
          currentLineIsBlank = false;
        }
      }
    }

    delay(1);

    client.stop();

  }
}

HttpResponse reactOnHTTPCall(String message) {
  //debugInfo += message + "\n";
  String temp = "";
  String output = "HTTP/1.1 200 OK";
  int match = -1;
  String html = "";
  //debugInfo += message + "\n";
  /*Finding out what endpoint is called*/
  for (int i = 0; i < ENDPOINT_COUNT; i++) {
    if (message.startsWith(endpoints[i])) {
      temp = message.substring(endpoints[i].length() + 1);
      match = i;
    }
  }
  /*
   * 
   * Variable temp will contain any text send with the post request
   */
  //debugInfo += temp + "\n";
  /*Replacing http substituted character*/
  temp.replace("%20", " ");
  //debugInfo += temp + "\n";

  /*Parsing the endpoint info*/
  if (match == 0) {
    int r = temp.substring(0, 3).toInt();
    int g = temp.substring(3, 6).toInt();
    int b = temp.substring(6, 9).toInt();

    //color = CRGB(r, g, b);
    //onColorChanged = true;

  } 
  if (match == -1) {
    output = "HTTP/1.1 404 NO ENDPOINT";
    html = "Endpoint tried: " + message;
  }
  HttpResponse resp;
  resp.httpCode = output;
  resp.html = html;

  return resp;

}

CRGB rainbowColor(int i) {
  i %= 256;
  CHSV hsv(i, 255, 255);
  CRGB rgb;
  hsv2rgb_rainbow( hsv, rgb); return rgb;
}

CRGB rainbowColor(int i, int brightness) {
  i %= 256;
  CHSV hsv(i, 255, brightness);
  CRGB rgb;
  hsv2rgb_rainbow( hsv, rgb); return rgb;
}
CRGB rainbowColor(int i, int saturation, int brightness) {
  i %= 256;
  CHSV hsv(i, saturation, brightness);
  CRGB rgb;
  hsv2rgb_rainbow( hsv, rgb); return rgb;
}
void clearLED(int side) {
  for (int i = 0; i < NUM_LEDS; i++) {
    leds[side][i] = CRGB(0, 0, 0);
  }
}
void fadeAll() {
  for (int i = NUM_LEDS - 1; i >= 0; i--) {
    leds[0][i].fadeToBlackBy(4);
    leds[1][i].fadeToBlackBy(4);
  }
}
