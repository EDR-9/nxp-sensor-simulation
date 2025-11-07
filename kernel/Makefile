# Defining the kernel module to create and its dependencies
obj-m := nxp_simtemp.o
nxp_simtemp-objs := nxp_simtemp_main.o nxp_simtemp_sysfs.o buffer_type.o

# Compilation rule
all:
	$(MAKE) -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

# Compilation files cleaning rule
clean:
	$(MAKE) -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean