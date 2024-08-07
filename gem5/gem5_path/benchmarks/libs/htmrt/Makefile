# htmrt library

ifndef ARCH
$(error ARCH not set)
endif

ifndef HANDLER
$(error HANDLER not set)
endif

$(info Building libhtmrt: $$ARCH is [${ARCH}] and $$HANDLER is [${HANDLER}])

# Set ARCH dependent variables
ifeq ($(ARCH),aarch64)
CFLAGS += -march=armv8-a+tme # TODO: check if this is really necessary with newer versions of GCC. TME may be enabled by default
GCC_PREFIX := ${AARCH64_CROSS_GCC_PREFIX}
else ifeq ($(ARCH),x86_64)
#CFLAGS += -march=… -mtune=…
GCC_PREFIX := ${X86_CROSS_GCC_PREFIX}
else
$(error ARCH $(ARCH) not supported)
endif

CC := $(GCC_PREFIX)gcc
AR := $(GCC_PREFIX)ar
LD := $(GCC_PREFIX)gcc

M5IFACE_PATH=../m5iface

$(info CC is $(shell which $(CC)))

# Set HANDLER dependent variables
ifeq ($(HANDLER),empty)
CFLAGS += -DHANDLER_EMPTY
else ifeq ($(HANDLER),fallbacklock)
CFLAGS += -DHANDLER_FALLBACKLOCK
else ifeq ($(HANDLER),fallbacklock2phase)
CFLAGS += -DHANDLER_FALLBACKLOCK -DHANDLER_FALLBACKLOCK_2PHASE
else ifeq ($(HANDLER),sgl)
CFLAGS += -DHANDLER_SGL
else ifeq ($(HANDLER),powertm)
CFLAGS += -DHANDLER_FALLBACKLOCK -DHANDLER_POWERTM
else ifeq ($(HANDLER),powertmplus)
CFLAGS += -DHANDLER_FALLBACKLOCK -DHANDLER_POWERTM -DHANDLER_POWERTM_PRECISEABORTS
else
$(error HANDLER $(HANDLER) not supported)
endif

# Pass fallback lock address to simulator (clasify aborts)
CFLAGS += -DANNOTATE_FALLBACKLOCK_ADDR

# Annotate code regions for more precise HTM visualizer
CFLAGS += -DANNOTATE_CODE_REGIONS

# Enable notification of fallback lock acquisition to
# allow a precise profiling of the fallback lock
CFLAGS += -DANNOTATE_FALLBACKLOCK

CFLAGS += -I$(M5IFACE_PATH)

CFLAGS += -O3 -Wall -fgnu-tm -std=c11

CFLAGS += -g

# Needed to build the dynamic library version
CFLAGS += -fPIC

all: build/$(ARCH)/libhtmrt-$(HANDLER).a # build/$(ARCH)/libhtmrt-$(HANDLER).so

libhtmrt-$(HANDLER)_HEADERS=\
	htmrt.h \
	htmrt-arch.h \
	abort_codes.h \
	env_globals.h \
	logtm.h \
	mt19937ar_1.h \
	spinlock.h \
	thread_context.h \
	util.h \
	xbeginFlags.h

libhtmrt-$(HANDLER)_OBJS_BASENAMES=\
	htmrt.o \
	thread_context.o \
	util.o \
	env_globals.o \
	spinlock.o \
	logtm.o \
	mt19937ar_1.o

libhtmrt-$(HANDLER)_OBJS=$(foreach f,$(libhtmrt-$(HANDLER)_OBJS_BASENAMES),build/$(ARCH)/$(f:.o=-$(HANDLER).o))

build/$(ARCH)/libhtmrt-$(HANDLER).a: $(libhtmrt-$(HANDLER)_OBJS) | build/$(ARCH)/
	$(AR) rvs $@ $(libhtmrt-$(HANDLER)_OBJS)

build/$(ARCH)/%-$(HANDLER).o: %.c $(libhtmrt-$(HANDLER)_HEADERS) | build/$(ARCH)/
	$(CC) $(CFLAGS) -c $< -o $@

build/$(ARCH)/:
	mkdir -p $@

.PHONY: clean
clean:
	rm -f $(libhtmrt-$(HANDLER)_OBJS) build/$(ARCH)/libhtmrt-$(HANDLER).a build/$(ARCH)/libhtmrt-$(HANDLER).so
	[ -e build/$(ARCH) ] && rmdir --ignore-fail-on-non-empty build/$(ARCH) || true
	[ -e build ] && rmdir --ignore-fail-on-non-empty build/ || true
