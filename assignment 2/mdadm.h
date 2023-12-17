#ifndef MDADM_H_
#define MDADM_H_

#include <stdint.h>
#include "jbod.h"

/* Return 1 on success and -1 on failure */
int mdadm_mount(void);

/* Return 1 on success and -1 on failure */
int mdadm_unmount(void);

/* Return the number of bytes read on success, -1 on failure. */
int mdadm_read(uint32_t start_addr, uint32_t read_len, uint8_t *read_buf);

#endif
