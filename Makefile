obj-m += blue_snowball.o

KDIR := /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	rm -f *.o *.ko *.mod.c *.mod *.symvers *.order

install:
	$(MAKE) -C $(KDIR) M=$(PWD) modules_install
	depmod -a

uninstall:
	rm -f /lib/modules/$(shell uname -r)/extra/blue_snowball.ko
	depmod -a

load:
	insmod blue_snowball.ko

unload:
	rmmod blue_snowball || true

reload: unload load

test: all
	./test_suite.sh

.PHONY: all clean install uninstall load unload reload test
