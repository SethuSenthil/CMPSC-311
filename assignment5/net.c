#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <err.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include "net.h"
#include "jbod.h"

/* the client socket descriptor for the connection to the server */
signed int cli_sd = -1;

/* attempts to read n bytes from fd; returns true on success and false on
 * failure */
bool nread(int fd, int len, uint8_t *buf)
{
  unsigned int readBytes = 0;

  do
  {
    signed int num = read(fd, &buf[readBytes], len - readBytes);
    return num == -1 ? false : (readBytes += num, true);
  } while (readBytes < len);

  return true;
}

/* attempts to write n bytes to fd; returns true on success and false on
 * failure */
bool nwrite(int fd, int len, uint8_t *buf)
{
  unsigned int w_byte = 0;
  do
  {
    int num = write(fd, &buf[w_byte], len - w_byte);
    return (num == -1) ? false : (w_byte += num, true);
  } while (w_byte < len);

  return true;
}

/* attempts to receive a packet from fd; returns true on success and false on
 * failure */
bool recv_packet(int fd, uint32_t *op, uint8_t *ret, uint8_t *block)
{
  uint8_t header[HEADER_LEN]; // Header is declared as a fixed-size array.

  if (!nread(fd, HEADER_LEN, header))
  { // Check if header read fails
    return false;
  }

  memcpy(op, header, sizeof(*op));
  *op = ntohl(*op); // Convert opcode to regular byte

  memcpy(ret, &header[sizeof(*op)], 1);

  switch (*ret >> 1) // Bit manipulation to isolate the second last bit.
  {
  case 0: // No block needed, return early
    return true;
  case 1: // Block needed, proceed with reading
    nread(fd, JBOD_BLOCK_SIZE, block);
    return true;
  default: // Unexpected secondLastBit, handle as error
    return false;
  }
}

/* attempts to send a packet to sd; returns true on success and false on
 * failure */
bool send_packet(int sd, uint32_t op, uint8_t *block)
{
  uint8_t packet[JBOD_BLOCK_SIZE + HEADER_LEN]; // Packet is a fixed-size array.
  op = htonl(op);                               // op converted to netbyte.

  memcpy(packet, &op, sizeof(op));
  uint8_t inCode = 0;   // inCode initialization.
  bool isBlock = false; // To determine if block needs to be sent.

  // Use a flag variable instead of directly checking the pointer.
  bool isNullBlock = block == NULL;

  switch (isNullBlock)
  {
  case true:
    isBlock = false;
    inCode = 0;
    break;
  default:
    isBlock = true;
    inCode = 2;
  }

  memcpy(&packet[sizeof(op)], &inCode, 1);
  bool retVal; // Determines return value.

  // Switch statement based on block existence flag.
  switch (isBlock)
  {
  case true:
    memcpy(&packet[sizeof(op) + 1], block, JBOD_BLOCK_SIZE);
    retVal = nwrite(sd, JBOD_BLOCK_SIZE + HEADER_LEN, packet); // Write with data.
    break;
  default:
    retVal = nwrite(sd, HEADER_LEN, packet); // Write without data.
  }

  return retVal;
}

/* connect to server and set the global client variable to the socket */
bool jbod_connect(const char *ip, uint16_t port)
{
  struct sockaddr_in caddr;

  caddr.sin_family = AF_INET; // Set family to IPv4
  caddr.sin_port = htons(port);

  switch (inet_aton(ip, &caddr.sin_addr))
  {
  case 0:
    return false; // IP conversion failure
  case 1:
    // IP successfully converted, proceed with socket creation
    cli_sd = socket(AF_INET, SOCK_STREAM, 0);
    if (cli_sd == -1)
      return false; // Socket creation failure

    // Socket created, proceed with connection
    return (connect(cli_sd, (const struct sockaddr *)&caddr, sizeof(caddr)) == -1) ? false : true;
  default:
    // Unexpected return value from inet_aton, handle as error
    return false;
  }
}

void jbod_disconnect(void)
{
  close(cli_sd); // Close socket
  cli_sd = -1;
}

int jbod_client_operation(uint32_t op, uint8_t *block)
{
  uint8_t returned_ret; // Variable to store returned ret value.
  uint32_t returned_op; // Variable to store returned op code.

  bool returned = send_packet(cli_sd, op, block); // Packet is sent.

  if (!returned)
  {
    return -1;
  }

  returned = recv_packet(cli_sd, &returned_op, &returned_ret, block); // Packet is received.

  if (!returned)
  {
    return -1;
  }

  switch ((returned_ret >> 7) & 0x01)
  { // Bit manipulation is conducted to isolate the rightmost bit.
  case 1:
    return -1;
  case 0:
    return 0;
  default:
    return -1; // Add an error handling case for unexpected signifier value.
  }
}