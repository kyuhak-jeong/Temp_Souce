//
// Created by ihz on 2020/6/4.
//



#ifndef __NMEA_PARSER_H__
#define __NMEA_PARSER_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>


#define DEBUG_DATA 0

#define PRE_GGA     "$GNGGA"
#define PRE_GLL     "$GNGLL"
#define PRE_GSA     "$GNGSA"
#define PRE_GPGSV   "$GPGSV"
#define PRE_GNGSV   "$GNGSV"
#define PRE_GLGSV   "$GLGSV"
#define PRE_RMC     "$GNRMC"
#define PRE_VTG     "$GNVTG"

#define ENABLE_GGA  1
#define ENABLE_GLL  1
#define ENABLE_GSA  1
#define ENABLE_GSV  1
#define ENABLE_RMC  1
#define ENABLE_VTG  1
#define ENABLE_UTC  1

#if ENABLE_GGA
typedef struct
{
    char utc[11];
    double lat;
    char lat_dir;
    double lon;
    char lon_dir;
    unsigned char quality;
    unsigned char sats;
    double hdop;
    double alt;
    double undulation;
    unsigned char age;
    unsigned short stn_ID;
} GGA;
#endif

#if ENABLE_GLL
typedef struct
{
    double lat;
    char lat_dir;
    double lon;
    char lon_dir;
    char utc[11];
    char data_status;
}GLL;
#endif

#if ENABLE_GSA
typedef struct
{
    unsigned char total;
    unsigned char prn_ID;
    unsigned char prn;
}GSA_PRN;

typedef struct
{
    unsigned char mode_MA;
    unsigned char mode_123;
    double pdop;
    double hdop;
    double vdop;
    GSA_PRN *gsa_prn;
}GSA;
#endif

#if ENABLE_GSV
typedef struct
{
    unsigned char prn;
    unsigned char elev;
    unsigned short azimuth;
    unsigned char SNR;
}SAT_INFO;

typedef struct
{
    unsigned char msgs;
    unsigned char msg;
    unsigned char sats;
    SAT_INFO *sat_info;
}GSV;
#endif

#if ENABLE_RMC
typedef struct
{
    char utc[11];
    unsigned char pos_status;
    double lat;
    char lat_dir;
    double lon;
    char lon_dir;
    double speed_Kn;
    double track_true;
    char date[7]; 
    double mag_var;
    char var_dir;
    char mode_ind;
}RMC;
#endif

#if ENABLE_VTG
typedef struct
{
    double track_true;
    double track_mag;
    double speed_Kn;
    double speed_Km;
}VTG;
#endif

#if ENABLE_UTC
typedef struct
{
    unsigned char YY;
    unsigned char DD;
    unsigned char MM;
    unsigned char hh;
    unsigned char mm;
    unsigned char ss;
    unsigned short ds;
}UTC;
#endif

typedef struct
{
#if ENABLE_GGA
    GGA gga_data;
#endif
#if ENABLE_GLL
    GLL gll_data;
#endif
#if ENABLE_GSA
    GSA gsa_data;
#endif
#if ENABLE_GSV
    GSV gpgsv_data;
    GSV gngsv_data;
    GSV glgsv_data;
#endif
#if ENABLE_RMC
    RMC rmc_data;
#endif
#if ENABLE_VTG
    VTG vtg_data;
#endif
#if ENABLE_UTC
    UTC utc;
#endif
}GPS;

#ifdef __cplusplus
extern "C"{
#endif

GPS gps_data_parse(char* gps_src_str);

#ifdef __cplusplus
};
#endif

#endif //__NMEA_PARSER_H__
