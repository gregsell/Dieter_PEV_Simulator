#include <Arduino.h>

# define U_INLET_INPUT A0
# define CP_DUTY_CYCLE_INPUT 7

# define POWER_RELAY_OUTPUT 10
# define CP_MOSFET_OUTPUT 12
// add connector_lock

int u_inlet;
int cp_duty_cycle;

void readInputs() {
  float tmp = 0;
  int n_average = 5;
  // avergaing ADC readings
  for (int i = 0; i < n_average; i++) {
    tmp += map(analogRead(U_INLET_INPUT), 0, 1023 , 0, 1000); // dummy test value, later read from ext ADC
  }
  u_inlet = (int) tmp /n_average;
  // separate loop for averaging PWM, as timeout can mess things up
  for (int i = 0; i < n_average; i++) {
    int raw = pulseIn(CP_DUTY_CYCLE_INPUT, HIGH, 2000);
    if (raw == 0) i--;
    // i-- can lead to endless loop
    // but n_average-- can lead to division by zero :((
    tmp += raw;
  }
  cp_duty_cycle = (int)
}
void setup() {
  pinMode(POWER_RELAY_OUTPUT, OUTPUT);
  pinMode(CP_MOSFET_OUTPUT, OUTPUT);
  
  digitalWrite(POWER_RELAY_OUTPUT, 0);
  digitalWrite(CP_MOSFET_OUTPUT,0);

  analogReference(INTERNAL);

  Serial.begin(19200);
  
}

void loop() {
  uint32_t t;

  // put your main code here, to run repeatedly:
  Serial.println("AD=1234");
  delay(3000);
}
