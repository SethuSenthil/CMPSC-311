#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "mdadm.h"
#include "jbod.h"
#include <stdbool.h>
#include <math.h>


bool MOUNTED = false;

/**
 * Mounts the disk if it is not already mounted.
 *
 * @return 1 if the disk is mounted successfully, -1 if it is already mounted
 */
int mdadm_mount(void)
{
  if (MOUNTED)
  {
    return -1; // already mounted
  }

  // otherwise:

  jbod_operation(JBOD_MOUNT, NULL);
  MOUNTED = true;
  return 1;

  return -1;
}

/**
 * Unmounts the disk if it is mounted.
 *
 * @return 1 if the disk is unmounted successfully, -1 if it is not mounted
 */
int mdadm_unmount(void)
{
  if (!MOUNTED)
  {
    // nothing mounted
    return -1;
  }
  jbod_operation(JBOD_UNMOUNT, NULL);
  // mountes system
  MOUNTED = false;
  return 1;
}

/**
 * Creates an operation code from the given parameters.
 *
 * @param diskID the ID of the disk
 * @param bockID the ID of the block
 * @param command the command to be executed
 * @param reserved reserved bits
 * @return the operation code
 */
uint32_t op(uint32_t diskID, uint32_t bockID, uint32_t command, uint32_t reserved)
{
  uint32_t retval = 0x0, tempDiskID, tempBockID, tempCommand, tempReserved;


  //values retrieved from PDF documentation
  tempDiskID = (diskID & 0xff);
  tempBockID = (bockID & 0xff) << 4;
  tempCommand = (command & 0xff) << 12;
  tempReserved = (reserved & 0xfff) << 20;
  retval = tempDiskID | tempBockID | tempCommand | tempReserved;

  return retval;
}

/**
 * Reads data from the disk.
 *
 * @param start_addr the starting address to read from
 * @param read_len the number of bytes to read
 * @param read_buf the buffer to store the read data
 * @return the number of bytes read, or -1 if there was an error
 */
int mdadm_read(uint32_t start_addr, uint32_t read_len, uint8_t *read_buf)
{

  // Make sure the disk is mounted and the addresses are in bound and valid
  int megabtye = pow((JBOD_BLOCK_SIZE * 4), 2); //megabytes in bytes

  if (MOUNTED == false || (start_addr + read_len > megabtye) || start_addr < 0 || read_len < 0 || read_len > (JBOD_BLOCK_SIZE * 4) || (read_buf == NULL && read_len != 0) || sizeof(read_buf) > (JBOD_BLOCK_SIZE * 4))
  {
    return -1;
  }

  // Read the disks
  // Use While loop to read until last byte

  int current_address = start_addr;
  int current_disk = start_addr / JBOD_DISK_SIZE;
  int current_block = (start_addr % JBOD_DISK_SIZE) / JBOD_BLOCK_SIZE;
  int offset = current_address % JBOD_BLOCK_SIZE;
  int read_bytes = 0;
  uint8_t temp_buf[JBOD_BLOCK_SIZE];
  int remaining_bytes = 0;

  while (read_bytes < read_len)
  {
    // Seek to the current disk and block
    jbod_operation(op(current_disk, 0, JBOD_SEEK_TO_DISK, 0), NULL);
    jbod_operation(op(0, current_block, JBOD_SEEK_TO_DISK, 0), NULL);

    // Read the current block
    jbod_operation(op(0, 0, JBOD_READ_BLOCK, 0), temp_buf);

    // Calculate the number of remaining bytes to read
    remaining_bytes = read_len - read_bytes;

    // If the remaining bytes fit within the current block, copy them to the read buffer
    if (offset + remaining_bytes < JBOD_BLOCK_SIZE)
    {
      memcpy(read_buf + read_bytes, temp_buf + offset, remaining_bytes);
      read_bytes += remaining_bytes;
    }
    // Otherwise, copy the bytes that fit within the current block and continue to the next block
    else
    {
      memcpy(read_buf + read_bytes, temp_buf + offset, JBOD_BLOCK_SIZE - offset);
      read_bytes += JBOD_BLOCK_SIZE - offset;
    }

    // Update the current address and offset
    current_address += read_bytes;
    offset = 0;
  }

  return read_bytes;
}