/*
 * Paweł Smolak
 * 282306
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>
#include <errno.h>
#include <sys/time.h>

#include "misc.h"

void die(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  vfprintf(stderr, fmt, args);
  va_end(args);
  exit(EXIT_FAILURE);
}

struct timeval timeval_diff(struct timeval a, struct timeval b) {
  struct timeval diff = {
    .tv_sec = a.tv_sec - b.tv_sec,
    .tv_usec = a.tv_usec - b.tv_usec,
  };

  if (diff.tv_usec < 0) {
    diff.tv_sec--;
    diff.tv_usec += 1000000;
  }

  return diff;
}

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
