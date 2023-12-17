#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "mdadm.h"
#include "math.h"
#include "jbod.h"
#include <stdbool.h>
#include "net.h"

bool MOUNTED = false;
bool WRITEPERM = false;

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

  jbod_client_operation(JBOD_MOUNT, NULL);
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
  jbod_client_operation(JBOD_UNMOUNT, NULL);
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

  // values retrieved from PDF documentation
  tempDiskID = (diskID & 0xff);
  tempBockID = (bockID & 0xff) << 4;
  tempCommand = (command & 0xff) << 12;
  tempReserved = (reserved & 0xfff) << 20;
  retval = tempDiskID | tempBockID | tempCommand | tempReserved;

  return retval;
}

void seekToStart(uint32_t starting_disk)
{
  jbod_operation(op(starting_disk, 0, JBOD_SEEK_TO_DISK, 0), NULL);
}

void seekToFirstBlock(uint32_t starting_block)
{
  jbod_operation(op(0, starting_block, JBOD_SEEK_TO_BLOCK, 0), NULL);
}

int calcStartingPos(uint32_t start_addr)
{
  return start_addr % JBOD_BLOCK_SIZE;
}

int calcEndingPos(uint32_t start_addr, uint32_t read_length)
{
  return (start_addr + read_length - 1) % JBOD_BLOCK_SIZE;
}

int calcStartingDisk(uint32_t start_addr)
{
  return start_addr / JBOD_DISK_SIZE;
}

int calcStartingBlock(uint32_t start_addr)
{
  return start_addr % JBOD_DISK_SIZE / JBOD_BLOCK_SIZE;
}

int calcEndingDisk(uint32_t start_addr, uint32_t read_length)
{
  return (start_addr + read_length - 1) / JBOD_DISK_SIZE;
}

int calcEndingBlock(uint32_t start_addr, uint32_t read_length)
{
  return ((start_addr + read_length - 1) % JBOD_DISK_SIZE) / JBOD_BLOCK_SIZE;
}

void writeDataToBlock(uint8_t * write_tempry_buf)
{
  jbod_client_operation(op(0, 0, JBOD_WRITE_BLOCK, 0), write_tempry_buf);
}

void readDataFromBlock(uint8_t * temp)
{
  jbod_client_operation(op(0, 0, JBOD_READ_BLOCK, 0), temp);
}

void seekToStartBlock(unsigned int starting_block){
    jbod_client_operation(op(0, starting_block, JBOD_SEEK_TO_BLOCK, 0), NULL);
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

  unsigned int megabtye = pow((JBOD_BLOCK_SIZE * 4), 2); // megabytes in bytes

  // Check for errors in the input parameters
  if (MOUNTED == false || (start_addr + read_len > megabtye) || start_addr < 0 || read_len < 0 || read_len > (JBOD_BLOCK_SIZE * 4) || (read_buf == NULL && read_len != 0) || sizeof(read_buf) > (JBOD_BLOCK_SIZE * 4))
  {
    return -1;
  }

  // Calculate the start and end disks and blocks
  unsigned int starting_disk = calcStartingDisk(start_addr),
               starting_block = calcStartingBlock(start_addr),

               ending_disk = calcEndingDisk(start_addr, read_len),
               ending_block = calcEndingBlock(start_addr, read_len);

  // Seek to the start disk
  seekToStart(starting_disk);

  // Seek to the start block
  seekToStartBlock(starting_block);

  unsigned int current_block = starting_block,
               current_dick = starting_disk;
  uint8_t *current_pointer = read_buf;

  // Loop through each block and read the data
  for (unsigned int read = 0; read < read_len; read += 0)
{
    int condition = 0;  // Variable to represent the condition

    // Check conditions using a series of assignments
    condition = (starting_disk == ending_disk && starting_block == ending_block);
    if (condition)
    {
        // Code for the first condition
        unsigned int start_position = calcStartingPos(start_addr),
            end_position = calcEndingPos(start_addr, read_len),
            length = ((JBOD_BLOCK_SIZE - start_position + 1) - (JBOD_BLOCK_SIZE - end_position - 1) - 1);
        uint8_t temp[JBOD_BLOCK_SIZE];

        // Read the data from the block
        readDataFromBlock(temp);

        // Copy the data to current_pointer
        int j = 0;
        int i = start_position;
        do
        {
            current_pointer[j] = temp[i];
            j++;
            i++;
        } while (i <= end_position);

        // Update pointers and counters
        read += length;
        current_pointer += length;
        goto update_counters;
    }

    condition = (starting_disk == current_dick && starting_block == current_block);
    if (condition)
    {
        // Code for the second condition
        int start_position = start_addr % JBOD_BLOCK_SIZE;
        int len = JBOD_BLOCK_SIZE - start_position;
        uint8_t temp[JBOD_BLOCK_SIZE];
        readDataFromBlock(temp);

        // Copy the data to current_pointer
        int j = 0;
        int i = start_position;
        do
        {
            current_pointer[j] = temp[i];
            j++;
            i++;
        } while (i < JBOD_BLOCK_SIZE);

        // Update pointers and counters
        current_pointer += len;
        read += len;
        goto update_counters;
    }

    condition = (ending_disk == current_dick && ending_block == current_block);
    if (condition)
    {
        // Code for the third condition
        int end_position = (start_addr + read_len - 1) % JBOD_BLOCK_SIZE;
        uint8_t temp[JBOD_BLOCK_SIZE];
        readDataFromBlock(temp);

        // Copy the data to current_pointer
        int i = 0;
        do
        {
            current_pointer[i] = temp[i];
            i++;
        } while (i <= end_position);

        // Update pointers and counters
        current_pointer += (end_position + 1);
        read += (end_position + 1);
        goto update_counters;
    }

    // Code for the fourth condition
    {
        uint8_t temp[JBOD_BLOCK_SIZE];
        readDataFromBlock(temp);

        // Copy the data to current_pointer
        int i = 0;
        do
        {
            current_pointer[i] = temp[i];
            i++;
        } while (i < JBOD_BLOCK_SIZE);

        // Update pointers and counters
        current_pointer += JBOD_BLOCK_SIZE;
        read += JBOD_BLOCK_SIZE;
    }

    // Label to update block and disk counters
    update_counters:

    // Move to the next block
    current_block++;
    if (JBOD_NUM_BLOCKS_PER_DISK - 1 <  current_block)
    {
        current_block = 0x0;
        current_dick += 1;
        jbod_client_operation(op(current_dick, 0, JBOD_SEEK_TO_DISK, 0), NULL);
    }
}

 return read_len;
}

