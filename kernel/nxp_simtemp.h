#ifndef  NXP_SIMTEMP_H
#define  NXP_SIMTEMP_H


#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/types.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/mutex.h>
#include <linux/spinlock.h>
#include <linux/device.h>
#include <linux/sysfs.h>
#include <linux/wait.h>
//#include "buff_type.h"


/* General constants */

#define  DRIVER_KERNEL_ID                        "nxp_simtemp"
#define  TEMP_BUFFER_SIZE                                  64
#define  TEMP_BUFFER_LOGICAL_TRAVERSAL                   0x00
#define  TEMP_BUFFER_RAW_TRAVERSAL                       0x01

/* Sensor properties default values */

#define  SAMPLING_PERIOD_INIT_MS                         500u
#define  THRESHOLD_TEMP_INIT_MC                         45000
#define  SCALED_TEMP_UNIT_MC                             1000  

/* Test values for temperature samples calculation */

#define  AMBIENT_TEMP_INIT_MC                            27000
#define  TARGET_TEMP_MC                                  15000
#define  GAIN_FACTOR_TEMP                                  10u
#define  ALERT_TEMP_ENABLED                              0x01u
#define  ALERT_TEMP_DISABLED                             0x00u
#define  THRESHOLD_TEMP_REACHED             ALERT_TEMP_ENABLED
#define  THRESHOLD_TEMP_DIS                ALERT_TEMP_DISABLED


/* Sensor modes representation */

enum {NORMAL_MODE, NOISY_MODE, RAMP_MODE};
const char* sensorModes[] = {
    [NORMAL_MODE] = "normal",
    [NOISY_MODE]  = "noisy",
    [RAMP_MODE]   = "ramp"
};


/* Objects definition for sensor simulation */

struct simtemp_sample {
    __u64 timestamp_ns;
    __s32 temp_mC;
    __u32 flags;
};


/* Ring buffer type */
struct TempBuffer_t {
    struct simtemp_sample _samples[TEMP_BUFFER_SIZE];
    __u8 _head;
    __u8 _tail;
};


struct SimSensorStats_t {
    __u64 _allSamples;
    __u64 _allAlerts;
    __s32 _lastErr;
};


struct SimSensor_t {
    /* Sensor properties */
    struct hrtimer _timer;
    ktime_t _period;
    __s32 _temperatureValue;
    __u8  _alertState;
    __u8  _thresholdReached;
    /* sysfs controls */
    __u32 sampling_ms;
    __s32 threshold_mC;
    __s8  mode;
    struct SimSensorStats_t stats;
    /* Lock types */
    struct mutex _mutexLock;
    spinlock_t _spinLock;
    /* Buffer to store temp values */
    struct TempBuffer_t tempBuffer; 
    /* Queue for polling */
    wait_queue_head_t waitQueue;
};


/* Function declarations */



void rbuffer_init( struct TempBuffer_t* buffer );
__u8 rbuffer_empty( struct TempBuffer_t* buffer );
__u8 rbuffer_full( struct TempBuffer_t* buffer );
void rbuffer_enqueue( struct TempBuffer_t* buffer, struct simtemp_sample newTempSample );
void rbuffer_dequeue( struct TempBuffer_t* buffer, struct simtemp_sample* extUserRead );
void rbuffer_show( struct TempBuffer_t* buffer, __u8 traversalType );



#endif