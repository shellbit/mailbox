#include <Adafruit_NeoPixel.h>
#include <EEPROM.h>
#include <avr/sleep.h>

const int SLEEP_PIN_INTERRUPT = 0; //SCL
const int RADIO_POWER_SWITCH_PIN = 2; //SDA
const int DOOR_SENSOR_PIN = 3; //SCL
const int DOOR_LIGHT_PIN = 6;
const int BOARD_LIGHT_PIN = 7;
const int AMBIENT_LIGHT_SENSOR_PIN = 12;

const int ADDRESS_DOOR_STATE = 200;

const byte MESSAGE_PREFIX[] = {0x41, 0x44, 0x41, 0x4D};
const byte MESSAGE_SUFFIX = 0x0A;
const byte MESSAGE_MAILBOX[] = {0x4D, 0x41, 0x49, 0x4C};
const byte HIGH_BYTE = 0x48;
const byte LOW_BYTE = 0x4C;
const byte MESSAGE_SLEEP[] = {0x53};
const byte MESSAGE_WAKE[] = {0x57};

const int TOTAL_LIGHTS = 13;
const int MIN_LIGHTING = 500;
const unsigned long EVENT_DELAY = 10000;
const unsigned long LIGHT_ON_MILLIS = 10000;
const unsigned long MAX_UPTIME = 10000;
const unsigned long WRITE_BUFFER_DELAY = 100;
const unsigned long RADIO_POWER_SWITCH_DELAY = 200;

boolean _doorState;
boolean _doorStateLastSent;
unsigned long _nextTimeToSendDoorOpened;
unsigned long _nextTimeToSendDoorClosed;
boolean _lightOn;
long _lightOffTime;
long _offtime;

Adafruit_NeoPixel strip = Adafruit_NeoPixel(TOTAL_LIGHTS, DOOR_LIGHT_PIN, NEO_GRB + NEO_KHZ800);

void setup() {
  //while (!Serial);
  //Serial.begin(9600);
  while (!Serial1);
  Serial1.begin(9600);
  
  pinMode(DOOR_SENSOR_PIN, INPUT);
  _doorState = EEPROM.read(ADDRESS_DOOR_STATE) == HIGH_BYTE;
  _doorStateLastSent = _doorState;
  _nextTimeToSendDoorOpened = millis();
  _nextTimeToSendDoorClosed = millis();
  
  pinMode(AMBIENT_LIGHT_SENSOR_PIN, INPUT);
  
  pinMode(BOARD_LIGHT_PIN, OUTPUT);
  digitalWrite(BOARD_LIGHT_PIN, LOW);
  _lightOn = false;
  _lightOffTime = millis();
  strip.begin();
  strip.show();
  
  pinMode(RADIO_POWER_SWITCH_PIN, OUTPUT);
  digitalWrite(RADIO_POWER_SWITCH_PIN, LOW);
  
  resetOfftime();
}

void loop() {
  if(doorStateChanged()) {
    toggleLight(_doorState);
    writeDoorEvent();
  }
  checkDoorLight();
  checkDoorState();
  checkUptime();
}

boolean doorStateChanged() {
  boolean doorState = digitalRead(DOOR_SENSOR_PIN);
  if(doorState != _doorState) {
    _doorState = doorState;
    if(_doorState) {
      EEPROM.write(ADDRESS_DOOR_STATE, HIGH_BYTE);
    }
    else {
      EEPROM.write(ADDRESS_DOOR_STATE, LOW_BYTE);
    }
    return true;
  }
  return false;
}

void toggleLight(boolean turnOn) {
  if(turnOn) {
    turnOnLight();
  }
  else {
    turnOffLight();
  }
}

void turnOnLight() {
  digitalWrite(BOARD_LIGHT_PIN, HIGH);
  for (int i = 0; i < TOTAL_LIGHTS; i++) {
    strip.setPixelColor(i, 119, 221, 237);
  }
  strip.show();
  _lightOn = true;
  _lightOffTime = millis() + LIGHT_ON_MILLIS;
}

void turnOffLight() {
  _lightOn = false;
  digitalWrite(BOARD_LIGHT_PIN, LOW);
  for (int i = 0; i < TOTAL_LIGHTS; i++) {
    strip.setPixelColor(i, 0);
  }
  strip.show();
}

void checkDoorLight() {
  if(_lightOn && _lightOffTime < millis()) {
    turnOffLight();
  }
}

void checkDoorState() {
  if(_doorState != _doorStateLastSent && waitTimeToSendDoorEventIsExpired()) {
    writeDoorEvent();
  }
}

boolean waitTimeToSendDoorEventIsExpired() {
  return (_doorState && _nextTimeToSendDoorOpened < millis()) || (!_doorState && _nextTimeToSendDoorClosed < millis());
}

void writeDoorEvent() {
  if(waitTimeToSendDoorEventIsExpired()){
    byte message[1];
    if(_doorState) {
      message[0] = HIGH_BYTE;
    }
    else {
      message[0] = LOW_BYTE;
    }
    writeBuffer(message, sizeof(message));
    _doorStateLastSent = _doorState;
  }
  if(_doorState) {
    _nextTimeToSendDoorOpened = millis() + EVENT_DELAY;
  }
  else {
    _nextTimeToSendDoorClosed = millis() + EVENT_DELAY;
  }
  resetOfftime();
}

void writeBuffer(const byte buffer[], int len) {
  Serial1.write(MESSAGE_PREFIX, sizeof(MESSAGE_PREFIX));
  Serial1.write(MESSAGE_MAILBOX, sizeof(MESSAGE_MAILBOX));
  Serial1.write(buffer, len);
  Serial1.write(MESSAGE_SUFFIX);
  delay(WRITE_BUFFER_DELAY);
}

void resetOfftime() {
  _offtime = millis() + MAX_UPTIME;
}

void checkUptime() {
  if(_offtime < millis()) {
    sleepNow();
  }
}

void sleepNow() {
  writeBuffer(MESSAGE_SLEEP, sizeof(MESSAGE_SLEEP));
  delay(RADIO_POWER_SWITCH_DELAY);
  digitalWrite(RADIO_POWER_SWITCH_PIN, HIGH);
  
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  sleep_enable();
  attachInterrupt(SLEEP_PIN_INTERRUPT, wakeNow, CHANGE);
  sleep_mode(); // device is put to sleep
  // Continue here after waking up
  sleep_disable();
  detachInterrupt(SLEEP_PIN_INTERRUPT);
  
  digitalWrite(RADIO_POWER_SWITCH_PIN, LOW);
  delay(RADIO_POWER_SWITCH_DELAY);
  writeBuffer(MESSAGE_WAKE, sizeof(MESSAGE_WAKE));
  resetOfftime();
}

void wakeNow() {
}
