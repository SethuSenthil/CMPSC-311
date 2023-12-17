#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "mdadm.h"
#include "math.h"
#include "jbod.h"
#include <stdbool.h>

bool MOUNTED = false;
bool WRITEPERM = false;

/**
 * Mounts the disk if it is not already mounted.
 *
 * @return 1 if the disk is mounted successfully, -1 if it is already mounted
 */
int mdadm_mount(void)
{
    return MOUNTED ? -1 : (jbod_operation(JBOD_MOUNT, NULL), MOUNTED = true, 1);
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

void cache_handler(int current_dick, int current_block, uint8_t *block_buf, uint8_t *write_tempry_buf)
{
    switch (cache_lookup(current_dick, current_block, block_buf))
    {
    case 1:
        cache_update(current_dick, current_block, write_tempry_buf);
        break;
    default:
        cache_insert(current_dick, current_block, write_tempry_buf);
        break;
    }
}

void cache_handler_2(int current_dick, int current_block, uint8_t *block_buf, uint8_t *temp)
{
    switch (cache_lookup(current_dick, current_block, temp))
    {
    case 1:
        jbod_operation(op(0, current_block + 1, JBOD_SEEK_TO_BLOCK, 0), NULL);
        break;
    default:
        jbod_operation(op(0, 0, JBOD_READ_BLOCK, 0), temp);
        break;
    }
}

/**
 * Reads data from the disk.
 *
 * @param start_addr the starting address to read from
 * @param read_length the number of bytes to read
 * @param read_buf the buffer to store the read data
 * @return the number of bytes read, or -1 if there was an error
 */
int mdadm_read(uint32_t start_addr, uint32_t read_length, uint8_t *read_buf)
{

    int megabtye = pow((JBOD_BLOCK_SIZE * 4), 2); // megabytes in bytes

    // Check for errors in the input parameters
    if (MOUNTED == false || (start_addr + read_length > megabtye) || start_addr < 0 || read_length < 0 || read_length > (JBOD_BLOCK_SIZE * 4) || (read_buf == NULL && read_length != 0) || sizeof(read_buf) > (JBOD_BLOCK_SIZE * 4))
    {
        return -1;
    }

    // Calculate the start and end disks and blocks
    int starting_disk = calcStartingDisk(start_addr),
        starting_block = calcStartingBlock(start_addr),

        ending_disk = calcEndingDisk(start_addr, read_length),
        ending_block = calcEndingBlock(start_addr, read_length);

    // Seek to the start disk
    seekToStart(starting_disk);

    // Seek to the start block
    jbod_operation(op(0, starting_block, JBOD_SEEK_TO_BLOCK, 0), NULL);

    int current_block = starting_block,
        current_dick = starting_disk;
    uint8_t *current_pointer = read_buf;

    // Loop through each block and read the data
    // Loop through the read length
    for (int read = 0; read < read_length; read += 0)
    {
        int switch_case = 0; // Variable to determine the case

        // Determine the switch case based on the starting and ending disk and block
        if (starting_disk == ending_disk && starting_block == ending_block)
        {
            switch_case = 1;
        }
        else if (starting_disk == current_dick && starting_block == current_block)
        {
            switch_case = 2;
        }
        else if (ending_disk == current_dick && ending_block == current_block)
        {
            switch_case = 3;
        }
        else
        {
            switch_case = 4;
        }

        // Perform the appropriate action based on the switch case
        switch (switch_case)
        {
        case 1:
        {
            // Calculate the start and end positions, and the length of the data to be read
            int start_position = calcStartingPos(start_addr),
                end_position = calcEndingPos(start_addr, read_length),
                length = ((JBOD_BLOCK_SIZE - start_position + 1) - (JBOD_BLOCK_SIZE - end_position - 1) - 1);
            uint8_t temp[JBOD_BLOCK_SIZE];

            // Look up the data in the cache, or read it from disk if it's not in the cache
            cache_lookup(starting_disk, starting_block, temp) == 1 ? jbod_operation(op(0, starting_block + 1, JBOD_SEEK_TO_BLOCK, 0), NULL) : jbod_operation(op(0, 0, JBOD_READ_BLOCK, 0), temp);

            // Copy the data to the current pointer
            int j = 0;
            int i = start_position;
            while (i <= end_position)
            {
                current_pointer[j] = temp[i];
                i++;
                j++;
            }

            // Move the current pointer and update the read length
            current_pointer += length;
            read += length;

            break;
        }
        case 2:
        {
            // Calculate the start position and length of the data to be read
            int start_position = calcStartingPos(start_addr),
                length = JBOD_BLOCK_SIZE - start_position;
            uint8_t temp[JBOD_BLOCK_SIZE];

            // Read the data from the cache
            cache_handler_2(current_dick, current_block, temp, temp);

            // Copy the data to the current pointer
            int j = 0,
                i = start_position;
            while (i < JBOD_BLOCK_SIZE)
            {
                current_pointer[j] = temp[i];
                i++;
                j++;
            }

            // Move the current pointer and update the read length
            current_pointer += (length);
            read += length;

            break;
        }
        case 3:
        {
            // Calculate the end position of the data to be read
            int end_position = calcEndingPos(start_addr, read_length);
            uint8_t temp[JBOD_BLOCK_SIZE];

            // Read the data from the cache
            cache_handler_2(current_dick, current_block, temp, temp);

            // Copy the data to the current pointer
            int i = 0;
            while (i <= end_position)
            {
                current_pointer[i] = temp[i];
                i++;
            }

            // Move the current pointer and update the read length
            current_pointer += (end_position + 1);
            read += (end_position + 1);

            break;
        }
        default:
        {
            uint8_t temp[JBOD_BLOCK_SIZE];

            // Read the data from the cache
            cache_handler_2(current_dick, current_block, temp, temp);

            // Copy the data to the current pointer
            int i = 0;
            while (i < JBOD_BLOCK_SIZE)
            {
                current_pointer[i] = temp[i];
                i++;
            }

            // Move the current pointer and update the read length
            current_pointer += JBOD_BLOCK_SIZE;
            read += JBOD_BLOCK_SIZE;

            break;
        }
        }

        // Move to the next block
        current_block++;

        // If we've reached the end of the disk, move to the next disk
        switch (current_block > JBOD_NUM_BLOCKS_PER_DISK - 1)
        {
        case true:
            current_block = 0;
            current_dick += 1;
            jbod_operation(op(current_dick, 0, JBOD_SEEK_TO_DISK, 0), NULL);
            break;
        case false:
            // Do nothing
            break;
        }
    }

    return read_length;
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
    int result = jbod_operation(op(0, 0, JBOD_WRITE_PERMISSION, 0), NULL);
    WRITEPERM = (result == 0) ? true : false;
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
    int result = jbod_operation(op(0, 0, JBOD_REVOKE_WRITE_PERMISSION, 0), NULL);
    WRITEPERM = (result == 0) ? false : WRITEPERM;
    return result;
}

void read_block(int current_dick, int current_block, uint8_t *read_tempry_buf)
{
    switch (cache_lookup(current_dick, current_block, read_tempry_buf))
    {
    case 1:
        jbod_operation(op(0, current_block + 1, JBOD_SEEK_TO_BLOCK, 0), NULL);
        break;
    default:
        jbod_operation(op(0, 0, JBOD_READ_BLOCK, 0), read_tempry_buf);
        break;
    }
}

/**
 * @brief Writes data to the JBOD disks starting from the specified address.
 *
 * @param start_addr The starting address to write data to.
 * @param write_length The lengthgth of the data to write.
 * @param write_buf The buffer containing the data to write.
 * @return int The number of bytes written, or -1 if there was an error.
 */
int mdadm_write(uint32_t start_addr, uint32_t write_length, const uint8_t *write_buf)
{

    int megabtye = pow((JBOD_BLOCK_SIZE * 4), 2); // megabytes in bytes
    uint8_t block_buf[JBOD_BLOCK_SIZE];

    // Check for write permission and valid input parameters
    if (!WRITEPERM || (start_addr + write_length > megabtye) || start_addr < 0 || write_length < 0 || write_length > (JBOD_BLOCK_SIZE * 4) || (write_buf == NULL && write_length != 0) || sizeof(write_buf) > (JBOD_BLOCK_SIZE * 4))
    {
        return -1;
    }

    // Calculate the start and end disk and block numbers
    int starting_disk = calcStartingDisk(start_addr),
        starting_block = calcStartingBlock(start_addr),

        ending_disk = calcEndingDisk(start_addr, write_length),
        ending_block = calcEndingBlock(start_addr, write_length);

    // Seek to the start disk
    seekToStart(starting_disk);

    // Seek to the start block
    seekToFirstBlock(starting_disk);

    int current_block = starting_block,
        current_dick = starting_disk;
    uint8_t *current_pointer = (uint8_t *)write_buf;
    // int write = 0;

    // Write data to the disks
    for (int write = 0; write < write_length; write += 0)
    {
        int switch_case = 0; // Variable to determine the case

        if (starting_disk == ending_disk && starting_block == ending_block)
        {
            switch_case = 1;
        }
        else if (starting_disk == current_dick && starting_block == current_block)
        {
            switch_case = 2;
        }
        else if (ending_disk == current_dick && ending_block == current_block)
        {
            switch_case = 3;
        }
        else
        {
            switch_case = 4;
        }

        switch (switch_case)
        {
        case 1:
        {
            int start_position = calcStartingPos(start_addr),
                end_position = (start_addr + write_length - 1) % JBOD_BLOCK_SIZE,
                length = ((JBOD_BLOCK_SIZE - start_position + 1) - (JBOD_BLOCK_SIZE - end_position - 1) - 1);
            uint8_t write_tempry_buf[JBOD_BLOCK_SIZE], read_tempry_buf[JBOD_BLOCK_SIZE];

            // Read the block to be written to
            read_block(current_dick, current_block, read_tempry_buf);

            // Copy the data before the start position
            int i = 0;
            while (i < start_position)
            {
                write_tempry_buf[i] = read_tempry_buf[i];
                i++;
            }

            // Copy the data after the end position
            i = end_position + 1;
            while (i < JBOD_BLOCK_SIZE)
            {
                write_tempry_buf[i] = read_tempry_buf[i];
                i++;
            }

            // Copy the data to be written
            int j = 0;
            i = start_position;
            while (i <= end_position)
            {
                write_tempry_buf[i] = current_pointer[j];
                i++;
                j++;
            }

            // Seek to the block to be written to and write the data
            jbod_operation(op(0, current_block, JBOD_SEEK_TO_BLOCK, 0), NULL);

            cache_handler(current_dick, current_block, block_buf, write_tempry_buf);

            jbod_operation(op(0, 0, JBOD_WRITE_BLOCK, 0), write_tempry_buf);

            // Update the write pointer and counter
            current_pointer += length;
            write += length;

            break;
        }
        case 2:
        {
            int start_position = calcStartingPos(start_addr),
                length = JBOD_BLOCK_SIZE - start_position;
            uint8_t write_tempry_buf[JBOD_BLOCK_SIZE],
                read_tempry_buf[JBOD_BLOCK_SIZE];

            // Read the block to be written to
            read_block(current_dick, current_block, read_tempry_buf);

            // Copy the data before the start position
            int i = 0;
            while (i < start_position)
            {
                write_tempry_buf[i] = read_tempry_buf[i];
                i++;
            }

            // Copy the data to be written
            jbod_operation(op(0, current_block, JBOD_SEEK_TO_BLOCK, 0), NULL);
            int j = 0;
            int ii = start_position;
            while (ii < JBOD_BLOCK_SIZE)
            {
                write_tempry_buf[ii] = current_pointer[j];
                j++;
                ii++;
            }

            // might need to fix this

            cache_handler(current_dick, current_block, block_buf, write_tempry_buf);

            // Write the data to the block
            jbod_operation(op(0, 0, JBOD_WRITE_BLOCK, 0), write_tempry_buf);

            // Update the write pointer and counter
            write += length;
            current_pointer += (length);

            break;
        }
        case 3:
        {
            int end_position = (start_addr + write_length - 1) % JBOD_BLOCK_SIZE;
            uint8_t write_tempry_buf[JBOD_BLOCK_SIZE], read_tempry_buf[JBOD_BLOCK_SIZE];

            // Read the block to be written to
            read_block(current_dick, current_block, read_tempry_buf);

            int i = 0;
            while (i <= end_position)
            {
                write_tempry_buf[i] = current_pointer[i];
                i++;
            }

            // Copy the data after the end position
            jbod_operation(op(0, ending_block, JBOD_SEEK_TO_BLOCK, 0), NULL);
            int ii = end_position + 1;
            while (ii < JBOD_BLOCK_SIZE)
            {
                write_tempry_buf[ii] = read_tempry_buf[ii];
                ii++;
            }

            cache_handler(current_dick, current_block, block_buf, write_tempry_buf);

            // Write the data to the block
            jbod_operation(op(0, 0, JBOD_WRITE_BLOCK, 0), write_tempry_buf);

            // Update the write pointer and counter
            current_pointer += (end_position + 1);
            write += (end_position + 1);

            break;
        }
        default:
        {
            uint8_t temp[JBOD_BLOCK_SIZE];

            int i = 0;
            while (i < JBOD_BLOCK_SIZE)
            {
                temp[i] = current_pointer[i];
                i++;
            }

            cache_handler(current_dick, current_block, block_buf, temp);

            // Write the data to the block
            jbod_operation(op(0, 0, JBOD_WRITE_BLOCK, 0), temp);

            // Update the write pointer and counter
            current_pointer += JBOD_BLOCK_SIZE;
            write += JBOD_BLOCK_SIZE;

            break;
        }
        }

        // Update the block and disk counters
        current_block++;
        if (current_block > JBOD_BLOCK_SIZE - 1)
        {
            current_block = 0;
            current_dick += 1;
            jbod_operation(op(current_dick, 0, JBOD_SEEK_TO_DISK, 0), NULL);
        }
    }

    // Return the number of bytes written
    return write_length;
}