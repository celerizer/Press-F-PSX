# The name of the binary to be created.
TARGET = press-f-psx

# The type of the binary to be created - ps-exe is most common.
TYPE = ps-exe

# Setting the minimum version of the C++. C++-20 is the minimum required version by PSYQo.
CXXFLAGS = -std=c++20

CFLAGS += \
  -DPF_AUDIO_FREQUENCY=44100 \
  -DPF_BIG_ENDIAN=0 \
  -DPF_ROMC=0 \
  -DPF_NO_DMA=1 \
  -DPF_NO_DMA_SIZE=10240

CPPFLAGS += $(CFLAGS)

CXXFLAGS += $(CFLAGS)

include libpressf/libpressf.mk

SRCS = $(PRESS_F_SOURCES) \
	main.cpp \

# This will activate the PSYQo library and the rest of the toolchain.
include nugget/psyqo/psyqo.mk
