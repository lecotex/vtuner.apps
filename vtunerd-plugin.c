#include <lib/base/object.h>
#include <lib/base/ebase.h>
#include <lib/base/init_num.h>
#include <lib/base/init.h>

#include <pthread.h>
#include <syslog.h>

extern "C" {
#include "vtunerd-service.h"
#include "vtuner-utils.h"
}

int dbg_level  = 0x00ff;
int use_syslog = 1;

class evTuner {
	DECLARE_REF(evTuner);
public:
	evTuner();
};

DEFINE_REF(evTuner);

evTuner::evTuner() {
  openlog("vtunerd", LOG_PERROR, LOG_USER);

  #if DVB_API_VERSION >= 5
  INFO("S2API tuning support.\n");
  #endif

}

// eAutoInitPtr<evTuner> init_evTuner(eAutoInitNumbers::service+1, "evTuner");

PyMODINIT_FUNC initvtunerdservice(void) {
        Py_InitModule("vtunerdservice", NULL);
}

