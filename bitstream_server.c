#include <stdio.h>      /* printf(), fprintf(), perror(), getc() */
#include <sys/socket.h> /* socket(), bind(), sendto(), recvfrom() */
#include <arpa/inet.h>  /* struct sockaddr_in, struct sockaddr, inet_ntoa(), inet_aton() */
#include <stdlib.h>     /* atoi(), exit(), EXIT_FAILURE, EXIT_SUCCESS */
#include <string.h>     /* memset(), strcmp() */
#include <unistd.h>     /* close() */


/*
 * Migration Controller
 * - 192.168.1.2
 * - 10.24.2.121 (DEBUG)
 * Bitstream Server
 * - 192.168.1.2
 * - 10.24.2.121 (DEBUG)
 * FPGA0 MAC0
 * - 192.168.1.10
 * - 10.24.13.14 (DEBUG)
 * FPGA1 MAC0
 * - 192.168.1.10
 * - 10.24.13.13 (DEBUG)
 */
#ifdef DEBUG
#define MC_IP_ADDR     0x0A180279
#define BS_IP_ADDR     0x0A180279
#define FPGA0_IP_ADDR0 0x0A180D0E
#define FPGA1_IP_ADDR0 0x0A180D0D
#else
#define MC_IP_ADDR     0xC0A80102
#define BS_IP_ADDR     0xC0A80102
#define FPGA0_IP_ADDR0 0xC0A8010A
#define FPGA1_IP_ADDR0 0xC0A8010B
#endif

#define BS_PORT_NUM    5000
#define MC_PORT_NUM    5001

#define BSHDR_LEN      12
#define BSHDR_PROTOCOL 0x4253
#define BSHDR_REQUEST  0x01
#define BSHDR_REPLY    0x02
#define BSHDR_FIN      0x01
#define BSHDR_SYN      0x02
#define BSHDR_ACK      0x04
#define BSHDR_ERR      0x08

#define MCHDR_LEN      12
#define MCHDR_PROTOCOL 0x4D43
#define MCHDR_REQUEST  0x01
#define MCHDR_REPLY    0x02
#define MCHDR_FIN      0x01
#define MCHDR_SYN      0x02
#define MCHDR_ACK      0x04
#define MCHDR_NONE     0x00
#define MCHDR_PACKET   0x01
#define MCHDR_STREAM   0x02

#define BITSTREAM_MAX 0x1E0000
#define SEND_SIZE     1024


enum STATE {
  RECV_RQST,
  SEND_ERR_RPLY,
  SEND_RPLY,
  RESEND_RPLY,
  RECV_ACK
};


/* Bitstream Server (BS) Header */
struct bshdr {
  u_int16_t protocol;   /* Protocol (BS (0x4253)) */
  u_int8_t ptype;       /* Protocol Type (1:Request, 2:Reply) */
  u_int8_t flag;        /* Flag (0bit:FIN, 1bit:SYN, 2bit:ACK) */
  u_int32_t offset;
  u_int32_t id;
};


/* Migration Controller (MC) Header */
struct mchdr {
  u_int16_t protocol;   /* CTRL (0x4D43) */
  u_int8_t ptype;       /* Protocol Type (1:Request, 2:Reply) */
  u_int8_t flag;        /* Flag (0bit:FIN, 1bit:SYN, 2bit:ACK) */
  u_int16_t pnumber;    /* Partition Number */
  u_int8_t btype;       /* Buffering Type (0:None, 1:Packet, 2:Stream) */
  u_int8_t bport;       /* Buffering Port */
  u_int32_t id;
};


