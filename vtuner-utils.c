#include <stdio.h>
#include <string.h>
#include <syslog.h>
#include <stdarg.h>

#include "vtuner-utils.h"

void write_message(int level, const char* fmt, ... ) {
  if( level & dbg_level  ) {
    va_list ap;
    va_start(ap, fmt);
    if(use_syslog) {
      int priority;
      switch(level) {
        case 0: priority=LOG_ERR; break;
        case 1: priority=LOG_WARNING; break;
        case 2: priority=LOG_INFO; break;
        default: priority=LOG_DEBUG; break;
      }
      vsyslog(priority, fmt, ap);
    } else {
      vfprintf(stderr, fmt, ap);
    }
    va_end(ap);
  } 
}
