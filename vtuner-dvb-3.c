#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <string.h>
#include <poll.h>

#include "vtuner-dvb-3.h"

int hw_init(vtuner_hw_t* hw, int adapter, int frontend, int demux, int dvr) {

  char devstr[80];
  int i;

  hw->adapter = adapter;
  hw->frontend = frontend;
  hw->demux = demux;

  hw->frontend_fd = 0;
  hw->streaming_fd = 0;
  memset(hw->demux_fd, 0, sizeof(hw->demux_fd)); 
  memset(hw->pids, 0xff, sizeof(hw->pids));

  sprintf( devstr, "/dev/dvb/adapter%d/frontend%d", hw->adapter, hw->frontend);
  hw->frontend_fd = open( devstr, O_RDWR);
  if(hw->frontend_fd < 0) {
    ERROR("failed to open %s\n", devstr);
    goto error;
  }

  if(ioctl(hw->frontend_fd, FE_GET_INFO, &hw->fe_info) != 0) {
    ERROR("FE_GET_INFO failed for %s\n", devstr);
    goto error;    
  }

  switch(hw->fe_info.type) {
    case FE_QPSK: hw->type = VT_S; break;
    case FE_QAM:  hw->type = VT_C; break;
    case FE_OFDM: hw->type = VT_T; break;
    default: 
      ERROR("Unknown frontend type %d\n", hw->fe_info.type); 
      goto cleanup_fe;
  }
  INFO("FE_GET_INFO dvb-type:%d vtuner-type:%d\n", hw->fe_info.type, hw->type);

  sprintf( devstr, "/dev/dvb/adapter%d/dvr%d", hw->adapter, dvr); 
  hw->streaming_fd = open( devstr, O_RDONLY);
  if(hw->streaming_fd < 0) {
    ERROR("failed to open %s\n", devstr);
    goto cleanup_fe;
  }

  sprintf( devstr, "/dev/dvb/adapter%d/demux%d", hw->adapter, demux);
  for(i=0; i<MAX_DEMUX; ++i) {
    hw->demux_fd[i] = open(devstr, O_RDWR);
    if(hw->demux_fd[i]<0) {
      ERROR("failed to open %s\n", devstr);
      goto cleanup_demux;
    }

    if( ioctl(hw->demux_fd[i], DMX_SET_BUFFER_SIZE, 1024*16) != 0 ) {
      ERROR("DMX_SET_BUFFER_SIZE failed for %s\n",devstr);
      goto cleanup_demux;
    }
    if( fcntl(hw->demux_fd[i], F_SETFL, O_NONBLOCK) != 0) {
      ERROR("O_NONBLOCK failed for %s\n",devstr);
      goto cleanup_demux;
    }
  }

  return 0;

cleanup_demux:
  for(i=0;i<MAX_DEMUX; ++i) 
    if(hw->demux_fd[i] > 0) 
      close(hw->demux_fd[i]);

cleanup_dvr:
  close(hw->streaming_fd);

cleanup_fe:
  close(hw->frontend_fd);

error:
  return -1;
} 

void print_frontend_parameters(vtuner_hw_t* hw, struct dvb_frontend_parameters* fe_params, char *msg, size_t msgsize) {
  switch(hw->type) {
    case VT_S: snprintf(msg, msgsize, "freq:%d inversion:%d SR:%d FEC:%d\n", \
                        fe_params->frequency, fe_params->inversion, \
                        fe_params->u.qpsk.symbol_rate, fe_params->u.qpsk.fec_inner);
               break;
    case VT_C: snprintf(msg, msgsize, "freq:%d inversion:%d SR:%d FEC:%d MOD:%d\n", \
                        fe_params->frequency, fe_params->inversion, \
                        fe_params->u.qam.symbol_rate, fe_params->u.qam.fec_inner, fe_params->u.qam.modulation);
               break;
    case VT_T: break;
  }
}

int hw_get_frontend(vtuner_hw_t* hw, struct dvb_frontend_parameters* fe_params) {
  int ret;
  ret = ioctl(hw->frontend_fd, FE_GET_FRONTEND, fe_params);
  if( ret != 0 ) WARN("FE_GET_FRONTEND failed\n");
  return ret;
}

