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

	// values retrieved from PDF documentation
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

	int megabtye = pow((JBOD_BLOCK_SIZE * 4), 2); // megabytes in bytes

	// Check for errors in the input parameters
	if (MOUNTED == false || (start_addr + read_len > megabtye) || start_addr < 0 || read_len < 0 || read_len > (JBOD_BLOCK_SIZE * 4) || (read_buf == NULL && read_len != 0) || sizeof(read_buf) > (JBOD_BLOCK_SIZE * 4))
	{
		return -1;
	}

	// Calculate the start and end disks and blocks
	uint32_t starting_disk = start_addr / JBOD_DISK_SIZE;
	uint32_t ending_disk = (start_addr + read_len - 1) / JBOD_DISK_SIZE;
	uint32_t starting_block = start_addr % JBOD_DISK_SIZE / JBOD_BLOCK_SIZE;
	uint32_t ending_block = ((start_addr + read_len - 1) % JBOD_DISK_SIZE) / JBOD_BLOCK_SIZE;

	// Seek to the start disk
	jbod_operation(op(starting_disk, 0, JBOD_SEEK_TO_DISK, 0), NULL);

	// Seek to the start block
	jbod_operation(op(0, starting_block, JBOD_SEEK_TO_BLOCK, 0), NULL);

	int current_block = starting_block;
	int current_dick = starting_disk;
	uint8_t *current_pointer = read_buf;
	int read = 0;

	// Loop through each block and read the data
	while (read < read_len)
	{
		if (starting_disk == ending_disk && starting_block == ending_block)
		{
			// If the start and end blocks are the same, read the data from the block
			int start_pos = start_addr % JBOD_BLOCK_SIZE;
			int end_pos = (start_addr + read_len - 1) % JBOD_BLOCK_SIZE;
			int len = ((JBOD_BLOCK_SIZE - start_pos + 1) - (JBOD_BLOCK_SIZE - end_pos - 1) - 1);
			uint8_t temp[JBOD_BLOCK_SIZE];
			jbod_operation(op(0, 0, JBOD_READ_BLOCK, 0), temp);
			int j = 0;
			for (int i = start_pos; i <= end_pos; i++)
			{
				current_pointer[j] = temp[i];
				j++;
			}
			current_pointer += len;
			read += len;
		}

		else if (starting_disk == current_dick && starting_block == current_block)
		{
			// If the current block is the start block, read the data from the block
			int start_pos = start_addr % JBOD_BLOCK_SIZE;
			int len = JBOD_BLOCK_SIZE - start_pos;
			uint8_t temp[JBOD_BLOCK_SIZE];
			jbod_operation(op(0, 0, JBOD_READ_BLOCK, 0), temp);
			int j = 0;
			for (int i = start_pos; i < JBOD_BLOCK_SIZE; i++)
			{
				current_pointer[j] = temp[i];
				j++;
			}
			current_pointer += (len);
			read += len;
		}

		else if (ending_disk == current_dick && ending_block == current_block)
		{
			// If the current block is the end block, read the data from the block
			int end_pos = (start_addr + read_len - 1) % JBOD_BLOCK_SIZE;
			uint8_t temp[JBOD_BLOCK_SIZE];
			jbod_operation(op(0, 0, JBOD_READ_BLOCK, 0), temp);
			for (int i = 0; i <= end_pos; i++)
			{
				current_pointer[i] = temp[i];
			}
			current_pointer += (end_pos + 1);
			read += (end_pos + 1);
		}

		else
		{
			// If the current block is not the start or end block, read the data from the block
			uint8_t temp[JBOD_BLOCK_SIZE];
			jbod_operation(op(0, 0, JBOD_READ_BLOCK, 0), temp);
			for (int i = 0; i < JBOD_BLOCK_SIZE; i++)
			{
				current_pointer[i] = temp[i];
			}
			current_pointer += JBOD_BLOCK_SIZE;
			read += JBOD_BLOCK_SIZE;
		}

		// Move to the next block
		current_block++;
		if (current_block > JBOD_NUM_BLOCKS_PER_DISK - 1)
		{
			current_block = 0;
			current_dick += 1;
			jbod_operation(op(current_dick, 0, JBOD_SEEK_TO_DISK, 0), NULL);
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
	int result = jbod_operation(op(0, 0, JBOD_WRITE_PERMISSION, 0), NULL);
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
	int result = jbod_operation(op(0, 0, JBOD_REVOKE_WRITE_PERMISSION, 0), NULL);
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

	int megabtye = pow((JBOD_BLOCK_SIZE * 4), 2); // megabytes in bytes

	// Check for write permission and valid input parameters
	if (!WRITEPERM || (start_addr + write_len > megabtye) || start_addr < 0 || write_len < 0 || write_len > (JBOD_BLOCK_SIZE * 4) || (write_buf == NULL && write_len != 0) || sizeof(write_buf) > (JBOD_BLOCK_SIZE * 4))
	{
		return -1;
	}

	// Calculate the start and end disk and block numbers
	uint32_t starting_disk = start_addr / ((uint32_t)JBOD_DISK_SIZE);
	uint32_t ending_disk = (start_addr + write_len - 1) / ((uint32_t)JBOD_DISK_SIZE);
	uint32_t starting_block = start_addr % JBOD_DISK_SIZE / JBOD_BLOCK_SIZE;
	uint32_t ending_block = ((start_addr + write_len - 1) % JBOD_DISK_SIZE) / JBOD_BLOCK_SIZE;

	// Seek to the start disk
	jbod_operation(op(starting_disk, 0, JBOD_SEEK_TO_DISK, 0), NULL);

	// Seek to the start block
	jbod_operation(op(0, starting_block, JBOD_SEEK_TO_BLOCK, 0), NULL);

	int current_block = starting_block;
	int current_dick = starting_disk;
	uint8_t *current_pointer = (uint8_t *)write_buf;
	int write = 0;

	// Write data to the disks
	while (write < write_len)
	{
		if (starting_disk == ending_disk && starting_block == ending_block)
		{
			int start_pos = start_addr % JBOD_BLOCK_SIZE;
			int end_pos = (start_addr + write_len - 1) % JBOD_BLOCK_SIZE;
			int len = ((JBOD_BLOCK_SIZE - start_pos + 1) - (JBOD_BLOCK_SIZE - end_pos - 1) - 1);
			uint8_t write_temp[JBOD_BLOCK_SIZE];
			uint8_t read_temp[JBOD_BLOCK_SIZE];

			// Read the block to be written to
			jbod_operation(op(0, 0, JBOD_READ_BLOCK, 0), read_temp);

			// Copy the data before the start position
			for (int i = 0; i < start_pos; i++)
			{
				write_temp[i] = read_temp[i];
			}

			// Copy the data after the end position
			for (int i = end_pos + 1; i < JBOD_BLOCK_SIZE; i++)
			{
				write_temp[i] = read_temp[i];
			}

			// Copy the data to be written
			int j = 0;
			for (int i = start_pos; i <= end_pos; i++)
			{
				write_temp[i] = current_pointer[j];
				j++;
			}

			// Seek to the block to be written to and write the data
			jbod_operation(op(0, current_block, JBOD_SEEK_TO_BLOCK, 0), NULL);
			jbod_operation(op(0, 0, JBOD_WRITE_BLOCK, 0), write_temp);

			// Update the write pointer and counter
			current_pointer += len;
			write += len;
		}

		else if (starting_disk == current_dick && starting_block == current_block)
		{
			int start_pos = start_addr % JBOD_BLOCK_SIZE;
			int len = JBOD_BLOCK_SIZE - start_pos;
			uint8_t write_temp[JBOD_BLOCK_SIZE];
			uint8_t read_temp[JBOD_BLOCK_SIZE];

			// Read the block to be written to
			jbod_operation(op(0, 0, JBOD_READ_BLOCK, 0), read_temp);

			// Copy the data before the start position
			for (int i = 0; i < start_pos; i++)
			{
				write_temp[i] = read_temp[i];
			}

			// Copy the data to be written
			jbod_operation(op(0, current_block, JBOD_SEEK_TO_BLOCK, 0), NULL);
			int j = 0;
			for (int i = start_pos; i < JBOD_BLOCK_SIZE; i++)
			{
				write_temp[i] = current_pointer[j];
				j++;
			}

			// Write the data to the block
			jbod_operation(op(0, 0, JBOD_WRITE_BLOCK, 0), write_temp);

			// Update the write pointer and counter
			current_pointer += (len);
			write += len;
		}

		else if (ending_disk == current_dick && ending_block == current_block)
		{
			int end_pos = (start_addr + write_len - 1) % JBOD_BLOCK_SIZE;
			uint8_t write_temp[JBOD_BLOCK_SIZE];
			uint8_t read_temp[JBOD_BLOCK_SIZE];

			// Read the block to be written to
			jbod_operation(op(0, 0, JBOD_READ_BLOCK, 0), read_temp);

			// Copy the data to be written
			for (int i = 0; i <= end_pos; i++)
			{
				write_temp[i] = current_pointer[i];
			}

			// Copy the data after the end position
			jbod_operation(op(0, ending_block, JBOD_SEEK_TO_BLOCK, 0), NULL);
			for (int i = end_pos + 1; i < JBOD_BLOCK_SIZE; i++)
			{
				write_temp[i] = read_temp[i];
			}

			// Write the data to the block
			jbod_operation(op(0, 0, JBOD_WRITE_BLOCK, 0), write_temp);

			// Update the write pointer and counter
			current_pointer += (end_pos + 1);
			write += (end_pos + 1);
		}
		else
		{
			uint8_t temp[JBOD_BLOCK_SIZE];

			// Copy the data to be written
			for (int i = 0; i < JBOD_BLOCK_SIZE; i++)
			{
				temp[i] = current_pointer[i];
			}

			// Write the data to the block
			jbod_operation(op(0, 0, JBOD_WRITE_BLOCK, 0), temp);

			// Update the write pointer and counter
			current_pointer += JBOD_BLOCK_SIZE;
			write += JBOD_BLOCK_SIZE;
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
	return write_len;
}