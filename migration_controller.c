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

#define BSHDR_LEN      12
#define BSHDR_PROTOCOL 0x4253
#define BSHDR_REQUEST  0x01
#define BSHDR_REPLY    0x02
#define BSHDR_FIN      0x01
#define BSHDR_SYN      0x02
#define BSHDR_ACK      0x04

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
  INPUT_MODE,
  INPUT_START,
  INPUT_MIGRATION,
  START_APP,
  MIGRATE_APP
};

enum MCPKT_STATE {
  SEND_RQST,
  RECV_ACK,
  RECV_FIN
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


int send_mcpkt (int sock, struct sockaddr_in addr, int fpga_id, int bs_id);


int main () {
  int status;
  enum STATE state;
  int sock;
  struct sockaddr_in addr;
  int n;
  int fpga_id, src_fpga_id, dest_fpga_id;
  int bs_id, src_bs_id, dest_bs_id;
  int p_id, src_p_id, dest_p_id, app_id;
  int run_app[2][4] = {{1, 1, 1, 1}, {1, 1, 1, 1}};

  sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (sock < 0) {
    printf("failed to open socket.\n");
    return 1;
  }

  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(MC_IP_ADDR); /* INADDR_ANY */
  addr.sin_port = htons(MC_PORT_NUM);

  status = bind(sock, (struct sockaddr*)&addr, sizeof(addr));
  if (status == -1) {
    printf("failed to bind socket.\n");
    return 1;
  }

  state = INPUT_MODE;

  while (1) {
    switch (state) {
      case INPUT_MODE:
        printf("0. Start a new application\n");
        printf("1. Migrate a application\n");
        printf("input number: ");
        scanf("%x", &n);
        switch (n) {
          case 0:
            state = INPUT_START;
            break;
          case 1:
            state = INPUT_MIGRATION;
            break;
        }
        break;

      case INPUT_START:
        printf("Please input a FPGA ID.\nID: ");
        scanf("%x", &fpga_id);
        printf("Please input a partition ID.\nID: ");
        scanf("%x", &p_id);
        printf("Please input an application ID.\nID: ");
        scanf("%x", &app_id);
        bs_id = app_id << 4 | p_id;
        run_app[fpga_id][p_id] = app_id;
        printf("Bitstream ID is %08x.\n", bs_id);
        state = START_APP;
        break;

      case INPUT_MIGRATION:
        printf("Please input a source FPGA ID.\nID: ");
        scanf("%x", &src_fpga_id);
        printf("Please input a source partition ID.\nID: ");
        scanf("%x", &src_p_id);
        printf("Please input a destination FPGA ID.\nID: ");
        scanf("%x", &dest_fpga_id);
        printf("Please input a destination partition ID.\nID: ");
        scanf("%x", &dest_p_id);
        src_bs_id = 0x10 | src_p_id;
        dest_bs_id = run_app[src_fpga_id][src_p_id] << 4 | dest_p_id;
        run_app[dest_fpga_id][dest_p_id] = run_app[src_fpga_id][src_p_id];
        run_app[src_fpga_id][src_p_id] = 1;
        printf("Source bitstream ID is %08x.\n", src_bs_id);
        printf("Destination bitstream ID is %08x.\n", dest_bs_id);
        state = MIGRATE_APP;
        break;

      case START_APP:
        state = INPUT_MODE;
        send_mcpkt(sock, addr, fpga_id, bs_id);
        break;

      case MIGRATE_APP:
        state = INPUT_MODE;
        send_mcpkt(sock, addr, src_fpga_id, src_bs_id);
        send_mcpkt(sock, addr, dest_fpga_id, dest_bs_id);
        break;
    }
  }

  return 0;
}


int send_mcpkt (int sock, struct sockaddr_in addr, int fpga_id, int bs_id) {
  int state = SEND_RQST;
  static char buf[2048];
  int len;
  int n;
  fd_set fds, readfds;
  struct mchdr mc_sd, *mc_rv;
  struct timeval tv;
  socklen_t addrlen;

  FD_ZERO(&readfds);
  FD_SET(sock, &readfds);

  tv.tv_sec = 1;
  tv.tv_usec = 0;

  mc_sd.protocol = MCHDR_PROTOCOL;
  mc_sd.ptype = MCHDR_REQUEST;
  mc_sd.flag = MCHDR_SYN;
  mc_sd.id = bs_id;
  mc_sd.pnumber = mc_sd.id & 0xF;
  mc_sd.btype = MCHDR_NONE;
  mc_sd.bport = 0;

  mc_rv = (struct mchdr*) buf;

  while (1) {
    switch (state) {
      case SEND_RQST:
        if (fpga_id == 0) {
          addr.sin_addr.s_addr = htonl(FPGA0_IP_ADDR0);
        } else if (fpga_id == 1) {
          addr.sin_addr.s_addr = htonl(FPGA1_IP_ADDR0);
        } else {
          printf("cannot find the target fpga\n");
          return -1;
        }

        len = sendto(sock, &mc_sd, sizeof(mc_sd), 0, (struct sockaddr*)&addr, sizeof(addr));
        if (len == -1) {
          printf("failed to send RQST.\n");
          break;
        }
        printf("RQST message sent to %s.\n", inet_ntoa(addr.sin_addr));

        state = RECV_ACK;
        break;

      case RECV_ACK:
        memcpy(&fds, &readfds, sizeof(fd_set));
        tv.tv_sec = 1;
        tv.tv_usec = 0;
        n = select(sock + 1, &fds, NULL, NULL, &tv);
        if (n == -1) {
          printf("failed to select.\n");
          break;
        } else if (n == 0) {
          printf("timeout.\n");
          state = SEND_RQST;
          break;
        }

        addrlen = sizeof(addr);
        len = recvfrom(sock, buf, sizeof(buf), 0, (struct sockaddr*)&addr, &addrlen);
        if (len == -1) {
          printf("failed to recv ACK.\n");
          break;
        }
        printf("ACK message received from %s.\n", inet_ntoa(addr.sin_addr));

        if (mc_rv->protocol == MCHDR_PROTOCOL && mc_rv->flag & MCHDR_ACK) {
          state = RECV_FIN;
        }
        break;

      case RECV_FIN:
        addrlen = sizeof(addr);
        len = recvfrom(sock, buf, sizeof(buf), 0, (struct sockaddr*)&addr, &addrlen);
        if (len == -1) {
          printf("failed to recv FIN.\n");
          break;
        }

        if (mc_rv->protocol == MCHDR_PROTOCOL && mc_rv->flag & MCHDR_FIN) {
          printf("FIN message received from %s.\n", inet_ntoa(addr.sin_addr));
          return 0;
        }
        break;
    }
  }
}