int hw_set_frontend(vtuner_hw_t* hw, struct dvb_frontend_parameters* fe_params) {
  int ret;
  char msg[1024];
  print_frontend_parameters(hw, fe_params, msg, sizeof(msg));
  INFO("FE_SET_FRONTEND parameters: %s", msg);
  #if DVB_API_VERSION < 5 
    ret = ioctl(hw->frontend_fd, FE_SET_FRONTEND, fe_params);
  #else
    struct dtv_property p[] = {
      { .cmd = DTV_DELIVERY_SYSTEM,   .u.data = SYS_DVBS },
      { .cmd = DTV_FREQUENCY,         .u.data = fe_params->frequency },
      { .cmd = DTV_MODULATION,        .u.data = QPSK },
      { .cmd = DTV_SYMBOL_RATE,       .u.data = fe_params->u.qpsk.symbol_rate },
      { .cmd = DTV_INNER_FEC,         .u.data = fe_params->u.qpsk.fec_inner },
      { .cmd = DTV_INVERSION,         .u.data = INVERSION_AUTO },
      { .cmd = DTV_ROLLOFF,           .u.data = ROLLOFF_AUTO },
      { .cmd = DTV_PILOT,             .u.data = PILOT_AUTO },
      { .cmd = DTV_TUNE },
    };
    struct dtv_properties cmdseq = {
      .num = 9,
      .props = p
    };
    if( fe_params->inversion & 0x20 ) {
      p[0].u.data = SYS_DVBS2;
      p[2].u.data = PSK_8;
      switch(fe_params->u.qpsk.fec_inner) {
        case 18: p[4].u.data = FEC_9_10; break;
        case 20: p[4].u.data = FEC_2_3; break;
      }
      DEBUGHW("DVB-S2 tuning\n");
    } 
    ret=ioctl(hw->frontend_fd, FE_SET_PROPERTY, &cmdseq);
  #endif
  if( ret != 0 ) WARN("FE_SET_FRONTEND failed %s\n", msg);
  #if DVB_API_VERSION == 5
    DEBUGHW("S2API tuning\n");
  #endif    
  return ret;
}

int hw_read_status(vtuner_hw_t* hw, __u32* status) {
  int ret;
  ret = ioctl(hw->frontend_fd, FE_READ_STATUS, status);
  if( ret != 0 ) WARN("FE_READ_STATUS failed\n");
  return ret;
}

int hw_set_tone(vtuner_hw_t* hw, __u8 tone) {
  int ret=0;
  ret = ioctl(hw->frontend_fd, FE_SET_TONE, tone);
  if( ret != 0 ) WARN("FE_SET_TONE failed - %m\n");
  return ret;
}

int hw_set_voltage(vtuner_hw_t* hw, __u8 voltage) {
  int ret=0;
  if( hw->type == VT_S || hw->type == VT_S2 ) { // Dream supports this on DVB-T, but not plain linux
    ret = ioctl(hw->frontend_fd, FE_SET_VOLTAGE, voltage);
    if( ret != 0 ) WARN("FE_SET_VOLTAGE failed - %m\n");
  }
  return ret;
}

int hw_send_diseq_msg(vtuner_hw_t* hw, __u8* pad) {
  int ret=0;
  ret=ioctl(hw->frontend_fd, FE_DISEQC_SEND_MASTER_CMD, pad);
  if( ret != 0 ) WARN("FE_DISEQC_SEND_MASTER_CMD failed - %m\n");
  return ret;
}

int hw_send_diseq_burst(vtuner_hw_t* hw, __u8* pad) {
  int ret=0;
  ret=ioctl(hw->frontend_fd, FE_DISEQC_SEND_BURST, pad);
  if( ret != 0 ) WARN("FE_DISEQC_SEND_BURST  - %m\n");
  return ret;
}

int hw_pidlist(vtuner_hw_t* hw, __u16* pidlist) {
  int i,j;
  struct dmx_pes_filter_params flt;

  DEBUGHWI("hw_pidlist befor: ");
  for(i=0; i<MAX_DEMUX; ++i) if(hw->pids[i] != 0xffff) DEBUGHWC("%d ", hw->pids[i]);
  DEBUGHWF("\n");
  DEBUGHWI("hw_pidlist sent:  ");
  for(i=0; i<MAX_DEMUX; ++i) if(pidlist[i] != 0xffff) DEBUGHWC("%d ", pidlist[i]);
  DEBUGHWF("\n");

  for(i=0; i<MAX_DEMUX; ++i) 
    if(hw->pids[i] != 0xffff) {
      for(j=0; j<MAX_DEMUX; ++j) 
        if(hw->pids[i] == pidlist[j])
          break;
      if(j == MAX_DEMUX) {
        ioctl(hw->demux_fd[i], DMX_STOP, 0);
        hw->pids[i] = 0xffff;
      }
    }

  for(i=0; i<MAX_DEMUX; ++i) 
    if(pidlist[i] != 0xffff) {
      for(j=0; j<MAX_DEMUX; ++j) 
        if(pidlist[i] == hw->pids[j])
          break;
      if(j == MAX_DEMUX) {
        for(j=0; j<MAX_DEMUX; ++j) 
          if(hw->pids[j] == 0xffff) 
            break;
        if(j==MAX_DEMUX) {
          WARN("no free demux found. skip pid %d\n",pidlist[i]);
        } else {
          flt.pid = hw->pids[j] = pidlist[i];
          flt.input = DMX_IN_FRONTEND;
          flt.pes_type = DMX_PES_OTHER;
          flt.output = DMX_OUT_TS_TAP;
          flt.flags = DMX_IMMEDIATE_START;
          ioctl(hw->demux_fd[j], DMX_SET_PES_FILTER, &flt);
        }
      }
    }

  DEBUGHWI("hw_pidlist after: ");
  for(i=0; i<MAX_DEMUX; ++i) if(hw->pids[i] != 0xffff) DEBUGHWC("%d ", hw->pids[i]);  
  DEBUGHWF("\n");

  return 0;
}
