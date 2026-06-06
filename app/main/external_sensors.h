#ifndef __EXTERNAL_SENSORS_H__
#define __EXTERNAL_SENSORS_H__

#include "app_vars.h"

#define GBUFF_SIZE 12 /* Rx buffer size */
#define GSENSOR_IOCTL_MAGIC 'a'

#define GSENSOR_IOCTL_START _IO(GSENSOR_IOCTL_MAGIC, 0x03)
#define GSENSOR_IOCTL_CLOSE _IO(GSENSOR_IOCTL_MAGIC, 0x02)
#define GSENSOR_IOCTL_APP_SET_RATE _IOW(GSENSOR_IOCTL_MAGIC, 0x10, short)
#define GSENSOR_IOCTL_GETDATA _IOR(GSENSOR_IOCTL_MAGIC, 0x08, char[GBUFF_SIZE + 1])

const float             accel_msg_per_lsb_2g    = 0.061035;
const float             accel_msg_per_lsb_4g    = 0.122070; // use
const float             accel_msg_per_lsb_8g    = 0.244140;
const float             accel_msg_per_lsb_16g   = 0.488281;
const float             g_const                 = 9.80665;
const double            gyro_scale              = 0.06097561;

static constexpr int    REQUIRED                = 5;
static const int        time_offset             = 9 * 3600;
// static bool         g_time_synced   = false;

enum sensor_type
{
    ICM42670_ACCEL = 0,
    ICM42670_GRYO,
    ICM42670_MAXIMUM_TYPE
};

struct GsensotAxis
{
    int x;
    int y;
    int z;
};

struct UTC 
{
    uint8_t YY, MM, DD, hh, mm, ss;
};

struct GPS 
{
    struct RMC 
    {
        char pos_status;
        char lat_dir, lon_dir;
        double lat, lon;
    } rmc_data;
    UTC utc;
};

struct UTCBuffer
{
    UTC data[REQUIRED];
    int count = 0;

    void clear()
    {
        count = 0;
        memset(data, 0, sizeof(data));
        printf("[Parser] UTC buffer cleared\r\n");
    }

    bool full() const { return count >= REQUIRED; }

    void push (const UTC& u) { data[count++] = u; }

    bool isOutlier(const UTC& u) const
    {
        if (count == 0) return false;
        
        const UTC& ref = data[0];

        if (u.YY != ref.YY || u.MM != ref.MM || u.DD != ref.DD)
        {
            printf("[Parser] Data mismatch: ref=%02d-%02d-%02d, cur=%02d-%02d-%02d\r\n", ref.YY, ref.MM, ref.DD, u.YY, u.MM, u.DD);
            return true;
        }

        int hh_diff = (int)u.hh - (int)ref.hh;
        if (hh_diff < -1 || hh_diff > 1)
        {
            printf("Parser] Hour mismatch: ref=%02d, cur=%02d\r\n", ref.hh, u.hh);
            return true;
        }

        return false;
    }
};

///////////////////////////////////////////////////////////////////////////////
// Task Entry Points
///////////////////////////////////////////////////////////////////////////////

#ifdef USE_GPS
    extern void GPS_Task();
    extern void SetTimeforGpsTask(const UTC& utc);
    extern int OpenGpsSerial();
#endif

#if defined(DEVICE_RK3588) && defined(USE_DVR) && defined(USE_TAXI)
    extern void imuData();
#endif


#ifdef USE_SERIAL
    extern int  configure_serial_port(const char* device, int baud_rate);
    extern void SerialDataReceiveTask();
#endif

#ifdef USE_TAXI
    extern void Process_DrSafe_Data(const char* data, int length);
#endif

#if defined(USE_BTO) && !defined(USE_TAXI)
    extern int write_uart_command(const char* command);
#endif

#endif // __EXTERNAL_SENSORS_H__
