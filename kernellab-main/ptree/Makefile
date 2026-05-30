KDIR ?= /lib/modules/$(shell uname -r)/build

obj-m := dbfs_ptree.o

.PHONY: all insmod rmmod clean

all :
	$(MAKE) -C $(KDIR) M=$(CURDIR) modules

insmod :
	sudo insmod dbfs_ptree.ko

rmmod :
	sudo rmmod dbfs_ptree

clean :
	$(MAKE) -C $(KDIR) M=$(CURDIR) clean
