#ifndef MODBUSINO_MODBUSINO_PLATFORM_H
#define MODBUSINO_MODBUSINO_PLATFORM_H

#include <stdint.h>
#include <stdlib.h>

/* ### Endianness macros.
 * In most cases, endianness will be detected automatically, so they can be left commented
*/
/* Uncomment if your platform is big endian */
// #define MBSN_BIG_ENDIAN

/* Uncomment if your platform is little endian */
// #define MBSN_LITTLE_ENDIAN


/* ### Transport function pointers.
 * Point them to your platform-specific methods that read/write data to/from a serial port or a TCP connection and
 * flush their receive buffers.
 *
 * read()/write() methods should block until the requested byte is read/written.
 * If your implementation uses a read/write timeout, and the timeout expires, the methods should return 0.
 * Their return values should be:
 * - 1 in case of success
 * - 0 if no data is available immediately or after an internal timeout expiration
 * - -1 in case of error
 *
 * The primary effect of flush() methods should be the flushing of the underlying receive buffer.
 * These methods will be called in case of error, in order to "reset" the state of the connection.
 * On most platforms
 *
 * You can leave some of them NULL if you don't plan to use a certain transport.
 */
int (*mbsn_rtu_flush)() = NULL;

int (*mbsn_rtu_read_byte)(uint8_t* b) = NULL;

int (*mbsn_rtu_write_byte)(uint8_t b) = NULL;

int (*mbsn_tcp_flush)() = NULL;

int (*mbsn_tcp_read_byte)(uint8_t* b) = NULL;

int (*mbsn_tcp_write_byte)(uint8_t b) = NULL;


#endif    //MODBUSINO_MODBUSINO_PLATFORM_H
