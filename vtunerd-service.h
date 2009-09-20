#ifndef _VTUNERDSERVICE_H_
#define _VTUNEDRSERVICE_H_

#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "vtuner-network.h"

#if HAVE_DVB_API_VERSION < 3
  #include "vtuner-dmm-2.h"
#else
  #ifdef HAVE_DREAMBOX_HARDWARE
    #include "vtuner-dmm-3.h"
  #else
    #include "vtuner-dvb-3.h"
  #endif
#endif

#define MAX_SESSIONS 8

typedef enum vtuner_session_status {
  SST_UNKNOWN,
  SST_IDLE,
  SST_BUSY
} vtuner_session_status_t;

typedef struct vtuner_session {
  vtuner_hw_t hw;
  __u16 port;
  __u16 data_port;
  char tuner_group[80];
  struct sockaddr_in ctrl_so;
  vtuner_session_status_t status;
} vtuner_session_t;

#define DEBUGSRV(msg, ...)  write_message(0x0040, "[%d %s:%u] debug: " msg, getpid(), __FILE__, __LINE__, ## __VA_ARGS__)
#define DEBUGSRVC(msg, ...) write_message(0x0040, msg, ## __VA_ARGS__)

void *discover_worker(void*);
void start_sessions(int, vtuner_session_t*);

#endif
