#!/bin/sh
rm -f config.cache
rm -fr autom4te.cache
autoheader
aclocal -I m4
libtoolize -c -f
automake --add-missing --copy --gnu
autoconf
