ifeq ($(KERNELRELEASE),)
KERNEL_DIR ?= /home/horizon/work/junzheng/Ingenic-SDK-3.10.1-20170310/opensource/kernel
PWD := $(shell pwd)

modules:
	$(MAKE) -C $(KERNEL_DIR) M=$(PWD) modules

.PHONY:modules clean
clean:
	$(MAKE) -C $(KERNEL_DIR) M=$(PWD) clean
else
	obj-m := rb_dev.o
endif
