#include "vtuner-network.h"
#include <string.h>
#include <stdio.h>

#define NTOHB(host,net,field) host->field=net.field
#define NTOHS(host,net,field) host->field=ntohs(net.field)
#define NTOHL(host,net,field) host->field=ntohl(net.field)

#define HTONB(net, host, field) net.field=host->field
#define HTONS(net, host, field) net.field=htons(host->field);
#define HTONL(net, host, field) net.field=htonl(host->field);

#define HTONLc(net,field) net.field=htonl(net.field)
#define HTONSc(net,field) net.field=htons(net.field)

#define NTOHLc(net,field) net.field=ntohl(net.field)
#define NTOHSc(net,field) net.field=ntohs(net.field)



#if HAVE_DVB_API_VERSION < 3
  void get_dvb_frontend_info( FrontendInfo* fe_info, vtuner_net_message_t* netmsg) {
    switch(netmsg->u.discover.fe_info.type) {
      case VT_S: fe_info->type = FE_QPSK; break;
      case VT_C: fe_info->type = FE_QAM; break;
      case VT_T: fe_info->type = FE_OFDM; break;
    }
    fe_info->minFrequency = netmsg->u.discover.fe_info.frequency_min;
    fe_info->maxFrequency = netmsg->u.discover.fe_info.frequency_max;
    fe_info->minSymbolRate = netmsg->u.discover.fe_info.symbol_rate_min;
    fe_info->maxSymbolRate = netmsg->u.discover.fe_info.symbol_rate_max;
    fe_info->hwType = 0;
    fe_info->hwVersion = 0;
  }

  void set_dvb_frontend_info( vtuner_net_message_t* netmsg, FrontendInfo* fe_info) {
    memset(netmsg->u.discover.fe_info.name, 0, sizeof(netmsg->u.discover.fe_info.name));
    switch(fe_info->type) {
      case FE_QPSK: netmsg->u.discover.fe_info.type = VT_S; break;
      case FE_QAM:  netmsg->u.discover.fe_info.type = VT_C; break;
      case FE_OFDM: netmsg->u.discover.fe_info.type = VT_T; break;
    }
    netmsg->u.discover.fe_info.frequency_min = fe_info->minFrequency;
    netmsg->u.discover.fe_info.frequency_max = fe_info->maxFrequency;
    netmsg->u.discover.fe_info.frequency_stepsize = 0;
    netmsg->u.discover.fe_info.frequency_tolerance = 0;
    netmsg->u.discover.fe_info.symbol_rate_min = fe_info->minSymbolRate;
    netmsg->u.discover.fe_info.symbol_rate_max = fe_info->maxSymbolRate;
    netmsg->u.discover.fe_info.symbol_rate_tolerance = 0;
    netmsg->u.discover.fe_info.notifier_delay = 0;
    netmsg->u.discover.fe_info.caps = 0; //FE_IS_STUPID
  }

  void get_dvb_frontend_parameters( FrontendParameters* hfe, vtuner_message_t* netmsg, vtuner_type_t type) {
    memset(hfe, 0, sizeof(hfe));

    hfe->Frequency			= netmsg->body.fe_params.frequency;
    hfe->Inversion			= netmsg->body.fe_params.inversion;
    switch(type) {
      case VT_S:
        hfe->u.qpsk.SymbolRate		= netmsg->body.fe_params.u.qpsk.symbol_rate;
        switch(netmsg->body.fe_params.u.qpsk.fec_inner) {
          case 0: hfe->u.qpsk.FEC_inner   = FEC_NONE; break;
          case 1: hfe->u.qpsk.FEC_inner   = FEC_1_2;  break;
          case 2: hfe->u.qpsk.FEC_inner   = FEC_2_3;  break;
          case 3: hfe->u.qpsk.FEC_inner   = FEC_3_4;  break;
          case 5: hfe->u.qpsk.FEC_inner   = FEC_5_6;  break;
          case 7: hfe->u.qpsk.FEC_inner   = FEC_7_8;  break;
          default: hfe->u.qpsk.FEC_inner   = FEC_AUTO; break;
        }	
        break;
      case VT_C:
        hfe->u.qam.SymbolRate		= netmsg->body.fe_params.u.qam.symbol_rate;
        switch(netmsg->body.fe_params.u.qam.fec_inner) {
          case 0: hfe->u.qam.FEC_inner   = FEC_NONE; break;
          case 1: hfe->u.qam.FEC_inner   = FEC_1_2;  break;
          case 2: hfe->u.qam.FEC_inner   = FEC_2_3;  break;
          case 3: hfe->u.qam.FEC_inner   = FEC_3_4;  break;
          case 5: hfe->u.qam.FEC_inner   = FEC_5_6;  break;
          case 7: hfe->u.qam.FEC_inner   = FEC_7_8;  break;
          default: hfe->u.qam.FEC_inner   = FEC_AUTO; break;
        }
        hfe->u.qam.QAM			= netmsg->body.fe_params.u.qam.modulation;
        break;
      case VT_T:
        hfe->u.ofdm.bandWidth		= netmsg->body.fe_params.u.ofdm.bandwidth;
        hfe->u.ofdm.HP_CodeRate		= netmsg->body.fe_params.u.ofdm.code_rate_HP;
        hfe->u.ofdm.LP_CodeRate		= netmsg->body.fe_params.u.ofdm.code_rate_LP;
        hfe->u.ofdm.Constellation		= netmsg->body.fe_params.u.ofdm.constellation;
        hfe->u.ofdm.TransmissionMode	= netmsg->body.fe_params.u.ofdm.transmission_mode;
        hfe->u.ofdm.guardInterval		= netmsg->body.fe_params.u.ofdm.guard_interval;
        hfe->u.ofdm.HierarchyInformation	= netmsg->body.fe_params.u.ofdm.hierarchy_information;
        break;
    }
  }

  void set_dvb_frontend_parameters( vtuner_message_t* netmsg, FrontendParameters* hfe, vtuner_type_t type) {
    netmsg->body.fe_params.frequency			= hfe->Frequency;
    netmsg->body.fe_params.inversion			= hfe->Inversion;
    switch(type) {
      case VT_S:
        netmsg->body.fe_params.u.qpsk.symbol_rate		= hfe->u.qpsk.SymbolRate;
        switch(hfe->u.qpsk.FEC_inner) {
          case FEC_NONE: netmsg->body.fe_params.u.qpsk.fec_inner = 0; break;
          case FEC_1_2:  netmsg->body.fe_params.u.qpsk.fec_inner = 1; break;
          case FEC_2_3:  netmsg->body.fe_params.u.qpsk.fec_inner = 2; break;
          case FEC_3_4:  netmsg->body.fe_params.u.qpsk.fec_inner = 3; break;
          case FEC_5_6:  netmsg->body.fe_params.u.qpsk.fec_inner = 5;  break;
          case FEC_7_8:  netmsg->body.fe_params.u.qpsk.fec_inner = 7;  break;
          default:       netmsg->body.fe_params.u.qpsk.fec_inner = FEC_AUTO; break;
        }
        break;
      case VT_C:
        netmsg->body.fe_params.u.qam.symbol_rate		= hfe->u.qam.SymbolRate;
        switch(hfe->u.qam.FEC_inner) {
          case FEC_NONE: netmsg->body.fe_params.u.qam.fec_inner = 0; break;
          case FEC_1_2:  netmsg->body.fe_params.u.qam.fec_inner = 1; break;
          case FEC_2_3:  netmsg->body.fe_params.u.qam.fec_inner = 2; break;
          case FEC_3_4:  netmsg->body.fe_params.u.qam.fec_inner = 3; break;
          case FEC_5_6:  netmsg->body.fe_params.u.qam.fec_inner = 5;  break;
          case FEC_7_8:  netmsg->body.fe_params.u.qam.fec_inner = 7;  break;
          default:       netmsg->body.fe_params.u.qam.fec_inner = FEC_AUTO; break;
        }
        netmsg->body.fe_params.u.qam.modulation		= hfe->u.qam.QAM;
        break;
      case VT_T:
        netmsg->body.fe_params.u.ofdm.bandwidth		= hfe->u.ofdm.bandWidth;
        netmsg->body.fe_params.u.ofdm.code_rate_HP		= hfe->u.ofdm.HP_CodeRate;
        netmsg->body.fe_params.u.ofdm.code_rate_LP		= hfe->u.ofdm.LP_CodeRate;
        netmsg->body.fe_params.u.ofdm.constellation		= hfe->u.ofdm.Constellation;
        netmsg->body.fe_params.u.ofdm.transmission_mode	= hfe->u.ofdm.TransmissionMode;
        netmsg->body.fe_params.u.ofdm.guard_interval	= hfe->u.ofdm.guardInterval;
        netmsg->body.fe_params.u.ofdm.hierarchy_information	= hfe->u.ofdm.HierarchyInformation;
        break;
    }
  }
