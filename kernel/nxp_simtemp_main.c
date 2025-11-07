/*********************************************************************************************************************************
Temperature sensor simulation as a kernel device driver allowing user communication

Challenge proposed by NXP Semiconductors

Author: Diego Castillo
*********************************************************************************************************************************/


#include "nxp_simtemp_main.h"
#include "nxp_simtemp_sysfs.h"
#include "buffer_type.h"


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
TIMER AND TEMPERATURE VALUES GENERATION
*********************************************************************************************************************************/

/* Generate temperature values used in each sampling period */
static __s32 simtemp_gen_values( struct SimSensor_t* sensor )
{
    __s32 genTemperature = sensor->_temperatureValue;

    switch ( sensor->mode )
    {
        case NORMAL_MODE:
            genTemperature += get_random_u32_below(4*SCALED_TEMP_UNIT_MC) - (SCALED_TEMP_UNIT_MC);
            break;
        
        case NOISY_MODE:
            genTemperature += get_random_u32_below(20*SCALED_TEMP_UNIT_MC) - (5*SCALED_TEMP_UNIT_MC);
            break;
        
        case RAMP_MODE:
            genTemperature += SCALED_TEMP_UNIT_MC/2;
            if ( genTemperature > sensor->threshold_mC + (5*SCALED_TEMP_UNIT_MC) )
            {
                genTemperature = AMBIENT_TEMP_INIT_MC;
            }
            break;
        
        default:
            /* Takes the ambient as default */
            genTemperature = AMBIENT_TEMP_INIT_MC;
            break;
    }

    return genTemperature;
}


static enum hrtimer_restart simtemp_timer( struct hrtimer* ptrTimer )
{
    /* Getting the struct address for full access to its members */
    struct SimSensor_t* sensor = container_of(ptrTimer, struct SimSensor_t, _timer);
    struct simtemp_sample newSample;
    unsigned long flags = 0;

    /* Update sensor field with lock to protect shared resources */
    spin_lock_irqsave(&sensor->_spinLock, flags);
    sensor->_temperatureValue = simtemp_gen_values(sensor);
    sensor->stats._allSamples++;
    spin_unlock_irqrestore(&sensor->_spinLock, flags);

    newSample.timestamp_ns = ktime_get_real_ns();
    newSample.temp_mC = sensor->_temperatureValue;

    /* Determine threshold crossing */
    if (newSample.temp_mC >= sensor->threshold_mC)
    {
        flags |= THRESHOLD_TEMP_CROSSED;
    }
    else
    {
        flags |= THRESHOLD_TEMP_AWAY;
    }

    flags |= NEW_TEMP_SAMPLE;
    newSample.flags = flags;

    pr_info("sample ts=%llu temp=%d flags=0x%x\n", newSample.timestamp_ns, newSample.temp_mC, newSample.flags);

    /* Writing sensor data to buffer */
    spin_lock_irqsave(&sensor->_spinLock, flags);

    /* Update alert state: changes with threshold crossing */
    if ( (newSample.flags & THRESHOLD_TEMP_CROSSED) && (sensor->_alertState == ALERT_TEMP_DISABLED) )
    {
        sensor->_thresholdCrossed = THRESHOLD_TEMP_CROSSED;
        sensor->stats._allAlerts++;
        sensor->_alertState = ALERT_TEMP_ENABLED;
        pr_info_ratelimited("%s: SENT ALERT ---> temperature = %d mC\n", DRIVER_KERNEL_ID, newSample.temp_mC);
    }

    /* Enqueue sample; check return for overflow */
    if ( rbuffer_enqueue(&sensor->tempBuffer, &newSample))
    {
        /* handle overflow; decide whether to drop oldest or this sample */
        //sensor->stats._overflow++;
        pr_warn("%s: ring buffer overflow\n", DRIVER_KERNEL_ID);
    }

    sensor->stats._allSamples++;

    spin_unlock_irqrestore(&sensor->_spinLock, flags);

    /* New sensor data wake up */
    wake_up_interruptible(&sensor->waitQueue);

    pr_info_ratelimited("%s: temperature = %d mC\n", DRIVER_KERNEL_ID, sensor->_temperatureValue);

    // Restart timer for the next sample
    hrtimer_forward_now(&sensor->_timer, sensor->_period);

    return HRTIMER_RESTART;
}


