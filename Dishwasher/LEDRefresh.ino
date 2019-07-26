int lastRefresh = 0;
int ledstate;
long frameTime;

void refreshLED() {
  if (millis() - frameTime > 100) {
    frameTime=millis();
    if (whatToDo == 1) {
      closeTheDoor();
    } else if (whatToDo == 2) {
      change();
    }
  }
}


void closeTheDoor() {

  for (int box = 1; box < 5; box++)
  {
    leds[0][box * 7 - ledstate] = CRGB::Black;
    leds[1][box * 7 - ledstate] = CRGB::Black;
  }
  FastLED.show();

  ledstate++;
  if (ledstate == 8) {
    whatToDo = 0;
    ledstate = 1;
  }
}

void change() {

  for (int box = 1; box < 5; box++)
  {
    leds[0][box * 7 - ledstate] = rainbowColor(10 + states[0][box - 1] * 60, 40);
    leds[1][box * 7 - ledstate] = rainbowColor(10 + states[1][box - 1] * 60, 40);
  }
  FastLED.show();


  ledstate++;
  if (ledstate == 8) {
    whatToDo = 0;
    ledstate = 1;
  }

}
