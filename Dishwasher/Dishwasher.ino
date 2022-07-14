#include <FastLED.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

//#define DEBUG

#define WIFI_SSID "my ssid"
#define WIFI_PASSWORD "my pw"

#define MQTT_DEVICE_NAME "FIUS-DISHWASHER_LED-ESP"
#define MQTT_SERVER "my server"
#define MQTT_PORT 1883
#define MQTT_USER "my user"
#define MQTT_PASS "my pw"

#define MQTT_TOPIC_POWERSTATE "my topic"
#define MQTT_TOPIC_DISHWASHER_PREFIX "my topic"
#define MQTT_TOPIC_DISHWASHER_SUFFIX "state"

#define MQTT_MAX_PAYLOAD_SIZE 500

//times in msclientclient
#define LED_PATTERN_WIFI_CONN_ON_TIME 1000
#define LED_PATTERN_WIFI_CONN_OFF_TIME 1000
#define LED_PATTERN_MQTT_CONN_ON_TIME 2000
#define LED_PATTERN_MQTT_CONN_OFF_TIME 2000
#define LED_PATTERN_PUBLISH_ON_TIME 500
#define LED_PATTERN_STARTUP_ON_TIME 3000
#define SERIAL_STATE_INFORM_PERIOD 5000
#define REBOOT_AFTER_DISCONNECTED_FROM_WIFI_FOR 300000 //5 minutes
#define LED_STRIP_ANIMATION_FRAME_TIME 500

#define NUM_LEDS 28 // 4 * 7

#define LED_BRIGHTNESS 128

#define DATA_PIN_UP 2
#define DATA_PIN_DOWN 4

//Use this file to overwrite any settings from above
#include "settings.h"

enum class State { WifiConnecting, MqttConnecting, Running };
enum class DishwasherState { Empty, Running, Done };

State current_state;
State previous_state;
long current_time;

boolean blink_state;
long blink_last_switch;

long last_serial_state_inform;

long last_wifi_connection;
int current_wifi_status;

int current_mqtt_state;

boolean current_sensor_state;
boolean last_sent_sensor_state;
int sensor_state_debounce_count;

WiFiClient wifiClient = WiFiClient();
PubSubClient mqttClient = PubSubClient(wifiClient);

CRGB led_array[2][NUM_LEDS];

int number_of_active_leds;
DishwasherState dishwasher_states[8];
boolean on_state;

long last_animation_frame;

void setup() {
  // Initialize Serial
  Serial.begin(9600);
  Serial.println("[BOOTUP] ESP bootup.");

#ifdef DEBUG
  Serial.print("[WIFI] Connecting to wifi ");
  Serial.print(WIFI_SSID);
  Serial.print(" with password ");
  Serial.print(WIFI_PASSWORD);
  Serial.println(".");
#endif

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

#ifdef DEBUG
  Serial.print("[MQTT] Setting mqtt server to ");
  Serial.print(MQTT_SERVER);
  Serial.print(" and port to ");
  Serial.print(MQTT_PORT);
  Serial.println(".");
#endif

  mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
  mqttClient.setCallback(callback);

  current_state = State::WifiConnecting;
  current_time = 0;
  blink_state = false;
  blink_last_switch = 0;
  last_serial_state_inform = -SERIAL_STATE_INFORM_PERIOD;
  last_wifi_connection = 0;
  number_of_active_leds = 0;
  last_animation_frame = 0;

  FastLED.addLeds<NEOPIXEL, DATA_PIN_UP>(led_array[0], NUM_LEDS);
  FastLED.addLeds<NEOPIXEL, DATA_PIN_DOWN>(led_array[1], NUM_LEDS);

  FastLED.setBrightness(LED_BRIGHTNESS);

  //signal bootup
  led_array[0][0] = CRGB::White;
  FastLED.show();
  delay(LED_PATTERN_STARTUP_ON_TIME);
  led_array[0][0] = CRGB::Black;
  FastLED.show();
  Serial.println("[BOOTUP] setup done.");
}

