/*
 * Paweł Smolak
 * 282306
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/select.h>
#include <sys/time.h>
#include <arpa/inet.h>

#include "utils.h"

#define TTLMAX 32
#define ECHOMAX 3
#define TIMEOUT 1

static void raport(reply_t replies[], int n, int ttl);
static void send_icmp_echo(const conn_t *con, int ttl);
static int valid_icmp_packet(const void *packet, int ttl);
static int await_icmp_packet(const conn_t *con, struct timeval *timeout);
static int collect_icmp_replies(const conn_t *con, reply_t replies[], int ttl);
static int traceroute(const conn_t *con);
static conn_t makecon(const char *ip);
static int duplicate(reply_t replies[], int n, int i);
static reply_t convert_packet_to_reply(const void *packet, struct timeval start,
                                       struct timeval timestamp);

static pid_t pid;
static const char *usage = "Usage: ./traceroute x.x.x.x\n";

int duplicate(reply_t replies[], int n, int i) {
  for (int j = i + 1; j < n; j++) {
    if (strcmp(replies[j].ip, replies[i].ip) == 0)
      return true;
  }
  return false;
}

void raport(reply_t replies[], int n, int ttl) {
  long long avg = 0;

  printf("%d ", ttl);
  for (int i = 0; i < n; i++) {
    avg += replies[i].rtt.tv_usec;
    if (!duplicate(replies, n, i))
      printf("%s ", replies[i].ip);
  }

  if (n == ECHOMAX)
    printf("%lldms\n", avg / n / 1000);
  else if (n > 0)
    printf("???\n");
  else /* n <= 0 */
    printf("*\n");
}

void send_icmp_echo(const conn_t *con, int ttl) {
  struct icmphdr hdr;
  struct sockaddr *addr = (struct sockaddr *)&con->addr;

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

int valid_icmp_packet(const void *packet, int ttl) {
  if (TYPE(packet) == ICMP_ECHOREPLY)
    return ID(packet) == pid && SEQUENCE(packet) == ttl;

  if (TYPE(packet) == ICMP_TIME_EXCEEDED)
    return ID(INNER(packet)) == pid && SEQUENCE(INNER(packet)) == ttl;

  return false;
}

/*
 * Waits at most 'timeout' sec for any packet on con->sock to arrive.
 * Returns 0 on timeout, 1 when something arrived on socket.
 */
int await_icmp_packet(const conn_t *con, struct timeval *timeout) {
  int ready;
  fd_set fds;

  if (timeout->tv_sec <= 0 && timeout->tv_usec <= 0)
    return 0;

  FD_ZERO(&fds);
  FD_SET(con->sock, &fds);
  if ((ready = select(con->sock + 1, &fds, NULL, NULL, timeout)) < 0)
    die("select() failed with '%s'\n", strerror(errno));

  return ready > 0;
}

reply_t convert_packet_to_reply(const void *packet, struct timeval start,
                               struct timeval timestamp) {
  reply_t reply;

  if (inet_ntop(AF_INET, &(IPHDR(packet)->saddr), reply.ip, INET_ADDRSTRLEN) == NULL)
    die("inet_ntop() failed with '%s'\n", strerror(errno));
  reply.rtt = timeval_diff(timestamp, start);
  reply.type = TYPE(packet);

  return reply;
}

int collect_icmp_replies(const conn_t *con, reply_t replies[], int ttl) {
  int n = 0;
  packet_t packet;
  struct timeval start, timestamp, timeout;

  gettimeofday(&start, NULL);
  timeout.tv_sec = TIMEOUT;
  timeout.tv_usec = 0;

  /*
   * On Linux, select() modifies 'timeout' to reflect the amount of
   * time not slept. Therefore there's no need to calculate diffs manually.
   */
  while (n < ECHOMAX && await_icmp_packet(con, &timeout)) {
    gettimeofday(&timestamp, NULL);

    if (recvfrom(con->sock, packet, IP_MAXPACKET, 0, NULL, NULL) < 0)
      die("recvfrom() failed with '%s'\n", strerror(errno));

    if (valid_icmp_packet(packet, ttl))
      replies[n++] = convert_packet_to_reply(packet, start, timestamp);
  }

  return n;
}

int traceroute(const conn_t *con) {
  int ttl, n;
  reply_t replies[ECHOMAX];

  for (ttl = 1; ttl <= TTLMAX; ttl++) {
    for (int i = 0; i < ECHOMAX; i++)
      send_icmp_echo(con, ttl);

    n = collect_icmp_replies(con, replies, ttl);
    raport(replies, n, ttl);

    /* destination host reached */
    if (n > 0 && replies[0].type == ICMP_ECHOREPLY)
      break;
  }

  return ttl < TTLMAX ? EXIT_SUCCESS : EXIT_FAILURE;
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

int main(int argc, char *argv[]) {
  conn_t con;

  if (argc != 2)
    die("Wrong number of arguments\n%s", usage);

  if ((pid = getpid()) < 0)
    die("getpid() failed with '%s'\n", strerror(errno));

  con = makecon(argv[1]);
  exit(traceroute(&con));
}
