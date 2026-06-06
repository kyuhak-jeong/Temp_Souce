//
// Created by ihz on 2020/6/4.
//
#include "nmea_parser.h"

static char* strsplit(char** stringp, const char* delim)
{
    char* start = *stringp;
    char* p;

    p = (start != NULL) ? strpbrk(start, delim) : NULL;

    if (p == NULL)
    {
        *stringp = NULL;
    }
    else
    {
        *p = '\0';
        *stringp = p + 1;
    }

    return start;
}

static int strstr_cnt(char *str, char *substr)
{
    char *srcStr = str;
    int count = 0;

    do
    {
        srcStr = strstr(srcStr, substr);
        if(srcStr != NULL)
        {
            count++;
            srcStr = srcStr + strlen(substr);
        }
        else
        {
            break;
        }
    }while (*srcStr != '\0');

    return count;
}


#if ENABLE_GGA
static GGA gga_data_parse(char *gga_data)
{
    GGA gga;
    unsigned char times = 0;
    char *p;
    char *end;
    char *s = strdup(gga_data);

    p = strsplit(&s, ",");
    while (p)
    {
        switch (times)
        {
            case 1:   // UTC
                strcpy(gga.utc, p);
                break;
            case 2:   // lat
                gga.lat = strtod(p, NULL)/100.0f;
                break;
            case 3:   // lat dir
                gga.lat_dir = p[0];
                break;
            case 4:   // lon
                gga.lon = strtod(p, NULL)/100.0f;
                break;
            case 5:   // lon dir
                gga.lon_dir = p[0];
                break;
            case 6:   // quality
                gga.quality = (unsigned char)strtol(p, NULL, 10);
                break;
            case 7:   // sats
                gga.sats = (unsigned char)strtol(p, NULL, 10);
                break;
            case 8:   // hdop
                gga.hdop = (unsigned char)strtol(p, NULL, 10);
                break;
            case 9:   // alt
                gga.alt = strtof(p, NULL);
                break;
            case 11:  // undulation
                gga.undulation = strtof(p, NULL);
                break;
            case 13:  // age
                gga.age = (unsigned char)strtol(p, NULL, 10);
                break;
            case 14:  // stn_ID
                end = (char *)malloc(sizeof(p));
                strncpy(end, p, strlen(p)-3);
                end[strlen(p)-3] = '\0';
                gga.stn_ID = (unsigned short )strtol(end, NULL, 10);
                free(end);
                break;
            default:
                break;
        }
        p = strsplit(&s, ",");
        times++;
    }
    free(s);
    return gga;
}
#endif

#if ENABLE_GLL
static GLL gll_data_parse(char *gll_data)
{
    GLL gll;
    unsigned char times = 0;
    char *p;
    char *s = strdup(gll_data);

    p = strsplit(&s, ",");
    while (p)
    {
        switch (times)
        {
            case 1:   // lat
                gll.lat = strtod(p, NULL)/100.0f;
                break;
            case 2:   // lat dir
                gll.lat_dir = p[0];
                break;
            case 3:   // lon
                gll.lon = strtod(p, NULL)/100.0f;
                break;
            case 4:   // lon dir
                gll.lon_dir = p[0];
                break;
            case 5:   // lon dir
                strcpy(gll.utc, p);
                break;
            case 6:  // data status
                gll.data_status = p[0];
                break;
            default:
                break;
        }
        p = strsplit(&s, ",");
        times++;
    }
    free(s);
    return gll;
}
#endif

#if ENABLE_GSA
static GSA_PRN *get_prn_data(char *gps_data)
{
    GSA_PRN *gsa_prn;
    unsigned char times = 0;
    unsigned char i;
    unsigned char sentences_index = 0; 
    char *p;
    char *s;
    char *sentences;
    int gsa_count;

    gsa_count = strstr_cnt(gps_data, (char*)PRE_GSA);

    gsa_prn = (GSA_PRN *)malloc(sizeof(GSA_PRN) * (gsa_count * 12 + 1));
    memset(gsa_prn, 0, sizeof(GSA_PRN) * (gsa_count * 12 + 1));
    sentences = strtok(gps_data, "\r\n");
    while (sentences)
    {
        if (strstr(sentences, (char*)PRE_GSA))
        {
            sentences_index++;
            s = strdup(sentences);
            p = strsplit(&s, ",");
            while (p)
            {
                if (times == 2)
                {
                    for (i=0; i<12; i++)
                    {
                        p = strsplit(&s, ",");
                        (gsa_prn+i+(sentences_index-1)*12)->total = (unsigned char)gsa_count * 12;
                        (gsa_prn+i+(sentences_index-1)*12)->prn_ID = i + (sentences_index - 1) * 12;
                        (gsa_prn+i+(sentences_index-1)*12)->prn = (unsigned char)strtol(p, NULL, 10);
                    }
                }
                p = strsplit(&s, ",");
                times++;
            }
            times = 0;
        }
        sentences = strtok(NULL, "\r\n");
    }
    free(s);
    return gsa_prn;
}

