#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <poll.h>
#include <netdb.h>
#include <pthread.h>

#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

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

#ifdef DEBUG_MAIN
#define DEBUGMAIN(msg, ...) fprintf(stderr,"[%d %s:%u] debug: " msg, getpid(), __FILE__, __LINE__, ## __VA_ARGS__)
#define DEBUGMAINC(msg, ...) fprintf(stderr,msg, ## __VA_ARGS__)
#else
#define DEBUGMAIN(msg, ...)
#define DEBUGMAINC(msg, ...)
#endif

int dbg_level = 2;

typedef enum vtuner_session_status {
  SST_UNKNOWN,
  SST_IDLE,
  SST_BUSY
} vtuner_session_status_t;

typedef struct vtuner_session {
  vtuner_hw_t hw;
  __u16	port;
  struct sockaddr_in ctrl_so;
  vtuner_session_status_t status;
} vtuner_session_t;
#define MAX_SESSIONS 1

void *discover_worker(void *data) {
  vtuner_session_t* session = (vtuner_session_t*)data;
  struct sockaddr_in discover_so, client_so;
  int clientlen = sizeof(client_so);
  int discover_fd;
  vtuner_net_message_t msg;
  int i;

  INFO("autodiscver thread started.\n");

  memset(&discover_so, 0, sizeof(discover_so));
  discover_so.sin_family = AF_INET;
  discover_so.sin_addr.s_addr = htonl(INADDR_ANY);
  discover_so.sin_port = htons(0x9989);
  discover_fd = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);

  if( bind(discover_fd, (struct sockaddr *) &discover_so, sizeof(discover_so)) < 0) {
    ERROR("failed to bind autodiscover socket - %m\n");
    exit(1);
  } else {
    INFO("waiting for autodiscover packet ...\n");
    while( recvfrom(discover_fd, &msg, sizeof(msg), 0, (struct sockaddr *) &client_so, &clientlen) >0 ) {
      ntoh_vtuner_net_message(&msg, 0); // we don't care frontend type
      if(msg.msg_type == MSG_DISCOVER ) {
        INFO("received discover request\n");
        for(i=0; i<MAX_SESSIONS; ++i)
          if(session[i].status == SST_IDLE) {
            DEBUGMAIN("Session %d device type %d is idle\n", i, session[i].hw.type);
            if(session[i].hw.type == msg.u.discover.fe_info.type)
              break;
          }
        if( i==MAX_SESSIONS ) {
          INFO("No idle device of type %d\n", msg.u.discover.fe_info.type);
        } else {
          msg.u.discover.port = session[i].port;
          set_dvb_frontend_info( &msg, &session[i].hw.fe_info );
          DEBUGMAIN("GET_FE_INFO type: %d frq_min: %d frq_max: %d\n", msg.u.discover.fe_info.type, msg.u.discover.fe_info.frequency_min, msg.u.discover.fe_info.frequency_max);
          hton_vtuner_net_message(&msg, 0);
          if(sendto(discover_fd, &msg, sizeof(msg), 0, (struct sockaddr *)&client_so, sizeof(client_so))>0) 
            INFO("Answered discover request with session %d\n",i);
        }
      }
      INFO("waiting for autodiscover packet ...\n");
    }
  }
}

typedef enum tsdata_worker_status {
  DST_UNKNOWN,
  DST_RUNNING,
  DST_EXITING
} tsdata_worker_status_t;

typedef struct tsdata_worker_data {
  int in;
  int out;
  struct sockaddr* data_so;
  tsdata_worker_status_t status;
} tsdata_worker_data_t;

void *tsdata_worker(void *d) {
  tsdata_worker_data_t* data = (tsdata_worker_data_t*)d;

  data->status = DST_RUNNING;
  DEBUGMAIN("tsdata_worker thread started.\n");

  #ifdef DBGTS
  int dbg_fd=open(DBGTS, O_RDWR);
  if(dbg_fd<0)
    DEBUGMAIN("Can't open debug ts file %s - %m\n",DBGTS);
  else
    DEBUGMAIN("copy TS data to %s\n", DBGTS);
  #endif

  unsigned char buffer[188*4096]; 
  int bufptr = 0, bufptr_write = 0;

  while( data->status == DST_RUNNING) {
    struct pollfd pfd[] = { { data->in, POLLIN, 0 } };
    poll(pfd, 1, 5);
    if(pfd[0].revents & POLLIN) {
      int rlen = read(data->in, buffer + bufptr, sizeof(buffer) - bufptr);
      if(rlen>0) bufptr += rlen;
      int w = bufptr - bufptr_write;
      if( w > 32768) {
	if( w > 65424) w = 65424; // cap write to max. udp msg size rounded down to ts
        int wlen = sendto(data->out, buffer + bufptr_write, w, 0, data->data_so, sizeof(*data->data_so));
        if(wlen>0) bufptr_write += wlen;
        if (bufptr_write == bufptr) bufptr_write = bufptr = 0;
        #ifdef DBGTS
          //FIXME
          write(dbg_fd, buffer, rlen);
        #endif
      }
    }
  }

  data->status = DST_UNKNOWN; 
}

