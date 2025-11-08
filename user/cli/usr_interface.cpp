/*********************************************************************************************************************************
Command line interface application to config and read the nxp_simtemp kernel module

Author: Diego Castillo
*********************************************************************************************************************************/



#include "usr_interface.h"



const char* sensorModes[] = {
    [NORMAL_MODE] = "normal",
    [NOISY_MODE] = "noisy",
    [RAMP_MODE] = "ramp"
};



bool sysfs_read( const std::string& attributePath, std::string& rvalue )
{
    std::ifstream attributeFile(attributePath);

    if ( !attributeFile.is_open() )
    {
        std::cout << "\n  File not available: " << attributePath << std::endl;
        return false;
    }

    std::getline(attributeFile, rvalue);
    attributeFile.close();

    return true;
}


bool sysfs_write( const std::string& attributePath, const std::string& wvalue )
{
    std::ofstream attributeFile(attributePath);

    if ( !attributeFile.is_open() )
    {
        std::cout << "\n  File not available: " << attributePath << std::endl;
        return false;
    }

    attributeFile << wvalue;
    attributeFile.close();

    return true;
}


void device_polling(  )
{
    simtemp_sample sample;
    const char* device = "/dev/nxp_simtemp";
    int fileId = open(device, O_RDONLY | O_NONBLOCK);

    if ( fileId < 0 )
    {
        std::cerr << "Failed to open device: " << device << std::endl;
    }

    struct pollfd fileIdPoll = {fileId, POLLIN, 0};

    std::cout << "Polling " << device << " for sensor data...\n" << std::endl;

    while ( true )
    {
        int pollEvents = poll(&fileIdPoll, 1, POLLING_TIMEOUT_MS); // 1 second poll timeout

        if ( pollEvents < 0 )
        {
            std::cerr << "Poll error: " << std::endl;
            break;
        }
        
        else if ( pollEvents == 0 )
        {
            std::cout << "" << std::flush; // keep alive indicator
            continue;
        }

        else
        {
            if ( fileIdPoll.revents & POLLIN )
            {
                ssize_t bytesRead = read(fileId, &sample, sizeof(sample));
                if ( bytesRead > 0 )
                {
                    time_t timeFormat = time(0);
                    char* currentTime = ctime(&timeFormat);
                    currentTime[strlen(currentTime) - 1] = '\0'; // removing line feed

                    std::cout  << currentTime
                               << "\ttemp = " << std::fixed << std::setprecision(2) << sample.temp_mC/1000.0 << " C"
                               << " \talert = " << ((sample.flags & 0x02) ? "1" : "0") << std::endl;
                }

                else
                {
                    std::cerr << "Read error: " << std::endl;
                    break;
                }
            }
        }
    }

    close(fileId);
}


bool strtoint( std::string& string )
{
    if ( string.empty() )
    {
        return false;
    }

    ssize_t strLenght = string.size();
    uint8_t startPosition = 0, digitCount = 0;

    if ( string[0] == '-' )
    {
        startPosition = 1;
    }

    for (size_t i = startPosition; i < strLenght; i++)
        if ( std::isdigit((string[i])))
        {
            digitCount++;
        }

    if ( startPosition )
    {
        if ( digitCount != strLenght - 1 )
        {
            return false;
        }
    }
    else
    {
        if ( digitCount != strLenght )
        {
            return false;
        }
        
    }

    return true;
}



