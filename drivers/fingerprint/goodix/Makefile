MODULE_NAME := gf
obj-y := $(MODULE_NAME).o

ifeq ($(TARGET_PRODUCT),Z01C)
$(MODULE_NAME)-objs := gf3208_gf_spi.o \
                       gf3208_platform.o \
                       gf3208_netlink.o
else
$(MODULE_NAME)-objs := gf316m_gf_spi.o \
                       gf316m_platform.o
endif


