/*
   Author: Robin Szemeti, G1YFG

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.

Thanks to Andy Pugh for the core of the resolver code.

*/

#include "avr/pgmspace.h"
#include <Arduino.h>
#include <EEPROM.h>

const unsigned int directionPin[] = { 3,4 };
const unsigned int pwmPin[] = { 5,6 };

// P values for PID loop
const unsigned int P[] = { 100,100 };
// I values for PID loop ... not implemented yet, will be.
const unsigned int I[] = { 0,0 };
// D values for PID loop ... not implemented yet, may never be
const unsigned int D[] = { 0,0 };

#define DEAD_ZONE 0.2
#define AVERAGE_COUNT 200

/*
  ########  HERE BE DRAGONS  ....
  Generally, configuration can be done above this line to define pins etc.
  
*/
// Sqaure wave
PROGMEM const unsigned char sq16[] = { 255, 255, 255, 255, 255, 255, 255, 255, 0, 0, 0, 0, 0, 0, 0, 0 };

// Covenience methods
#define cbi(sfr, bit) (_SFR_BYTE(sfr) &= ~_BV(bit))
#define sbi(sfr, bit) (_SFR_BYTE(sfr) |= _BV(bit))

//
volatile int V[4];      // Analogue values
int zeroOffset[4];      // Zero-offset values for each channel

volatile boolean F[4];  // "You have new Volts" flag.

double currentPos[2];   // Two Angles
double targetPos[2];    // 
double offsets[2];
bool active[2];



volatile byte index;  // index to current mux channel

// variables used inside interrupt service declared as voilatile
volatile byte icnt;  // var inside interrupt

// serial input handling
int cmdPointer;
char cmdBuffer[40];

typedef enum {
  ESTOP,
  MANUAL,
  RUN
} status_t;

status_t status;

// Loop filter constants
double f1,f2;

void setup() {

  Serial.begin(115200);  // connect to the serial port
  Serial.println("# Resolver based servo");

  int eepromAdrr = 0;

  for(int i=0; i<=1; i++){
    active[i]=false;
    pinMode(directionPin[i], OUTPUT);
    pinMode(pwmPin[i], OUTPUT);
    analogWrite(pwmPin[i],0);
    EEPROM.get(eepromAdrr, offsets[i]);
    eepromAdrr += sizeof(double);
  }

  cmdPointer=0;

  // Setup the avergaing used in the angular readings
  f2 = 1.0/AVERAGE_COUNT;
  f1 = 1.0 - f2;

  //First populate the zero-volt value for each channel
  //analogReference(EXTERNAL);
  char buff[40];
  delay(2000);                    // big cap, let it settle
  for (int i = 0; i <= 3; i++) {  // iterate through channels
    long Accum = 0;
    for (int j = 1; j <= 1000; j++) {  // 1000 samples should do
      Accum = Accum + analogRead(i);
    }
    zeroOffset[i] = Accum / 1000;  // div 1000
    sprintf(buff, "# Zero for channel %d is %d", i, zeroOffset[i]);
    Serial.println(buff);
  }

  status = ESTOP;
  Serial.println("STATUS,STOP");

  DDRB = B00111111;  // PORTB as output
  DDRD = B11111100;  // PortD as output, except serial IO
  Setup_timer2();

  // disable interrupts to avoid timing distortion
  cbi(TIMSK0, TOIE0);  // disable Timer0 !!! delay() is now not available

  SetupADC();

  sbi(TIMSK2, TOIE2);  // enable Timer2 Interrupt
}

int t=0;



void loop() {

  if (F[0] || F[1]) {
    readResolver(0);
  }
  if (F[2] || F[3]) {
    readResolver(1);
  }

  if(status==RUN){
    servoLoop(0);
    servoLoop(1);
  }

  // Just some loop count to print something on the serial bus a few times a second, no need to flood it.
  // remeber, we can't use delay() or millis() as TIMSK0 is off.
  t++;
  if( (status==RUN  && (t > 1000)) || (t > 20000)){
    Serial.print("POS,");
    Serial.print(currentPos[0]-offsets[0]);
    Serial.print(",");
    Serial.println(currentPos[1]-offsets[1]);
    t=0;
  }

  if (Serial.available() > 0) {
    // read the incoming byte:
    cmdBuffer[cmdPointer] = Serial.read();

    // say what you got:
    Serial.print("I received: ");
    Serial.println(cmdBuffer[cmdPointer], DEC);

    if(cmdBuffer[cmdPointer]==10){
      processCommand();
      cmdPointer=0;
    }else{
    cmdPointer++;
      if(cmdPointer>=40){
        cmdPointer=0;
      }
    }
  }
}

void servoLoop(int i){
  if(targetPos[i]>(currentPos[i] - offsets[i])){
    digitalWrite(directionPin[i],HIGH);
  }else{
    digitalWrite(directionPin[i],LOW);
  }

  double error = abs(targetPos[i] - (currentPos[i]-offsets[i]));

  if(error < DEAD_ZONE){
    error = 0;
  }
  error = error * P[i];
  if(error > 255){
    error = 255;
  }

  if(i==0){
    //Serial.println((int)error);
  }
  analogWrite(pwmPin[i], (int)error);
}

void processCommand(){
  //null terminate the input string
  cmdBuffer[cmdPointer]=0;
  switch(cmdBuffer[0]){
    case 'A':
      setAxis(0);
      break;
    case 'E':
      setAxis(1);
      break;
    case 'F':
      refAxis(0);
      refAxis(1);
      break;
    case 'S':
      estop();
      break;
    case 'M':
      status=MANUAL;
      break;
    case 'R':
      run();
      break;
    case 'Z':
      setOffsets();
      break;
  }
}