void manageTime() {
  long new_current_time = millis();
  if(new_current_time < current_time) {
    //handle potential overflow
    blink_last_switch = new_current_time + (blink_last_switch - current_time);
    last_serial_state_inform = new_current_time + (last_serial_state_inform - current_time);
    last_wifi_connection = new_current_time + (last_wifi_connection - current_time);
  }
  current_time = new_current_time;
}

void manageLED() {
  if (current_state != previous_state) {
    blink_state = false;
    blink_last_switch = current_time;
  }

  long target_time;
  if(current_state == State::WifiConnecting) {
    if(blink_state) {
      target_time = blink_last_switch + LED_PATTERN_WIFI_CONN_ON_TIME;
    } else {
      target_time = blink_last_switch + LED_PATTERN_WIFI_CONN_OFF_TIME;
    }
  } else if (current_state == State::MqttConnecting ) {
    if(blink_state) {
      target_time = blink_last_switch + LED_PATTERN_MQTT_CONN_ON_TIME;
    } else {
      target_time = blink_last_switch + LED_PATTERN_MQTT_CONN_OFF_TIME;
    }
  } else {
    // We do not have a debug LED, just the display LEDs.
    // Therefore we don't do anything when running.
    return;
  }

  if(target_time < current_time) {
    blink_state = ! blink_state;
    blink_last_switch = current_time;
  }

  if(blink_state) {
    led_array[0][0] = CRGB::White;
  } else {
    led_array[0][0] = CRGB::Black;
  }

  FastLED.show();
}

void informOverSerial() {
  long target_time;
  target_time = last_serial_state_inform + SERIAL_STATE_INFORM_PERIOD;
  if(target_time < current_time || current_state != previous_state) {
    String state_info = "";

    switch (current_state) {
      case State::WifiConnecting:
        state_info = "WifiConnecting. Wifi Status: ";
        state_info.concat(current_wifi_status);
        state_info.concat("; Time until reset: ");
        state_info.concat((last_wifi_connection + REBOOT_AFTER_DISCONNECTED_FROM_WIFI_FOR - current_time)/1000);
        state_info.concat("s");
        break;
      case State::MqttConnecting:
        state_info = "MqttConnecting. MQTT State: ";
        state_info.concat(current_mqtt_state);
        break;
      case State::Running:
        state_info = "Running. On-State: ";
        if(on_state) {
          state_info.concat("On");
        } else {
          state_info.concat("Off");
        }
        state_info.concat("; Dishwasher_State: ");
        for(int i = 0; i < 8; i++) {
          DishwasherState dishwasher_state = dishwasher_states[i];
          if(dishwasher_state == DishwasherState::Empty) {
            state_info.concat("E");
          } else if(dishwasher_state == DishwasherState::Running) {
            state_info.concat("R");
          } else {
            state_info.concat("D");
          }
        }
        break;
    }

    Serial.println("[State] Current State: " + state_info);
    last_serial_state_inform = current_time;
  }
}

void manageWifi() {
  current_wifi_status = WiFi.status();
  if(current_wifi_status == WL_CONNECTED) {
    if(current_state == State::WifiConnecting) {
      current_state = State::MqttConnecting;
    }
    last_wifi_connection = current_time;
  } else {
    if(current_state != State::WifiConnecting) {
      Serial.print("[WIFI] Lost Wifi connection. State: ");
      Serial.println(current_wifi_status);
    }
    current_state = State::WifiConnecting;

    if(last_wifi_connection + REBOOT_AFTER_DISCONNECTED_FROM_WIFI_FOR < current_time) {
      Serial.println("[WIFI] No wifi for to long. Resetting.");
      Serial.flush();
      ESP.restart();
    }
  }
}