#else
  void get_dvb_frontend_info( struct dvb_frontend_info* fe_info, vtuner_net_message_t* netmsg) {
    strncpy(fe_info->name, netmsg->u.discover.fe_info.name, sizeof(fe_info->name));
    switch(netmsg->u.discover.fe_info.type) {
      case VT_S: fe_info->type = FE_QPSK; break;
      case VT_C: fe_info->type = FE_QAM; break;
      case VT_T: fe_info->type = FE_OFDM; break;
    }
    fe_info->frequency_min = netmsg->u.discover.fe_info.frequency_min;
    fe_info->frequency_max = netmsg->u.discover.fe_info.frequency_max;
    fe_info->frequency_stepsize = netmsg->u.discover.fe_info.frequency_stepsize;
    fe_info->frequency_tolerance = netmsg->u.discover.fe_info.frequency_tolerance;
    fe_info->symbol_rate_min = netmsg->u.discover.fe_info.symbol_rate_min;
    fe_info->symbol_rate_max = netmsg->u.discover.fe_info.symbol_rate_max;
    fe_info->symbol_rate_tolerance = netmsg->u.discover.fe_info.symbol_rate_tolerance;
    fe_info->notifier_delay = netmsg->u.discover.fe_info.notifier_delay;
    fe_info->caps = netmsg->u.discover.fe_info.caps;
  }

  void set_dvb_frontend_info( vtuner_net_message_t* netmsg, struct dvb_frontend_info* fe_info) {
    strncpy(netmsg->u.discover.fe_info.name, fe_info->name, sizeof(fe_info->name));
    switch(fe_info->type) {
      case FE_QPSK: netmsg->u.discover.fe_info.type = VT_S; break;
      case FE_QAM:  netmsg->u.discover.fe_info.type = VT_C; break;
      case FE_OFDM: netmsg->u.discover.fe_info.type = VT_T; break;
    }
    netmsg->u.discover.fe_info.frequency_min = fe_info->frequency_min;
    netmsg->u.discover.fe_info.frequency_max = fe_info->frequency_max;
    netmsg->u.discover.fe_info.frequency_stepsize = fe_info->frequency_stepsize;
    netmsg->u.discover.fe_info.frequency_tolerance = fe_info->frequency_tolerance;
    netmsg->u.discover.fe_info.symbol_rate_min = fe_info->symbol_rate_min;
    netmsg->u.discover.fe_info.symbol_rate_max = fe_info->symbol_rate_max;
    netmsg->u.discover.fe_info.symbol_rate_tolerance = fe_info->symbol_rate_tolerance;
    netmsg->u.discover.fe_info.notifier_delay = fe_info->notifier_delay;
    netmsg->u.discover.fe_info.caps = fe_info->caps;
  }

  void get_dvb_frontend_parameters(struct dvb_frontend_parameters* hfe, vtuner_message_t* netmsg, vtuner_type_t type) {
    memset(hfe, 0, sizeof(hfe));

    hfe->frequency 		= netmsg->body.fe_params.frequency;
    hfe->inversion		= netmsg->body.fe_params.inversion;
    switch (type) {
      case VT_S:
        hfe->u.qpsk.symbol_rate	= netmsg->body.fe_params.u.qpsk.symbol_rate;
        hfe->u.qpsk.fec_inner	= netmsg->body.fe_params.u.qpsk.fec_inner;
        break;
      case VT_C:
        hfe->u.qam.symbol_rate	= netmsg->body.fe_params.u.qam.symbol_rate;
        hfe->u.qam.fec_inner	= netmsg->body.fe_params.u.qam.fec_inner;
        hfe->u.qam.modulation	= netmsg->body.fe_params.u.qam.modulation;
        break;
      case VT_T:
        hfe->u.ofdm.bandwidth			= netmsg->body.fe_params.u.ofdm.bandwidth;
        hfe->u.ofdm.code_rate_HP		= netmsg->body.fe_params.u.ofdm.code_rate_HP;
        hfe->u.ofdm.code_rate_LP		= netmsg->body.fe_params.u.ofdm.code_rate_LP;
        hfe->u.ofdm.constellation		= netmsg->body.fe_params.u.ofdm.constellation;
        hfe->u.ofdm.transmission_mode		= netmsg->body.fe_params.u.ofdm.transmission_mode;
        hfe->u.ofdm.guard_interval		= netmsg->body.fe_params.u.ofdm.guard_interval;
        hfe->u.ofdm.hierarchy_information	= netmsg->body.fe_params.u.ofdm.hierarchy_information;
    }
  }

  void set_dvb_frontend_parameters( vtuner_message_t* netmsg, struct dvb_frontend_parameters* hfe, vtuner_type_t type) {
    netmsg->body.fe_params.frequency		= hfe->frequency;
    netmsg->body.fe_params.inversion		= hfe->inversion;
    switch (type) {
      case VT_S:
        netmsg->body.fe_params.u.qpsk.symbol_rate = hfe->u.qpsk.symbol_rate;
        netmsg->body.fe_params.u.qpsk.fec_inner   = hfe->u.qpsk.fec_inner;
        break;
      case VT_C:
        netmsg->body.fe_params.u.qam.symbol_rate  = hfe->u.qam.symbol_rate;
        netmsg->body.fe_params.u.qam.fec_inner    = hfe->u.qam.fec_inner;
        netmsg->body.fe_params.u.qam.modulation   = hfe->u.qam.modulation;
        break;
      case VT_T:
        netmsg->body.fe_params.u.ofdm.bandwidth                   = hfe->u.ofdm.bandwidth;
        netmsg->body.fe_params.u.ofdm.code_rate_HP                = hfe->u.ofdm.code_rate_HP;
        netmsg->body.fe_params.u.ofdm.code_rate_LP                = hfe->u.ofdm.code_rate_LP;
        netmsg->body.fe_params.u.ofdm.constellation               = hfe->u.ofdm.constellation;
        netmsg->body.fe_params.u.ofdm.transmission_mode           = hfe->u.ofdm.transmission_mode;
        netmsg->body.fe_params.u.ofdm.guard_interval              = hfe->u.ofdm.guard_interval;
        netmsg->body.fe_params.u.ofdm.hierarchy_information       = hfe->u.ofdm.hierarchy_information;
    }
  }