void setOffsets(){
   int eepromAddr=0;
   for(int i=0; i<=1; i++){
     // Only set/store an offset if the axis has a target position set
     if(active[i]){
        offsets[i]=currentPos[i]-targetPos[i];
        EEPROM.put(eepromAddr,offsets[i]);
        eepromAddr += sizeof(double);
     }
   } 
}

void run(){
  if( active[0] && active[1] ){
    status= RUN;
    Serial.println("STATUS,RUN");
  }
} 

void estop(){
   status=ESTOP;
   analogWrite(pwmPin[0], 0);
   analogWrite(pwmPin[1], 0);
   digitalWrite(directionPin[0], LOW);
   digitalWrite(directionPin[1], LOW);
   Serial.println("STATUS,STOP");
}

void refAxis(int i){
    Serial.println(i);
    String pos  = String(cmdBuffer+1);
    targetPos[i]=currentPos[i];
    Serial.println(targetPos[i]);
    active[i]=true;
}

void setAxis(int i){
    Serial.println(i);
    String pos  = String(cmdBuffer+1);
    targetPos[i]=pos.toDouble();
    Serial.println(targetPos[i]);
    active[i]=true;
}

void readResolver(int c){
      int i = c *2;
      F[i] = false;
      F[i+1] = false;

      double r;
      double a;

      // Need to grab them in case the get changed by interrupt when around the zero point
      int v1 = V[i] - zeroOffset[i];
      int v2 = V[i+1] - zeroOffset[i+1];

      // Always put the bigger number on the bottom to avoid divide by zero nonesense
      if (abs(v1) < abs(v2)) {
        //Serial.print("T ");
        r = double(double(v1) / double(v2));
        a = atan(r) * 4068.0 / 71.0;
        if (v2 <= 0) {
          a += 180.0;
        } else {
          if (v1 <= 0) {
            a += 360.0;
          }
        }
      } else {
        //Serial.print("B ");
        r = double(double(v2) / (double(v1)));
        a = atan(r) * 4068.0 / 71.0;
        if (v1 <= 0) {
          if (v2 <= 0) {
            a += 360.0;
          }
        } else {
          a += 180.0;
        }
        a = 270.0 - a;
        if(a < 0){
          a+=360.0;
        }
      }

      currentPos[c] = f1 * currentPos[c] + f2 * a;
}

//******************************************************************
// timer2 setup
// set prscaler to 1, PWM mode to phase correct PWM,  16000000/510 = 31372.55 Hz clock
void Setup_timer2() {

  sbi(TCCR2B, CS20);  // Timer2 Clock Prescaler to : 1
  cbi(TCCR2B, CS21);
  cbi(TCCR2B, CS22);

  // Timer2 PWM Mode set to Phase Correct PWM
  cbi(TCCR2A, COM2A0);  // clear Compare Match
  sbi(TCCR2A, COM2A1);

  sbi(TCCR2A, WGM20);  // Mode 1  / Phase Correct PWM
  cbi(TCCR2A, WGM21);
  cbi(TCCR2B, WGM22);
}

//Setup ADC
void SetupADC() {
  // cbi(ADMUX, REFS1);
  // cbi(ADMUX, REFS0); // External Aref
  analogReference(EXTERNAL);

  cbi(ADMUX, ADLAR);  // Left-aligned for 8-bit data
  cbi(ADMUX, MUX3);
  cbi(ADMUX, MUX2);
  cbi(ADMUX, MUX1);
  cbi(ADMUX, MUX0);  // Set the Mux to zero to begin

  sbi(ADCSRA, ADEN);   // Enable the ADC
  cbi(ADCSRA, ADSC);   // Don't start conversion yet
  cbi(ADCSRA, ADATE);  // No auto-trigger
  cbi(ADCSRA, ADIF);   // Not sure if that is possible or wise
  sbi(ADCSRA, ADIE);   // Conversion-complete Interrupt to process data
  sbi(ADCSRA, ADPS2);
  sbi(ADCSRA, ADPS1);
  cbi(ADCSRA, ADPS0);  // ADC Clock prescalar 110 => 16MHz/64
}

//******************************************************************
// Timer2 Interrupt Service at 31372,550 KHz = 32uSec
// this is the timebase REFCLOCK for the DDS generator
// FOUT = (M (REFCLK)) / (2 exp 32)
// runtime : 8 microseconds ( inclusive push and pop)
ISR(TIMER2_OVF_vect) {

  icnt = 15 & (++icnt);  // use upper 8 bits for phase accu as frequency information
                         // read value fron ROM sine table and send to PWM DAC
  OCR2A = pgm_read_byte_near(sq16 + icnt);


  // Position our ADC read at a stable place in the cycle
  if (icnt == 14) {
    sbi(ADCSRA, ADSC);  // Start Conversion
    sbi(PORTB, 5);      // Set pin to view conversion on 'scope
  }
}

// ADC Conversion Complete Interrupt
ISR(ADC_vect) {
  V[index] = ADCL + (((uint16_t)(ADCH)) << 8);  //get the data
  F[index] = true;
  if (++index > 3) index = 0;           // increment the ADC channel number with wrap
  ADMUX = (ADMUX & B11110000) | index;  // set the ACDC channel
  cbi(PORTB, 5);                        // clear the 'scope bit
}
