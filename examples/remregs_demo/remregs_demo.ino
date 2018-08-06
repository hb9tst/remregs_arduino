#include <Servo.h> 
#include <remregs.h>

/**********************************************************************
  \file   remregs_demo.ino
  \author Alessandro Crespi
  \date   August 2016, modified August 2018
  \brief  Arduino remregs text/example
 
  This program is a basic demonstration of how to implement a register
  callback to read and write data from an Arduino, using the remregs
  library.
 
  The following 8-bit registers can be written (see ::register_callback
  implementation for details):
 
    * 1: controls the LED on the Arduino board (0 = off, 1 = on)
    * 2: controls the 1st servomotor (pin 3)
    * 3: controls the second servomotor (pin 5)

  Reading back those registers will not return anything (0xff, i.e.,
  the default value) because no read handler has been implemented.
  This clearly shows that these registers are not a real memory-based
  register bank.
  
  A multibyte register at address 1 can be written too, the first two
  bytes in the received data will control the first and second motor,
  respectively.
  
  The 32-bit register at address 1 can be read and returns the timer
  of the Arduino (millis() function).
  
**********************************************************************/

// The servo motors to remotely control
Servo motor1, motor2;

// Register bank, using Serial as communication port
RegisterBank regs(Serial);

/// \brief Register callback called on register operations
/// \return true if the callback handled the operation, false otherwise
bool register_callback(uint8_t operation, uint8_t address, RegisterData* data)
{
  switch (operation)
  {
    case ROP_WRITE_8:   // 8-bit writes from master
      if (address == 1) {
        digitalWrite(LED_BUILTIN, data->byte);
        return true;
      } else if (address == 2) {
        motor1.write(data->byte);
        return true;
      } else if (address == 3) {
        motor2.write(data->byte);
        return true;
      }
      break;
    case ROP_WRITE_MB:  // multibyte (variable size) writes from master
      if (address == 1) {
        motor1.write(data->multibyte.data[0]);
        motor2.write(data->multibyte.data[1]);
        return true;
      }
      break;
    case ROP_READ_32:   // 32-bit reads from master
      if (address == 1) {
        data->dword = millis();
        return true;
      }
      break;
  }
  return false;
}

void setup() {
  // built-in LED is an output
  pinMode(LED_BUILTIN, OUTPUT);
  // initializes the serial port at 115.2 kbps
  Serial.begin(115200);
  // turns on the LED by default
  digitalWrite(LED_BUILTIN, HIGH);
  // intializes the callback for the 
  regs.add_callback(register_callback);
  // attaches the motors
  motor1.attach(3);
  motor2.attach(5);
}

void loop()
{
  // calls the event handler of the register bank to process any pending requests
  regs.event_handler();
}
