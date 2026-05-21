obj-m := dht11_driver.o

SRC := $(shell pwd)

all:
	$(MAKE) -C $(KERNEL_SRC) M=$(SRC)

modules_install:
	$(MAKE) -C $(KERNEL_SRC) M=$(SRC) modules_install

clean: 
	rm -f *.o *- core .depend .*.cmd *.ko *.mod.C
	rm -f Module.markers Module.symvers module.order
	rm -f .tmp_versions Module.symvers
