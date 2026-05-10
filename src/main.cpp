/*
description of protocol DieterEvDriver <-> Dieter:

commands (host -> MCU)
  keys                   possible values            
  set_contactor             {0,1}                            
  set_connector_lock        {0,1}   
  set_state_c               {0,1}

measurements (MCU -> host)
  u_inlet                   {0,1,2...}
  cp_duty_cycle             {0,1,..99,100}
  connector_lock_confirmed  {0,1}

each newline-terminated and separated by colon.

examples:
  "set_contactor:0\n"
  "set_cp_state:C\n"
  "u_inlet:800\n"
*/

#include <Arduino.h>


# define PIN_U_INLET_INPUT A0
# define PIN_CP_DUTY_CYCLE_INPUT 7
# define PIN_CONNECTOR_LOCK_FB 9

# define PIN_POWER_RELAY_OUTPUT 6
# define PIN_CP_MOSFET_OUTPUT 8
// 3 contactor FB
# define PIN_MOTOR_DRIVER_A 4
# define PIN_MOTOR_DRIVER_B 5

const unsigned long LOCK_TIMEOUT_MS = 600; // time-based control (not current)
const int N_AVERAGE = 10;   // averaging over several read input values
const int LINE_MAX = 42;    // top limit for msg length

char in_line[LINE_MAX];     // input line buffer
int in_len = 0;             // current length of buffer

// states of lock motor
enum class LockState {
  IDLE,
  LOCKING,
  UNLOCKING
};
// keep track of what lock motor is doing
LockState lock_state = LockState::IDLE;

unsigned long lasttime_500ms = 0;
unsigned long lock_timer_start = 0;

int u_inlet;                // DC voltage measured at HV connect, before relays
int cp_duty_cycle;          // duty cycle of C_p line
int connector_lock_confirmed = 0;// feedback switch status from CCS connector lock

void readInletVoltage() {
  float tmp = 0;
  // averaging ADC readings
  for (int i = 0; i < N_AVERAGE; i++) {
    tmp += map(analogRead(PIN_U_INLET_INPUT), 0, 1023 , 0, 1000); // dummy test value, later read from ext ADC
  }
  u_inlet = (int) tmp / N_AVERAGE;
  // later do SPI/I2C stuff here
}

void readCpDutyCycle() {
  // three cases to distinguish: logic high, logic low, or 1 kHz PWM
  unsigned long t = pulseIn(PIN_CP_DUTY_CYCLE_INPUT, HIGH, 3000); // use pulseIn timeout of 3 ms to rule out PWM
  if (t == 0) {
    // constant voltage on cp, as timeout was triggered
    if (digitalRead(PIN_CP_DUTY_CYCLE_INPUT)) cp_duty_cycle = 100; // logic high is treated as duty cycle = 100% 
    else cp_duty_cycle = 0;                                            // logic low is treated as duty cycle = 0%
  }
  else { // else there is a "true" PWM
    float tmp = 0;
    // averaging PWM
    int counter = N_AVERAGE;
    for (int i = 0; i < counter; i++) {
      int raw = pulseIn(PIN_CP_DUTY_CYCLE_INPUT, HIGH, 2000);
      if (raw == 0) counter--; // in case of timeout the reading is omitted
      tmp += raw;
    }
    if (tmp == 0) { // If the sum ist still zero, there is an error
      Serial.println("error: invalid cp duty cycle reading");
      cp_duty_cycle = -1; 
      return;
    }
    // one period is 1 ms => 50 % duty cycle: 500 us => additional division by ten
    cp_duty_cycle = (int) tmp / (counter * 10);
  }
}

void readConnLockFB() {
  // internal pullup resistor
  // for now assume low-side closing switch
  //connector_lock_confirmed = !digitalRead(PIN_CONNECTOR_LOCK_FB);
}

void publishMeasurements() {
  // schema:
  // key:value\n
  char s[30];
  sprintf(s, "u_inlet:%d", u_inlet);  
  Serial.println(s);
  sprintf(s, "cp_duty_cycle:%d", cp_duty_cycle);
  Serial.println(s);
  sprintf(s, "connector_lock_confirmed:%d", connector_lock_confirmed);
  Serial.println(s);
}