String getDishwasherName(int id) {
  switch(id) {
    case 0: return "miraculix";
    case 1: return "idefix";
    case 2: return "obelix";
    case 3: return "asterix";
    case 4: return "donald";
    case 5: return "track";
    case 6: return "trick";
    case 7: return "tick";
  }
  return "Unknown";
}

/*
 * Subsribe to all required mqtt topics
 */
void subscribeToTopics() {
  Serial.print("[MQTT] Subscribing to on/off topic: ");
  Serial.println(MQTT_TOPIC_POWERSTATE);
  mqttClient.subscribe(MQTT_TOPIC_POWERSTATE);
#ifndef DEBUG
    Serial.print("[MQTT] Subscribing to dishwasher topics like: ");
    Serial.print(MQTT_TOPIC_DISHWASHER_PREFIX);
    Serial.print("x");
    Serial.println(MQTT_TOPIC_DISHWASHER_SUFFIX);
#endif
  for(int i = 0; i < 8; i++) {
    String topic = MQTT_TOPIC_DISHWASHER_PREFIX;
    topic.concat(getDishwasherName(i));
    topic.concat(MQTT_TOPIC_DISHWASHER_SUFFIX);
    char* topic_chars = new char[topic.length() + 1];
    topic.toCharArray(topic_chars, topic.length() + 1);
#ifdef DEBUG
    Serial.print("[MQTT] Subscribing to dishwasher topic: ");
    Serial.println(topic_chars);
#endif
    mqttClient.subscribe(topic_chars);
  }
}

/*
 * Handles the mqtt messages for turning on and off.
 */
void handleOnOff(StaticJsonDocument<MQTT_MAX_PAYLOAD_SIZE> payload) {
  if(!payload.containsKey("powerstate")) {
    Serial.println("[MQTT][ON/OFF] Missing powerstate field in payload.");
  }
  String powerstate = String((const char*) payload["powerstate"]);

  if(powerstate == "off") {
    on_state = false;
    Serial.println("[MQTT][ON/OFF] Turning off.");
  } else if (powerstate == "on") {
    on_state = true;
    Serial.println("[MQTT][ON/OFF] Turning on.");
  } else {
    Serial.println("[MQTT][ON/OFF] Unknown powerstate.");
  }
}


/*
 * Handles the mqtt messages for the dishwasher state.
 */
void handleDishwasherState(String topic, StaticJsonDocument<MQTT_MAX_PAYLOAD_SIZE> payload) {
  if(! topic.startsWith(MQTT_TOPIC_DISHWASHER_PREFIX)) {
    Serial.println("[MQTT][DISHWASHER] Weird topic start");
  }
  if(! topic.endsWith(MQTT_TOPIC_DISHWASHER_SUFFIX)) {
    Serial.println("[MQTT][DISHWASHER] Weird topic end");
  }
  if(!payload.containsKey("dishwasher")) {
    Serial.println("[MQTT][DISHWASHER] Missing dishwasher field in payload.");
    return;
  }
  String dishwasher_name = String((const char*)payload["dishwasher"]);

  int dishwasher = -1;
  for(int i = 0; i < 8; i++) {
    if(dishwasher_name == getDishwasherName(i)) {
      dishwasher = i;
    }
  }

  if(dishwasher == -1) {
    Serial.print("[MQTT][DISHWASHER] Unknown dishwasher: ");
    Serial.println(dishwasher_name);
    return;
  }

  if(!payload.containsKey("state")) {
    Serial.println("[MQTT][DISHWASHER] Missing state field in payload.");
    return;
  }
  String state = String((const char*)payload["state"]);

  if(state == "empty") {
    dishwasher_states[dishwasher] = DishwasherState::Empty;
    Serial.print("[DISHWASHER] New state for ");
    Serial.print(dishwasher);
    Serial.println(": Empty.");
  } else if (state == "running") {
    dishwasher_states[dishwasher] = DishwasherState::Running;
    Serial.print("[DISHWASHER] New state for ");
    Serial.print(dishwasher);
    Serial.println(": Running.");
  } else if (state == "done") {
    dishwasher_states[dishwasher] = DishwasherState::Done;
    Serial.print("[DISHWASHER] New state for ");
    Serial.print(dishwasher);
    Serial.println(": Done.");
  } else {
    Serial.print("[MQTT][DISHWASHER] Unknown state: ");
    Serial.println(state);
  }
}

