#ifndef __REMREGS_H
#define __REMREGS_H

/**
 * \file   remregs.h
 * \brief  Header for Arduino implementation of a register bank compatible with the remregs library
 * \author Alessandro Crespi
 * \date   August 2016
 * \note   Derived from AmphiBot III/Envirobot radio register bank
 */

#include <stdint.h>
#include <HardwareSerial.h>

/// 8-bit register read
#define ROP_READ_8   0
/// 16-bit register read
#define ROP_READ_16  1
/// 32-bit register read
#define ROP_READ_32  2
/// multibyte register read
#define ROP_READ_MB  3
/// 8-bit register write
#define ROP_WRITE_8  4
/// 16-bit register write
#define ROP_WRITE_16 5
/// 32-bit register write
#define ROP_WRITE_32 6
/// multibyte register write
#define ROP_WRITE_MB 7

/// maximal size (in bytes) of a multibyte register
#define MAX_MB_SIZE  29

/// maximum number of register callbacks
#define MAX_REG_CALLBACKS 16

/// Structure used to exchange data with register callbacks
typedef union {
	uint8_t byte;       ///< register contents as 8-bit
	uint16_t word;      ///< register contents as 16-bit
	uint32_t dword;     ///< register contents as 32-bit

  /// structure for multibyte registers
	struct {
		uint8_t size;               ///< size of the data
		uint8_t data[MAX_MB_SIZE];  ///< the actual data
	} multibyte;

	uint8_t bytes[MAX_MB_SIZE + 1];  ///< raw access to the buffer (internal use only!)
} RegisterData;

/** \brief Type definition for register read/write callback functions
  * \param operation Operation. Operations are currently: ROP_READ_8 = 8-bit read,
  *   ROP_READ_16 = 16-bit read, ROP_READ_32 = 32-bit read, ROP_READ_MB = multibyte
  *   read, ROP_WRITE_8 = 8-bit write, ROP_WRITE_16 = 16-bit write, ROP_WRITE_32 =
  *   32-bit write, ROP_WRITE_MB = multibyte write.
  * \param address Address of the register to write to or read from.
  * \param data A pointer to the data to be read/written. Data can be accessed
  *   through different fields of the RadioData union, depending on the operation
  *   type. Note that when handling a ROP_READ_MB operation, the user must also
  *   set data->multibyte.size.
  * \return true if the operation was handled by the callback, false otherwise.
  */
typedef bool (*register_callback_t)
             (uint8_t operation, uint8_t address, RegisterData* data);

class RegisterBank {
  
public:

  /// Constructor
  RegisterBank(HardwareSerial& s);
  
  /// Destructor
  virtual ~RegisterBank();

  /// Periodic function called by loop() in user program
  void event_handler();

  /// Adds a register callback function
  void add_callback(register_callback_t addr);

  /// Removes a register callback function
  void del_callback(register_callback_t addr);

private:

  /// Reference to the serial port to use
  HardwareSerial& port;

  /// Synchronization state (by default: not synchronized -> no communication possible)
  uint8_t sync_state;
  
  /// Register callback array
  register_callback_t callbacks[MAX_REG_CALLBACKS];

  /// Data structure used for data exchange
  RegisterData regdata2;

  /// Call all the callback functions (reg write operations)
  void callbacks_call_all(const uint8_t operation, const uint8_t address);
  
  /// Call the callback functions until one succeeds (reg read operations)
  bool callbacks_call_one(const uint8_t operation, const uint8_t address);
  
  /// true if a timeout occurred during the last call to ::read_byte()
  bool timeout;

  /// Utility function: reads a single byte from the input port, setting ::timeout to
  /// true in case of timeout
  uint8_t read_byte();
  
  /// Sends a stream of 0xff bytes to (eventually) inform the master that
  /// there is a synchronization problem
  void desync(bool force = false);
};

#endif
