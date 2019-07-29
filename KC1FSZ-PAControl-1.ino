// This is a control program for an adjustable bias circuit
// Bruce MacKinnon KC1FSZ 27 July 2019
//
// For information about the MCP4131 pleas see:
// http://ww1.microchip.com/downloads/en/DeviceDoc/22060a.pdf
//
// For information on the TS1638 display/input library:
// https://github.com/gavinlyonsrepo/TM1638plus

// NOTE: I've been having a lot of problems getting this chip to work using
// the SPI library on the Teensy 3.2.  I'm not completely sure what the issue 
// is, but when I manipulate the pins to simulate the SPI sequence it works 
// perfectly.  
//
#include <TM1638plus.h>
#include <DebouncedSwitch3.h>
#include <OneWire.h> 
#include <DallasTemperature.h>

// How often we look at the temperature
#define TEMP_INTERVAL_MS (5 * 1000)
// How often we update the display
#define DISPLAY_INTERVAL_MS (5 * 1000)

// CHANGE THESE ACCORDING TO WHAT PINS YOU ARE USING:
#define ONE_WIRE_BUS 2 
const int ssPin = 4;
const int mosiPin = 5;
const int clkPin = 6;

#define TM1638_STROBE_TM 7
#define TM1638_CLOCK_TM 9
#define TM1638_DIO_TM 8
#define FAN_CONTROL_PIN 10

OneWire OneWire(ONE_WIRE_BUS); 
DallasTemperature Sensors(&OneWire);

TM1638plus tm(TM1638_STROBE_TM,TM1638_CLOCK_TM,TM1638_DIO_TM);

DebouncedSwitch3 Debouncer0(10);
DebouncedSwitch3 Debouncer1(10);
DebouncedSwitch3 Debouncer2(10);

int FanSpeed = 0;
int Level = 0;
int Temp = 0;
long LastTempSampleStamp = 0;
long LastDisplayStamp = 0;
long DisplaySequence = 0;

void spiWriteBit(int bit) {
  // Setup the data
  digitalWrite(mosiPin,bit);
  // The slave samples when clock goes high
  digitalWrite(clkPin,HIGH);
  digitalWrite(clkPin,LOW);
}

void spiWriteByte(int b) {
  int work = b;
  for (int i = 0; i < 8; i++) {
    // Always focus on the MSB 
    spiWriteBit((work & 0x80) ? 1 : 0);
    work = work << 1;
  }
}

// NOTE: There are no delays of any kind.  The assumption is that the chip 
// can keep up with the maxium data rate.
//
void spiWrite(int address, int value) {
  // Take the SS pin low to select the chip:
  digitalWrite(ssPin,LOW);
  // Address
  spiWriteByte(address);
  // Data
  spiWriteByte(value);
  // Take the SS pin high to de-select the chip:
  digitalWrite(ssPin,HIGH); 
}

void updateDisplay() {
  LastDisplayStamp = millis();
  DisplaySequence++;
  char buffer[9];
  /*
  if (DisplaySequence % 2 == 0) {  
    sprintf(buffer,"%8d",Level);
    tm.displayText(buffer);
  } else {
    sprintf(buffer,"%8d",Temp);
    tm.displayText(buffer);
    tm.displayASCII(0,"T");
  }
  */
  sprintf(buffer,"%4d",Level);
  sprintf(buffer+4,"%4d",Temp);
  tm.displayText(buffer);
}

void updateBias() {
  spiWrite(0,Level/2);
}

void setup() {
  
  Serial.begin(9600);

  pinMode(FAN_CONTROL_PIN,OUTPUT);
  analogWrite(FAN_CONTROL_PIN,0);

  tm.reset();
  
  pinMode (ssPin,OUTPUT);
  pinMode (mosiPin,OUTPUT);
  pinMode (clkPin,OUTPUT);
  digitalWrite(ssPin,HIGH); 
  digitalWrite(clkPin,LOW); 

  // Start up the DS18B20 library 
  Sensors.begin(); 
  
  updateDisplay();
  updateBias();
}

// This is just a simple loop that sequences the pot through the full range of 
// values.  Address 0x00 on the MCP4131 is the potentiometer value. 
//
void loop() { 
  
  uint8_t buttons = tm.readButtons();
  
  Debouncer0.loadSample((buttons & 2) != 0);
  Debouncer1.loadSample((buttons & 1) != 0);
  Debouncer2.loadSample((buttons & 4) != 0);

  if (millis() - LastTempSampleStamp > TEMP_INTERVAL_MS) {
    LastTempSampleStamp = millis();
    Sensors.requestTemperatures();
    Temp = (int)Sensors.getTempFByIndex(0);
    if (Temp > 90) { 
      analogWrite(FAN_CONTROL_PIN,128);
    } else if (Temp > 95) {
      analogWrite(FAN_CONTROL_PIN,255);
    } else {
      analogWrite(FAN_CONTROL_PIN,0);
    }
  }
  
  if (millis() - LastDisplayStamp > DISPLAY_INTERVAL_MS) {
    updateDisplay();
  }
  
  if (Debouncer0.getState() /*&& Debouncer0.isEdge()*/) {
    if (Level < 255) {
      Level++;
      updateBias();
      updateDisplay();
    }
  }
  if (Debouncer1.getState() /*&& Debouncer1.isEdge()*/) {
    if (Level > 0) {
      Level--;
      updateBias();
      updateDisplay();
    }
  }
  if (Debouncer2.getState() && Debouncer2.isEdge()) {
    if (FanSpeed == 0) {
      FanSpeed = 255;
    } else {
      FanSpeed = 0;  
    }
    analogWrite(FAN_CONTROL_PIN,FanSpeed);
  }
}