static GSA gsa_data_parse(char *gsa_data, char *gpsdata)
{
    GSA gsa;
    unsigned char times = 0;
    char *p;
    char *end;
    char *s = strdup(gsa_data);
    char *alldata = strdup(gpsdata);

    p = strsplit(&s, ",");
    while (p)
    {
        switch (times)
        {
            case 1:   // mode_MA
                gsa.mode_MA = p[0];
                break;
            case 2:   // mode_123
                gsa.mode_123 = p[0];
                break;
            case 3:   // prn
                gsa.gsa_prn = get_prn_data(alldata);
                break;
            case 15:  // pdop
                gsa.pdop = strtod(p, NULL);
                break;
            case 16:  // hdop
                gsa.hdop = strtod(p, NULL);
                break;
            case 17:  // vdop
                end = (char *)malloc(sizeof(p));
                strncpy(end, p, strlen(p)-3);
                end[strlen(p)-3] = '\0';
                gsa.vdop = strtod(end, NULL);
                free(end);
            default:
                break;
        }
        p = strsplit(&s, ",");
        times++;
    }
    free(s);
    return gsa;
}
#endif

#if ENABLE_RMC
static RMC rmc_data_parse(char *rmc_data)
{
    RMC rmc;
    unsigned char times = 0;
    char *p;
    char *s = strdup(rmc_data);

    p = strsplit(&s, ",");
    while (p)
    {
        switch (times)
        {
            case 1:   // UTC
                strcpy(rmc.utc, p);
                break;
            case 2:   // pos status
                rmc.pos_status = p[0];
                break;
            case 3:   // lat
                rmc.lat = strtod(p, NULL)/100.0f;
                break;
            case 4:   // lat dir
                rmc.lat_dir = p[0];
                break;
            case 5:   // lon
                rmc.lon = strtod(p, NULL)/100.0f;
                break;
            case 6:   // lon dir
                rmc.lon_dir = p[0];
                break;
            case 7:   // speen Kn
                rmc.speed_Kn = strtod(p, NULL);
                break;
            case 8:   // track true
                rmc.track_true = strtod(p, NULL);
                break;
            case 9:   // date
                strcpy(rmc.date, p);
                break;
            case 10:  // mag var
                rmc.mag_var = strtod(p, NULL);
                break;
            case 11:  // var dir
                rmc.var_dir = p[0];
                break;
            case 14:  // mode ind
                rmc.mode_ind = p[0];
                break;
            default:
                break;
        }
        p = strsplit(&s, ",");
        times++;
    }
    free(s);
    return rmc;
}
#endif

#if ENABLE_VTG
static VTG vtg_data_parse(char *vtg_data)
{
    VTG vtg;
    unsigned char times = 0;
    char *p;
    char *s = strdup(vtg_data);

    p = strsplit(&s, ",");
    while (p)
    {
        switch (times)
        {
            case 1:   // track true
                vtg.track_true = strtod(p, NULL);
                break;
            case 3:   // track mag
                vtg.track_mag = strtod(p, NULL);
                break;
            case 5:   // speed Kn
                vtg.speed_Kn = strtod(p, NULL);
                break;
            case 7:   // speed Km
                vtg.speed_Km = strtod(p, NULL);
                break;
            default:
                break;
        }
        p = strsplit(&s, ",");
        times++;
    }
    free(s);
    return vtg;
}
#endif

