#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <string.h>
#include <poll.h>
#include <ost/dmx.h>

#include "vtuner-dmm-2.h"

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

  sprintf( devstr, "/dev/dvb/card%d/frontend%d", hw->adapter, hw->frontend);
  hw->frontend_fd = open( devstr, O_RDWR);
  if(hw->frontend_fd < 0) {
    ERROR("failed to open %s\n", devstr);
    goto error;
  }

  FrontendInfo info;
  if(ioctl(hw->frontend_fd, FE_GET_INFO, &info) != 0) {
    ERROR("FE_GET_INFO failed for %s\n", devstr);
    goto cleanup_fe;    
  }

  switch(info.type) {
    case FE_QPSK: hw->type = VT_S; break;
    case FE_QAM:  hw->type = VT_C; break;
    case FE_OFDM: hw->type = VT_S; break;
    default:
      ERROR("Unknown frontend type %d\n", info.type);
      goto cleanup_fe;
  }
  INFO("FE_GET_INFO type:%d\n", info.type);

  if(ioctl(hw->frontend_fd, FE_SET_POWER_STATE, FE_POWER_ON ) != 0 ) {
    ERROR("FE_SET_POWER_STATE failed - %m\n");
    goto cleanup_fe;
  }

  sprintf( devstr, "/dev/dvb/card%d/demux%d", hw->adapter, demux);
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

  struct dmxPesFilterParams flt;
  flt.pid = 0;
  flt.input = DMX_IN_FRONTEND;
  flt.pesType = DMX_PES_OTHER;
  flt.output = DMX_OUT_TAP;
  flt.flags = 0;
  if(ioctl(hw->demux_fd, DMX_SET_PES_FILTER, &flt) !=0) {
    ERROR("DMX_SET_PES_FILTER failed for %s\n", devstr);
    goto cleanup_demux;
  }

/*
  sprintf( devstr, "/dev/dvb/card%d/dvr%d", hw->adapter, 0);
  hw->streaming_fd = open(devstr, O_RDONLY);
  if(hw->streaming_fd<0) {
    ERROR("failed to open %s\n", devstr);
    goto cleanup_demux;
  }

  if(ioctl(hw->demux_fd, DMX_START, 0) != 0) {
    ERROR("DMX_START failed for %s\n", devstr);
    goto cleanup_dvr;
  }
*/

  sprintf( devstr, "/dev/dvb/card%d/sec%d", hw->adapter, 0);
  hw->sec_fd= open(devstr, O_RDWR);
  if(hw->sec_fd<0) {
    ERROR("failed to open %s\n", devstr);
    goto cleanup_dvr;
  }

  return 0;

cleanup_dvr:
  close(hw->streaming_fd);

cleanup_demux:
  close(hw->demux_fd);

cleanup_fe:
  close(hw->frontend_fd);

error:
  return -1;
} 

int hw_get_frontend(vtuner_hw_t* hw, FrontendParameters* fe_params) {
  int ret;
  ret = ioctl(hw->frontend_fd, FE_GET_FRONTEND, fe_params);
  if( ret != 0 ) WARN("FE_GET_FRONTEND failed\n");
  return ret;
}

int hw_set_frontend(vtuner_hw_t* hw, FrontendParameters* fe_params) {
  int ret;
  ret = ioctl(hw->frontend_fd, FE_SET_FRONTEND, fe_params);
  if( ret != 0 ) {
    WARN("FE_SET_FRONTEND failed %d\n", hw->frontend_fd);
    switch(hw->type) {
      case VT_S: 
        WARN("FE_SET_FRONTEND parameters: Freq:%d Inversion: %d SymbolRate: %d FEC: %d\n", fe_params->Frequency, fe_params->Inversion, fe_params->u.qpsk.SymbolRate, fe_params->u.qpsk.FEC_inner);
        break;
     case VT_C:
        //FIXME: DVB_C Params
        break;
     case VT_T:
        //FIXME: DVB_C Params
        break;
    }
  }
  return ret;
}

int hw_read_status(vtuner_hw_t* hw, __u32* status) {
  int ret;
  __u32 ts;

  ret = ioctl(hw->frontend_fd, FE_READ_STATUS, &ts);
  if( ret != 0 ) WARN("FE_READ_STATUS failed - %m\n");
  DEBUGHW("FE_READ_STATUS: 0x%x\n", ts);

  *status = ( ts & FE_HAS_SIGNAL ) >> 1 |
           ( ts & FE_HAS_LOCK   ) << 1 |
           ( ts & FE_HAS_CARRIER ) >> 3 |
           ( ts & FE_HAS_VITERBI ) >> 3 |
           ( ts & FE_HAS_SYNC ) >> 3 ;

  return ret;
}

int hw_set_tone(vtuner_hw_t* hw, __u8 tone) {
  int ret=0;
  ret = ioctl(hw->sec_fd, SEC_SET_TONE, tone);
  if( ret != 0 ) WARN("SEC_SET_TONE failed - %m\n");
  return ret;
}

int hw_set_voltage(vtuner_hw_t* hw, __u8 voltage) {
  int ret=0;

  secVoltage vt = SEC_VOLTAGE_OFF;
  if( voltage == 0 ) {
    vt = SEC_VOLTAGE_13;
  }
  else if( voltage == 1 ) {
    vt = SEC_VOLTAGE_18;
  }
  ret = ioctl(hw->sec_fd, SEC_SET_VOLTAGE, &vt);
  if( ret != 0 ) WARN("SEC_SET_VOLTAGE failed - %m\n");
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
        ioctl(hw->demux_fd, DMX_REMOVE_PID, hw->pids[i]);
        DEBUGHW("remove pid %d\n", hw->pids[i]);
      }
    }

  for(i=0; i<30; ++i) 
    if(pidlist[i] != 0xffff) {
      for(j=0; j<30; ++j) 
        if(pidlist[i] == hw->pids[j])
          break;
      if(j == 30) {
        ioctl(hw->demux_fd, DMX_ADD_PID, pidlist[i]);
        DEBUGHW("add pid %d\n",  pidlist[i]);
      }
    }

  memcpy(hw->pids, pidlist, sizeof(hw->pids));

  if(ioctl(hw->demux_fd, DMX_START, 0) != 0) {
    ERROR("DMX_START failed\n");
  }

  return 0;
}
