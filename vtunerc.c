#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
#include <syslog.h>

#include "vtuner-network.h"

#define PVR_FLUSH_BUFFER    0
#define VTUNER_GET_MESSAGE  1
#define VTUNER_SET_RESPONSE 2
#define VTUNER_SET_NAME     3
#define VTUNER_SET_TYPE     4
#define VTUNER_SET_HAS_OUTPUTS 5
#define VTUNER_SET_FE_INFO 6

int dbg_level =  0x00ff;
int use_syslog = 0;

#define DEBUGMAIN(msg, ...)  write_message(0x0010, "[%d %s:%u] debug: " msg, getpid(), __FILE__, __LINE__, ## __VA_ARGS__)
#define DEBUGMAINC(msg, ...) write_message(0x0010, msg, ## __VA_ARGS__)

typedef enum tsdata_worker_status {
  DST_UNKNOWN,
  DST_RUNNING,
  DST_EXITING,
  DST_FAILED
} tsdata_worker_status_t;

typedef struct tsdata_worker_data {
  int in;
  int out;
  tsdata_worker_status_t status;  
} tsdata_worker_data_t;

void *tsdata_worker(void *d) {
  tsdata_worker_data_t* data = (tsdata_worker_data_t*)d;

  data->status = DST_RUNNING;

  char buf[188*384];

  while(data->status == DST_RUNNING) {
    struct pollfd pfd[] = { { data->in, POLLIN, 0 } };
    // don't poll forever to catch data->status != DST_RUNNING
    if( poll(pfd, 1, 500) != 0) {
      // we're polling one fd here so we know that reading can't block
      int r = read(data->in, buf, sizeof(buf) );
      if (r <= 0) {
        ERROR("tcp read\n");
        data->status = DST_FAILED;
      } else {
        if (write(data->out, buf, r) != r) {
          ERROR("write failed - %m");
          data->status = DST_FAILED;
        }
      }
    }
  }

  ERROR("TS data copy thread terminated.\n");
  data->status = DST_EXITING;
}

typedef enum discover_worker_status {
  DWS_IDLE,
  DWS_RUNNING,
  DWS_DISCOVERD,
  DWS_FAILED
} discover_worker_status_t;

typedef struct discover_worker_data {
  __u32 types;
  struct sockaddr_in server_addr;
  discover_worker_status_t status;
  vtuner_net_message_t msg;
} discover_worker_data_t;

int *discover_worker(void *d) {
  discover_worker_data_t* data = (discover_worker_data_t*)d;

  INFO("starting discover thread\n");
  data->status = DWS_RUNNING;
  int discover_fd = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
  int broadcast = -1;
  setsockopt(discover_fd, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast)); 
  
  struct sockaddr_in discover_addr;
  memset(&discover_addr, 0, sizeof(discover_addr));
  discover_addr.sin_family = AF_INET;
  discover_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  discover_addr.sin_port = 0;

  if (bind(discover_fd, (struct sockaddr *) &discover_addr, sizeof(discover_addr)) < 0) {
    ERROR("can't bind discover socket - %m\n");
    close(discover_fd);
    data->status = DWS_FAILED;
    goto discover_worker_end;
  } 

  struct sockaddr_in  msg_addr;
  msg_addr.sin_family = AF_INET;
  msg_addr.sin_addr.s_addr = htonl(INADDR_BROADCAST);
  msg_addr.sin_port = htons(0x9989);

  memset(&data->msg, 0, sizeof(data->msg));
  data->msg.msg_type = MSG_DISCOVER;
  data->msg.u.discover.vtype = data->types;
  data->msg.u.discover.port = 0;
  hton_vtuner_net_message(&data->msg, 0); // we don't care tuner type for conversion of discover

  int timeo = 100;
  do {
    INFO("Sending discover message for device types %x\n", data->types);
    sendto(discover_fd, &data->msg, sizeof(data->msg), 0, (struct sockaddr *) &msg_addr, sizeof(msg_addr));
    struct pollfd pfd[] = { { discover_fd, POLLIN, 0 } }; 
    while( data->msg.u.discover.port == 0 &&
           poll(pfd, 1, timeo) == 1 ) {
      int server_addrlen = sizeof(data->server_addr);
      recvfrom( discover_fd,  &data->msg, sizeof(data->msg), 0, (struct sockaddr *)&data->server_addr, &server_addrlen );
    }
    if( timeo < 10000) timeo *= 2;
  } while (data->msg.u.discover.port == 0); 

  data->server_addr.sin_port = data->msg.u.discover.port; // no need for ntoh here
  ntoh_vtuner_net_message(&data->msg, 0);
  
  INFO("Received discover message from %s control %d data %d\n", inet_ntoa(data->server_addr.sin_addr), data->msg.u.discover.port, data->msg.u.discover.tsdata_port);
  data->status = DWS_DISCOVERD;

