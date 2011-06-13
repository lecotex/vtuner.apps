## Makefile.am -- Process this file with automake to produce Makefile.in
LIBS += $(PTHREAD_LIBS)
AM_CFLAGS = $(PTHREAD_CFLAGS)

bin_PROGRAMS = vtunerd
vtunerd_SOURCES = vtuner-utils.c vtuner-network.c vtunerd-service.c vtunerd.c    

if DMM2
  AM_CFLAGS += -DHAME_DVB_API_VERSION=2
  vtunerd_SOURCES += vtuner-dmm-2.c
endif

if DMM3
  bin_PROGRAMS += vtunerc
  vtunerd_SOURCES += vtuner-dmm-3.c
  vtunerc_SOURCES = vtuner-utils.c vtuner-network.c vtunerc.c
  AM_CFLAGS += -DHAME_DVB_API_VERSION=3
endif

if DVB
  vtunerd_SOURCES += vtuner-dvb-3.c
  AM_CFLAGS += -DHAME_DVB_API_VERSION=3
endif