/**
 *
 * @brief: This function sends a JBOD_WRITE_PERMISSION operation to the JBOD device to request write permission.
 * If the operation is successful, the WRITEPERM flag is set to true.
 *
 * @return: 0 if the operation is successful, otherwise an error code.
 */
int mdadm_write_permission(void)
{
  int result = jbod_client_operation(op(0, 0, JBOD_WRITE_PERMISSION, 0), NULL);
  if (result == 0)
  {
    WRITEPERM = true;
  }
  return result;
}

/**
 *
 * @brief: This function sends a JBOD_REVOKE_WRITE_PERMISSION operation to the JBOD device to revoke write permission.
 * If the operation is successful, the WRITEPERM flag is set to false.
 *
 * @return: 0 if the operation is successful, otherwise an error code.
 */
int mdadm_revoke_write_permission(void)
{
  int result = jbod_client_operation(op(0, 0, JBOD_REVOKE_WRITE_PERMISSION, 0), NULL);
  if (result == 0)
  {
    WRITEPERM = false;
  }
  return result;
}

/**
 * @brief Writes data to the JBOD disks starting from the specified address.
 *
 * @param start_addr The starting address to write data to.
 * @param write_len The length of the data to write.
 * @param write_buf The buffer containing the data to write.
 * @return int The number of bytes written, or -1 if there was an error.
 */
