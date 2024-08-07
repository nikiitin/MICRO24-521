# gem5 hooks library

GEM5_ROOT = ../../../..

ifndef ARCH
$(error ARCH not set)
endif

$(info Building libm5iface: $$ARCH is [${ARCH}])

# Set ARCH dependent variables
ifeq ($(ARCH),aarch64)
GCC_PREFIX := ${AARCH64_CROSS_GCC_PREFIX}
else ifeq ($(ARCH),x86_64)
GCC_PREFIX := ${X86_CROSS_GCC_PREFIX}
else
$(error ARCH $(ARCH) not supported)
endif

CC := $(GCC_PREFIX)gcc
AR := $(GCC_PREFIX)ar
LD := $(GCC_PREFIX)gcc

$(info CC is $(shell which $(CC)))

CFLAGS += -I$(GEM5_ROOT)/include

# Enable m5 ops instrumentation for gem5
CFLAGS += -DENABLE_M5OPS

# Pass memory regions in /proc/<pid>/maps to simulator
CFLAGS += -DANNOTATE_PROC_MAPS

# Annotate code regions for more precise HTM visualizer
CFLAGS += -DANNOTATE_CODE_REGIONS

# Enable notification of fallback lock acquisition to
# allow a precise profiling of the fallback lock
CFLAGS += -DNOTIFY_FALLBACKLOCK

# Address for memory mapped simulator interface
CFLAGS += -DM5OP_ADDR=0xFFFF0000

CFLAGS += -O3 -Wall -std=c11

CFLAGS += -g

CFLAGS += -fPIC

all: build/$(ARCH)/libm5iface.a build/$(ARCH)/libm5iface.so

libm5iface_HEADERS=\
	m5iface.h 		\
	m5ops_indirect.h 	\
	m5_mmap.h 		\
	annotated_regions.h

libm5iface_OBJS_BASENAMES=\
	m5iface.o 	\
	m5_mmap.o 	\
	m5ops.o 	\
	m5ops_indirect.o

libm5iface_OBJS=$(foreach f,$(libm5iface_OBJS_BASENAMES),build/$(ARCH)/$(f))

build/$(ARCH)/libm5iface.a: $(libm5iface_OBJS) | build/$(ARCH)/
	$(AR) rvs $@ $(libm5iface_OBJS)

build/$(ARCH)/libm5iface.so: $(libm5iface_OBJS) | build/$(ARCH)/
	$(CC) -shared $(CFLAGS) -o $@ $(libm5iface_OBJS)

build/$(ARCH)/%.o: %.c $(libm5iface_HEADERS) | build/$(ARCH)/
	$(CC) $(CFLAGS) -c $< -o $@

build/$(ARCH)/%.o: %.S $(libm5iface_HEADERS) | build/$(ARCH)/
	$(CC) $(CFLAGS) -fPIC -c $< -o $@

m5ops.o: m5ops.S $(shell find asm -type f -name '*.S')

build/$(ARCH)/:
	mkdir -p $@

annotated_regions.h:
	ln -sf $$(realpath --relative-to=. $(GEM5_ROOT)/src/mem/ruby/profiler/annotated_regions.h) $@

.PHONY: clean
clean:
	rm -f $(libm5iface_OBJS) build/$(ARCH)/libm5iface.a build/$(ARCH)/libm5iface.so
	[ -e build/$(ARCH) ] && rmdir --ignore-fail-on-non-empty build/$(ARCH) || true
	[ -e build ] && rmdir --ignore-fail-on-non-empty build/ || true