discover_worker_end:
  close(discover_fd);
}

typedef enum vtuner_status {
  VTS_DISCONNECTED,
  VTS_DISCOVERING,
  VTS_CONNECTED
} vtuner_status_t;

// dvb_frontend_info for DVB-S2
struct dvb_frontend_info fe_info_dvbs2 = {
  .name                  = "vTuner DVB-S2",
  .type                  = 0,
  .frequency_min         = 925000,
  .frequency_max         = 2175000,
  .frequency_stepsize    = 125000,
  .frequency_tolerance   = 0,
  .symbol_rate_min       = 1000000,
  .symbol_rate_max       = 45000000,
  .symbol_rate_tolerance = 0,
  .notifier_delay        = 0,
  .caps                  = 0x400006ff
};


struct dvb_frontend_info fe_info_dvbs = {
  .name                  = "vTuner DVB-S",
  .type                  = 0,
  .frequency_min         = 950000,
  .frequency_max         = 2150000,
  .frequency_stepsize    = 125,
  .frequency_tolerance   = 0,
  .symbol_rate_min       = 1000000,
  .symbol_rate_max       = 45000000,
  .symbol_rate_tolerance = 500,
  .notifier_delay        = 0,
  .caps                  = 0x6af
};

struct dvb_frontend_info fe_info_dvbc = {
  .name                  = "vTuner DVB-C",
  .type                  = 1,
  .frequency_min         = 55000000,
  .frequency_max         = 862000000,
  .frequency_stepsize    = 0,
  .frequency_tolerance   = 0,
  .symbol_rate_min       = 451875,
  .symbol_rate_max       = 7230000,
  .symbol_rate_tolerance = 0,
  .notifier_delay        = 0,
  .caps                  = 0x1
};

struct dvb_frontend_info fe_info_dvbt = {
  .name                  = "vTuner DVB-T",
  .type                  = 2,
  .frequency_min         = 51000000,
  .frequency_max         = 858000000,
  .frequency_stepsize    = 166667,
  .frequency_tolerance   = 0,
  .symbol_rate_min       = 0,
  .symbol_rate_max       = 0,
  .symbol_rate_tolerance = 0,
  .notifier_delay        = 0,
  .caps                  = 0xb2eaf
};

