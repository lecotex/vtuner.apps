ARCH=mipsel
-include ../../Make.config

CC=$(CC-$(ARCH))
CPP=$(CPP-$(ARCH))
CFLAGS=-fpic -DHAVE_DVB_API_VERSION=3 -DHAVE_DREAMBOX_HARDWARE $(DBGFLAGS)
LDFLAGS=-lpthread -lrt
DRIVER=vtuner-dmm-3

all: ../../dist/$(ARCH)/vtunerd.$(ARCH) ../../dist/$(ARCH)/vtunerc.$(ARCH) ../../dist/$(ARCH)/vtunerdservice.so

-include ../../Make.rules

../../dist/$(ARCH)/vtunerc.$(ARCH): ../../vtunerc.c vtuner-network.o vtuner-utils.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o ../../dist/$(ARCH)/vtunerc.$(ARCH) vtuner-network.o vtuner-utils.o ../../vtunerc.c
	
vtuner-dmm-3.o: ../../vtuner-dmm-3.c ../../vtuner-dmm-3.h 
	$(CC) $(CFLAGS) -c -o vtuner-dmm-3.o ../../vtuner-dmm-3.c

vtunerd-plugin.o: ../../vtunerd-plugin.c ../../vtunerd-service.h ../../vtuner-utils.h
	$(CPP) $(CFLAGS) $(PLUGIN_CFLAGS-$(ARCH)) -c -o vtunerd-plugin.o ../../vtunerd-plugin.c

../../dist/$(ARCH)/vtunerdservice.so: vtuner-network.o vtunerd-service.o vtuner-dmm-3.o vtuner-utils.o vtunerd-plugin.o
	$(CC) $(PLUGIN_LDFLAGS-$(ARCH)) $(LDFLAGS) -o ../../dist/$(ARCH)/vtunerdservice.so vtuner-network.o vtunerd-service.o vtuner-dmm-3.o vtuner-utils.o vtunerd-plugin.o
