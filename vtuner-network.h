#ifndef _VTUNERNETWORK_H_
#define _VTUNERNETWORK_H_

#if HAVE_DVB_API_VERSION < 3
  #include <ost/frontend.h>
  #include <ost/dmx.h>
  #include <ost/sec.h>
#else
  #include <linux/dvb/frontend.h>
  #include <linux/dvb/dmx.h>
#endif

typedef enum vtuner_type {
  VT_S,
  VT_C,
  VT_T
} vtuner_type_t;

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define MSG_SET_FRONTEND         1
#define MSG_GET_FRONTEND         2
#define MSG_READ_STATUS          3
#define MSG_READ_BER             4
#define MSG_READ_SIGNAL_STRENGTH 5
#define MSG_READ_SNR             6
#define MSG_READ_UCBLOCKS        7
#define MSG_SET_TONE             8
#define MSG_SET_VOLTAGE          9
#define MSG_ENABLE_HIGH_VOLTAGE  10
#define MSG_SEND_DISEQC_MSG      11
#define MSG_SEND_DISEQC_BURST    13
#define MSG_PIDLIST              14

#define MSG_NULL		 1024
#define MSG_DISCOVER		 1025

extern int dbg_level;
#define ERROR(msg, ...) if(dbg_level>=0) fprintf(stderr,"[%d %s:%u] error: " msg, getpid(), __FILE__, __LINE__, ## __VA_ARGS__)  
#define  WARN(msg, ...) if(dbg_level>=1) fprintf(stderr,"[%d %s:%u] warn: " msg, getpid(), __FILE__, __LINE__, ## __VA_ARGS__) 
#define  INFO(msg, ...) if(dbg_level>=2) fprintf(stderr,"[%d %s:%u] info: " msg, getpid(), __FILE__, __LINE__, ## __VA_ARGS__)

#ifdef DEBUG_NET
#define DEBUGNET(msg, ...) fprintf(stderr,"[%d %s:%u] debug: " msg, getpid(), __FILE__, __LINE__, ## __VA_ARGS__)
#define DEBUGNETC(msg, ...) fprintf(stderr,msg, ## __VA_ARGS__)
#else
#define DEBUGNET(msg, ...)
#define DEBUGNETC(msg, ...)
#endif


typedef struct vtuner_message {
        __s32 type;
        union {
		struct {
			__u32	frequency;
			__u8	inversion;
			union {
				struct {
					__u32	symbol_rate;
					__u32	fec_inner;
				} qpsk;
				struct {
					__u32   symbol_rate;
					__u32   fec_inner;
					__u32	modulation;
				} qam;
				struct {
					__u32	bandwidth;
					__u32	code_rate_HP;
					__u32	code_rate_LP;
					__u32	constellation;
					__u32	transmission_mode;
					__u32	guard_interval;
					__u32	hierarchy_information;
				} ofdm;
				struct {
					__u32	modulation;
				} vsb;
			} u;
		} fe_params;
                __u32 status;
                __u32 ber;
                __u16 ss, snr;
                __u32 ucb;
                __u8 tone;
                __u8 voltage;
                __u8 burst;
                __u16 pidlist[30];
                __u8  pad[60];
        } body;
} vtuner_message_t;

typedef struct vtuner_frontend_info {
  char name[128];
  vtuner_type_t type;
  __u32 frequency_min;
  __u32 frequency_max;
  __u32 frequency_stepsize;
  __u32 frequency_tolerance;
  __u32 symbol_rate_min;
  __u32 symbol_rate_max;
  __u32 symbol_rate_tolerance; 
  __u32 notifier_delay;
  __u8 caps;
} vtuner_frontend_info_t;

typedef struct vtuner_discover {
  vtuner_frontend_info_t fe_info;   
  __u16 port;
} vtuner_discover_t;

typedef struct vtuner_net_message {
  __s32 msg_type;
  __u32 serial;
  union {
    vtuner_message_t vtuner;
    vtuner_discover_t discover;
  } u;
} vtuner_net_message_t;

#if HAVE_DVB_API_VERSION < 3
  void get_dvb_frontend_info( FrontendInfo*, vtuner_net_message_t*);
  void set_dvb_frontend_info( vtuner_net_message_t*, FrontendInfo*);
  void get_dvb_frontend_parameters( FrontendParameters*, vtuner_message_t*, vtuner_type_t);
  void set_dvb_frontend_parameters( vtuner_message_t*, FrontendParameters*, vtuner_type_t); 
#else
  void get_dvb_frontend_info( struct dvb_frontend_info*, vtuner_net_message_t*);
  void set_dvb_frontend_info( vtuner_net_message_t*, struct dvb_frontend_info*);
  void get_dvb_frontend_parameters( struct dvb_frontend_parameters*, vtuner_message_t*, vtuner_type_t); 
  void set_dvb_frontend_parameters( vtuner_message_t*, struct dvb_frontend_parameters*, vtuner_type_t);
#endif

int ntoh_get_message_type( vtuner_net_message_t*);
void hton_vtuner_net_message( vtuner_net_message_t*, vtuner_type_t);
void ntoh_vtuner_net_message( vtuner_net_message_t*, vtuner_type_t); 

void print_vtuner_net_message(vtuner_net_message_t*);
#endif