/* To wake up when a new sample is received or temperature cross the threshold  */
static __poll_t simtemp_poll( struct file* kernelFile, poll_table* wait )
{
    struct SimSensor_t* sensor = kernelFile->private_data;
    __u32 pollingMask = 0x0000;  /* to set bits that will be checked in polling */

    /* Add the waitQueue to processes in wait list */
    poll_wait(kernelFile, &sensor->waitQueue, wait);

    /* Setting mask according to what event will be allowed  */
    spin_lock(&sensor->_spinLock);
    if ( !rbuffer_empty(&sensor->tempBuffer) )
    {
        pollingMask |= (POLLIN | POLLRDNORM);  /* mask: 0x41 = 01000001, to read sensor data */
    }
    spin_unlock(&sensor->_spinLock);
    
    return pollingMask;
}


/*********************************************************************************************************************************
CHARACTER DEVICE
*********************************************************************************************************************************/

/* Kernel-user interface to make device data accesible to user when reads /dev/simtemp */
static ssize_t simtemp_read( struct file* kernelFile, char __user *usrBuffer, size_t usrRequestedBytes, loff_t* filePosition )
{
    if ( !kernelFile->private_data )
    { 
        pr_err("simtemp_read: no private_data\n");
        return -EIO;
    }

    struct SimSensor_t* sensor = kernelFile->private_data;
    struct simtemp_sample sampleToRead;
    unsigned long flags;

    if ( usrRequestedBytes < sizeof(sampleToRead))
    {
        return -EINVAL;
    }

    spin_lock_irqsave(&sensor->_spinLock, flags);

    if ( rbuffer_empty(&sensor->tempBuffer) )
    {
        spin_unlock_irqrestore(&sensor->_spinLock, flags);

        /* Check for file access block. If there's no new sensor data O_NONBLOCK is set */
        if ( kernelFile->f_flags & O_NONBLOCK )
        {
            /* Try again if there's blocking */
            return -EAGAIN;
        }

        /* Check if sleep until buffer is not empty */
        if ( wait_event_interruptible(sensor->waitQueue, (sensor->tempBuffer._head != sensor->tempBuffer._tail)) )
        {
            /* If process is interupted */
            return -ERESTARTSYS;
        }

        spin_lock_irqsave(&sensor->_spinLock, flags);
    }

    /* User gets sensor data */
    rbuffer_dequeue(&sensor->tempBuffer, &sampleToRead);

    spin_unlock_irqrestore(&sensor->_spinLock, flags);

    /* Passing device data at kernel to user variable */
    if ( copy_to_user(usrBuffer, &sampleToRead, sizeof(sampleToRead)) )
    {
        return -EFAULT;
    }

    return sizeof(sampleToRead);
}


static int simtemp_open(struct inode* inode, struct file* kernelFile)
{
    struct miscdevice* miscdev;
    struct SimSensor_t* sensor;
    
    if ( kernelFile->private_data )
    {
        miscdev = kernelFile->private_data;
    }
    else
    {
        pr_warn("%s: in open, file->private_data NULL, trying dev_get_drvdata\n", DRIVER_KERNEL_ID);
        return -ENODEV;
    }
    
    sensor = dev_get_drvdata(miscdev->this_device);
    if ( !sensor )
    {
        pr_err("%s: in open, dev_get_drvdata failed\n", DRIVER_KERNEL_ID);
        return -ENODEV;
    }

    /* Store the sensor address in the private data */
    kernelFile->private_data = sensor;

    if ( !try_module_get(THIS_MODULE) )
    {
        kernelFile->private_data = NULL;
        return -ENODEV;
    }

    return 0;
}


static int simtemp_release(struct inode* inode, struct file* kernelFile)
{
    struct SimSensor_t *sensor = kernelFile->private_data;
    if ( sensor )
    {
        kernelFile->private_data = NULL;
        module_put( THIS_MODULE );
        pr_info("%s: module release, sensor = %p\n", DRIVER_KERNEL_ID, sensor);
    }
    else
    {
        pr_warn("%s: simtemp_release called with NULL private_data\n", DRIVER_KERNEL_ID);
    }

    return 0;
}


/* File Operations */
static const struct file_operations simtempFileOps = {
    .owner = THIS_MODULE,
    .open = simtemp_open,
    .release = simtemp_release,
    .read = simtemp_read,
    .poll = simtemp_poll
};


