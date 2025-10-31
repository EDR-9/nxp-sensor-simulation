/*********************************************************************************************************************************
Temperature sensor simulation as a kernel device driver allowing user communication
Challenge proposed by NXP Semiconductors

Author: Diego Castillo
*********************************************************************************************************************************/


#include "nxp_simtemp.h"


/*********************************************************************************************************************************
KERNEL MODULE CONFIG DEVICE DRIVERS STRUCTURES INIT
*********************************************************************************************************************************/

/* Device Tree match table */
static const struct of_device_id simtempOFMatch[] =  {
    {.compatible = "nxp,simtemp"},
    {}
};

static struct platform_device* simtempPlatformDevice;

MODULE_DEVICE_TABLE(of, simtempOFMatch);



/*********************************************************************************************************************************
TIMER AND TEMPERATURE SAMPLES GENERATION
*********************************************************************************************************************************/

// Temperature calculation variables
static __s32 currentTemperature = AMBIENT_TEMP_INIT_MC;
//static __u32 samplingPeriod = SENSOR_SAMPLING_PERIOD_MS;
//static struct hrtimer temperatureTimer;
//static spinlock_t temperatureLock;
//static __u32 gain = 10;
//static __s32 desiredTemperature = TEST_TARGET_TEMP;

// Generate temperature measurement used in each sampling period
static void simtemp_gen_values( struct SimSensor_t* sensor )
{
    //__s32 temperatureDifference = ((TARGET_TEMP_MC - currentTemperature)* GAIN_FACTOR_TEMP)/1000;

    if ( (currentTemperature < sensor->threshold_mC) && !sensor->_thresholdReached )
    {
        currentTemperature += SCALED_TEMP_UNIT_MC;  //temperatureDifference;   
    }
    else
    {
        sensor->_thresholdReached = ALERT_TEMP_ENABLED;   
        currentTemperature -= SCALED_TEMP_UNIT_MC;

        if ( currentTemperature <= SCALED_TEMP_UNIT_MC )
        {
            sensor->_thresholdReached = ALERT_TEMP_DISABLED;
            sensor->_alertState = ALERT_TEMP_DISABLED;
        }
    }

    //return currentTemperature;
}

static enum hrtimer_restart simtemp_timer( struct hrtimer* ptrTimer )
{
    // Getting the struct address for full access to its members
    struct SimSensor_t* sensor = (struct SimSensor_t*)ptrTimer;
    unsigned long flags;
    simtemp_gen_values(sensor);
    
    // Locking
    spin_lock_irqsave(&sensor->_spinLock, flags);
    sensor->_temperatureValue = currentTemperature;  //temperatureVariation;
    sensor->stats._allSamples++;
    //sensor->stats._lasterr = currentTemperature;  //temperatureVariation;

    if ( currentTemperature >= sensor->threshold_mC )
    {
        if ( sensor->_alertState == ALERT_TEMP_DISABLED )
        {
            sensor->stats._allAlerts++;
            sensor->stats._lastErr = currentTemperature;
            sensor->_alertState = ALERT_TEMP_ENABLED;
            pr_info("%s: SENT ALERT ---> temperature: %d mC\n", DRIVER_KERNEL_ID, currentTemperature);
        }
        else
        {
            pr_info("%s: alert previously sent\n", DRIVER_KERNEL_ID);
        }
    }

    spin_unlock_irqrestore(&sensor->_spinLock, flags);

    pr_info("%s: temperature = %d mC\n", DRIVER_KERNEL_ID, sensor->_temperatureValue);

    // Restart timer for the next sample
    hrtimer_forward_now(&sensor->_timer, sensor->_period);

    //wake_up_interrutible("");

    return HRTIMER_RESTART;
}

/* -------------------------- Character Device Implementation ---------------------------- */

// When user reads /dev/simtemp
static ssize_t simtemp_read( struct file* kernelFile, char* __user usrBuffer, size_t size, loff_t* ppos )
{
    char tmp[32];
    __u16 bytesToRead;

    bytesToRead = snprintf(tmp, sizeof tmp, "%d\n", currentTemperature);

    if ( *ppos > 0 )
    {
        return 0;
    }
    if ( copy_to_user(usrBuffer, tmp, bytesToRead) )
    {
        return -EFAULT;
    }
    
    *ppos += bytesToRead;
    return bytesToRead;
}

// File Operations
static const struct file_operations simtempFileOps = {
    .owner = THIS_MODULE,
    .read = simtemp_read
};

// Misc Device
static struct miscdevice simtempMiscDevice = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = "simtemp",
    .fops = &simtempFileOps,
    .mode = 0444
};



