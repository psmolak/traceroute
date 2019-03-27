/*
 * Paweł Smolak
 * 282306
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <sys/time.h>
#include <arpa/inet.h>

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

conn_t makecon(const char *ip) {
  conn_t con;
  memset(&con.addr, 0, sizeof(con.addr));
  con.addr.sin_family = AF_INET;

  if (inet_pton(AF_INET, ip, &con.addr.sin_addr) == 0)
    die("Provided IP address '%s' is invalid\n", ip);

  if ((con.sock = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP)) < 0)
    die("socket() failed with '%s'\n", strerror(errno));

  return con;
}