#if ENABLE_GSV
static SAT_INFO *get_sats_info(char *gps_data, unsigned char sats, char *prefix)
{
    SAT_INFO *sats_info;
    unsigned char times = 0;
    unsigned char msgs = 0;
    unsigned char msg = 0;
    unsigned char for_times;
    unsigned char i;
    char *p;
    char *s;
    char *sentences;

    sats_info = (SAT_INFO *)malloc(sizeof(SAT_INFO) * (sats+1));
    memset(sats_info, 0, sizeof(SAT_INFO) * (sats+1));
    sentences = strtok(gps_data, "\r\n");

    while (sentences)
    {
        if (strstr(sentences, prefix))
        {
            s = strdup(sentences);
            p = strsplit(&s, ",");
            while (p)
            {
                switch (times)
                {
                    case 1:   // msgs
                        msgs = (unsigned char) strtol(p, NULL, 10);
                        break;
                    case 2:   // msg
                        msg = (unsigned char) strtol(p, NULL, 10);
                        break;
                    case 3:   // sat info
                        for_times = (msgs == msg) ? ((sats % 4) ? sats % 4 : 4) : 4;
                        for (i = 0; i < for_times; i++)
                        {
                            p = strsplit(&s, ",");
                            (sats_info+(msg-1)*4+i)->prn = (unsigned char) strtol(p, NULL, 10);
                            p = strsplit(&s, ",");
                            (sats_info+(msg-1)*4+i)->elev = (unsigned char) strtol(p, NULL, 10);
                            p = strsplit(&s, ",");
                            (sats_info+(msg-1)*4+i)->azimuth = (unsigned short) strtol(p, NULL, 10);
                            p = strsplit(&s, ",");
                            (sats_info+(msg-1)*4+i)->SNR = (unsigned char) strtol(p, NULL, 10);
                        }
                        break;
                    default:
                        break;
                }
                p = strsplit(&s, ",");
                times++;
            }
            times = 0;
        }
        sentences = strtok(NULL, "\r\n");
    }
    free(s);
    return sats_info;
}

static GSV gsv_data_parse(char *gsv_data, char *gps_data, char *prefix)
{
    GSV gsv;
    unsigned char times = 0;
    char *p;
    char *s = strdup(gsv_data);
    char *src_data = strdup(gps_data);

    p = strsplit(&s, ",");
    while (p)
    {
        switch (times)
        {
            case 1:   // msgs
                gsv.msgs = (unsigned char)strtol(p, NULL, 10);
                break;
            case 2:   // msg
                gsv.msg = (unsigned char)strtol(p, NULL, 10);
                break;
            case 3:   // sats
                gsv.sats = (unsigned char)strtol(p, NULL, 10);
                gsv.sat_info = get_sats_info(src_data, gsv.sats, prefix);
                break;
            default:
                break;
        }
        p = strsplit(&s, ",");
        times++;
    }
    free(s);
    return gsv;
}
#endif

#if ENABLE_UTC
static UTC utc_parse(char *date, char *time)
{
    UTC utc_data;
    unsigned int date_int;
    double time_float;

    date_int = (unsigned int)strtol(date, NULL, 10);
    utc_data.DD = date_int / 10000;
    utc_data.MM = date_int % 10000 / 100;
    utc_data.YY = date_int % 100;
    time_float = strtod(time, NULL);
    utc_data.hh = (unsigned int)time_float / 10000;
    utc_data.mm = (unsigned int)time_float % 10000 / 100;
    utc_data.ss = (unsigned int)time_float % 100;
    utc_data.ds = (unsigned short)(time_float - (unsigned int)time_float);

    return utc_data;
}
#endif


