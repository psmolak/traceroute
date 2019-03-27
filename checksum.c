/*
 * Paweł Smolak
 * 282306
 */

#include <stdint.h>
#include <assert.h>

#include "checksum.h"

/* http://www.ii.uni.wroc.pl/~mbi/dyd/sieci_data/icmp/icmp_checksum.c */
uint16_t checksum(const void *data, int len) {
  uint32_t sum;
  const uint16_t *ptr = data;

  assert(len % 2 == 0);
  for (sum = 0; len > 0; len -= 2)
    sum += *ptr++;

  sum = (sum >> 16) + (sum & 0xffff);
  sum = ~(sum + (sum >> 16));
  return sum;
}
