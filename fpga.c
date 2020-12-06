#include <stdio.h>      /* printf(), fprintf(), perror(), getc() */
#include <sys/socket.h> /* socket(), bind(), sendto(), recvfrom() */
#include <arpa/inet.h>  /* struct sockaddr_in, struct sockaddr, inet_ntoa(), inet_aton() */
#include <stdlib.h>     /* atoi(), exit(), EXIT_FAILURE, EXIT_SUCCESS */
#include <string.h>     /* memset(), strcmp() */
#include <unistd.h>     /* close() */


/*
 * Migration Controller
 * - 192.168.1.2
 * - 10.24.13.11 (DEBUG)
 * Bitstream Server
 * - 192.168.1.2
 * - 10.24.13.11 (DEBUG)
 * FPGA MAC0
 * - 192.168.1.10
 * - 10.24.2.121   (DEBUG)
 */
#ifdef DEBUG
#define MC_IP_ADDR    0x0A180D0B
#define BS_IP_ADDR    0x0A180D0B
#define FPGA_IP_ADDR0 0x0A180279
#else
#define MC_IP_ADDR    0xC0A80102
#define BS_IP_ADDR    0xC0A80102
#define FPGA_IP_ADDR0 0xC0A8010A
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
  RECV_MC_RQST,
  SEND_MC_ACK,
  SEND_BS_RQST,
  RECV_BS_RPLY,
  SEND_BS_ACK,
  WRITE_PB
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
  int sock_mc, sock_bs;
  char buf[2048];
  char *bitstream;
  int bitstream_len;
  struct sockaddr_in addr_mc, addr_bs;
  socklen_t addrlen;
  struct mchdr mc;
  struct bshdr bs_sd, *bs_rv;
  int len;
  FILE *fp;

  sock_mc = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (sock_mc < 0) {
    printf("failed to open socket.\n");
    return 1;
  }

  sock_bs = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (sock_bs < 0) {
    printf("failed to open socket.\n");
    return 1;
  }

  memset(&addr_mc, 0, sizeof(addr_mc));
  addr_mc.sin_family = AF_INET;
  addr_mc.sin_addr.s_addr = htonl(FPGA_IP_ADDR0); /* INADDR_ANY */
  addr_mc.sin_port = htons(MC_PORT_NUM);

  memset(&addr_bs, 0, sizeof(addr_bs));
  addr_bs.sin_family = AF_INET;
  addr_bs.sin_addr.s_addr = htonl(FPGA_IP_ADDR0); /* INADDR_ANY */
  addr_bs.sin_port = htons(BS_PORT_NUM);

  status = bind(sock_mc, (struct sockaddr*)&addr_mc, sizeof(addr_mc));
  if (status == -1) {
    printf("failed to bind socket.\n");
    return 1;
  }

  status = bind(sock_bs, (struct sockaddr*)&addr_bs, sizeof(addr_bs));
  if (status == -1) {
    printf("failed to bind socket.\n");
    return 1;
  }

  bs_rv = (struct bshdr *) buf;
  bitstream = malloc(BITSTREAM_MAX);

  state = RECV_MC_RQST;

  while (1) {
    switch (state) {
      case RECV_MC_RQST:
        addrlen = sizeof(addr_mc);
        len = recvfrom(sock_mc, &mc, sizeof(mc), 0, (struct sockaddr*)&addr_mc, &addrlen);
        if (len == -1) {
          printf("failed to recv mc request.");
          break;
        }
        printf("MC_RQST message received from %s.\n", inet_ntoa(addr_mc.sin_addr));

        if (mc.protocol == MCHDR_PROTOCOL) {
          state = SEND_MC_ACK;
        }
        break;

      case SEND_MC_ACK:
        mc.flag |= MCHDR_ACK;

        addr_mc.sin_addr.s_addr = htonl(MC_IP_ADDR);

        len = sendto(sock_mc, &mc, sizeof(mc), 0, (struct sockaddr*)&addr_mc, sizeof(addr_mc));
        if (len == -1) {
          printf("failed to send mc ack.");
          break;
        }
        printf("MC_ACK message sent to %s.\n", inet_ntoa(addr_mc.sin_addr));

        state = SEND_BS_RQST;
        break;

      case SEND_BS_RQST:
        bs_sd.protocol = BSHDR_PROTOCOL;
        bs_sd.ptype = BSHDR_REQUEST;
        bs_sd.flag = 0;
        bs_sd.offset = 0;
        bs_sd.id = mc.id;

        addr_bs.sin_addr.s_addr = htonl(BS_IP_ADDR);

        len = sendto(sock_bs, &bs_sd, sizeof(bs_sd), 0,
            (struct sockaddr*)&addr_bs, sizeof(addr_bs));
        if (len == -1) {
          printf("failed to send bs rqst.");
          break;
        }
        printf("BS_RQST message sent to %s.\n", inet_ntoa(addr_bs.sin_addr));

        state = RECV_BS_RPLY;
        break;

      case RECV_BS_RPLY:
        printf("load: %d [byte]\n", bitstream_len);
        addrlen = sizeof(addr_bs);
        len = recvfrom(sock_bs, buf, sizeof(buf), 0, (struct sockaddr*)&addr_bs, &addrlen);
        if (len == -1) {
          printf("failed to recv mc request.");
          break;
        }
        printf("BS_RPLY message received from %s.\n", inet_ntoa(addr_bs.sin_addr));

        if (bs_rv->protocol == BSHDR_PROTOCOL) {
          if (bs_rv->flag & BSHDR_ERR) {
            printf("ERR message received.\n");
            state = RECV_MC_RQST;
          } else {
            memcpy(bitstream + bs_rv->offset, buf + BSHDR_LEN, len - BSHDR_LEN);
            bitstream_len = bs_rv->offset + len - BSHDR_LEN;
            state = SEND_BS_ACK;
          }
        }
        break;

      case SEND_BS_ACK:
        memcpy(&bs_sd, bs_rv, sizeof(bs_sd));
        bs_sd.flag |= BSHDR_ACK;

        addr_bs.sin_addr.s_addr = htonl(BS_IP_ADDR);

        len = sendto(sock_bs, &bs_sd, sizeof(bs_sd), 0,
            (struct sockaddr*)&addr_bs, sizeof(addr_bs));
        if (len == -1) {
          printf("failed to send bs rqst.");
          break;
        }
        printf("BS_ACK message sent to %s.\n", inet_ntoa(addr_bs.sin_addr));

        if (bs_rv->flag & BSHDR_FIN) {
          state = WRITE_PB;
        } else {
          state = RECV_BS_RPLY;
        }
        break;

      case WRITE_PB:
        state = RECV_MC_RQST;

        fp = fopen("pb.bin", "w");
        if (fp == NULL) {
          printf("failed to open file.\n");
          break;
        }

        len = fwrite(bitstream, sizeof(char), bitstream_len, fp);
        if (len != bitstream_len) {
          printf("failed to write bitstream.\n");
        }

        fclose(fp);

        printf("finished to write bitstream!\n");
        break;
    }
  }

  free(bitstream);

  return 0;
}
