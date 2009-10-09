ARCH=mipsel
-include ../../Make.config

CC=$(CC-$(ARCH))
CFLAGS=-fpic -DHAVE_DVB_API_VERSION=3 -DHAVE_DREAMBOX_HARDWARE $(DBGFLAGS)
LDFLAGS=-lpthread -lrt
DRIVER=vtuner-dmm-3

all: vtunerd-$(ARCH) vtunerc-$(ARCH)

-include ../../Make.rules

vtunerc-$(ARCH): ../../vtunerc.c vtuner-network.o vtuner-utils.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o ../../dist/$(ARCH)/vtunerc.$(ARCH) vtuner-network.o vtuner-utils.o ../../vtunerc.c
	
vtuner-dmm-3.o: ../../vtuner-dmm-3.c ../../vtuner-dmm-3.h 
	$(CC) $(CFLAGS) -c -o vtuner-dmm-3.o ../../vtuner-dmm-3.c

