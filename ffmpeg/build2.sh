#!/bin/sh
export CFLAGS="-noixemul -O4"
export LDFLAGS="-noixemul"
./configure --cc=/gg/bin/ppc-morphos-gcc-4.4.5 --ld=/gg/bin/ppc-morphos-gcc-4.4.5 --enable-cross-compile --cross-prefix=/gg/bin/ --arch=powerpc --cpu=powerpc --target-os=linux --enable-runtime-cpudetect --enable-memalign-hack --disable-pthreads --disable-shared --disable-indevs