int main(int argc, char **argv) {

  openlog("vtunerc", LOG_PERROR, LOG_USER);

  int type;
  char ctype[7];
  struct dvb_frontend_info* vtuner_info;

  int vtuner_control = open("/dev/misc/vtuner0", O_RDWR);
  if (vtuner_control < 0) {
    perror("/dev/misc/vtuner0");
    return 1;
  }

  if (ioctl(vtuner_control, VTUNER_SET_NAME, "vTuner"))  {
    ERROR("VTUNER_SET_NAME failed - %m\n");
    exit(1);
  }

  if (ioctl(vtuner_control, VTUNER_SET_HAS_OUTPUTS, "no")) {
    ERROR("VTUNER_SET_HAS_OUTPUTS failed - %m\n");
    exit(1);
  }

  if(strstr(argv[0],"vtunercs") != NULL) {
    type = VT_S; 
    strncpy(ctype,"DVB-S2",sizeof(ctype));
    vtuner_info = &fe_info_dvbs2;
  } else if(strstr(argv[0],"vtunerct") != NULL ) {
    type = VT_T;
    strncpy(ctype,"DVB-T",sizeof(ctype));
    vtuner_info = &fe_info_dvbt;
  } else if(strstr(argv[0],"vtunercc") != NULL ) {
    type = VT_C;
    strncpy(ctype,"DVB-C",sizeof(ctype));
    vtuner_info = &fe_info_dvbc;
  } else {
    ERROR("unknown filename\n");
    exit(1);
  }
  INFO("Simulating a %s tuner\n", ctype); 

  if (ioctl(vtuner_control, VTUNER_SET_TYPE, ctype)) {
    ERROR("VTUNER_SET_TYPE failed - %m\n");
    exit(1);
  }

  if (ioctl(vtuner_control, VTUNER_SET_FE_INFO, vtuner_info)) {
    ERROR("VTUNER_SET_NAME failed - %m\n");
    exit(1);
  }

  discover_worker_data_t dsd;
  dsd.status = DWS_IDLE;
  vtuner_status_t vts = VTS_DISCONNECTED;
  long values_received = 0;
  vtuner_update_t values;
  int vfd;

  #define RECORDLEN 5
  vtuner_net_message_t record[RECORDLEN]; // SET_TONE, SET_VOLTAGE, SEND_DISEQC_MSG, MSG_PIDLIST
  memset(&record, 0, sizeof(vtuner_net_message_t)*RECORDLEN);

  while(dsd.status != DWS_FAILED) {
    pthread_t dwt, dst;
    tsdata_worker_data_t dwd;
    vtuner_net_message_t msg;

    if( vts == VTS_DISCONNECTED ) {
      if( dsd.status == DWS_IDLE ) {
        DEBUGMAIN("Start discover worker for device type %x\n", type);
        dsd.types = type;
        dsd.status = DWS_RUNNING;
        pthread_create( &dst, NULL, discover_worker, &dsd);
        vts = VTS_DISCOVERING;
      }
    }

    if( vts == VTS_DISCOVERING ) {
      if(dsd.status == DWS_DISCOVERD) {

        // pthread_join(&dst, NULL); // wait till discover threads finish, if hasn't
        dsd.status = DWS_IDLE;    // now it's sure to be idle

        vfd = socket(PF_INET, SOCK_STREAM, 0);
        if(vfd<0) {
          ERROR("Can't create server message socket - %m\n");
          exit(1);
        }
        
        INFO("connect control socket to %s:%d\n", inet_ntoa(dsd.server_addr.sin_addr), ntohs(dsd.server_addr.sin_port));
        if(connect(vfd, (struct sockaddr *)&dsd.server_addr, sizeof(dsd.server_addr)) < 0) {
          ERROR("Can't connect to server control socket - %m\n");
          vts = VTS_DISCONNECTED;
          close(vfd);

        } else {
          
          dsd.server_addr.sin_port = htons(dsd.msg.u.discover.tsdata_port);
          dwd.in = socket(PF_INET, SOCK_STREAM, 0);
          INFO("connect data socket to %s:%d\n", inet_ntoa(dsd.server_addr.sin_addr), ntohs(dsd.server_addr.sin_port));
          if( connect(dwd.in, (struct sockaddr *)&dsd.server_addr, sizeof(dsd.server_addr)) < 0) {
            ERROR("Can't connect to server data socket -%m\n");
            close(vfd);
            close(dwd.in);
            vts = VTS_DISCONNECTED;
          } else {

            dwd.out = vtuner_control;
            dwd.status = DST_UNKNOWN; 
            pthread_create( &dwt, NULL, tsdata_worker, &dwd ); 
            vts = VTS_CONNECTED;

            // send null message to fully open connection;
            msg.msg_type = MSG_NULL;
            write(vfd, &msg, sizeof(msg));
            read(vfd, &msg, sizeof(msg));

            int i;
            for(i=0; i<RECORDLEN; ++i) {
              if(record[i].msg_type != 0) {
                memcpy(&msg, &record[i], sizeof(msg));
                hton_vtuner_net_message( &msg, type );
                if(write(vfd, &msg, sizeof(msg))>0) {
                  INFO("replay message %d\n", i);
                  if(record[i].msg_type != MSG_PIDLIST) {
                    if(read(vfd, &msg, sizeof(msg))>0) {
                      INFO("got response for message %d\n", i);
                    }
                  }
                }
              }
            }
            
          }
        }

      }
    }

    int nrp = 2;
    struct pollfd pfd[] = { { vtuner_control, POLLPRI, 0 }, {vfd, POLLIN, 0} };
    if( vts != VTS_CONNECTED) nrp = 1; // don't poll for vfd if not connected
    poll(pfd, nrp, 100); // don't poll forever cause of status changes can happen

    if(pfd[0].revents & POLLPRI) {
      INFO("vtuner message!\n");
      if (ioctl(vtuner_control, VTUNER_GET_MESSAGE, &msg.u.vtuner)) {
        ERROR("VTUNER_GET_MESSAGE- %m\n");
        exit(1);
      }
      // we need to save to msg_type here as hton works in place
      // so it's not save to access msg_type afterwards
      int msg_type = msg.msg_type = msg.u.vtuner.type;

      // fill the record array in the correct order
      int recordnr = -1;
      switch(msg.u.vtuner.type) {
        case MSG_SET_FRONTEND:     recordnr=3; break;
        case MSG_SET_TONE:         recordnr=1; break;
        case MSG_SET_VOLTAGE:      recordnr=2; break;
        case MSG_SEND_DISEQC_MSG:  recordnr=0; break;
        case MSG_PIDLIST:          recordnr=4; break;
      }

      if( recordnr != -1 ) {
         memcpy(&record[recordnr], &msg, sizeof(msg));
         // this is a "state changeing" msg, make cache expired
         values_received = 0;
      }

      struct timespec t;
      clock_gettime(CLOCK_MONOTONIC, &t);
      long now=t.tv_sec*1000 + t.tv_nsec/1000000;

      int dontsend = ( now - values_received < 1000 ) && 
                     ( vts == VTS_CONNECTED ) &&
                     ( msg_type == MSG_READ_STATUS || msg_type == MSG_READ_BER || 
                       msg_type == MSG_READ_SIGNAL_STRENGTH || msg_type == MSG_READ_SNR || 
                       msg_type == MSG_READ_UCBLOCKS );

      if( vts == VTS_CONNECTED && ! dontsend )  {
        hton_vtuner_net_message( &msg, type );
        write(vfd, &msg, sizeof(msg));
      }

      if( dontsend ) {
        switch(msg_type) {
          case MSG_READ_STATUS:          msg.u.vtuner.body.status = values.status; break;
          case MSG_READ_BER:             msg.u.vtuner.body.ber    = values.ber; break;
          case MSG_READ_SIGNAL_STRENGTH: msg.u.vtuner.body.ss     = values.ss; break;
          case MSG_READ_SNR:             msg.u.vtuner.body.snr    = values.snr; break; 
          case MSG_READ_UCBLOCKS:        msg.u.vtuner.body.ucb    = values.ucb; break;
        }
        DEBUGMAIN("cached values are up to date\n");
      }

      if (msg_type != MSG_PIDLIST) {
        if( vts == VTS_CONNECTED ) {
          if( ! dontsend ) {
            read(vfd, &msg, sizeof(msg));
            ntoh_vtuner_net_message( &msg, type );
          }
        } else {
          INFO("fake server answer\n");
          switch(msg.u.vtuner.type) {
            case MSG_READ_STATUS:  
              msg.u.vtuner.body.status = 0; // tuning failed
              break; 
          }
          msg.u.vtuner.type = 0; // report success
        }
        if (ioctl(vtuner_control, VTUNER_SET_RESPONSE, &msg.u.vtuner)) {
          ERROR("VTUNER_SET_RESPONSE - %m\n");
          exit(1);
        }
      }
      INFO("msg: %d completed\n", msg_type);
    }
          
    if( pfd[1].revents & POLLIN ) {
      int rlen = read(vfd, &msg, sizeof(msg));
      if(rlen == 0) {
        ERROR("Server disconncted\n");
        vts = VTS_DISCONNECTED;        
        close(vfd);
      } 
      if( rlen == sizeof(msg)) {
        ntoh_vtuner_net_message( &msg, type );
        switch(msg.msg_type) {
          case MSG_UPDATE: {
            struct timespec t;
            clock_gettime(CLOCK_MONOTONIC, &t);
            values_received=t.tv_sec*1000 + t.tv_nsec/1000000;
            memcpy(&values, &msg.u.update, sizeof(values));
            break;
          }
        } 
      }
    }
  }

  return 0;
}
