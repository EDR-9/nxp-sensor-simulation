# NXP Sensor Simulation

**Instructions to run module**

1. From /simtemp, launch ./scripts/build.sh. It checks if there're binary files, if so, there's nothing to
   build, otherwise, start compilation

2. Next, execute run_demo.sh in the same scripts directory. This script loads the module, reads sysfs attributes 
   default values, modifies those values, reads from device and unloads the module
