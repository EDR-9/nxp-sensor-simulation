#ifndef  NXP_SIMTEMP_SYSFS_H
#define  NXP_SIMTEMP_SYSFS_H


#include "nxp_simtemp_main.h"



/* Sensor modes representation */

enum {NORMAL_MODE, NOISY_MODE, RAMP_MODE};
extern const char* sensorModes[3];


/* sysfs attributes declaration */

extern struct device_attribute dev_attr_sampling;
extern struct device_attribute dev_attr_threshold;
extern struct device_attribute dev_attr_mode;
extern struct device_attribute dev_attr_stats;


/* sysfs attribute creation functions declaration */

ssize_t sampling_show( struct device* device, struct device_attribute* attr, char* buffer );
ssize_t sampling_store( struct device* device, struct device_attribute* attr, const char* buffer, size_t count );

ssize_t threshold_show( struct device* device, struct device_attribute* attr, char* buffer );
ssize_t threshold_store( struct device* device, struct device_attribute* attr, const char* buffer, size_t count );

ssize_t mode_show( struct device* device, struct device_attribute* attr, char* buffer );
ssize_t mode_store( struct device* device, struct device_attribute* attr, const char* buffer, size_t count );

ssize_t stats_show( struct device* device, struct device_attribute* attr, char* buffer );



#endif