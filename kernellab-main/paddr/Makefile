KDIR ?= /lib/modules/$(shell uname -r)/build

obj-m := dbfs_paddr.o

.PHONY: all insmod rmmod test clean

all :
	$(MAKE) -C $(KDIR) M=$(CURDIR) modules
	gcc -Wall -o app app.c

insmod :
	sudo insmod dbfs_paddr.ko

rmmod :
	sudo rmmod dbfs_paddr

test : all
	sudo ./app

clean :
	$(MAKE) -C $(KDIR) M=$(CURDIR) clean
	rm -f app
