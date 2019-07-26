int lastRefresh = 0;

void refreshLED() {
 if (whatToDo == 0) {
    closeTheDoor();
  } else if (whatToDo == 1) {
    openTheDoor();
  } else if (whatToDo == 2) {
    changeColor();
  }
  
  
  openTheDoor();
delay(2000);
  closeTheDoor();
  }

void changeColor() {

}

void closeTheDoor() {
  for (int i = 1; i < 8; i++)
  {
    for (int box = 1; box < 5; box++)
    {
      leds[0][box*7-i] = CRGB::Black;
      leds[1][box*7-i] = CRGB::Black;
    }
    FastLED.show();
    delay(100);

  }
}

void openTheDoor() {
  for (int i = 1; i < 8; i++)
  {
    for (int box = 1; box < 5; box++)
    {
      Serial.println(box-1);
      leds[0][box*7-i] = rainbowColor(10+states[0][box-1]*60,40);
      leds[1][box*7-i] = rainbowColor(10+states[1][box-1]*60,40);
    }
    FastLED.show();
    delay(100);

  }
}
