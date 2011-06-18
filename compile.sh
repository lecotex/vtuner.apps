#!/bin/bash -x

OEPATH=/stuff-1.6/dm500hd
BINUTILS=$OEPATH/build/tmp/cross/mipsel/bin
CC=$BINUTILS/mipsel-oe-linux-gcc
CXX=$BINUTILS/mipsel-oe-linux-g++
OESTGDIR=$OEPATH/build/tmp/staging/mipsel-oe-linux
CFLAGS="-fPIC -g"
PLUGIN_CFLAGS=-I$OESTGDIR/usr/include/python2.6
PLUGIN_LDFLAGS=-shared
DAEMON_LDFLAGS=-lpthread

$CC $CFLAGS $PLUGIN_CFLAGS -I. -c dreamtuner-py.c -o dreamtuner-py.o
# $CC $CFLAGS $LDFLAGS $PLUGIN_LDFLAGS -Wl,-soname,dreamtuner-python.so -o DreamtunerAPI.so dreamtuner-py.o
$CC $CFLAGS $LDFLAGS $PLUGIN_LDFLAGS -o DreamtunerAPI.so dreamtuner-py.o

# copy lib to NFS for dreambox
cp DreamtunerAPI.so ../../../enigma/src/Plugins/Extensions/Dreamtuner/