/*********************************************************************************************************************************
SYSFS FOR SAMPLING_MS
*********************************************************************************************************************************/

static ssize_t sampling_show( struct device* device, struct device_attribute* attr, char* buffer )
{
    struct SimSensor_t* sensor = dev_get_drvdata(device);
    return scnprintf(buffer, PAGE_SIZE, "%u\n", sensor->sampling_ms);
}

static ssize_t sampling_store( struct device* device, struct device_attribute* attr, const char* buffer, size_t count )
{
    struct SimSensor_t* sensor = dev_get_drvdata(device);
    __u32 val;
    __s32 ret;

    ret = kstrtouint(buffer, 10, &val);
    if (ret)
    {
        return ret;
    }

    if (val < 10 || val > 10000)  // 10 ms to 10 sec
    {
        return -EINVAL;
    }

    mutex_lock(&sensor->_mutexLock);

    sensor->sampling_ms = val;
    sensor->_period = ktime_set(0, sensor->sampling_ms * 1000000ULL);

    /* restart timer safely */
    hrtimer_cancel(&sensor->_timer);
    hrtimer_start(&sensor->_timer, sensor->_period, HRTIMER_MODE_REL);

    mutex_unlock(&sensor->_mutexLock);

    pr_info("%s: new sampling period = %u ms\n", DRIVER_KERNEL_ID, sensor->sampling_ms);
    return count;
}

static DEVICE_ATTR_RW(sampling);


/*********************************************************************************************************************************
SYSFS FOR THRESHOLD_MC
*********************************************************************************************************************************/

static ssize_t threshold_show( struct device* device, struct device_attribute* attr, char* buffer )
{
    struct SimSensor_t* sensor = dev_get_drvdata(device);
    return sprintf(buffer, "%d\n", sensor->threshold_mC);
}

static ssize_t threshold_store( struct device* device, struct device_attribute* attr, const char* buffer, size_t count )
{
    struct SimSensor_t* sensor = dev_get_drvdata(device);
    __s32 thresholdValue;
    if ( kstrtoint(buffer, 0, &thresholdValue) )
    {
        return -EINVAL;
    }

    /* Temperature range: -50 °C to 100 °C */
    if ( thresholdValue < -50000 || thresholdValue > 100000 )
    {
        return -EINVAL;
    }
    

    sensor->threshold_mC = thresholdValue;
    return count;
}

static DEVICE_ATTR_RW(threshold);


/*********************************************************************************************************************************
SYSFS FOR MODE
*********************************************************************************************************************************/

static ssize_t mode_show( struct device* device, struct device_attribute* attr, char* buffer )
{
    struct SimSensor_t* sensor = dev_get_drvdata(device);
    return sprintf(buffer, "%s\n", sensorModes[sensor->mode]);
}

static ssize_t mode_store( struct device* device, struct device_attribute* attr, const char* buffer, size_t count )
{
    struct SimSensor_t* sensor = dev_get_drvdata(device);

    for ( __u8 i = 0; i < (sizeof(sensorModes)/sizeof(sensorModes[0])); i++ ) {
        if ( sysfs_streq(buffer, sensorModes[i]) ) {
            sensor->mode = i;
            return count;
        }
    }

    return -EINVAL;
}

static DEVICE_ATTR_RW(mode);


/*********************************************************************************************************************************
SYSFS FOR STATS (READ ONLY)
*********************************************************************************************************************************/

static ssize_t stats_show( struct device* device, struct device_attribute* attr, char* buffer )
{
    struct SimSensor_t* sensor = dev_get_drvdata(device);
    struct SimSensorStats_t stats;

    // copy atomically under lock
    spin_lock_irq(&sensor->_spinLock);
    stats = sensor->stats;
    spin_unlock_irq(&sensor->_spinLock);

    return sprintf(buffer, "updates = %llu\t alerts = %llu\t lasterr = %d\n", stats._allSamples, stats._allAlerts, stats._lastErr);
}

static DEVICE_ATTR_RO(stats);


/*********************************************************************************************************************************
DEVICE DRIVER LOADING AND UNLOADING
*********************************************************************************************************************************/

