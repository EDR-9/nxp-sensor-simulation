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
#include <linux/poll.h>
#include <linux/string.h>
#include <linux/compiler.h>
#include <linux/prandom.h>


/* General constants */

#define  DRIVER_KERNEL_ID                        "nxp_simtemp"
#define  TEMP_BUFFER_SIZE                                  63
#define  TEMP_BUFFER_LOGICAL_TRAVERSAL                   0x00
#define  TEMP_BUFFER_RAW_TRAVERSAL                       0x01

/* Sensor properties default values */

#define  SAMPLING_PERIOD_INIT_MS                        5000u
#define  THRESHOLD_TEMP_INIT_MC                         45000
#define  SCALED_TEMP_UNIT_MC                             1000  

/* Test values for temperature samples calculation */

#define  AMBIENT_TEMP_INIT_MC                            27000
#define  TARGET_TEMP_MC                                -273150
#define  GAIN_FACTOR_TEMP                                  10u
#define  ALERT_TEMP_ENABLED                              0x01u
#define  ALERT_TEMP_DISABLED                             0x00u
#define  THRESHOLD_TEMP_CROSSED                          0x02u
#define  THRESHOLD_TEMP_AWAY                             0x00u
#define  NEW_TEMP_SAMPLE                                 0x01u

//#define FLAG   BIT


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
    __u32 _lastErr;
};


struct SimSensor_t {
    /* Sensor properties */
    struct miscdevice miscdev;
    struct hrtimer _timer;
    ktime_t _period;
    __s32 _temperatureValue;
    __u8  _alertState;
    __u8  _thresholdCrossed;
    /* sysfs controls */
    __u32 sampling_ms;
    __s32 threshold_mC;
    __u8  mode;
    struct SimSensorStats_t stats;
    /* Lock types */
    struct mutex _mutexLock;
    spinlock_t _spinLock;
    /* Buffer to store temp values */
    struct TempBuffer_t tempBuffer; 
    /* Queue for processes in waiting */
    wait_queue_head_t waitQueue;
};



#endif