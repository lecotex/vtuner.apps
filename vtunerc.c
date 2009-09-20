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

#include "vtuner-network.h"

#define PVR_FLUSH_BUFFER    0
#define VTUNER_GET_MESSAGE  1
#define VTUNER_SET_RESPONSE 2
#define VTUNER_SET_NAME     3
#define VTUNER_SET_TYPE     4
#define VTUNER_SET_HAS_OUTPUTS 5
#define VTUNER_SET_FE_INFO 6
int dbg_level = 2;

#ifdef DEBUG_MAIN
#define DEBUG(msg, ...) fprintf(stderr,"[%d %s:%u] debug: " msg, getpid(), __FILE__, __LINE__, ## __VA_ARGS__)
#define DEBUGC(msg, ...) fprintf(stderr,msg, ## __VA_ARGS__)
#else
#define DEBUG(msg, ...)
#define DEBUGC(msg, ...)
#endif

typedef enum tsdata_worker_status {
  DST_UNKNOWN,
  DST_RUNNING,
  DST_EXITING
} tsdata_worker_status_t;

typedef struct tsdata_worker_data {
  int in;
  int out;
  tsdata_worker_status_t status;  
} tsdata_worker_data_t;

void *tsdata_worker(void *d) {
  tsdata_worker_data_t* data = (tsdata_worker_data_t*)d;

  data->status = DST_RUNNING;

  unsigned char buf[4096*188];
  int bufptr = 0, bufptr_write = 0;

  while(data->status == DST_RUNNING) {
    struct pollfd pfd[] = { { data->in, 0, 0 }, { data->out, 0, 0 } };
    int can_read = (sizeof(buf) - bufptr) > 1500;
    int can_write = ( bufptr - 65536 > bufptr_write);
    if (can_read) pfd[0].events |= POLLIN;
    if (can_write) pfd[1].events |= POLLOUT;
    poll(pfd, 2, 5);  // don't poll forever to catch data->status != DST_RUNNING

    if (pfd[0].revents & POLLIN) {
      int r = read(data->in, buf + bufptr, sizeof(buf) - bufptr);
      if (r <= 0) {
        WARN("udp read: %m\n");
      } else {
        bufptr += r;
      }
    }

    if (pfd[1].revents & POLLOUT) {
      int w = bufptr - bufptr_write;
      if (write(data->out, buf + bufptr_write, w) != w) {
        ERROR("write failed - %m");
        exit(1);
      }
      bufptr_write += w;
      if (bufptr_write == bufptr) bufptr_write = bufptr = 0;
    }

  }
  data->status = DST_EXITING;
}

