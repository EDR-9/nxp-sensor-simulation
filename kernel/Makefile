# Defining the kernel module and its dependencies
obj-m := nxp_simtemp.o
nxp_simtemp-objs := buff_type.o

# SS
all:
	$(MAKE) -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

# ff
clean:
	$(MAKE) -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean