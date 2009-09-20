#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <pthread.h>

#include "vtunerd-service.h"

#ifdef DEBUG_MAIN
#define DEBUGMAIN(msg, ...) fprintf(stderr,"[%d %s:%u] debug: " msg, getpid(), __FILE__, __LINE__, ## __VA_ARGS__)
#define DEBUGMAINC(msg, ...) fprintf(stderr,msg, ## __VA_ARGS__)
#else
#define DEBUGMAIN(msg, ...)
#define DEBUGMAINC(msg, ...)
#endif

int dbg_level = 2;

int main(int argc, char **argv) {

  int i;
  vtuner_session_t session[MAX_SESSIONS]; 
  pthread_t worker[MAX_SESSIONS], discover;

  for(i=0; i<MAX_SESSIONS; ++i) session[i].status = SST_UNKNOWN;
  int hw_count;
  if(argc == 1) {
    if(hw_init(&session[0].hw, 0, 0, 0, 0) != 0) {  // init adapter0 with frontend0 and demux0
      ERROR("failed to init hardware (adapter 0, frontend 0, demux 0, dvr 0)\n");
      exit(1);
    }
    hw_count = 1;
  } else {
    hw_count=atoi(argv[1]);
    if( hw_count*4 + 2 != argc ) {
      ERROR("Parameter mismatch. %d tuner(s) requires %d arguments, but %d given.\n", hw_count, hw_count*4 + 1, argc-1);
      exit(2);
    }
    DEBUGMAIN("try to init %d tuner(s)\n", hw_count);
    for(i=0;i<hw_count;++i) {
      int a=atoi(argv[i*4+2]);
      int f=atoi(argv[i*4+3]);
      int dm=atoi(argv[i*4+4]);
      int dv=atoi(argv[i*4+5]);
      DEBUGMAIN("init hardware adapter %d, frontend %d, demux %d, dvr %d\n",a,f,dm,dv);
      if(hw_init(&session[i].hw, a, f, dm, dv) != 0) {
        ERROR("failed to init hardware (adapter %d, frontend %d, demux %d, dvr %d)\n",a,f,dm,dv);
        exit(1);
      }
    }
  }

  start_sessions(hw_count, session);
}