static int simtemp_probe( struct platform_device* platformDevice )
{
    struct SimSensor_t* sensor;
    __s32 miscAnswer, samplingSysfs, thresholdSysfs, modeSysfs, statsSysfs;

    pr_info("%s: probe called\n", DRIVER_KERNEL_ID);

    sensor = devm_kzalloc(&platformDevice->dev, sizeof(*sensor), GFP_KERNEL);
    miscAnswer = misc_register(&simtempMiscDevice);

    if ( !sensor )
    {
        return -ENOMEM;
    }

    if ( miscAnswer )
    {
        pr_err("%s: failed to register misc device\n", DRIVER_KERNEL_ID);
        return miscAnswer;
    }

    /* Set the sensor data to the sensor device driver */
    platform_set_drvdata(platformDevice, sensor);
    mutex_init(&sensor->_mutexLock);

    /* Initialization of sensor properties */
    sensor->sampling_ms = SAMPLING_PERIOD_INIT_MS;
    sensor->_temperatureValue = AMBIENT_TEMP_INIT_MC;
    sensor->threshold_mC = THRESHOLD_TEMP_INIT_MC;
    sensor->_alertState = ALERT_TEMP_DISABLED;
    sensor->_thresholdReached = ALERT_TEMP_DISABLED;
    sensor->_period = ktime_set(0, sensor->sampling_ms*1000000ULL);  // 1000000 ns = 1 ms

    /* Timer initialization when device is found */
    hrtimer_init(&sensor->_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
    sensor->_timer.function = simtemp_timer;

    /* Periodic timer */
    hrtimer_start(&sensor->_timer, sensor->_period, HRTIMER_MODE_REL);

    /* Creating sysfs attributes */
    samplingSysfs = device_create_file(&platformDevice->dev, &dev_attr_sampling);
    if ( samplingSysfs )
    {
        pr_err("sampling_ms sysfs attribute cannot be created\n");
    }

    thresholdSysfs = device_create_file(&platformDevice->dev, &dev_attr_threshold);
    if ( thresholdSysfs )
    {
        pr_err("threshold_mC sysfs attribute cannot be created\n");
    }

    modeSysfs = device_create_file(&platformDevice->dev, &dev_attr_mode);
    if ( modeSysfs )
    {
        pr_err("mode sysfs attribute cannot be created\n");
    }

    statsSysfs = device_create_file(&platformDevice->dev, &dev_attr_stats);
    if ( statsSysfs )
    {
        pr_err("stats sysfs attribute cannot be created\n");
    }
    
    pr_info("%s: /dev/%s created. Timer started (%d ms period)", DRIVER_KERNEL_ID, simtempMiscDevice.name, SAMPLING_PERIOD_INIT_MS);
    return 0;
}

/* remove: called on module unload */
static void simtemp_remove( struct platform_device* platformDevice )
{
    /* Access to device driver data */
    struct SimSensor_t* sensor = platform_get_drvdata(platformDevice);

    /* Removing sysfs attribute files */
    device_remove_file(&platformDevice->dev, &dev_attr_sampling);
    device_remove_file(&platformDevice->dev, &dev_attr_threshold);
    device_remove_file(&platformDevice->dev, &dev_attr_mode);
    device_remove_file(&platformDevice->dev, &dev_attr_stats);

    if ( sensor )
    {
        hrtimer_cancel(&sensor->_timer);
        pr_info("%s: timer stopped\n", DRIVER_KERNEL_ID);
    }

    pr_info("%s: remove called\n", DRIVER_KERNEL_ID);
    
    misc_deregister(&simtempMiscDevice);
}

/* Platform driver struct */
static struct platform_driver simtempDriver = {
    .driver = {.name = DRIVER_KERNEL_ID, .of_match_table = simtempOFMatch,},
    .probe = simtemp_probe,
    .remove = simtemp_remove,
};

/* ---------------------- Module Initialization and Exit ------------------------------- */

static int __init simtemp_init( void )
{
    int initResult;

    simtempPlatformDevice = platform_device_register_simple(DRIVER_KERNEL_ID, -1, NULL, 0);
    if ( IS_ERR(simtempPlatformDevice) )
    {
        pr_err("%s: failed to register platform device\n", DRIVER_KERNEL_ID);
        return PTR_ERR(simtempPlatformDevice);
    }
    
    initResult = platform_driver_register(&simtempDriver);
    if ( initResult )
    {
        platform_device_unregister(simtempPlatformDevice);
        return initResult;
    }
    
    pr_info("%s: module loaded - simulted device created\n", DRIVER_KERNEL_ID);
    return 0;
}

static void __exit simtemp_exit( void )
{
    platform_driver_unregister(&simtempDriver);
    platform_device_unregister(simtempPlatformDevice);
    pr_info("%s: module unloaded\n", DRIVER_KERNEL_ID);
}



module_init(simtemp_init);
module_exit(simtemp_exit);

/* Register/unregister driver */
// module_platform_driver(simtempDriver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("DiegoCastillo");
MODULE_DESCRIPTION("Device driver for simulated temperature sensor");
