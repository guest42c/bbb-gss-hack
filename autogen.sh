#!/bin/sh

autoreconf -i -f
#patch -p0 <patch-configure
./configure $confflags $@

