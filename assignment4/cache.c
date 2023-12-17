#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>

#include "cache.h"
#include "jbod.h"

// Uncomment the below code before implementing cache functions.
/*static cache_entry_t *cache = NULL;
static int cache_size = 0;
static int clock = 0;*/

static int num_queries = 0;
static int num_hits = 0;
static cache_entry_t *cache = NULL;
static int cache_size = 0;

/**
 * Creates a cache with the given number of entries.
 *
 * @param num_entries The number of entries in the cache.
 * @return 1 if the cache was successfully created, -1 otherwise.
 */
int cache_create(int num_entries)
{
  if ((num_entries >= 2) && (num_entries <= (JBOD_BLOCK_SIZE * JBOD_NUM_DISKS)) && (cache == NULL))
  { // check if cache is not already created and num_entries is within the valid range
    cache_size = num_entries;
    cache = malloc(cache_size * sizeof(cache_entry_t)); // allocate memory for the cache

    int i = 0;
    do
    {
      cache[i].valid = false; // initialize each entry in the cache as invalid
      i++;
    } while (i < num_entries);

    return 1; // return 1 to indicate successful cache creation
  }
  return -1; // return -1 to indicate cache creation failed
}

/**
 * @brief Frees the memory allocated for the cache and sets the cache pointer to NULL.
 *
 * @return int Returns 1 if the cache was successfully destroyed, 0 otherwise.
 */
int cache_destroy(void)
{
  int success = 0;
  if (cache != NULL)
  {                 // Check if cache is not NULL
    free(cache);    // Free the memory allocated for cache
    cache = NULL;   // Set cache pointer to NULL
    cache_size = 0; // Set cache size to 0
    success = 1;    // Set success flag to 1
  }
  return success; // Return success flag
}

/**
 * This function looks up a block in the cache and returns it if it exists.
 *
 * @param disk_num The disk number of the block to be looked up.
 * @param block_num The block number of the block to be looked up.
 * @param buf A pointer to the buffer where the block will be stored if found.
 *
 * @return Returns 1 if the block is found in the cache, -1 otherwise.
 */
int cache_lookup(int disk_num, int block_num, uint8_t *buf)
{
  if ((buf == NULL) || !cache_enabled())
    return -1;
  // Increment number of queries
  num_queries++;

  // Loop through cache to find block
  int i = 0;
  do
  {
    if ((cache[i].valid == true) && (block_num == cache[i].block_num) && (disk_num == cache[i].disk_num))
    {
      // Block found, copy to buffer and update cache statistics
      memcpy(buf, cache[i].block, JBOD_BLOCK_SIZE);
      cache[i].clock_accesses++;
      num_hits++;
      return 1;
    }
    i++;
  } while (i < cache_size);

  // Block not found in cache
  return -1;
}

/**
 * Updates the cache with the given disk number, block number, and buffer.
 * If the block is already in the cache, it updates the block with the new buffer.
 * If the block is not in the cache, it adds the block to the cache.
 *
 * @param disk_num The disk number of the block.
 * @param block_num The block number.
 * @param buf The buffer containing the block data.
 */
void cache_update(int disk_num, int block_num, const uint8_t *buf)
{
  int i = 0;
  do
  {
    // Check if the block is already in the cache
    if ((cache[i].valid == true) && (cache[i].block_num == block_num) && (disk_num == cache[i].disk_num))
    {
      // Update the block with the new buffer
      cache[i].clock_accesses = 1;
      memcpy(cache[i].block, buf, JBOD_BLOCK_SIZE);
    }
    i++;
  } while (i < cache_size);
}

/**
 * Inserts a block into the cache.
 *
 * @param disk_num The disk number of the block.
 * @param block_num The block number of the block.
 * @param buf The buffer containing the block data.
 * @return 1 if the block was successfully inserted, -1 otherwise.
 */
int cache_insert(int disk_num, int block_num, const uint8_t *buf)
{

  // Validate parameters.
  if (buf == NULL || cache_size == 0 || cache == NULL)
  {
    return -1;
  }
  if (disk_num < 0 || disk_num > (JBOD_NUM_DISKS - 1) || block_num < 0 || block_num > (JBOD_BLOCK_SIZE - 1))
  {
    return -1;
  }

  // Check if the block is already in the cache.
  int i = 0;
  while (i < cache_size)
  {
    if (cache[i].valid && (cache[i].disk_num == disk_num) &&
        (cache[i].block_num == block_num))
    {
      return -1;
    }
    i++;
  }

  // Find the least frequently used cache entry.
  int lfu_i = 0;
  int j = 0;
  while (j < cache_size)
  {
    if (cache[j].clock_accesses < cache[lfu_i].clock_accesses)
    {
      lfu_i = j;
    }
    j++;
  }

  // Insert the new block into the cache.
  memcpy(cache[lfu_i].block, buf, JBOD_BLOCK_SIZE);
  cache[lfu_i].block_num = block_num;
  cache[lfu_i].disk_num = disk_num;
  cache[lfu_i].valid = true;
  cache[lfu_i].clock_accesses = 1;

  return 1;
}

bool cache_enabled(void)
{
  switch (cache != NULL && cache_size > 0)
  {
    case true:
      return true;
    case false:
      return false;
  }
  return false;
}

void cache_print_hit_rate(void)
{
  fprintf(stderr, "num_hits: %d, num_queries: %d\n", num_hits, num_queries);
  fprintf(stderr, "Hit rate: %5.1f%%\n", 100 * (float)num_hits / num_queries);
}

/**
 * Resizes the cache to the specified number of entries.
 *
 * @param new_num_entries The new number of entries for the cache.
 * @return Returns 1 on success, -1 on failure.
 */
int cache_resize(int new_num_entries)
{
  // Check if the new number of entries is within the valid range
  if (new_num_entries < 2 || new_num_entries > (JBOD_BLOCK_SIZE * JBOD_NUM_DISKS))
  {
    // Invalid number of entries
    return -1;
  }

  // Allocate memory for the new cache with the specified number of entries
  cache_entry_t *new_cache = malloc(new_num_entries * sizeof(cache_entry_t));
  if (new_cache == NULL)
  {
    // Memory allocation failure
    return -1;
  }

  // Initialize the new cache
  int i = 0;
  while (i < new_num_entries)
  {
    new_cache[i].clock_accesses = 0;
    new_cache[i].valid = false;
    i++;
  }

  // Copy valid entries from the old cache to the new cache
  int min_size = cache_size < new_num_entries ? cache_size : new_num_entries;
  i = 0;
  while (i < min_size)
  {
    if (cache[i].valid)
    {
      new_cache[i] = cache[i];
    }
    i++;
  }

  // Free the memory occupied by the old cache
  free(cache);

  // Update the cache pointer and size
  cache = new_cache;
  cache_size = new_num_entries;

  return 1;
}