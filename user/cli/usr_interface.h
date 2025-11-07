#ifndef  USR_INTERFACE_H
#define  USR_INTERFACE_H


#include <iostream>
#include <cstdint>
#include <string>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <poll.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>


#define  SYSFS_ATTRIBUTES_PATH                "/sys/devices/platform/nxp_simtemp"

#define  SAMPLE_RECORD_SIZE                     16
#define  POLLING_TIMEOUT_MS                   1000                      


/* Sensor modes */
enum {NORMAL_MODE, NOISY_MODE, RAMP_MODE};
extern const char* sensorModes[3];

enum {FIRST_ARG, SECOND_ARG, THIRD_ARG, FOURTH_ARG};


/* Binary record */
typedef struct simtemp_sample {
    uint64_t timestamp_ns;
    int32_t temp_mC;
    uint32_t flags;
} simtemp_sample;


/* Commnad elements */
typedef struct ParsedCmd_t {
    std::string fileObject;
    std::string moduleOperation;
    std::string valueToSet;
} ParsedCmd_t;



#endif