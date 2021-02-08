#include <TinyWire.h>
#include <avr/io.h>
#include <avr/interrupt.h>

#define ADC2 3
#define ADC3 2

#define REFERENCE_VOLTAGE 5
#define VOLTS_ADC_PIN     ADC2
#define CURRENT_ADC_PIN   ADC3
#define FIRMWARE_MAJOR    0x00
#define FIRMWARE_MINOR    0x01
#define ADDRESS           0x22

#define COMMAND_VOLTS            0x01
#define COMMAND_CURRENT          0x02
#define COMMAND_ERROR            0x99
#define COMMAND_UNSET            0x98
#define COMMAND_FIRMWARE_VERSION 0x97
#define COMMAND_CALIBRATE        0x96

#define RESPONSE_VOLTS            0x01
#define RESPONSE_CURRENT          0x02
#define RESPONSE_ERROR            0x99
#define RESPONSE_FIRMWARE_VERSION 0x98
#define RESPONSE_CALIBRATE        0x96
#define RESPONSE_INVALID_COMMAND  0x95

volatile byte command = COMMAND_UNSET;
volatile byte adcCheck = ADC2;

byte outputRegister[] = { 0x00, 0x00, 0x00 };
byte voltMeasure[] = {0x00, 0x00};
byte currentMeasure[] = {0x00, 0x00};

void writeOutputRegister() {
  for (int i=0; i<3; i++) {
    TinyWire.write(outputRegister[i]);
  }
}

void adcStart(uint8_t adc) {
  adcSet(adc);
  ADCSRA |= (1 << ADSC);
}

void adcSwap() {
  if (adcCheck == ADC2) {
    adcCheck = ADC3;
  } else {
    adcCheck = ADC2;
  }
}


void adcSet(uint8_t adc) {
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

  switch (adc) {
  case ADC2:
    ADMUX = (0 << ADLAR) | // right shift result
            (0 << REFS1) | // Sets ref. voltage to VCC, bit 1
            (0 << REFS0) | // Sets ref. voltage to VCC, bit 0
            (0 << MUX3)  | // use ADC2 for input (PB4), MUX bit 3
            (0 << MUX2)  | // use ADC2 for input (PB4), MUX bit 2
            (1 << MUX1)  | // use ADC2 for input (PB4), MUX bit 1
            (0 << MUX0);   // use ADC2 for input (PB4), MUX bit 0
    break;
  case ADC3:
    ADMUX = (0 << ADLAR) | // right shift result
            (0 << REFS1) | // Sets ref. voltage to VCC, bit 1
            (0 << REFS0) | // Sets ref. voltage to VCC, bit 0
            (0 << MUX3)  | // use ADC3 for input (PB3), MUX bit 3
            (0 << MUX2)  | // use ADC3 for input (PB3), MUX bit 2
            (1 << MUX1)  | // use ADC3 for input (PB3), MUX bit 1
            (1 << MUX0);   // use ADC3 for input (PB3), MUX bit 0
    break;
  }
}

void adcConfig() {
  ADCSRA = (1 << ADEN) |  // Enable ADC
           (1 << ADPS2) | // set prescaler to 64, bit 2
           (1 << ADPS1) | // set prescaler to 64, bit 1
           (0 << ADPS0) | // set prescaler to 64, bit 0
           (1 << ADIE);   // set to trigger iterrupts
}

void timerConfig() {
  TCCR1 |= (1 << CTC1) | // clear timer on compare match
           (1 << CS13) |
           (1 << CS12) |
           (1 << CS11) |
           (1 << CS10);  //clock prescaler 16384

  OCR1C = 0xFF; // compare match value 
  TIMSK |= (1 << OCIE1A); // enable compare match interrupt
}

void setup() {
  pinMode(VOLTS_ADC_PIN, INPUT);
  pinMode(CURRENT_ADC_PIN, INPUT);

  sei();
  timerConfig();
  adcConfig();

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
 * COMMAND_CURRENT
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
    case COMMAND_CURRENT:
      outputRegister[0] = RESPONSE_CURRENT;
      outputRegister[1] = currentMeasure[0];
      outputRegister[2] = currentMeasure[1];
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
      outputRegister[0] = RESPONSE_INVALID_COMMAND;
      outputRegister[1] = command;
      outputRegister[0] = 0x00;
      writeOutputRegister();
      command = COMMAND_UNSET;
      break;
  }
}

ISR(ADC_vect) {
  if (adcCheck == VOLTS_ADC_PIN) {
    voltMeasure[0] = ADCL;
    voltMeasure[1] = ADCH;
  } else {
    currentMeasure[0] = ADCL;
    currentMeasure[1] = ADCH;
  }

  adcSwap();
}

ISR(TIMER1_COMPA_vect) {
  adcStart(adcCheck);
}