int main(int argc, char **argv)
{
	int type;
	char ctype[7];
        if(strstr(argv[0],"vtunercs") != NULL) {
          type = VT_S; 
	  strncpy(ctype,"DVB-S2",sizeof(ctype));
        } else if(strstr(argv[0],"vtunerct") != NULL ) {
	  type = VT_T;
	  strncpy(ctype,"DVB-T",sizeof(ctype));
        } else if(strstr(argv[0],"vtunercc") != NULL ) {
	  type = VT_C;
          strncpy(ctype,"DVB-C",sizeof(ctype));
        } else {
          ERROR("unknown filename\n");
	  exit(1);
        }
       	INFO("Simulating a %s tuner\n", ctype); 

	struct sockaddr_in server_so;
        socklen_t serverlen = sizeof(server_so);

        int udpsock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
        struct sockaddr_in dataaddr;

        memset(&dataaddr, 0, sizeof(dataaddr));
        dataaddr.sin_family = AF_INET;
        dataaddr.sin_addr.s_addr = htonl(INADDR_ANY);
        dataaddr.sin_port = htons(0x9988);
        if (bind(udpsock, (struct sockaddr *) &dataaddr, sizeof dataaddr) < 0)
        {
                ERROR("bind: %m\n");
                return 1;
        }

        vtuner_net_message_t msg;
	struct sockaddr_in  msg_so;
	msg_so.sin_family = AF_INET;
	msg_so.sin_addr.s_addr = htonl(INADDR_BROADCAST);
	msg_so.sin_port = htons(0x9989);
	msg.msg_type = MSG_DISCOVER;
	msg.u.discover.fe_info.type = type;
	msg.u.discover.port = 0;
	int broadcast = -1;
	setsockopt(udpsock, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast));
	hton_vtuner_net_message(&msg, 0);
	sendto(udpsock, &msg, sizeof(msg), 0, (struct sockaddr *) &msg_so, sizeof(msg_so));

        while(msg.u.discover.port == 0)  {
          INFO("Waiting to receive autodiscovery packet.\n"); 
          if( recvfrom( udpsock,  &msg, sizeof(msg), 0, (struct sockaddr *)&server_so, &serverlen ) > 0 && msg.u.discover.port != 0 ) { 
            INFO("Received discover message from %s:%d\n", inet_ntoa(server_so.sin_addr), ntohs(server_so.sin_port));
          }
        }

        int vfd = socket(PF_INET, SOCK_STREAM, 0);
	if(vfd<0) {
          ERROR("socket - %m\n");
          exit(1);
        }

        server_so.sin_port = msg.u.discover.port;
        INFO("connect to %s:%d\n", inet_ntoa(server_so.sin_addr), ntohs(server_so.sin_port));
        if(connect(vfd, (struct sockaddr *)&server_so, serverlen) < 0) {
	  ERROR("connect - %m\n");
        }

	int vtuner_control = open("/dev/misc/vtuner0", O_RDWR);
	if (vtuner_control < 0)
	{
		perror("/dev/misc/vtuner0");
		return 1;
	}

		/* setup /proc/bus/nim_sockets */
        char name[80];
        sprintf(name, "VT-%s", inet_ntoa(server_so.sin_addr));
	if (ioctl(vtuner_control, VTUNER_SET_NAME, name))
		perror("VTUNER_SET_NAME");
	if (ioctl(vtuner_control, VTUNER_SET_TYPE, ctype))
		perror("VTUNER_SET_TYPE");
	if (ioctl(vtuner_control, VTUNER_SET_HAS_OUTPUTS, "no"))
		perror("VTUNER_SET_HAS_OUTPUTS");

	struct dvb_frontend_info fe_info;
        ntoh_vtuner_net_message(&msg, 0);
	get_dvb_frontend_info( &fe_info, &msg);
	DEBUG("SET_FE_INFO type: %d frq_min: %d frq_max: %d\n", msg.u.discover.fe_info.type, msg.u.discover.fe_info.frequency_min, msg.u.discover.fe_info.frequency_max);
        if (ioctl(vtuner_control, VTUNER_SET_FE_INFO, &fe_info)) {
		WARN("VTUNER_SET_FE_INFO failed - %m\n");
	}       

        // send empty message to fully open connection
        msg.msg_type = MSG_NULL;
        write(vfd, &msg, sizeof(msg));
        read(vfd, &msg, sizeof(msg));

	unsigned char buf[4096*188];
	int bufptr = 0, bufptr_write = 0;

        #ifdef DEBUG_MAIN
	int fe = open("/dev/dvb/adapter0/frontend1", O_RDWR);
	if(fe > 0) {
	  struct dvb_frontend_info info;
	  if( ioctl(fe, FE_GET_INFO, &info) == 0 ) {
            DEBUG("vTuner frontend type is %d (%d is DVB-S, %d is DVB-C, %d is DVB-T)\n", info.type, FE_QPSK, FE_QAM, FE_OFDM);
	  }
          close(fe);
	} 
        #endif
	
        tsdata_worker_data_t dwd;
        dwd.in = udpsock;
        dwd.out = vtuner_control;
        dwd.status = DST_UNKNOWN;
        pthread_t dwt;
        pthread_create( &dwt, NULL, tsdata_worker, &dwd );
	
	while (1) {
          struct pollfd pfd[] = { { vtuner_control, POLLPRI, 0 }, {vfd, POLLIN, 0} };
          poll(pfd, 2, -1);
          if(pfd[0].revents & POLLPRI) {
	    INFO("vtuner message!\n");
            if (ioctl(vtuner_control, VTUNER_GET_MESSAGE, &msg.u.vtuner)) {
              ERROR("VTUNER_GET_MESSAGE- %m\n");
              exit(1);
            }

            // we need to save to msg_type here as hton works in place
            // so it's not save to access msg_type afterwards
            int msg_type = msg.msg_type = msg.u.vtuner.type;
            hton_vtuner_net_message( &msg, type );
            write(vfd, &msg, sizeof(msg));

            if (msg_type != MSG_PIDLIST) {
              read(vfd, &msg, sizeof(msg));
              ntoh_vtuner_net_message( &msg, type );
              if (ioctl(vtuner_control, VTUNER_SET_RESPONSE, &msg.u.vtuner)) {
                ERROR("VTUNER_SET_RESPONSE - %m\n");
                exit(1);
              }
            }
            INFO("msg: %d completed\n", msg_type);
          }
          
          if(pfd[1].revents & POLLIN) {
            int rlen = read(vfd, &msg, sizeof(msg));
            if(rlen == 0) {
              ERROR("Server disconncted - %m\n");
              exit(1);
            }
            //FIXME: handle incomming updates from server (status, BER, SNR, etc)
          }
        }
	return 0;
}