void process_line(const char *line) {
  char* separator = strchr(line, ':');
  if (separator == nullptr) {
    Serial.println("invalid syntax");
    return; // found no ':'
  }
  // extract key
  int keyLen = separator - line; // key length by substracting memory adresses
  char key[32];
  strncpy(key, line, keyLen);
  key[keyLen] = '\0'; // zero termination

  char* value = separator + 1;

  if (strcmp(key, "set_contactor") == 0) {
    int cmd = atoi(value);
    if (cmd == 1 || cmd == 0) digitalWrite(PIN_POWER_RELAY_OUTPUT, cmd);
  } 
  else if (strcmp(key, "set_connector_lock") == 0) {
    int cmd = atoi(value);
    Serial.print("Dieter cmd: "); Serial.println(cmd);
    if (cmd == 1 || cmd == 0) {
      // drive differential pins of L298N input
      // (enable of L298N is pulled high => always max speed)
      digitalWrite(PIN_MOTOR_DRIVER_A, cmd);
      digitalWrite(PIN_MOTOR_DRIVER_B, !cmd);
      lock_state = cmd==1 ? LockState::LOCKING : LockState::UNLOCKING; // set lock motor state variable
      // REMOVE LATER
      connector_lock_confirmed = cmd; // for testing
      pinMode(5, OUTPUT);   // small LED
      digitalWrite(5, connector_lock_confirmed);
    }
  }
  else if (strcmp(key, "set_state_c") == 0) {
    int cmd = atoi(value);
    if (cmd == 1 || cmd == 0) digitalWrite(PIN_CP_MOSFET_OUTPUT, cmd);
    }
else Serial.println("unknown command");
  // else unknown command
}

void connector_lock_task() {
  if (millis() - lock_timer_start >= LOCK_TIMEOUT_MS) {
    // set both inputs equal, this stops motor (see L298 datasheet)
    digitalWrite(PIN_MOTOR_DRIVER_A, 0);
    digitalWrite(PIN_MOTOR_DRIVER_B, 0);
    readConnLockFB();
    lock_state = LockState::IDLE;
  }
}

void serial_rx_task() {
  while (Serial.available() > 0) {
    char c = (char)Serial.read();
    if (c == '\r') continue;        // ignore CR
    if (c == '\n') {                // every msg is newline terminated
      if (in_len > 0) {
        in_line[in_len] = '\0';     // get rid of the newline char
        process_line(in_line);
        in_len = 0;
      }
    } else {
      if (in_len + 1 < LINE_MAX) {
        in_line[in_len++] = c;
      } else {
        // overflow → reset line
        in_len = 0;
      }
    }
  }
}

void dummyValues(){
  u_inlet = 400;
  cp_duty_cycle = 5;
  //connector_lock_confirmed = 1;
}

void setup() {
  pinMode(PIN_POWER_RELAY_OUTPUT, OUTPUT);
  pinMode(PIN_CP_MOSFET_OUTPUT, OUTPUT);
  pinMode(PIN_MOTOR_DRIVER_A, OUTPUT);
  pinMode(PIN_MOTOR_DRIVER_B, OUTPUT);
  
  digitalWrite(PIN_POWER_RELAY_OUTPUT, 0);
  digitalWrite(PIN_CP_MOSFET_OUTPUT,0);

  pinMode(PIN_CONNECTOR_LOCK_FB, INPUT_PULLUP);
  pinMode(PIN_CP_DUTY_CYCLE_INPUT, INPUT);

  analogReference(INTERNAL);

  Serial.begin(19200);
  
}

void loop() {
  
  readInletVoltage();
  readCpDutyCycle();
  readConnLockFB();

  serial_rx_task();
  connector_lock_task();

  if ((millis() - lasttime_500ms)>500) {
    lasttime_500ms = millis();
    publishMeasurements();
  }
}
