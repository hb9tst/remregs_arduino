/**
 * \file   remregs.cpp
 * \brief  Arduino implementation of a register bank compatible with the remregs library
 * \author Alessandro Crespi, some code originally implemented by Jeremie Knuesel
 * \date   August 2016, updated May 2020
 */

#include "remregs.h"

#define SYNC_NONE     0        ///< no sync with client
#define SYNC_OK       1        ///< successfully synchronized with client
#define SYNC_CHECKSUM 2        ///< same as SYNC_OK, but enable use of data checksums (not implemented yet)

#define ACK           6        ///< acknowledge transmission
#define NAK          15        ///< negative acknowledge (e.g. checksum error, timeout, NAK on unhandled read)

RegisterBank::RegisterBank(HardwareSerial& s) : port(s), sync_state(SYNC_NONE), timeout(false)
{
  port.setTimeout(5000);
  for (uint8_t i(0); i < MAX_REG_CALLBACKS; i++) {
    callbacks[i] = NULL;
  }
  desync(true);
}

RegisterBank::~RegisterBank()
{
  // nothing to destroy (yet)
}

uint8_t RegisterBank::read_byte()
{
  timeout = false;
  
  uint8_t result;
  if (port.readBytes((char*) &result, 1) == 1) {
    return result;
  } else {
    timeout = true;
    return 0xff;
  }
}

void RegisterBank::event_handler()
{
  // checks if there is anything to handle
  if (!port.available()) {
    return;
  }

  // Reads the first request bytes
  uint8_t b1 = read_byte();
  
  // if we timed out, we are not anymore in sync,
  // this is not supposed to happen here as we just called port.available(), but let's be safe
  if (timeout) {
    desync();
    return;
  }
  
  // if we are not yet in sync state, process the input byte as sync
  if (sync_state == SYNC_NONE) {
    if (b1 == 0xAA || b1 == 0xA5) {
      if (b1 == 0xAA) {
        sync_state = SYNC_OK;
      } else {
        sync_state = SYNC_CHECKSUM;
      }
      port.write(0x55);
    }
    return;
  }
  
  // if we are already synchronized, read the second request byte
  uint8_t b2 = read_byte();

  // if we timed out, or received a resync pattern (0xff 0xff, corresponding to a currently
  // undefined operation 63 on register 0x3ff), we are not anymore in sync
  if (timeout || (b1 == 0xff && b2 == 0xff)) {
    desync();
    return;
  }

  // Extracts the operation and address from the request
  uint8_t op = (b1 >> 2);
  uint16_t addr = ((uint16_t)(b1 & 0x03) << 8) | b2;

  // Buffer for data
  uint8_t* input_buffer = regdata2.bytes;

  // Number of bytes to receive/send
  uint8_t cnt = 0;

  // Computes how many bytes we should read
  switch (op) {
    case ROP_WRITE_8:
      cnt = 1;
      break;
    case ROP_WRITE_16:
      cnt = 2;
      break;
    case ROP_WRITE_32:
      cnt = 4;
      break;
    case ROP_WRITE_MB:
      cnt = read_byte();
      if (timeout) {
        desync();
        return;
      }
      input_buffer = regdata2.multibyte.data;
      regdata2.multibyte.size = cnt;
      break;
    default:
      cnt = 0;
  }

  // Reads the request bytes
  for (uint8_t i = 0; i < cnt; i++) {
    input_buffer[i] = read_byte();
    if (timeout) {
      desync();
      return;
    }
  }

  bool result(true);

  // Calls the appropriate function
  switch (op) {
    case ROP_READ_8:  // byte read
      result = callbacks_call_one(op, addr);
      cnt = 1;
      break;
    case ROP_READ_16:  // word read
      result = callbacks_call_one(op, addr);
      cnt = 2;
      break;
    case ROP_READ_32:  // dword read
      result = callbacks_call_one(op, addr);
      cnt = 4;
      break;
    case ROP_READ_MB:  // multibyte read
      result = callbacks_call_one(op, addr);
      cnt = regdata2.multibyte.size + 1;
      break;
    case ROP_WRITE_8:  // 8-bit write
    case ROP_WRITE_16: // 16-bit write
    case ROP_WRITE_32: // 32-bit write
    case ROP_WRITE_MB: // multibyte write
      callbacks_call_all(op, addr);
      cnt = 0;
      break;
    default:
      cnt = 0;
  }
  
/* not implemented yet

  // checksum byte (only with newer version of remregs, if synced with checksum enabled
  if (sync_state[UART_number] == SYNC_CHECKSUM) {
    port.write(checksum);
  }
*/

  if (result) {
    // ACKnowledge byte
    port.write(ACK);
    
    // Sends any output bytes
    for (uint8_t i = 0; i < cnt; i++) {
      port.write(regdata2.bytes[i]);
    }
  } else {
    port.write(NAK);
  }

}

void RegisterBank::callbacks_call_all(const uint8_t operation, const uint8_t address)
{
  for (uint8_t i(0); i < MAX_REG_CALLBACKS; i++) {
    if (callbacks[i] != NULL) {
      callbacks[i](operation, address, &regdata2);
    }
  }
}

bool RegisterBank::callbacks_call_one(const uint8_t operation, const uint8_t address)
{
  for (uint8_t i(0); i < MAX_REG_CALLBACKS; i++) {
    if (callbacks[i] != NULL && callbacks[i](operation, address, &regdata2))
      return true;
  }
  return false;
}

void RegisterBank::add_callback(register_callback_t addr)
{
  for (uint8_t i(0); i < MAX_REG_CALLBACKS; i++) {
    if (callbacks[i] == NULL) {
      callbacks[i] = addr;
      break;
    }
  }
}

void RegisterBank::del_callback(register_callback_t addr)
{
  for (uint8_t i(0); i < MAX_REG_CALLBACKS; i++) {
    if (callbacks[i] == addr) {
      callbacks[i] = NULL;
      break;
    }
  }
}

void RegisterBank::desync(bool force)
{
  if (force || sync_state != SYNC_NONE) {
    for (uint8_t i(0); i < MAX_MB_SIZE + 5; i++) {
      port.write(0xff);
    }
  }
  sync_state = SYNC_NONE;
}

