/*
 * Paweł Smolak
 * 282306
 */

#include <stdio.h>
#include <stdlib.h>
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
#include "config.h"
#include "misc.h"

static pid_t pid;
static const char *usage = "Usage: ./traceroute x.x.x.x\n";

static void send_icmp_echo(conn_t *con, int ttl) {
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

static int valid_icmp_packet(void *packet, int ttl) {
  if (TYPE(packet) == ICMP_ECHOREPLY)
    return ID(packet) == pid && SEQUENCE(packet) == ttl;

  if (TYPE(packet) == ICMP_TIME_EXCEEDED)
    return ID(INNER(packet)) == pid && SEQUENCE(INNER(packet)) == ttl;

  return false;
}

static int await_icmp_packet(conn_t *con, struct timeval *timeout) {
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

static reply_t packet_to_reply(void *packet, struct timeval start,
                               struct timeval timestamp) {
  reply_t reply;

  if (inet_ntop(AF_INET, &(IPHDR(packet)->saddr), reply.ip, INET_ADDRSTRLEN) == NULL)
    die("inet_ntop() failed with '%s'\n", strerror(errno));
  reply.rtt = timeval_diff(timestamp, start);
  reply.type = TYPE(packet);

  return reply;
}

static int get_icmp_replies(conn_t *con, int ttl, reply_t replies[]) {
  int n = 0;
  packet_t packet;
  struct timeval start, timestamp, timeout;

  gettimeofday(&start, NULL);
  timeout.tv_sec = TIMEOUT;
  timeout.tv_usec = 0;

  while (n < 3 && await_icmp_packet(con, &timeout)) {
    gettimeofday(&timestamp, NULL);

    if (recvfrom(con->sock, packet, IP_MAXPACKET, 0, NULL, NULL) < 0)
      die("recvfrom() failed with '%s'\n", strerror(errno));

    if (valid_icmp_packet(packet, ttl))
      replies[n++] = packet_to_reply(packet, start, timestamp);
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
    printf("%lldms\n", avg / n / 1000);
}

static int traceroute(conn_t *con) {
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

  return ttl < MAX_TTL ? EXIT_SUCCESS : EXIT_FAILURE;
}

int main(int argc, char *argv[]) {
  conn_t connection;

  if (argc != 2)
    die("Wrong number of arguments\n%s", usage);

  if ((pid = getpid()) < 0)
    die("getpid() failed with '%s'\n", strerror(errno));

  connection = makecon(argv[1]);
  exit(traceroute(&connection));
}
