#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <string.h>
#include <poll.h>
#include <linux/dvb/dmx.h>

#include "vtuner-dmm-3.h"

typedef enum {
        DMX_TAP_TS = 0,
        DMX_TAP_PES = DMX_PES_OTHER, /* for backward binary compat. */
} dmx_tap_type_t;

#define DMX_ADD_PID              _IO('o', 51)
#define DMX_REMOVE_PID           _IO('o', 52)

int hw_init(vtuner_hw_t* hw, int adapter, int frontend, int demux) {

  char devstr[80];
  int i;

  hw->adapter = adapter;
  hw->frontend = frontend;
  hw->demux = demux;

  hw->frontend_fd = 0;
  hw->demux_fd = hw->streaming_fd = 0;
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
    case FE_OFDM: hw->type = VT_S; break;
    default:
      ERROR("Unknown frontend type %d\n", hw->fe_info.type);
      goto cleanup_fe;
  }

  INFO("FE_GET_INFO type:%d\n", hw->fe_info.type);
  
  sprintf( devstr, "/dev/dvb/adapter%d/demux%d", hw->adapter, 0);
  hw->demux_fd = hw->streaming_fd = open(devstr, O_RDWR);
  if(hw->demux_fd<0) {
    ERROR("failed to open %s\n", devstr);
    goto cleanup_fe;
  }

  if( ioctl(hw->demux_fd, DMX_SET_BUFFER_SIZE, 1024*1024) != 0 ) {
    ERROR("DMX_SET_BUFFER_SIZE failed for %s\n",devstr);
    goto cleanup_demux;
  }

  if( fcntl(hw->demux_fd, F_SETFL, O_NONBLOCK) != 0) {
    ERROR("O_NONBLOCK failed for %s\n",devstr);
    goto cleanup_demux;
  }

  struct dmx_pes_filter_params flt;
  flt.pid = -1;
  flt.input = DMX_IN_FRONTEND;
  flt.output = DMX_OUT_TAP;
  flt.pes_type = DMX_TAP_TS;
  flt.flags = 0;
  if(ioctl(hw->demux_fd, DMX_SET_PES_FILTER, &flt) != 0) {
    ERROR("DMX_SET_PES_FILTER failed for %s - %m\n", devstr);
    goto cleanup_demux;
  }

  if(ioctl(hw->demux_fd, DMX_START, 0) != 0) {
    ERROR("DMX_START failed for %s - %m\n", devstr);
  } 

  return 0;

cleanup_demux:
  close(hw->demux_fd);

cleanup_fe:
  close(hw->frontend_fd);

error:
  return -1;
} 

int hw_get_frontend(vtuner_hw_t* hw, struct dvb_frontend_parameters* fe_params) {
  int ret;
  ret = ioctl(hw->frontend_fd, FE_GET_FRONTEND, fe_params);
  if( ret != 0 ) WARN("FE_GET_FRONTEND failed\n");
  return ret;
}

int hw_set_frontend(vtuner_hw_t* hw, struct dvb_frontend_parameters* fe_params) {
  int ret;
  ret = ioctl(hw->frontend_fd, FE_SET_FRONTEND, fe_params);
  if( ret != 0 ) WARN("FE_SET_FRONTEND failed %d\n", hw->frontend_fd);
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
  // Dream supports this on DVB-T, but not plain linux
  ret = ioctl(hw->frontend_fd, FE_SET_VOLTAGE, voltage);
  if( ret != 0 ) WARN("FE_SET_VOLTAGE failed\n");
  return ret;
}

int hw_pidlist(vtuner_hw_t* hw, __u16* pidlist) {
  int i,j;

  DEBUGHW("hw_pidlist befor: ");
  for(i=0; i<30; ++i) DEBUGHWC("%d ", hw->pids[i]);
  DEBUGHWC("\n");
  DEBUGHW("hw_pidlist sent:  ");
  for(i=0; i<30; ++i) DEBUGHWC("%d ", pidlist[i]);
  DEBUGHWC("\n");

  for(i=0; i<30; ++i) 
    if(hw->pids[i] != 0xffff) {
      for(j=0; j<30; ++j) 
        if(hw->pids[i] == pidlist[j])
          break;
      if(j == 30) {
        if(ioctl(hw->demux_fd, DMX_REMOVE_PID, hw->pids[i]) != 0) {
          WARN("DMX_REMOVE_PID %d failed - %m\n", hw->pids[i]);
        }
        DEBUGHW("remove pid %d\n", hw->pids[i]);
      }
    }

  for(i=0; i<30; ++i) 
    if(pidlist[i] != 0xffff) {
      for(j=0; j<30; ++j) 
        if(pidlist[i] == hw->pids[j])
          break;
      if(j == 30) {
        if(ioctl(hw->demux_fd, DMX_ADD_PID, pidlist[i]) != 0) {
          WARN("DMX_ADD_PID %d failed - %m\n", pidlist[i]);
        }
        DEBUGHW("add pid %d\n",  pidlist[i]);
      }
    }

  memcpy(hw->pids, pidlist, sizeof(hw->pids));
  return 0;
}