#endif

int ntoh_get_message_type( vtuner_net_message_t* netmsg ) {
  fprintf(stderr,"ntoh_get_message_type(1): %x\n",netmsg->msg_type);
  int hmsgtype = ntohl(netmsg->msg_type);
  fprintf(stderr,"ntoh_get_message_type(2): %x\n", hmsgtype);
  return hmsgtype;
}

void hton_vtuner_net_message(vtuner_net_message_t* netmsg, vtuner_type_t type) {
  DEBUGNET(" %d %d", netmsg->msg_type, netmsg->u.vtuner.type );

  switch (netmsg->msg_type) {
    case MSG_GET_FRONTEND:
    case MSG_SET_FRONTEND:
      DEBUGNETC(" %d %d %d %d", netmsg->u.vtuner.body.fe_params.frequency, netmsg->u.vtuner.body.fe_params.inversion, netmsg->u.vtuner.body.fe_params.u.qpsk.symbol_rate, netmsg->u.vtuner.body.fe_params.u.qpsk.fec_inner);
      HTONLc(netmsg->u.vtuner.body.fe_params, frequency);
      switch (type) {
        case VT_S:
          DEBUGNETC(" VT_S");
          HTONLc( netmsg->u.vtuner.body.fe_params, u.qpsk.symbol_rate);
          HTONLc( netmsg->u.vtuner.body.fe_params, u.qpsk.fec_inner);
          break;
        case VT_C:
          DEBUGNETC(" VT_C");
          HTONLc( netmsg->u.vtuner.body.fe_params, u.qam.symbol_rate); 
          HTONLc( netmsg->u.vtuner.body.fe_params, u.qam.fec_inner);
          HTONLc( netmsg->u.vtuner.body.fe_params, u.qam.modulation);
          break;
        case VT_T:
          DEBUGNETC(" VT_T");
          HTONLc( netmsg->u.vtuner.body.fe_params, u.ofdm.bandwidth);
          HTONLc( netmsg->u.vtuner.body.fe_params, u.ofdm.code_rate_HP);
          HTONLc( netmsg->u.vtuner.body.fe_params, u.ofdm.code_rate_LP);
          HTONLc( netmsg->u.vtuner.body.fe_params, u.ofdm.constellation);
          HTONLc( netmsg->u.vtuner.body.fe_params, u.ofdm.transmission_mode);
          HTONLc( netmsg->u.vtuner.body.fe_params, u.ofdm.guard_interval);
          HTONLc( netmsg->u.vtuner.body.fe_params, u.ofdm.hierarchy_information);
	  break;
	default:
          WARN("unkown frontend type %d (known types are %d,%d,%d)\n",type,VT_S,VT_C,VT_T);
      };
      break;
    case MSG_READ_STATUS:
      HTONLc( netmsg->u.vtuner.body, status);
      DEBUGNETC(" %d", netmsg->u.vtuner.body.status);
      break;
    case MSG_READ_BER:
      DEBUGNETC(" %d", netmsg->u.vtuner.body.ber);
      HTONLc( netmsg->u.vtuner.body, ber);
      break;
    case MSG_READ_SIGNAL_STRENGTH:
      DEBUGNETC(" %d", netmsg->u.vtuner.body.ss);
      HTONSc( netmsg->u.vtuner.body, ss);
      break;
    case MSG_READ_SNR:
      DEBUGNETC(" %d", netmsg->u.vtuner.body.snr);
      HTONSc( netmsg->u.vtuner.body, snr);
      break;
    case MSG_READ_UCBLOCKS:
      DEBUGNETC(" %d", netmsg->u.vtuner.body.ucb);
      HTONLc( netmsg->u.vtuner.body, ucb);
      break;
    case MSG_PIDLIST: {
      int i;
      for(i=0; i<30; ++i) {
        DEBUGNETC(" %d", netmsg->u.vtuner.body.pidlist[i]);
        HTONSc( netmsg->u.vtuner.body, pidlist[i]);
      }
      break;
    case MSG_DISCOVER:
      DEBUGNETC(" %d %d %d %d", netmsg->u.discover.port, netmsg->u.discover.fe_info.type, netmsg->u.discover.fe_info.frequency_min, netmsg->u.discover.fe_info.frequency_max);
      HTONSc( netmsg->u.discover, port);
      HTONLc( netmsg->u.discover.fe_info, type);
      HTONLc( netmsg->u.discover.fe_info, frequency_min);
      HTONLc( netmsg->u.discover.fe_info, frequency_max);
      HTONLc( netmsg->u.discover.fe_info, frequency_stepsize);
      HTONLc( netmsg->u.discover.fe_info, frequency_tolerance);
      HTONLc( netmsg->u.discover.fe_info, symbol_rate_min);
      HTONLc( netmsg->u.discover.fe_info, symbol_rate_max);
      HTONLc( netmsg->u.discover.fe_info, symbol_rate_tolerance);
      HTONLc( netmsg->u.discover.fe_info, notifier_delay);
      break;    
    }
  }

  HTONLc( netmsg->u.vtuner, type );
  netmsg->msg_type = htonl( netmsg->msg_type );
  
  DEBUGNETC(" %x %x\n", netmsg->msg_type, netmsg->u.vtuner.type);
  #ifdef DEBUG_NET
    print_vtuner_net_message(netmsg);
  #endif
}

