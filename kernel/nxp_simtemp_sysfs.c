/*********************************************************************************************************************************
sysfs attributes implementation

- sampling_ms
- threshold
- mode
- stats

*********************************************************************************************************************************/


#include "nxp_simtemp_sysfs.h"


/* Definition of valid sensor modes */
const char* sensorModes[] = {
    [NORMAL_MODE] = "normal",
    [NOISY_MODE]  = "noisy",
    [RAMP_MODE]   = "ramp"
};


/*********************************************************************************************************************************
SYSFS FOR SAMPLING_MS
*********************************************************************************************************************************/

ssize_t sampling_show( struct device* device, struct device_attribute* attr, char* buffer )
{
    struct SimSensor_t* sensor = dev_get_drvdata(device);
    __u32 currentSampling;

    mutex_lock(&sensor->_mutexLock);
    currentSampling = sensor->sampling_ms;
    mutex_unlock(&sensor->_mutexLock);

    return scnprintf(buffer, PAGE_SIZE, "%u\n", currentSampling);
}

ssize_t sampling_store( struct device* device, struct device_attribute* attr, const char* buffer, size_t count )
{
    struct SimSensor_t* sensor = dev_get_drvdata(device);
    __u32 newSampling;

    if ( kstrtouint(buffer, 10, &newSampling) )
    {
        return -EINVAL;
    }

    /* Sampling bounded between 10 ms and 10 s */
    if ( newSampling < 10 || newSampling > 10000 )
    {
        return -EINVAL;
    }

    mutex_lock(&sensor->_mutexLock);

    sensor->sampling_ms = newSampling;
    sensor->_period = ktime_set(0, sensor->sampling_ms * 1000000ULL);

    /* Restart timer safely */
    hrtimer_cancel(&sensor->_timer);
    hrtimer_start(&sensor->_timer, sensor->_period, HRTIMER_MODE_REL);

    pr_info("%s: new sampling period = %u ms\n", DRIVER_KERNEL_ID, sensor->sampling_ms);

    mutex_unlock(&sensor->_mutexLock);

    return count;
}

DEVICE_ATTR_RW(sampling);


/*********************************************************************************************************************************
SYSFS FOR THRESHOLD_MC
*********************************************************************************************************************************/

ssize_t threshold_show( struct device* device, struct device_attribute* attr, char* buffer )
{
    struct SimSensor_t* sensor = dev_get_drvdata(device);
    __s32 currentThreshold;

    mutex_lock(&sensor->_mutexLock);
    currentThreshold = sensor->threshold_mC;
    mutex_unlock(&sensor->_mutexLock);

    return sprintf(buffer, "%d\n", currentThreshold);
}

ssize_t threshold_store( struct device* device, struct device_attribute* attr, const char* buffer, size_t count )
{
    struct SimSensor_t* sensor = dev_get_drvdata(device);
    __s32 thresholdValue;

    if ( kstrtoint(buffer, 10, &thresholdValue) )
    {
        return -EINVAL;
    }

    /* Temperature range: -50 °C to 100 °C */
    if ( thresholdValue < -50000 || thresholdValue > 100000 )
    {
        return -EINVAL;
    }

    mutex_lock(&sensor->_mutexLock);
    sensor->threshold_mC = thresholdValue;
    pr_info("%s: new threshold set to %d mC\n", DRIVER_KERNEL_ID, sensor->threshold_mC);
    
    mutex_unlock(&sensor->_mutexLock);

    return count;
}

DEVICE_ATTR_RW(threshold);


/*********************************************************************************************************************************
SYSFS FOR MODE
*********************************************************************************************************************************/

ssize_t mode_show( struct device* device, struct device_attribute* attr, char* buffer )
{
    struct SimSensor_t* sensor = dev_get_drvdata(device);
    //return sprintf(buffer, "%s\n", sensorModes[sensor->mode]);
    const char* currentMode;

    mutex_lock(&sensor->_mutexLock);
    currentMode = sensorModes[sensor->mode];
    mutex_unlock(&sensor->_mutexLock);

    return sysfs_emit(buffer, "%s\n", currentMode);
}

ssize_t mode_store( struct device* device, struct device_attribute* attr, const char* buffer, size_t count )
{
    struct SimSensor_t* sensor = dev_get_drvdata(device);

    mutex_lock(&sensor->_mutexLock);

    for ( __u8 mode = 0; mode < (sizeof(sensorModes)/sizeof(sensorModes[0])); mode++ ) {
        if ( sysfs_streq(buffer, sensorModes[mode]) )
        {
            sensor->mode = mode;
            pr_info("%s: sensor is set to %s mode\n", DRIVER_KERNEL_ID, sensorModes[mode]);
            mutex_unlock(&sensor->_mutexLock);

            return count;
        }
    }

    mutex_unlock(&sensor->_mutexLock);
    pr_warn("%s: invalid sensor mode %s\n", DRIVER_KERNEL_ID, buffer);

    return -EINVAL;
}

DEVICE_ATTR_RW(mode);


/*********************************************************************************************************************************
SYSFS FOR STATS (READ ONLY)
*********************************************************************************************************************************/

ssize_t stats_show( struct device* device, struct device_attribute* attr, char* buffer )
{
    struct SimSensor_t* sensor = dev_get_drvdata(device);
    struct SimSensorStats_t stats;

    mutex_lock(&sensor->_mutexLock);
    stats = sensor->stats;
    mutex_unlock(&sensor->_mutexLock);

    return sprintf(buffer, "updates = %llu\t alerts = %llu\t lasterr = %d\n", stats._allSamples, stats._allAlerts, stats._lastErr);
}

DEVICE_ATTR_RO(stats);