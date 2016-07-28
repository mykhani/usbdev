ifneq ($(KERNELRELEASE),)
	obj-m := tivausbdev.o
else
	KDIR := /lib/modules/`uname -r`/build
all:
	$(MAKE) -C $(KDIR) M=$$PWD
endif