int main () {
  int status;
  enum STATE state;
  char buf[2048];
  int sock;
  struct sockaddr_in addr;
  socklen_t addrlen;
  fd_set fds, readfds;
  struct timeval tv;
  int n;
  struct bshdr bs_sd, *bs_rv;
  int len;
  FILE *fp;
  char file_name[32];
  char pb[SEND_SIZE];

  sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (sock < 0) {
    printf("failed to open socket.\n");
    return 1;
  }

  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(BS_IP_ADDR); /* INADDR_ANY */
  addr.sin_port = htons(BS_PORT_NUM);

  status = bind(sock, (struct sockaddr*)&addr, sizeof(addr));
  if (status == -1) {
    printf("failed to bind socket.\n");
    return 1;
  }

  FD_ZERO(&readfds);
  FD_SET(sock, &readfds);

  tv.tv_sec = 1;
  tv.tv_usec = 0;

  bs_rv = (struct bshdr *) buf;
  bs_sd.protocol = BSHDR_PROTOCOL;
  bs_sd.ptype = BSHDR_REPLY;
  bs_sd.flag = 0;
  bs_sd.offset = 0;
  bs_sd.id = 0;

  state = RECV_RQST;

  while (1) {
    switch (state) {
      case RECV_RQST:
        addrlen = sizeof(addr);
        len = recvfrom(sock, buf, sizeof(buf), 0, (struct sockaddr*)&addr, &addrlen);
        if (len == -1) {
          printf("failed to recv RQST.");
          break;
        }
        printf("RQST message received from %s.\n", inet_ntoa(addr.sin_addr));

        if (bs_rv->protocol == BSHDR_PROTOCOL && bs_rv->ptype == BSHDR_REQUEST) {
          bs_sd.flag = BSHDR_SYN;
          bs_sd.offset = 0;
          bs_sd.id = bs_rv->id;

          sprintf(file_name, "bit/%08x.bit", bs_rv->id);
          fp = fopen(file_name, "r");
          if (fp == NULL) {
            printf("failed to open bit/%08x.bit.\n", bs_rv->id);
            state = SEND_ERR_RPLY;
            break;
          }
          state = SEND_RPLY;
        }
        break;

      case SEND_ERR_RPLY:
        bs_sd.flag = BSHDR_ERR;

        len = sendto(sock, &bs_sd, sizeof(bs_sd), 0, (struct sockaddr*)&addr, sizeof(addr));
        if (len == -1) {
          printf("failed to send ERR_RPLY.");
          break;
        }
        printf("ERR_RPLY message sent to %s.\n", inet_ntoa(addr.sin_addr));
        state = RECV_RQST;
        break;

      case SEND_RPLY:
        len = fread(pb, sizeof(char), SEND_SIZE, fp);
        if (len < SEND_SIZE) {
          bs_sd.flag = BSHDR_FIN;
        }

        memcpy(buf, &bs_sd, sizeof(bs_sd));
        memcpy(buf + sizeof(bs_sd), pb, len);

        len = sendto(sock, buf, len + sizeof(bs_sd), 0, (struct sockaddr*)&addr, sizeof(addr));
        if (len == -1) {
          printf("failed to send RPLY.");
          break;
        }
        printf("RPLY message sent to %s. %d [byte]\n", inet_ntoa(addr.sin_addr), bs_sd.offset);

        state = RECV_ACK;
        break;

      case RESEND_RPLY:
        memcpy(buf, &bs_sd, sizeof(bs_sd));
        memcpy(buf + sizeof(bs_sd), pb, len);

        len = sendto(sock, buf, len, 0, (struct sockaddr*)&addr, sizeof(addr));
        if (len == -1) {
          printf("failed to send RPLY.");
          break;
        }
        printf("RPLY message sent to %s.\n", inet_ntoa(addr.sin_addr));

        state = RECV_ACK;
        break;

      case RECV_ACK:
        memcpy(&fds, &readfds, sizeof(fd_set));
        tv.tv_sec = 1;
        tv.tv_usec = 0;
        n = select(sock + 1, &fds, NULL, NULL, &tv);
        if (n == -1) {
          printf("failed to select.");
          break;
        } else if (n == 0) {
          printf("timeout.\n");
          state = RESEND_RPLY;
          break;
        }

        addrlen = sizeof(addr);
        len = recvfrom(sock, buf, sizeof(buf), 0, (struct sockaddr*)&addr, &addrlen);
        if (len == -1) {
          printf("failed to recv ACK.");
          break;
        }
        printf("ACK message received from %s.\n", inet_ntoa(addr.sin_addr));

        if (bs_rv->protocol == BSHDR_PROTOCOL && bs_rv->flag & BSHDR_ACK
            && bs_rv->offset == bs_sd.offset) {
          if (bs_rv->flag & BSHDR_FIN) {
            printf("finished to send bitstream!\n");
            state = RECV_RQST;
          } else {
            bs_sd.offset += SEND_SIZE;
            state = SEND_RPLY;
          }
        }
        break;
    }
  }

  return 0;
}