int mdadm_write(uint32_t start_addr, uint32_t write_len, const uint8_t *write_buf)
{

  unsigned int megabtye = pow((JBOD_BLOCK_SIZE * 4), 2); // megabytes in bytes

  // Check for write permission and valid input parameters
  if (!WRITEPERM || (start_addr + write_len > megabtye) || start_addr < 0 || write_len < 0 || write_len > (JBOD_BLOCK_SIZE * 4) || (write_buf == NULL && write_len != 0) || sizeof(write_buf) > (JBOD_BLOCK_SIZE * 4))
  {
    return -1;
  }

  // Calculate the start and end disk and block numbers
  unsigned int starting_disk = calcStartingDisk(start_addr),
      starting_block = calcStartingBlock(start_addr),

      ending_disk = calcEndingDisk(start_addr, write_len),
      ending_block = calcEndingBlock(start_addr, write_len);

  // Seek to the start disk
  jbod_client_operation(op(starting_disk, 0, JBOD_SEEK_TO_DISK, 0), NULL);

  // Seek to the start block
  seekToStartBlock(starting_block);

  unsigned int current_block = starting_block;
  unsigned int current_dick = starting_disk;
  uint8_t *current_pointer = (uint8_t *)write_buf;

  // Write data to the disks
 for (int write = 0; write < write_len; write += 0)
{
    int condition = 0;  // Variable to represent the condition

    // Check conditions using a series of assignments
    condition = (starting_disk == ending_disk && starting_block == ending_block);
    if (condition)
    {
        // Code for the first condition
        int start_position = calcStartingPos(start_addr),
            end_position = (start_addr + write_len - 1) % JBOD_BLOCK_SIZE,
            length = ((JBOD_BLOCK_SIZE - start_position + 1) - (JBOD_BLOCK_SIZE - end_position - 1) - 1);
        uint8_t write_temp_buf[JBOD_BLOCK_SIZE], read_temp_buf[JBOD_BLOCK_SIZE];

        // Read the block to be written to
        jbod_client_operation(op(0, 0, JBOD_READ_BLOCK, 0), read_temp_buf);

        // Copy the data before the start position
        unsigned int i = 0;
        do
        {
            write_temp_buf[i] = read_temp_buf[i];
            i++;
        } while (i < start_position);

        // Copy the data after the end position
        i = end_position + 1;
        do
        {
            write_temp_buf[i] = read_temp_buf[i];
            i++;
        } while (i < JBOD_BLOCK_SIZE);

        // Copy the data to be written
        unsigned int j = 0;
        i = start_position;
        do
        {
            write_temp_buf[i] = current_pointer[j];
            j++;
            i++;
        } while (i <= end_position);

        // Seek to the block to be written to and write the data
        jbod_client_operation(op(0, current_block, JBOD_SEEK_TO_BLOCK, 0), NULL);
        writeDataToBlock(write_temp_buf);

        // Update the write pointer and counter
        current_pointer += length;
        write += length;
        goto update_counters;
    }

    condition = (starting_disk == current_dick && starting_block == current_block);
    if (condition)
    {
        // Code for the second condition
        unsigned int start_pos = start_addr % JBOD_BLOCK_SIZE,
            len = JBOD_BLOCK_SIZE - start_pos;
        uint8_t write_temp_buf[JBOD_BLOCK_SIZE],
                read_temp_buf[JBOD_BLOCK_SIZE];

        // Read the block to be written to
        jbod_client_operation(op(0, 0, JBOD_READ_BLOCK, 0), read_temp_buf);

        int i = 0;
        do
        {
            write_temp_buf[i] = read_temp_buf[i];
            i++;
        } while (i < start_pos);

        // Copy the data to be written
        jbod_client_operation(op(0, current_block, JBOD_SEEK_TO_BLOCK, 0), NULL);
        int j = 0;
        int k = start_pos;
        do
        {
            write_temp_buf[k] = current_pointer[j];
            j++;
            k++;
        } while (k < JBOD_BLOCK_SIZE);

        // Write the data to the block
        writeDataToBlock(write_temp_buf);

        // Update the write pointer and counter
        current_pointer += (len);
        write += len;
        goto update_counters;
    }

    condition = (ending_disk == current_dick && ending_block == current_block);
    if (condition)
    {
        // Code for the third condition
        int end_position = (start_addr + write_len - 1) % JBOD_BLOCK_SIZE;
        uint8_t write_temp_buf[JBOD_BLOCK_SIZE],
                read_temp_buf[JBOD_BLOCK_SIZE];

        // Read the block to be written to
        jbod_client_operation(op(0, 0, JBOD_READ_BLOCK, 0), read_temp_buf);

        // Copy the data to be written
        int i = 0;
        do
        {
            write_temp_buf[i] = current_pointer[i];
            i++;
        } while (i <= end_position);

        // Copy the data after the end position
        jbod_client_operation(op(0, ending_block, JBOD_SEEK_TO_BLOCK, 0), NULL);
        int n = end_position + 1;
        do
        {
            write_temp_buf[n] = read_temp_buf[n];
            n++;
        } while (n < JBOD_BLOCK_SIZE);

        // Write the data to the block
        writeDataToBlock(write_temp_buf);

        // Update the write pointer and counter
        current_pointer += (end_position + 1);
        write += (end_position + 1);
        goto update_counters;
    }

    // Code for the fourth condition
    {
        uint8_t temp[JBOD_BLOCK_SIZE];

        // Copy the data to be written
        int i = 0;
        while (i < JBOD_BLOCK_SIZE)
        {
            temp[i] = current_pointer[i];
            i++;
        }

        // Write the data to the block
        jbod_client_operation(op(0, 0, JBOD_WRITE_BLOCK, 0), temp);

        // Update the write pointer and counter
        current_pointer += JBOD_BLOCK_SIZE;
        write += JBOD_BLOCK_SIZE;
    }

    // Label to update block and disk counters
    update_counters:

    // Update the block and disk counters
    current_block++;
    if (current_block > JBOD_BLOCK_SIZE - 1)
    {
        current_block = 0x0;
        current_dick += 1;
        jbod_client_operation(op(current_dick, 0, JBOD_SEEK_TO_DISK, 0), NULL);
    }
}

  // Return the number of bytes written
  return write_len;
}