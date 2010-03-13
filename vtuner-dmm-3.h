#ifndef _VTUNER_DMM_3_H_
#define _VTUNER_DMM_3_H_

#include "vtuner-network.h"

#define DEBUGHW(msg, ...)  write_message(0x0020, "[%d %s:%u] debug: " msg, getpid(), __FILE__, __LINE__, ## __VA_ARGS__)
#define DEBUGHWI(msg, ...) init_message("[%d %s:%u] debug: " msg, getpid(), __FILE__, __LINE__, ## __VA_ARGS__)
#define DEBUGHWC(msg, ...) append_message(msg, ## __VA_ARGS__)
#define DEBUGHWF(msg, ...) write_message(0x0020, msg, ## __VA_ARGS__)

typedef struct vtuner_hw {

  vtuner_type_t type;
  struct dvb_frontend_info fe_info;

  int frontend_fd;
  int demux_fd;
  __u16 pids[30];
  int streaming_fd;

  int adapter;
  int frontend;
  int demux;
} vtuner_hw_t;

int hw_init(vtuner_hw_t*, int, int, int, int);
int hw_get_frontend(vtuner_hw_t*, struct dvb_frontend_parameters*);
int hw_set_frontend(vtuner_hw_t*, struct dvb_frontend_parameters*);
int hw_read_status(vtuner_hw_t*, __u32*);
int hw_set_tone(vtuner_hw_t*, __u8);
int hw_set_voltage(vtuner_hw_t*, __u8);
int hw_send_diseq_msg(vtuner_hw_t*, diseqc_master_cmd_t*);
int hw_send_diseq_burst(vtuner_hw_t*, __u8);
int hw_pidlist(vtuner_hw_t*, __u16*);

#endif