void ntoh_vtuner_net_message(vtuner_net_message_t* netmsg, vtuner_type_t type) {
  #ifdef DEBUG_NET
    print_vtuner_net_message(netmsg);
  #endif
  DEBUGNET(" %x %x", netmsg->msg_type, netmsg->u.vtuner.type );

  netmsg->msg_type = htonl( netmsg->msg_type );
  HTONLc( netmsg->u.vtuner, type );

  DEBUGNETC(" %d %d", netmsg->msg_type, netmsg->u.vtuner.type );

  switch (netmsg->msg_type) {
    case MSG_GET_FRONTEND: 
    case MSG_SET_FRONTEND: 
      NTOHLc( netmsg->u.vtuner.body.fe_params, frequency);
      switch (type) {
        case VT_S:
          DEBUGNETC(" VT_S");
          NTOHLc( netmsg->u.vtuner.body.fe_params, u.qpsk.symbol_rate);
          NTOHLc( netmsg->u.vtuner.body.fe_params, u.qpsk.fec_inner);
          break;
        case VT_C:
          DEBUGNETC(" VT_C");
          NTOHLc( netmsg->u.vtuner.body.fe_params, u.qam.symbol_rate);
          NTOHLc( netmsg->u.vtuner.body.fe_params, u.qam.fec_inner);
          NTOHLc( netmsg->u.vtuner.body.fe_params, u.qam.modulation);
          break;
        case VT_T:
          DEBUGNETC(" VT_T");
          NTOHLc( netmsg->u.vtuner.body.fe_params, u.ofdm.bandwidth);
          NTOHLc( netmsg->u.vtuner.body.fe_params, u.ofdm.code_rate_HP);
          NTOHLc( netmsg->u.vtuner.body.fe_params, u.ofdm.code_rate_LP);
          NTOHLc( netmsg->u.vtuner.body.fe_params, u.ofdm.constellation);
          NTOHLc( netmsg->u.vtuner.body.fe_params, u.ofdm.transmission_mode);
          NTOHLc( netmsg->u.vtuner.body.fe_params, u.ofdm.guard_interval);
          NTOHLc( netmsg->u.vtuner.body.fe_params, u.ofdm.hierarchy_information);
          break;
        default:
          WARN("unkown frontend type %d (known types are %d,%d,%d)\n",type,VT_S,VT_C,VT_T);
      }
      DEBUGNETC(" %d %d %d %d", netmsg->u.vtuner.body.fe_params.frequency, netmsg->u.vtuner.body.fe_params.inversion, netmsg->u.vtuner.body.fe_params.u.qpsk.symbol_rate, netmsg->u.vtuner.body.fe_params.u.qpsk.fec_inner);
      break;
    case MSG_READ_STATUS:
      NTOHLc( netmsg->u.vtuner.body, status);
      DEBUGNETC(" %d", netmsg->u.vtuner.body.status);
      break;
    case MSG_READ_BER:
      NTOHLc( netmsg->u.vtuner.body, ber);
      DEBUGNETC(" %d", netmsg->u.vtuner.body.ber);
      break;
    case MSG_READ_SIGNAL_STRENGTH:
      NTOHSc( netmsg->u.vtuner.body, ss);
      DEBUGNETC(" %d", netmsg->u.vtuner.body.ss);
      break;
    case MSG_READ_SNR:
      NTOHSc( netmsg->u.vtuner.body, snr);
      DEBUGNETC(" %d", netmsg->u.vtuner.body.snr);
      break;
    case MSG_READ_UCBLOCKS:
      NTOHLc( netmsg->u.vtuner.body, ucb);
      DEBUGNETC(" %d", netmsg->u.vtuner.body.ucb);
      break;
    case MSG_PIDLIST: {
      int i;
      for(i=0; i<30; ++i) {
        NTOHSc( netmsg->u.vtuner.body, pidlist[i]);
        DEBUGNETC(" %d", netmsg->u.vtuner.body.pidlist[i]);
      }
      break;
    case MSG_DISCOVER:
      NTOHSc( netmsg->u.discover, port);
      NTOHLc( netmsg->u.discover.fe_info, type);
      NTOHLc( netmsg->u.discover.fe_info, frequency_min);
      NTOHLc( netmsg->u.discover.fe_info, frequency_max);
      NTOHLc( netmsg->u.discover.fe_info, frequency_stepsize);
      NTOHLc( netmsg->u.discover.fe_info, frequency_tolerance);
      NTOHLc( netmsg->u.discover.fe_info, symbol_rate_min);
      NTOHLc( netmsg->u.discover.fe_info, symbol_rate_max);
      NTOHLc( netmsg->u.discover.fe_info, symbol_rate_tolerance);
      NTOHLc( netmsg->u.discover.fe_info, notifier_delay);
      DEBUGNETC(" %d %d %d %d", netmsg->u.discover.port, netmsg->u.discover.fe_info.type, netmsg->u.discover.fe_info.frequency_min, netmsg->u.discover.fe_info.frequency_max);
      break;
    }
  }
  DEBUGNETC("\n");
}

void print_vtuner_net_message(vtuner_net_message_t* netmsg) {
  char* bytes;
  int i;
  bytes=(char*)netmsg;
  DEBUGNET(" (%d) ",sizeof(vtuner_net_message_t));
  for(i=0; i<sizeof(vtuner_net_message_t); ++i) {
    DEBUGNETC("%x ", bytes[i]);
  }
  DEBUGNETC("\n");
}
