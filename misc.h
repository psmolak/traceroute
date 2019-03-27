#ifndef MISC_H
#define MISC_H

#include <sys/time.h>
#include <stdarg.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>

#define true 1
#define false 0

#define IPHDR(ptr)    ((struct iphdr *)(ptr))
#define ICMPHDR(ptr)  ((struct icmphdr *)((char *)ptr + 4 * IPHDR(ptr)->ihl))
#define INNER(ptr)    ((char *)((char *)(ICMPHDR(ptr)) + sizeof(struct icmphdr)))

#define ID(ptr)       (ICMPHDR(ptr)->un.echo.id)
#define SEQUENCE(ptr) (ICMPHDR(ptr)->un.echo.sequence)
#define TYPE(ptr)     (ICMPHDR(ptr)->type)

typedef uint8_t packet_t[IP_MAXPACKET];

typedef struct {
  int sock;
  struct sockaddr_in addr;
} conn_t;

typedef struct {
  char ip[INET_ADDRSTRLEN];
  struct timeval rtt;
  uint16_t type;
} reply_t;

void die(const char *fmt, ...);
conn_t makecon(const char *ip);
uint16_t checksum(const void *data, int len);
struct timeval timeval_diff(struct timeval a, struct timeval b);

#endif
