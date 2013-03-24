/*************************************************** 
  This is a library for the MCP23017 i2c port expander

  These displays use I2C to communicate, 2 pins are required to  
  interface
  Adafruit invests time and resources providing this open source code, 
  please support Adafruit and open-source hardware by purchasing 
  products from Adafruit!

  Written by Limor Fried/Ladyada for Adafruit Industries.  
  BSD license, all text above must be included in any redistribution
 ****************************************************/

#include "twi.h"
#include <avr/pgmspace.h>
#include "MCP23017.h"
#include "config.h"
#include <avr/interrupt.h>


////////////////////////////////////////////////////////////////////////////////
void init_MCP23017_interrupt(); // forward declaration

void MCP23017_begin(uint8_t addr) {
  i2caddr = MCP23017_ADDRESS | (addr&7);
  twi_init();
  // set defaults
  // all pins input direction
  static uint8_t localbuf[3];
  localbuf[0]=MCP23017_IODIRA; localbuf[1]=0xFF; localbuf[2]=0xFF;
  // out direction
  //uint8_t localbuf[] = {MCP23017_IODIRA, 0x00, 0x00};
  twi_writeTo(i2caddr, localbuf, 3, DO_WAIT);
  #ifdef USE_I2C_LIMITS
  // set up IOCON.SEQOP=1, BANK=0 to read repeatedly from both input latches
  //  also INT output is active low, active driver.
  localbuf[0]=MCP23017_IOCONA; localbuf[1]=0x20; 
  twi_writeTo(i2caddr, localbuf, 2, DO_WAIT);
  // set up INTCONA for interrupt on change (same as power-on default)
  localbuf[0]=MCP23017_INTCONA; localbuf[1]=0x00; 
  twi_writeTo(i2caddr, localbuf, 2, DO_WAIT);
  // set up GPINTENA to enable interrupt on all pins
  localbuf[0]=MCP23017_GPINTENA; localbuf[1]=0xFF; 
  twi_writeTo(i2caddr, localbuf, 2, DO_WAIT);
  init_MCP23017_interrupt();
  #endif
}



void MCP23017_pinMode(uint8_t p, uint8_t d) {
  uint8_t localbuf[2];

  // only 16 bits!
  if (p > 15)
    return;

  if (p < 8)
    localbuf[0] = MCP23017_IODIRA;
  else {
    localbuf[0] = MCP23017_IODIRB;
    p -= 8;
  }

  // read the current IODIR
  if(twi_writeTo(i2caddr, localbuf, 1, DO_WAIT) != 0) return;
  if(twi_readFrom(i2caddr, &localbuf[1], 1) != 1) return;
  // set the pin and direction
  if (d == INPUT) {
    localbuf[1] |= 1 << p; 
  } else {
    localbuf[1] &= ~(1 << p);
  }

  // write the new IODIR
  twi_writeTo(i2caddr, localbuf, 2, DO_WAIT);
}

uint16_t MCP23017_readGPIOAB() {
  uint16_t ba = 0;
  
  uint8_t localbuf[3] = {MCP23017_GPIOA, 0, 0};

  // read the current GPIO output latches
  if(twi_writeTo(i2caddr, localbuf, 1, DO_WAIT) != 0) return 0;
  if(twi_readFrom(i2caddr, &localbuf[1], 2) != 2) return 0;

  ba = localbuf[2];
  ba <<= 8;
  ba |= localbuf[1];

  return ba;
}

void MCP23017_writeGPIOAB(uint16_t ba) {
/*
  Wire.beginTransmission(MCP23017_ADDRESS | i2caddr);
  wiresend(MCP23017_GPIOA);	
  wiresend(ba & 0xFF);
  wiresend(ba >> 8);
  Wire.endTransmission();
*/
}

void MCP23017_digitalWrite(uint8_t p, uint8_t d) {
  uint8_t localbuf[2];
  uint8_t olataddr;

  // only 16 bits!
  if (p > 15)
    return;

  if (p < 8) {
    olataddr = MCP23017_OLATA;
    localbuf[0] = MCP23017_GPIOA;
  } else {
    olataddr = MCP23017_OLATB;
    localbuf[0] = MCP23017_GPIOB;
    p -= 8;
  }

  // read the current GPIO output latches
  uint8_t status = twi_writeTo(i2caddr, &olataddr, 1, DO_WAIT);
  if(twi_readFrom(i2caddr, &localbuf[1], 1) != 1) return;
  // set the pin 
  if (d != 0) {
    localbuf[1] |= 1 << p; 
  } else {
    localbuf[1] &= ~(1 << p);
  }
  // write the new GPIO
  status = twi_writeTo(i2caddr, localbuf, 2, DO_WAIT);
}

#ifdef MCP23017_INT_PIN // if defined, it is 0 or 1
// Use MCP23017's interrupt output to trigger a GPIO read operation
// Using dedicated interrupts INT0/INT1 (not pin-change interrupt)
// ...
uint8_t GPIO_read_buf[2];
twi_transaction_read GPIOread_trans;
void init_MCP23017_interrupt() {
  GPIOread_trans.address = i2caddr;
  GPIOread_trans.reg = MCP23017_GPIOA;
  GPIOread_trans.length = 2;
  GPIOread_trans.data = GPIO_read_buf;
  // set INTx falling edge sensitive
  EICRA = EICRA & ~( 3 << (2*MCP23017_INT_PIN) ) | ( 2 << (2*MCP23017_INT_PIN) );
  EIMSK |= 1 << (MCP23017_INT_PIN);
}  
ISR(MCP23017_INT_vect) 
{
  // schedule a read operation at priority 0
  twi_queue_read_transaction(&GPIOread_trans, 0);
}
#else
void init_MCP23017_interrupt() { }
#endif

#if 0
void Adafruit_MCP23017::pullUp(uint8_t p, uint8_t d) {
  uint8_t gppu;
  uint8_t gppuaddr;

  // only 16 bits!
  if (p > 15)
    return;

  if (p < 8)
    gppuaddr = MCP23017_GPPUA;
  else {
    gppuaddr = MCP23017_GPPUB;
    p -= 8;
  }


  // read the current pullup resistor set
  Wire.beginTransmission(MCP23017_ADDRESS | i2caddr);
  wiresend(gppuaddr);	
  Wire.endTransmission();
  
  Wire.requestFrom(MCP23017_ADDRESS | i2caddr, 1);
  gppu = wirerecv();

  // set the pin and direction
  if (d == HIGH) {
    gppu |= 1 << p; 
  } else {
    gppu &= ~(1 << p);
  }

  // write the new GPIO
  Wire.beginTransmission(MCP23017_ADDRESS | i2caddr);
  wiresend(gppuaddr);
  wiresend(gppu);	
  Wire.endTransmission();
}

uint8_t Adafruit_MCP23017::digitalRead(uint8_t p) {
  uint8_t gpioaddr;

  // only 16 bits!
  if (p > 15)
    return 0;

  if (p < 8)
    gpioaddr = MCP23017_GPIOA;
  else {
    gpioaddr = MCP23017_GPIOB;
    p -= 8;
  }

  // read the current GPIO
  Wire.beginTransmission(MCP23017_ADDRESS | i2caddr);
  wiresend(gpioaddr);	
  Wire.endTransmission();
  
  Wire.requestFrom(MCP23017_ADDRESS | i2caddr, 1);
  return (wirerecv() >> p) & 0x1;
}
#endif