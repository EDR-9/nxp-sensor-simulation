# Software Design Description

The device driver for temperature sensor is using the kernel API to perform all low level
operations. Kernel version used is 6.14 and latest one is 6.18 but according to current
documentation, there's no difference with all functions used here. Next a brief description of the code

Structure

- Initialization and exit
  simtemp_init(): registering both driver and device
  simtemp_exit(): unregistering driver and device

- Load and unload
  simtemp_probe(): is called when inserting module, configures sensor, timer, miscdevice and sysfs
  simtemp_remove(): remove al files created in loading, finishes timer and miscdevice

- Producer
  simtemp_gen_values(): generates temperature values according to sensor mode
  simtemp_timer(): set sensor with values from simtemp_gen_values() that will be read bythe consumer
  Stablishes the alert path according to threshold

- Consumer
  simtemp_read(): is the interface for the user app to communicate with kernel and read the sensor data
  simtemp_poll(): continously checks if there's new data or threshold is reached

- File operations
  simtemp_open(): once initial sensor configuration is done, checks if it's exist to get address
  simtemp_release(): for freeing allocated memory and releasing locks when device file is closed

- User app
  There is a cli for device configuration and data visualization. Admits commannds which are specified in
  the same program. From /simtemp run ./user/cli/usr_interface and vailable commans will be displayed.
  Anyway here there are
    - sampling get
    - sampling set [MS]
    - threshold get
    - threshold set [MC]
    - mode get
    - mode set [MODE]
    - stats
    - dev (device reading)