/*
 * Function receiving the mqtt messages
 */
void callback(char* topic_chars, byte* payload_bytes, unsigned int len) {
  StaticJsonDocument<MQTT_MAX_PAYLOAD_SIZE> payload;
  DeserializationError error = deserializeJson(payload, payload_bytes, len);
  if (error) {
    Serial.print("[MQTT] Deserialization error: ");
    Serial.println(error.f_str());
    return;
  }
  String topic = String(topic_chars);
#ifdef DEBUG
  Serial.print("[MQTT] Got message on ");
  Serial.print(topic);
  Serial.print(": ");
  serializeJson(payload, Serial);
  Serial.println("");
#endif
  if(topic == MQTT_TOPIC_POWERSTATE) {
    handleOnOff(payload);
  } else {
    handleDishwasherState(topic, payload);
  }
}

void manageMqtt() {
  current_mqtt_state = mqttClient.state();
  if(mqttClient.connected()) {
    if(current_state == State::MqttConnecting) {
      subscribeToTopics();
      current_state = State::Running;
    }
  } else {
    if(current_state == State::Running) {
      Serial.print("[MQTT] Lost MQTT connection. State: ");
      Serial.println(current_mqtt_state);
      current_state = State::MqttConnecting;
    } else if (current_state == State::MqttConnecting) {
      mqttClient.connect(MQTT_DEVICE_NAME, MQTT_USER, MQTT_PASS);
    }
  }
}

/*
 * Get the correct color for the dishwasher_state
 */
CRGB color_for_dishwasher_state(DishwasherState state) {
  if(state == DishwasherState::Empty) {
    return CRGB(0, 118, 186);
  } else if(state == DishwasherState::Running) {
    return CRGB(162, 255, 0);
  } else {
    return CRGB(255, 10, 0);
  }
}


/*
 * Render the given states to the led_array.
 *
 * The dishwasher_states array should be an array of 8 dishwasher states.
 *
 * The on_state is a boolean representing whether the sign should be on.
 */
void renderToLeds() {
  if(current_time < last_animation_frame + LED_STRIP_ANIMATION_FRAME_TIME) {
    return;
  }

  if(on_state) {
    if(number_of_active_leds < NUM_LEDS) {
      number_of_active_leds += 1;
    }
  } else {
    if(number_of_active_leds > 0) {
      number_of_active_leds -= 1;
    }
  }

  for(int i = NUM_LEDS - 1; i >= 0; i--) {
    int dishwasher_column = i/7;
    int dishwasher_led_num = i%7;
    CRGB color_upper_row, color_lower_row;
    if(i < number_of_active_leds) {
      if(dishwasher_led_num == 0) {
        color_upper_row = color_for_dishwasher_state(dishwasher_states[dishwasher_column]);
        color_lower_row = color_for_dishwasher_state(dishwasher_states[dishwasher_column + 4]);
      } else {
        color_upper_row = led_array[0][i-1];
        color_lower_row = led_array[1][i-1];
      }
    } else {
      color_upper_row = CRGB::Black;
      color_lower_row = CRGB::Black;
    }
    led_array[0][i] = color_upper_row;
    led_array[1][i] = color_lower_row;
  }

  FastLED.show();
}

void loop() {
  manageTime();
  manageWifi();
  manageMqtt();
  manageLED();
  informOverSerial();

  // Begin of actual logic
  if(current_state == State::Running) {
    renderToLeds();
  }
  // End of actual logic

  mqttClient.loop();
  delay(20);
  previous_state = current_state;
}