int main( int argc, char** argv )
{
    ParsedCmd_t cmd;
    time_t timeFormat = time(0);
    char* currentTime = ctime(&timeFormat);

    std::cout << "\nsimtemp kernel module interface" << "\t\t\tDate: " << currentTime;
    
    /* Checking input command */
    if ( argc > 1 )
    {   
        /* Command elements initialization. Second arg is always the sysfs attribute */
        cmd.fileObject = argv[SECOND_ARG];
        cmd.moduleOperation = "";
        cmd.valueToSet = "";

        std::string fileObjectPath = std::string(SYSFS_ATTRIBUTES_PATH) + '/' + cmd.fileObject;
        //std::cout << fileObjectPath << std::endl;

        if ( (cmd.fileObject == "sampling" || cmd.fileObject == "threshold" || cmd.fileObject == "mode") && argv[THIRD_ARG] != nullptr )
        {
            cmd.moduleOperation = argv[THIRD_ARG];

            if ( cmd.moduleOperation == "get" )
            {
                std::string readValue;
                if ( argv[FOURTH_ARG] != nullptr )
                {
                    std::cout << "\n  Invalid get command\n\n";
                    return 1;
                }
                
                /* Reading the kernel module data */
                std::cout << "\n  Get " << cmd.fileObject << " data\n";
                if ( sysfs_read(fileObjectPath, readValue) )
                {
                    std::cout << "  Current " << cmd.fileObject << " value: " << readValue << std::endl;
                }
                else
                {
                    std::cout << "\n  Failed to read " << cmd.fileObject << " attribute\n";
                }
            }

            else if ( cmd.moduleOperation == "set" )
            {
                if ( argv[FOURTH_ARG] != nullptr )
                {
                    std::string tmparg = argv[FOURTH_ARG];
                    if ( cmd.fileObject == "sampling" || cmd.fileObject == "threshold" )
                    {
                        if ( strtoint(tmparg) ) 
                        //try
                        {
                            {
                                //std::string writeValue;
                                cmd.valueToSet = argv[FOURTH_ARG];
                                std::cout << "\n  Set " <<  cmd.fileObject << " = " << argv[FOURTH_ARG] << std::endl;

                                /* Writing to sysfs attribute files */
                                if ( sysfs_write(fileObjectPath, cmd.valueToSet) )
                                {
                                    std::cout << "  Set " << cmd.fileObject << " to: " << cmd.valueToSet << std::endl;
                                }
                                else
                                {
                                    std::cout << "\n  Failed to write " << cmd.fileObject << " attribute\n";
                                }
                            }
                        }
                        else //catch ( std::invalid_argument )
                        {
                            std::cout << "\n  Invalid value for numeric variables\n\n";
                            return 1;
                        }
                    }
                    else if ( cmd.fileObject == "mode" )
                    {
                        if ( tmparg == sensorModes[NORMAL_MODE] || tmparg == sensorModes[NOISY_MODE] || tmparg == sensorModes[RAMP_MODE] )
                        {
                            cmd.valueToSet = argv[FOURTH_ARG];
                            std::cout << "\n  Set " << cmd.fileObject << " = " << cmd.valueToSet << std::endl;
                            /* Writing to sysfs attribute files */
                            if ( sysfs_write(fileObjectPath, cmd.valueToSet) )
                            {
                                std::cout << "  Set " << cmd.fileObject << " to: " << cmd.valueToSet << std::endl;
                            }
                            else
                            {
                                std::cout << "\n  Failed to write " << cmd.fileObject << " attribute\n";
                            }
                        }
                        else
                        {
                            std::cout << "\n  Invalid sensor mode argument\n\n";
                            return 1;
                        }
                    }
                }

                else
                {
                    std::cout << "\n  Invalid set value argument\n\n";
                    return 1;
                }
            }

            else
            {
                std::cout << "\n  Invalid module operation argument\n\n";
                return 1;
            }            
        }

        else if ( cmd.fileObject == "stats" && argv[THIRD_ARG] != nullptr )
        {
            std::cout << "\n  Invalid command\n\n";
            return 1;
        }

        else if ( cmd.fileObject == "stats" )
        {
            std::string stats;
            std::cout << "\n  Kernel module stats reading\n";

            if ( sysfs_read(fileObjectPath, stats) )
            {
                std::cout << "  " << cmd.fileObject << ":\t" << stats << std::endl;
            }
            else
            {
                std::cout << "\n  Failed to read " << cmd.fileObject << std::endl;
            }
        }

        else if ( cmd.fileObject == "dev" )
        {
            /* Reading continously the /dev/nxp_temp node */
            device_polling();
        }

        else
        {
            std::cout << "\n  Invalid command\n\n";
            return 1;
        }
        
        std::cout << "\n  sysfs: " << cmd.fileObject << "\n  operation: " << cmd.moduleOperation << "\n  value: " << cmd.valueToSet << "\n\n";
    }

    else
    {
        std::cout   << "\nNot enough arguments. All valid combinations:\n"
                    << "  - sampling get\n  - sampling set [MS]\n  - threshold get\n  - threshold set [MC]\n"
                    << "  - mode get\n  - mode set [MODE]\n  - stats\n  - dev (device reading)\n\n";
        return 0;
    }

    return 0;
}