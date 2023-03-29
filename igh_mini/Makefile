TOOLCHAIN_PATH ?= /home/cheney/nfs/work/hcq0/gcc-arm-8.2-2018.11-x86_64-arm-linux-gnueabihf

CROSS_COMPILE ?= $(TOOLCHAIN_PATH)/bin/arm-linux-gnueabihf-

CC = ${CROSS_COMPILE}gcc
CXX = ${CROSS_COMPILE}g++

CFLAGS = -Wall
CFLAGS += -I./ethercatlib/arm32/include
LDFLAGS = -L./ethercatlib/arm32/lib -lethercat


PKG_CONFIG_SYSROOT_DIR = $(TOOLCHAIN_PATH)/arm-linux-gnueabihf/libc
XENO_CONFIG = $(PKG_CONFIG_SYSROOT_DIR)/usr/xenomai/bin/xeno-config
XENO_CFLAGS = $(shell DESTDIR=${PKG_CONFIG_SYSROOT_DIR} $(XENO_CONFIG) --posix --cflags)
XENO_LDFLAGS = $(shell DESTDIR=${PKG_CONFIG_SYSROOT_DIR} $(XENO_CONFIG) --posix --ldflags)

all:
	${CC} main.c timer.c ${CFLAGS} ${LDFLAGS} -o arm32_ethercat_demo
	${CC} xenomai_main.c timer.c ${CFLAGS} ${LDFLAGS} $(XENO_CFLAGS) $(XENO_LDFLAGS) -o arm32_ethercat_xenomai_demo
	${CC} xenomai_main.c timer.c ${CFLAGS} ${LDFLAGS} -o arm32_ethercat_demo_new

clean:
	rm -rf a.out arm32_ethercat_demo* arm32_ethercat_xenomai_demo