void *session_worker(void *data) {
  vtuner_session_t *session = (vtuner_session_t*)data;
    
  vtuner_net_message_t msg;
  struct sockaddr_in data_so, ctrl_so;
  socklen_t ctrllen = sizeof(ctrl_so);
  int listen_fd, ctrl_fd, data_fd;
  #if HAVE_DVB_API_VERSION < 3
    FrontendParameters fe_params;
  #else
    struct dvb_frontend_parameters fe_params;
  #endif

  while(1) {
    listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(listen_fd < 0) {
      ERROR("Failed to create socket - %m\n");
      goto error;
    }

    memset((char *)&ctrl_so, 0, sizeof(ctrl_so));
    ctrl_so.sin_family = AF_INET;
    ctrl_so.sin_addr.s_addr = INADDR_ANY;
    ctrl_so.sin_port = 0;

    if( bind(listen_fd, (struct sockaddr *)&ctrl_so, ctrllen) < 0) {
      ERROR("failed to bind socket - %m\n");
      goto cleanup_listen;
    }

    ctrllen = sizeof(ctrl_so);
    getsockname(listen_fd, (struct sockaddr *)&ctrl_so, &ctrllen);
    session->port = ntohs(ctrl_so.sin_port);
    INFO("socket bound to %d\n", session->port);

    if( listen(listen_fd, 1) < 0 ) {
      ERROR("failed to listen on socket - %m\n");
      goto cleanup_listen;
    }

    session->status = SST_IDLE;
    INFO("waiting for connect on %d\n", session->port);
    ctrllen = sizeof(ctrl_so);
    ctrl_fd = accept(listen_fd, (struct sockaddr *)&ctrl_so, &ctrllen);
    if(ctrl_fd<0) {
      ERROR("accept failed on socket - %m\n");
      goto cleanup_listen;
    }
    // INFO("accecpted connect from: %s:%d\n", inet_ntoa(ctrl_so.sin_addr.s_addr), ntohs(ctrl_so.sin_port) );

    session->status = SST_BUSY;
    close(listen_fd);
    listen_fd=0;

    int opt;

    opt=1;
    if( setsockopt(ctrl_fd, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt)) < 0) 
      WARN("setsockopt TCP_NODELAY %d failed -%m\n",opt);
    else
      DEBUGMAIN("setsockopt TCP_NODELAY %d successful\n",opt);

    opt=1;
    if( setsockopt(ctrl_fd, SOL_SOCKET, SO_KEEPALIVE, &opt, sizeof(opt)) < 0) 
      WARN("setsockopt SO_KEEPALIVE %d failed -%m\n",opt);  
    else
      DEBUGMAIN("setsockopt SO_KEEPALIVE %d successful\n",opt);

    // keepalive interval 15;
    opt=15; 
    if( setsockopt(ctrl_fd, IPPROTO_TCP, TCP_KEEPIDLE, &opt, sizeof(opt)) < 0) 
      WARN("setsockopt TCP_KEEPIDLE %d failed -%m\n",opt);
    else
      DEBUGMAIN("setsockopt TCP_KEEPIDLE %d successful\n",opt);

    // retry twice
    opt=2;  
    if(setsockopt(ctrl_fd, IPPROTO_TCP, TCP_KEEPCNT, &opt, sizeof(opt))  < 0) 
      WARN("setsockopt TCP_KEEPCNT %d failed -%m\n",opt);
    else
      DEBUGMAIN("setsockopt TCP_KEEPCNT %d successful\n",opt);

    // allow 2 sec. to answer on keep alive
    opt=2;  
    if(setsockopt(ctrl_fd, IPPROTO_TCP, TCP_KEEPINTVL, &opt, sizeof(opt)) < 0) 
      WARN("setsockopt TCP_KEEPINTVL %d failed -%m\n",opt);
    else
      DEBUGMAIN("setsockopt TCP_KEEPINTVL %d successful\n",opt);

    data_fd=socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
    memset(&data_so, 0, sizeof(data_so));
    data_so.sin_family = AF_INET;
    data_so.sin_addr.s_addr = ctrl_so.sin_addr.s_addr;
    data_so.sin_port = htons(0x9988);
    fcntl(session->hw.streaming_fd, F_SETFL, O_NONBLOCK);

    // INFO("accecpted connect from: %s:%d\n", inet_ntoa(ctrl_so.sin_addr.s_addr), ntohs(ctrl_so.sin_port) );

    tsdata_worker_data_t dwd;
    dwd.in = session->hw.streaming_fd;
    dwd.out = data_fd;
    dwd.status = DST_UNKNOWN;
    dwd.data_so = (struct sockaddr*)&data_so;
    pthread_t dwt;
    pthread_create( &dwt, NULL, tsdata_worker, &dwd );
    
    while(1) {
      struct pollfd pfd[] = { { ctrl_fd, POLLIN, 0 } };
      poll(pfd, 1, 1000);
      if(pfd[0].revents & POLLIN) {
        int rlen = read(ctrl_fd, &msg, sizeof(msg));
        if(rlen<=0) goto cleanup_worker_thread;
        int ret=0;
        if( sizeof(msg) == rlen) {
          ntoh_vtuner_net_message(&msg, session->hw.type);
          if(msg.msg_type < 1023 )
          switch (msg.u.vtuner.type) {
            case MSG_SET_FRONTEND: 
              get_dvb_frontend_parameters( &fe_params, &msg.u.vtuner, session->hw.type);
              ret=hw_set_frontend( &session->hw, &fe_params);
              break;
            case MSG_GET_FRONTEND:
              ret=hw_get_frontend( &session->hw, &fe_params);
              set_dvb_frontend_parameters( &msg.u.vtuner, &fe_params, session->hw.type);
              break;
            case MSG_READ_STATUS:
              ret=hw_read_status( &session->hw, &msg.u.vtuner.body.status);
              DEBUGMAIN("MSG_READ_STATUS: 0x%x\n", msg.u.vtuner.body.status);
              break;
            case MSG_READ_BER:
              ret=ioctl(session->hw.frontend_fd, FE_READ_BER, &msg.u.vtuner.body.ber);
              break;
            case MSG_READ_SIGNAL_STRENGTH:
              ret=ioctl(session->hw.frontend_fd, FE_READ_SIGNAL_STRENGTH, &msg.u.vtuner.body.ss);
              break;
            case MSG_READ_SNR:
              ret=ioctl(session->hw.frontend_fd, FE_READ_SNR, &msg.u.vtuner.body.ss);
              break;
            case MSG_READ_UCBLOCKS:
              //FIXME: ioctl(session->hw.frontend_fd, FE_READ_UNCORRECTED_BLOCKS, &msg.u.vtuner.body.ucb);
              msg.u.vtuner.body.ucb = 0;
              break;
            case MSG_SET_TONE:
              ret=hw_set_tone(&session->hw, msg.u.vtuner.body.tone);
              DEBUGMAIN("MSG_SET_TONE: 0x%x\n", msg.u.vtuner.body.tone);
              break;
            case MSG_SET_VOLTAGE:
              ret=hw_set_voltage( &session->hw, msg.u.vtuner.body.voltage);
              DEBUGMAIN("MSG_SET_VOLTAGE: 0x%x\n", msg.u.vtuner.body.voltage);
              break;
            case MSG_ENABLE_HIGH_VOLTAGE:
              if(session->hw.type == VT_S) {
                //FIXME: no clue yet
              }
              break;
            case MSG_SEND_DISEQC_MSG:
              if(session->hw.type == VT_S) {
                //FIXME: DiSEQ control, no DiSEQ to test
              }
              break;
            case MSG_SEND_DISEQC_BURST:
              if(session->hw.type == VT_S) {
                //FIXME: DiSEQ control, no DiSEQ to test
              }
              break;
            case MSG_PIDLIST:
              ret=hw_pidlist( &session->hw, msg.u.vtuner.body.pidlist );
              break;
            default:
              ERROR("unknown vtuner message %d\n", msg.u.vtuner.type);
              goto cleanup_worker_thread;
          }

          if (msg.u.vtuner.type != MSG_PIDLIST ) {
	    if( ret!= 0 ) 
              WARN("vtuner call failed, type:%d reason:%d - %m\n", msg.u.vtuner.type, ret);
            msg.u.vtuner.type = ret;
            hton_vtuner_net_message(&msg, session->hw.type);
            write(ctrl_fd, &msg, sizeof(msg));        
          }
        }
      }
    }

    cleanup_worker_thread:
      dwd.status = DST_EXITING;
      pthread_join( dwt, NULL);
      DEBUGMAIN("tsdata_worker thread finished.\n");

    cleanup_data:
      close(data_fd);

    cleanup_ctrl:
      close(ctrl_fd);

    cleanup_listen:
      close(listen_fd);
 }

error:
  session->status = SST_UNKNOWN;
}

int main(int argc, char **argv) {
  
  int i;
  vtuner_session_t session[MAX_SESSIONS]; 
  pthread_t worker[MAX_SESSIONS], discover;

  int listen_fd;
  struct sockaddr_in listen_so;

  for(i=0; i<MAX_SESSIONS; ++i) session[i].status = SST_UNKNOWN;
  hw_init(&session[0].hw, 0, 0, 0);  // init adapter0 with frontend0 and demux0

  pthread_create( &discover, NULL, discover_worker, &session );

  for(i=0; i<MAX_SESSIONS; ++i) {
    pthread_create( &worker[i], NULL, session_worker, (void*)&session[i]);
  }

  pthread_join(discover, NULL);      
  pthread_join(worker[0], NULL);
}
