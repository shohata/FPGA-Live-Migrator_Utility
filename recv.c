#include <stdio.h>      /* printf(), fprintf(), perror(), getc() */
#include <sys/socket.h> /* socket(), bind(), sendto(), recvfrom() */
#include <arpa/inet.h>  /* struct sockaddr_in, struct sockaddr, inet_ntoa(), inet_aton() */
#include <stdlib.h>     /* atoi(), exit(), EXIT_FAILURE, EXIT_SUCCESS */
#include <string.h>     /* memset(), strcmp() */
#include <unistd.h>     /* close() */
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>


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
 * - 192.168.1.11
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
#define FPGA_RP0_PORT_NUM 4000
#define FPGA_RP1_PORT_NUM 4001
#define FPGA_RP2_PORT_NUM 4002
#define FPGA_RP3_PORT_NUM 4003

#define BSHDR_LEN      12
#define BSHDR_PROTOCOL 0x4253
#define BSHDR_REQUEST  0x01
#define BSHDR_REPLY    0x02
#define BSHDR_FIN      0x01
#define BSHDR_SYN      0x02
#define BSHDR_ACK      0x04
#define BSHDR_ERR      0x08

#define MCHDR_LEN      8
#define MCHDR_PROTOCOL 0x4D43
#define MCHDR_REQUEST  0x01
#define MCHDR_REPLY    0x02
#define MCHDR_FIN      0x01
#define MCHDR_SYN      0x02
#define MCHDR_ACK      0x04

#define BITSTREAM_MAX 0x800000
#define SEND_SIZE     1024

#define SERIAL_PORT "/dev/ttyUSB1"


enum STATE {
  RECV_MC_RQST,
  SEND_MC_ACK,
  SEND_BS_RQST,
  RECV_BS_RPLY,
  SEND_BS_ACK,
  SEND_MC_FIN,
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
  int sock;
  char buf[2048];
  int bitstream_len;
  struct sockaddr_in addr;
  socklen_t addrlen;

  sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (sock < 0) {
    printf("failed to open socket.\n");
    return 1;
  }

  memset(&addr, 0, sizeof(addr));
  addr_mc.sin_family = AF_INET;
  addr_mc.sin_addr.s_addr = htonl(WS_IP_ADDR0); /* INADDR_ANY */
  addr_mc.sin_port = htons(FPGA_RP0_PORT_NUM);

  status = bind(sock, (struct sockaddr*)&addr, sizeof(addr));
  if (status == -1) {
    printf("failed to bind socket.\n");
    return 1;
  }

  while (1) {
    addrlen = sizeof(addr);
    len = recvfrom(sock, buf, sizeof(buf), 0, (struct sockaddr*)&addr, &addrlen);
    if (len == -1) {
      printf("failed to recv mc request.");
      break;
    }
  }

  return 0;
}

