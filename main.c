#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/select.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>

#include "checksum.h"

#define true 1
#define false 0

#define MAX_TTL 64
#define TIMEOUT 1

#define IPHDR(ptr)      ((struct iphdr *)(ptr))
#define ICMPHDR(ptr)    ((struct icmphdr *)((char *)ptr + 4 * IPHDR(ptr)->ihl))
#define INNER(ptr)      ((char *) ((char *)(ICMPHDR(ptr)) + sizeof(struct icmphdr)))
#define ID(ptr)         (ICMPHDR(ptr)->un.echo.id)
#define SEQUENCE(ptr)   (ICMPHDR(ptr)->un.echo.sequence)

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

static const char *usage = "Usage: ./traceroute x.x.x.x\n";
static pid_t pid;

static void die(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  vfprintf(stderr, fmt, args);
  va_end(args);
  exit(EXIT_FAILURE);
}

static void send_icmp_echo(conn_t *con, int ttl) {
  struct icmphdr hdr;
  struct sockaddr *addr = (struct sockaddr*)&con->addr;

  memset(&hdr, 0, sizeof(hdr));
  hdr.type = ICMP_ECHO;
  hdr.code = 0;
  hdr.un.echo.id = pid;
  hdr.un.echo.sequence = ttl;
  hdr.checksum = checksum(&hdr, sizeof(hdr));

  if (setsockopt(con->sock, IPPROTO_IP, IP_TTL, &ttl, sizeof(ttl)) < 0)
    die("setsockopt() failed with '%s'\n", strerror(errno));

  if (sendto(con->sock, &hdr, sizeof(hdr), 0, addr, sizeof(con->addr)) < 0)
    die("sendto() failed with '%s'\n", strerror(errno));
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

static int await_packet(conn_t *con, struct timeval *timeout) {
  int ready;
  fd_set fds;

  if (timeout->tv_sec <= 0 && timeout->tv_usec <= 0)
    return false;

  FD_ZERO(&fds);
  FD_SET(con->sock, &fds);

  if ((ready = select(con->sock + 1, &fds, NULL, NULL, timeout)) < 0)
    die("select() failed with '%s'\n", strerror(errno));

  return ready;
}

static int valid_icmp_packet(void *packet, int ttl) {
  switch (ICMPHDR(packet)->type) {
  case ICMP_ECHOREPLY:
    return ID(packet) == pid && SEQUENCE(packet) == ttl;

  case ICMP_TIME_EXCEEDED:
    return ID(INNER(packet)) == pid && SEQUENCE(INNER(packet)) == ttl;

  default:
    return false;
  }
}

reply_t packet2reply(void *packet, struct timeval start, struct timeval timestamp) {
  reply_t reply;

  if (inet_ntop(AF_INET, &(IPHDR(packet)->saddr), reply.ip, INET_ADDRSTRLEN) == NULL)
    die("inet_ntop() failed with '%s'\n", strerror(errno));
  reply.rtt = timeval_diff(timestamp, start);
  reply.type = ICMPHDR(packet)->type;

  return reply;
}

static int get_icmp_replies(conn_t *con, int ttl, reply_t replies[]) {
  int n = 0;
  packet_t packet;
  struct timeval start, timestamp, timeout;

  gettimeofday(&start, NULL);
  timeout.tv_sec = TIMEOUT;
  timeout.tv_usec = 0;

  while (n < 3 && await_packet(con, &timeout)) {
    gettimeofday(&timestamp, NULL);

    if (recvfrom(con->sock, packet, IP_MAXPACKET, 0, NULL, NULL) < 0)
      die("recvfrom() failed with '%s'\n", strerror(errno));

    if (valid_icmp_packet(packet, ttl))
      replies[n++] = packet2reply(packet, start, timestamp);
  }

  return n;
}

static void print_raport(reply_t replies[], int n, int ttl) {
  long long avg = 0;

  printf("%d ", ttl);

  if (!n)
    printf("*\n");

  for (int i = 0; i < n; i++) {
    printf("%s ", replies[i].ip);
    avg += replies[i].rtt.tv_usec;
  }

  if (n)
    printf("%lldms\n", avg/n/1000);
}

static void traceroute(conn_t *con) {
  int ttl, n;
  reply_t replies[3];

  for (ttl = 1; ttl <= MAX_TTL; ttl++) {
    send_icmp_echo(con, ttl);
    send_icmp_echo(con, ttl);
    send_icmp_echo(con, ttl);

    n = get_icmp_replies(con, ttl, replies);
    print_raport(replies, n, ttl);

    if (n > 0 && replies[0].type == ICMP_ECHOREPLY)
      break;
  }
}

conn_t makecon(const char* ip) {
  conn_t con;
  memset(&con.addr, 0, sizeof(con.addr));
  con.addr.sin_family = AF_INET;

  if (inet_pton(AF_INET, ip, &con.addr.sin_addr) == 0)
    die("Provided IP address '%s' is invalid\n", ip);

  if ((con.sock = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP)) < 0)
    die("socket() failed with '%s'\n", strerror(errno));

  return con;
}

int main(int argc, char *argv[]) {
  conn_t connection;

  if (argc != 2)
    die("Wrong number of arguments\n%s", usage);

  if ((pid = getpid()) < 0)
    die("getpid() failed with '%s'\n", strerror(errno));

  connection = makecon(argv[1]);
  traceroute(&connection);

  return EXIT_SUCCESS;
}