GPS gps_data_parse(char* gps_src_str)
{
    GPS gps_all;
    //int i = 0;
#if ENABLE_GGA
    if(!strncmp(gps_src_str, PRE_GGA, 6))
    {
        gps_all.gga_data = gga_data_parse(strtok(strstr(gps_src_str, PRE_GGA), "\r\n"));
#if DEBUG_DATA
		printf("----------GGA DATA----------\n");
		printf("utc:%s\n", gps_all.gga_data.utc);
		printf("lat:%f\n", gps_all.gga_data.lat);
		printf("lat_dir:%c\n", gps_all.gga_data.lat_dir);
		printf("lon:%f\n", gps_all.gga_data.lon);
		printf("lon_dir:%c\n", gps_all.gga_data.lon_dir);
		printf("quality:%d\n", gps_all.gga_data.quality);
		printf("sats:%d\n", gps_all.gga_data.sats);
		printf("hdop:%f\n", gps_all.gga_data.hdop);
		printf("alt:%f\n", gps_all.gga_data.alt);
		printf("undulation:%f\n", gps_all.gga_data.undulation);
		printf("age:%d\n", gps_all.gga_data.age);
		printf("stn_ID:%d\n", gps_all.gga_data.stn_ID);
#endif
    }
#endif 
 
#if ENABLE_GLL
    if(!strncmp(gps_src_str, PRE_GLL, 6))
    {
        gps_all.gll_data = gll_data_parse(strtok(strstr(gps_src_str, PRE_GLL), "\r\n"));
#if DEBUG_DATA
		printf("----------GLL DATA----------\n");
		printf("utc:%s\n", gps_all.gll_data.utc);
		printf("lat:%f\n", gps_all.gll_data.lat);
		printf("lat_dir:%c\n", gps_all.gll_data.lat_dir);
		printf("lon:%f\n", gps_all.gll_data.lon);
		printf("lon_dir:%c\n", gps_all.gll_data.lon_dir);
		printf("data_status:%c\n", gps_all.gll_data.data_status);
#endif
    }
#endif 

#if ENABLE_GSA
    if(!strncmp(gps_src_str, PRE_GSA, 6))
    {
        gps_all.gsa_data = gsa_data_parse(strtok(strstr(gps_src_str, PRE_GSA), "\r\n"), gps_src_str);
#if DEBUG_DATA
		printf("----------GSA DATA----------\n");
		printf("mode_MA:%c\n", gps_all.gsa_data.mode_MA);
		printf("mode_123:%c\n", gps_all.gsa_data.mode_123);
		printf("total:%d\n", gps_all.gsa_data.gsa_prn[0].total);
		for (i=0; i<gps_all.gsa_data.gsa_prn[0].total; i++)
		{
		    printf("prn%d:%d\n", (i+1), gps_all.gsa_data.gsa_prn[i].prn);
		}
		printf("pdop:%f\n", gps_all.gsa_data.pdop);
		printf("hdop:%f\n", gps_all.gsa_data.hdop);
		printf("vdop:%f\n", gps_all.gsa_data.vdop);
#endif
		free(gps_all.gsa_data.gsa_prn);
    }
#endif 

#if ENABLE_RMC
    if(!strncmp(gps_src_str, PRE_RMC, 6))
    {
        gps_all.rmc_data = rmc_data_parse(strtok(strstr(gps_src_str, PRE_RMC), "\r\n"));
#if DEBUG_DATA        
        printf("----------RMC DATA----------\n");
		printf("utc:%s\n", gps_all.rmc_data.utc);
	    printf("status:%c\n", gps_all.rmc_data.pos_status);
		printf("lat:%f\n", gps_all.rmc_data.lat);
		printf("lat_dir:%c\n", gps_all.rmc_data.lat_dir);
		printf("lon:%f\n", gps_all.rmc_data.lon);
		printf("lon_dir:%c\n", gps_all.rmc_data.lon_dir);
		printf("speed_Kn:%f\n", gps_all.rmc_data.speed_Kn);
		printf("track_true:%f\n", gps_all.rmc_data.track_true);
		printf("date:%s\n", gps_all.rmc_data.date);
		printf("mag_var:%f\n", gps_all.rmc_data.mag_var);
		printf("var_dir:%c\n", gps_all.rmc_data.var_dir);
		printf("mode_ind:%c\n", gps_all.rmc_data.mode_ind);
#endif
    }
#endif

#if ENABLE_VTG
    if(!strncmp(gps_src_str, PRE_VTG, 6))
    {
        gps_all.vtg_data = vtg_data_parse(strtok(strstr(gps_src_str, PRE_VTG), "\r\n"));
#if DEBUG_DATA
		printf("----------VTG DATA----------\n");
		printf("track_true:%f\n", gps_all.vtg_data.track_true);
		printf("track_mag:%f\n", gps_all.vtg_data.track_mag);
		printf("speen_Kn:%f\n", gps_all.vtg_data.speed_Kn);
		printf("speed_Km:%f\n", gps_all.vtg_data.speed_Km);
#endif
    }
#endif

#if ENABLE_GSV
    if(!strncmp(gps_src_str, (char*)PRE_GPGSV, 6))
    {
        if(strlen(gps_src_str) > 18)
        {
		    gps_all.gpgsv_data = gsv_data_parse(strtok(strstr(gps_src_str, PRE_GPGSV), "\r\n"), gps_src_str, (char*)PRE_GPGSV);
	#if DEBUG_DATA
			printf("----------GPGSV DATA----------\n");
			printf("msgs:%d\n", gps_all.gpgsv_data.msgs);
			printf("msg:%d\n", gps_all.gpgsv_data.msg);
			printf("sats:%d\n", gps_all.gpgsv_data.sats);
			for (i=0;i<gps_all.gpgsv_data.sats; i++)
			{
				printf("prn%d:%d\n", i+1, gps_all.gpgsv_data.sat_info[i].prn);
				printf("evel%d:%d\n", i+1, gps_all.gpgsv_data.sat_info[i].elev);
				printf("azimuth%d:%d\n", i+1, gps_all.gpgsv_data.sat_info[i].azimuth);
				printf("SNR%d:%d\n", i+1, gps_all.gpgsv_data.sat_info[i].SNR);
			}
	#endif
			if (gps_all.gpgsv_data.sats) free(gps_all.gpgsv_data.sat_info);
		}
    }
    if(!strncmp(gps_src_str, (char*)PRE_GNGSV, 6))
    {
        if(strlen(gps_src_str) > 18)
        {
		    gps_all.gpgsv_data = gsv_data_parse(strtok(strstr(gps_src_str, PRE_GNGSV), "\r\n"), gps_src_str, (char*)PRE_GNGSV);
	#if DEBUG_DATA
			printf("----------GNGSV DATA----------\n");
			printf("msgs:%d\n", gps_all.gngsv_data.msgs);
			printf("msg:%d\n", gps_all.gngsv_data.msg);
			printf("sats:%d\n", gps_all.gngsv_data.sats);
			for (i=0; i<gps_all.gngsv_data.sats; i++)
			{
				printf("prn%d:%d\n", i+1, gps_all.gngsv_data.sat_info[i].prn);
				printf("evel%d:%d\n", i+1, gps_all.gngsv_data.sat_info[i].elev);
				printf("azimuth%d:%d\n", i+1, gps_all.gngsv_data.sat_info[i].azimuth);
				printf("SNR%d:%d\n", i+1, gps_all.gngsv_data.sat_info[i].SNR);
			}		
	#endif
		    if (gps_all.gngsv_data.sats) free(gps_all.gngsv_data.sat_info);
        }
    }
    if(!strncmp(gps_src_str, (char*)PRE_GLGSV, 6))
    {
        if(strlen(gps_src_str) > 18)
        {
		    gps_all.gpgsv_data = gsv_data_parse(strtok(strstr(gps_src_str, PRE_GLGSV), "\r\n"), gps_src_str, (char*)PRE_GLGSV);
	#if DEBUG_DATA
			printf("----------GLGSV DATA----------\n");
			printf("msgs:%d\n", gps_all.glgsv_data.msgs);
			printf("msg:%d\n", gps_all.glgsv_data.msg);
			printf("sats:%d\n", gps_all.glgsv_data.sats);
			for (i=0;i<gps_all.glgsv_data.sats; i++)
			{
				printf("prn%d:%d\n", i+1, gps_all.glgsv_data.sat_info[i].prn);
				printf("evel%d:%d\n", i+1, gps_all.glgsv_data.sat_info[i].elev);
				printf("azimuth%d:%d\n", i+1, gps_all.glgsv_data.sat_info[i].azimuth);
				printf("SNR%d:%d\n", i+1, gps_all.glgsv_data.sat_info[i].SNR);
			}
	#endif
			if (gps_all.glgsv_data.sats) free(gps_all.glgsv_data.sat_info);
		}
    }    
#endif
 
#if ENABLE_UTC && ENABLE_RMC
    gps_all.utc = utc_parse(gps_all.rmc_data.date, gps_all.rmc_data.utc);
#if DEBUG_DATA
    printf("----------UTC DATA----------\n");
    printf("year:20%02d\n", gps_all.utc.YY);
    printf("month:%02d\n", gps_all.utc.MM);
    printf("date:%02d\n", gps_all.utc.DD);
    printf("hour:%02d\n", gps_all.utc.hh);
    printf("minutes:%02d\n", gps_all.utc.mm);
    printf("second:%02d\n", gps_all.utc.ss);
    printf("ds:%02d\n", gps_all.utc.ds);
#endif
#endif

    return gps_all;
}