/*********************************************************************************************************************************
DEVICE DRIVER REGISTERING AND UNREGISTERING
*********************************************************************************************************************************/

/* Called when module is loaded */
static int simtemp_probe( struct platform_device* platformDevice )
{
    struct SimSensor_t* sensor;
    __s32 miscAnswer, samplingSysfs, thresholdSysfs, modeSysfs, statsSysfs;

    pr_info("%s: probe called\n", DRIVER_KERNEL_ID);

    /* Allocate memory for sensor device */
    sensor = devm_kzalloc(&platformDevice->dev, sizeof(struct SimSensor_t), GFP_KERNEL);

    /* Locking initializatio */
    mutex_init(&sensor->_mutexLock);
    spin_lock_init(&sensor->_spinLock);
    
    if ( !sensor )
    {
        return -ENOMEM;
    }
    
    /* Initializations after sensor configuration */
    init_waitqueue_head(&sensor->waitQueue);
    rbuffer_init(&sensor->tempBuffer);
    
    /* Initialization of sensor properties */
    sensor->miscdev.minor = MISC_DYNAMIC_MINOR;
    sensor->miscdev.name = DRIVER_KERNEL_ID;
    sensor->miscdev.fops = &simtempFileOps;
    sensor->miscdev.mode = 0444;
    sensor->miscdev.parent = &platformDevice->dev;

    sensor->sampling_ms = SAMPLING_PERIOD_INIT_MS;
    sensor->_temperatureValue = AMBIENT_TEMP_INIT_MC;
    sensor->threshold_mC = THRESHOLD_TEMP_INIT_MC;
    sensor->_alertState = ALERT_TEMP_DISABLED;
    sensor->_thresholdCrossed = THRESHOLD_TEMP_AWAY;
    sensor->_period = ktime_set(0, sensor->sampling_ms*1000000ULL);  // 1000000 ns = 1 ms

    /* Register the miscdevice after values configured */
    miscAnswer = misc_register(&sensor->miscdev);
    
    if ( miscAnswer )
    {
        pr_err("%s: failed to register misc device\n", DRIVER_KERNEL_ID);
        return miscAnswer;
    }

    /* Set the sensor data to the sensor device driver */
    platform_set_drvdata(platformDevice, sensor);

    /* Timer initialization when device is found */
    hrtimer_init(&sensor->_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
    sensor->_timer.function = simtemp_timer;

    /* Periodic timer */
    hrtimer_start(&sensor->_timer, sensor->_period, HRTIMER_MODE_REL);

    /* Creating sysfs attributes:
        - sampling_ms
        - threshold
        - mode
        - stats
    */
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
    
    pr_info("%s: /dev/%s created. Timer started (%d ms period)", DRIVER_KERNEL_ID, sensor->miscdev.name, SAMPLING_PERIOD_INIT_MS);
    return 0;
}

/* Called when module is unloaded */
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
    
    /* Unregister the miscdevice */
    misc_deregister(&sensor->miscdev);
}

/* Platform driver struct */
static struct platform_driver simtempDriver = {
    .driver = {.name = DRIVER_KERNEL_ID, .of_match_table = simtempOFMatch,},
    .probe = simtemp_probe,
    .remove = simtemp_remove,
};


/*********************************************************************************************************************************
MODULE INITIALIZATION AND EXIT
*********************************************************************************************************************************/

static int __init simtemp_init( void )
{
    int initResult;

    initResult = platform_driver_register(&simtempDriver);
    if ( initResult )
    {
        platform_device_unregister(simtempPlatformDevice);
        return initResult;
    }

    simtempPlatformDevice = platform_device_register_simple(DRIVER_KERNEL_ID, -1, NULL, 0);
    if ( IS_ERR(simtempPlatformDevice) )
    {
        pr_err("%s: failed to register platform device\n", DRIVER_KERNEL_ID);
        return PTR_ERR(simtempPlatformDevice);
    }

    pr_info("%s: module loaded - simulted device created\n", DRIVER_KERNEL_ID);
    return 0;
}

static void __exit simtemp_exit( void )
{
    platform_device_unregister(simtempPlatformDevice);
    platform_driver_unregister(&simtempDriver);
    pr_info("%s: module unloaded\n", DRIVER_KERNEL_ID);
}


module_init(simtemp_init);
module_exit(simtemp_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("DiegoCastillo");
MODULE_DESCRIPTION("Device driver for temperature sensor simulation");