#include <TinyWire.h>
#include <avr/io.h>
#include <avr/interrupt.h>

#define REFERENCE_VOLTAGE 5
#define ADC_PIN           3
#define FIRMWARE_MAJOR    0x00
#define FIRMWARE_MINOR    0x01
#define ADDRESS           0x22

#define COMMAND_VOLTS            0x01
#define COMMAND_ERROR            0x99
#define COMMAND_UNSET            0x98
#define COMMAND_FIRMWARE_VERSION 0x97
#define COMMAND_CALIBRATE        0x96

#define RESPONSE_VOLTS            0x01
#define RESPONSE_ERROR            0x99
#define RESPONSE_FIRMWARE_VERSION 0x98
#define RESPONSE_CALIBRATE        0x96

volatile byte command = COMMAND_UNSET;

byte outputRegister[] = { 0x00, 0x00, 0x00 };
byte voltMeasure[] = {0x00, 0x00};

void adcStart() {
  ADCSRA |= (1 << ADSC);
}

void adcConfig() {
  /*
   * ADLAR configures the way that data is presented in the ADCH and ADCL
   * registers When ADLAR is 0 results are kept in ADCH and ADCL (ADC high and
   * low bits) When reading results if ADLAR is 0 you must read ADCL first
   * before reading ADCH.
   *
   * If ADLAR is 1 (results left adjusted) and only 8 bits of precision are
   * needed You can simply ready ADCH.
   *
   * Selecting ADC4 will select a temperature sensor
   */
  sei();
  ADMUX = (0 << ADLAR) | // right shift result
          (0 << REFS1) | // Sets ref. voltage to VCC, bit 1
          (0 << REFS0) | // Sets ref. voltage to VCC, bit 0
          (0 << MUX3) |  // use ADC2 for input (PB4), MUX bit 3
          (0 << MUX2) |  // use ADC2 for input (PB4), MUX bit 2
          (1 << MUX1) |  // use ADC2 for input (PB4), MUX bit 1
          (0 << MUX0);   // use ADC2 for input (PB4), MUX bit 0

  ADCSRA = (1 << ADEN) |  // Enable ADC
           (1 << ADPS2) | // set prescaler to 64, bit 2
           (1 << ADPS1) | // set prescaler to 64, bit 1
           (0 << ADPS0) | // set prescaler to 64, bit 0
           (1 << ADIE);   // set to trigger iterrupts
}

void timerConfig() {
  TCCR1 |= (1 << CTC1);                             // clear timer on compare match
  TCCR1 |= (1 << CS13) | (1 << CS12) | (1 << CS11) | (1 << CS10) ; //clock prescaler 16384
  OCR1C = 0xFF;                                     // compare match value 
  TIMSK |= (1 << OCIE1A);                           // enable compare match interrupt
}

void setup() {
  pinMode(ADC_PIN, INPUT);
  adcConfig();
  timerConfig();

  TinyWire.begin(ADDRESS);
  TinyWire.onReceive(onReceive);
  TinyWire.onRequest(onRequest);
}

void loop() { }


/* Called whenever we receive bytes from the master
 * 
 * Supported commands:
 * 
 * COMMAND_VOLTS
 * COMMAND_FIRMWARE_VERSION
 * COMMAND_CALIBRATE
 * 
 * This method will check that the command received is recognized and valid
 * Assign it to the command register and then exit.
 * 
 * Subsequent processing is done in loop
 */
void onReceive(int _count) {
  // Think about a mutex around command_buffer to protect
  // it from being trampled by an aggressive master
  int i = 0;
  while (TinyWire.available()) {
    if (i == 0) {
      command = TinyWire.read();
    } else {
      TinyWire.read();
    }
    i++;
  }
}

void writeOutputRegister() {
  for (int i=0; i<3; i++) {
    TinyWire.write(outputRegister[i]);
  }
}

/* This interrupt is triggered when the master requests bytes to be sent from
 * the slave.
 * 
 * This method checks the command we're processing and returns data from the
 * appropriate data output register.
 */
void onRequest() {
  switch (command) {
    case COMMAND_VOLTS:
      outputRegister[0] = RESPONSE_VOLTS;
      outputRegister[1] = voltMeasure[0];
      outputRegister[2] = voltMeasure[1];
      writeOutputRegister();
      command = COMMAND_UNSET;
      break;
    case COMMAND_FIRMWARE_VERSION:
      outputRegister[0] = RESPONSE_FIRMWARE_VERSION;
      outputRegister[1] = FIRMWARE_MAJOR;
      outputRegister[2] = FIRMWARE_MINOR;
      writeOutputRegister();
      command = COMMAND_UNSET;
      break;
    case COMMAND_ERROR:
      outputRegister[0] = RESPONSE_ERROR;
      writeOutputRegister();
      command = COMMAND_UNSET;
      break;
    default:
      outputRegister[0] = RESPONSE_ERROR;
      writeOutputRegister();
      command = COMMAND_UNSET;
  }
}

ISR(ADC_vect) {
  voltMeasure[0] = ADCL;
  voltMeasure[1] = ADCH;
}

ISR(TIMER1_COMPA_vect) {
  adcStart();
}
