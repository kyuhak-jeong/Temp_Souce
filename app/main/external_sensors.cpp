#include "external_sensors.h"

///////////////////////////////////////////////////////////////////////////////
// GPS Task
///////////////////////////////////////////////////////////////////////////////

#ifdef USE_GPS

    static int       timeset     = 0;
    static const int time_offset = 9 * 3600;  // KST

    static constexpr double PI             = 3.14159265;
    static constexpr double TWO_PI         = 2.0 * PI;
    static constexpr double RADIANS        = PI / 180.0;
    static constexpr double SUN_DIAMETER   = 0.53;
    static constexpr double AIR_REFRACTION = 34.0 / 60.0;

    static double FNday(int y, int m, int d, float h)
    {
        int days = -7 * (y + (m + 9) / 12) / 4 + 275 * m / 9 + d + y * 367L;
        return (double)days - 730531.5 + h / 24.0;
    }

    static double FNrange(double x)
    {
        double angle = TWO_PI * (x / TWO_PI - (long)(x / TWO_PI));
        if (angle < 0) angle += TWO_PI;
        return angle;
    }

    static double f0(double lat, double declin)
    {
        double offset     = RADIANS * (0.5 * SUN_DIAMETER + AIR_REFRACTION);
        if (lat < 0.0) offset = -offset;

        double hour_angle = tan(declin + offset) * tan(lat * RADIANS);
        if (hour_angle > 0.99999) hour_angle = 1.0;

        return asin(hour_angle) + PI / 2.0;
    }

    static double FNsun(double d)
    {
        double L = FNrange(280.461 * RADIANS + .9856474 * RADIANS * d);
        double M = FNrange(357.528 * RADIANS + .9856003 * RADIANS * d);
        return FNrange(L + 1.915 * RADIANS * sin(M) + .02 * RADIANS * sin(2 * M));
    }

    static GPS gps_data_parse(const char* nmea)
    {
        GPS g{};
        return g;
    }

    void SetTimeforGpsTask(const UTC& utc)  
    {
        printf("%s()++\r\n", __func__);

        struct tm tm{};
        tm.tm_year = utc.YY + 100;
        tm.tm_mon  = utc.MM - 1;
        tm.tm_mday = utc.DD;
        tm.tm_hour = utc.hh;
        tm.tm_min  = utc.mm;
        tm.tm_sec  = utc.ss;

        time_t   gps_time = mktime(&tm);
        time_t   sys_time = time(nullptr);
        long int diff     = difftime(gps_time + time_offset, sys_time);

        if (diff > 120 || diff < -120)
        {
            struct timeval tv{};
            gettimeofday(&tv, nullptr);
            tv.tv_sec = gps_time + time_offset;
            settimeofday(&tv, nullptr);
            if (system("hwclock --systohc")) {};
        }

        timeset = 1;
        printf("%s()--\r\n", __func__);
    }

    static int OpenGpsSerial()
    {
        int fd = open("/dev/ttyS6", O_RDWR | O_NOCTTY | O_NDELAY);
        if (fd < 0)
        {
            printf("[GPS] Device open failed\r\n");
            return -1;
        }

        struct termios options;
        fcntl(fd, F_SETFL, 0);
        tcgetattr(fd, &options);

        cfsetispeed(&options, B9600);
        cfsetospeed(&options, B9600);

        options.c_iflag |=  ICRNL;
        options.c_cflag &= ~(PARENB | CSTOPB | CSIZE);
        options.c_cflag |=  CS8;
        options.c_lflag &= ~ECHO;
        options.c_lflag |=  ICANON;

        if (tcsetattr(fd, TCSAFLUSH, &options) != 0)
        {
            printf("[GPS] tcsetattr failed\r\n");
            close(fd);
            return -1;
        }

        return fd;
    }

    void GPS_Task()
    {
        char data[255] = {0};
        char buf;
        int  nbytes    = 0;
        int  i         = 0;
        int  count     = 0;
        timeset        = 0;

        int fd = OpenGpsSerial();
        if (fd < 0) return;

        while (APP::app_running)
        {
            nbytes = read(fd, &buf, 1);
            if (nbytes != 1) continue;

            if (buf == '$')
            {
                if (data[0] == '$')
                {
                    GPS gps = gps_data_parse(data);

                    if (gps.rmc_data.pos_status == 'A')
                    {
                        if (timeset == 0) SetTimeforGpsTask(gps.utc);

                        if (gps.rmc_data.lat_dir == 'S') gps.rmc_data.lat = -gps.rmc_data.lat;
                        if (gps.rmc_data.lon_dir == 'W') gps.rmc_data.lon = -gps.rmc_data.lon;

                        DataUtils::gps_update(APP::g_gps, gps.rmc_data.lat, gps.rmc_data.lon);

                        if (++count >= 60) count = 0;
                    }
                }
                memset(data, 0, sizeof(data));
                i = 0;
            }

            if (i >= 254)
            {
                memset(data, 0, sizeof(data));
                i = 0;
            }
            else
            {
                data[i++] = buf;
            }
        }

        close(fd);
        printf("[GPS] Task Exit\n");
    }

#endif  // USE_GPS


///////////////////////////////////////////////////////////////////////////////
// IMU Task
///////////////////////////////////////////////////////////////////////////////

#if defined(DEVICE_RK3588) && defined(USE_DVR)

    extern void onCollisionDetected(float gForce, const std::string& direction, int v_level);

    void imuData()
    {
        float prev_a_x = 0, prev_a_y = 0, prev_a_z = 0;
        float cur_a_x  = 0, cur_a_y  = 0, cur_a_z  = 0;
        float cur_g_x  = 0, cur_g_y  = 0, cur_g_z  = 0;

        const float accel_scale = accel_msg_per_lsb_4g;
        int fd = -1;

        while (APP::app_running)
        {
            fd = open("/dev/invn_dev", O_RDWR, 0);
            if (fd < 0) { usleep(100000); continue; }
            break;
        }

        if (ioctl(fd, GSENSOR_IOCTL_START) < 0)
        {
            printf("[IMU] Start error => Exit\n");
            return;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        struct GsensotAxis prev[ICM42670_MAXIMUM_TYPE];
        if (ioctl(fd, GSENSOR_IOCTL_GETDATA, prev) < 0)
        {
            printf("[IMU] Get prev data error => Exit\n");
            return;
        }

        prev_a_x = prev[ICM42670_ACCEL].x * accel_scale;
        prev_a_y = prev[ICM42670_ACCEL].y * accel_scale;
        prev_a_z = prev[ICM42670_ACCEL].z * accel_scale;

        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        while (APP::app_running)
        {
            struct GsensotAxis axis[ICM42670_MAXIMUM_TYPE];
            if (ioctl(fd, GSENSOR_IOCTL_GETDATA, axis) < 0)
            {
                printf("[IMU] Get axis data error\n");
                break;
            }

            cur_a_x = axis[ICM42670_ACCEL].x * accel_scale;
            cur_a_y = axis[ICM42670_ACCEL].y * accel_scale;
            cur_a_z = axis[ICM42670_ACCEL].z * accel_scale;
            cur_g_x = axis[ICM42670_GRYO].x  * gyro_scale;
            cur_g_y = axis[ICM42670_GRYO].y  * gyro_scale;
            cur_g_z = axis[ICM42670_GRYO].z  * gyro_scale;

            DataUtils::imu_update(APP::g_imu, cur_a_x, cur_a_y, cur_a_z, cur_g_x, cur_g_y, cur_g_z);

            float diff_x = std::abs(cur_a_x - prev_a_x);
            float diff_y = std::abs(cur_a_y - prev_a_y);
            float diff_z = std::abs(cur_a_z - prev_a_z);

            prev_a_x = cur_a_x;
            prev_a_y = cur_a_y;
            prev_a_z = cur_a_z;

            // onCollision Detected Threshold
            static const float thresholds[] = {0, 300, 500, 1000};
            int level = APP::dvrConf.vibrationLevel;

            if (level >= 1 && level <= 3)
            {
                float thr = thresholds[level];
                if (diff_x > thr || diff_y > thr || diff_z > thr)
                {
                    onCollisionDetected(thr, "", level);
                }
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        if (fd >= 0)
        {
            if (ioctl(fd, GSENSOR_IOCTL_CLOSE) < 0) printf("[IMU] Stop failed\n");
            if (close(fd) < 0)                       printf("[IMU] Close failed\n");
        }

        printf("[IMU] Task Exit\r\n");
    }

#endif  // DEVICE_RK3588 && USE_DVR && USE_TAXI


#if defined(DEVICE_RK3576) && defined(USE_DVR) && defined(USE_TAXI)

    extern void onCollisionDetected(float gForce, const std::string& direction, int v_level);

    static float prev_a_x = 0.0f;
    static float prev_a_y = 0.0f;
    static float prev_a_z = 0.0f;
    static bool  prev_valid = false;

    void CheckCollisionDetected()
    {
        float cur_a_x = 0.0f;
        float cur_a_y = 0.0f;
        float cur_a_z = 0.0f;

        {
            IMUData imu = APP::g_imu.read();
            cur_a_x = imu.accelX;
            cur_a_y = imu.accelY;
            cur_a_z = imu.accelZ;
        }

        if (!prev_valid)
        {
            prev_a_x  = cur_a_x;
            prev_a_y  = cur_a_y;
            prev_a_z  = cur_a_z;
            prev_valid = true;
            printf("[IMU] Init: cur=(%.1f, %.1f, %.1f)\r\n", cur_a_x, cur_a_y, cur_a_z);
            return;
        }

        float diff_x = std::abs(cur_a_x - prev_a_x);
        float diff_y = std::abs(cur_a_y - prev_a_y);
        float diff_z = std::abs(cur_a_z - prev_a_z);

        static const float thresholds[] = {0.0f, 7.0f, 15.0f, 20.0f};
        int level = APP::dvrConf.vibrationLevel;
        float thr = (level >= 1 && level <= 3) ? thresholds[level] : 0.0f;

        // printf("[IMU] cur=(%.1f,%.1f,%.1f) prev=(%.1f,%.1f,%.1f) diff=(%.1f,%.1f,%.1f) thr=%.1f lv=%d\r\n",
        //        cur_a_x, cur_a_y, cur_a_z,
        //        prev_a_x, prev_a_y, prev_a_z,
        //        diff_x, diff_y, diff_z, thr, level);

        prev_a_x = cur_a_x;
        prev_a_y = cur_a_y;
        prev_a_z = cur_a_z;

        if (level >= 1 && level <= 3)
        {
            if (diff_x > thr || diff_y > thr || diff_z > thr)
            {
                std::string direction = "";
                if      (diff_x >= diff_y && diff_x >= diff_z) direction = "X";
                else if (diff_y >= diff_x && diff_y >= diff_z) direction = "Y";
                else                                           direction = "Z";

                float max_diff = std::max({diff_x, diff_y, diff_z});
                // printf("[IMU] Collision detected: dir=%s diff=(%.1f,%.1f,%.1f) thr=%.1f lv=%d\r\n", direction.c_str(), diff_x, diff_y, diff_z, thr, level);
                onCollisionDetected(max_diff, direction, level);
            }
            else
            {
                // printf("[IMU] Skip: max_diff=%.1f below thr=%.1f\r\n", std::max({diff_x, diff_y, diff_z}), thr);
            }
        }
    }

#endif

///////////////////////////////////////////////////////////////////////////////
// Settime Log
///////////////////////////////////////////////////////////////////////////////

static void LogTimeSync(const char* source, time_t old_time, time_t new_time, long diff)
{
    std::ofstream log("/tmp/time_sync.log", std::ios::app);
    if (!log.is_open()) return;

    char old_buf[64], new_buf[64];

    struct tm old_tm{};
    struct tm new_tm{};

    localtime_r(&old_time, &old_tm);
    localtime_r(&new_time, &new_tm);

    strftime(old_buf, sizeof(old_buf), "%Y-%m-%d %H:%M:%S", &old_tm);
    strftime(new_buf, sizeof(new_buf), "%Y-%m-%d %H:%M:%S", &new_tm);

    time_t now = time(nullptr);
    struct tm now_tm{};
    localtime_r(&now, &now_tm);

    char now_buf[64];
    strftime(now_buf, sizeof(now_buf), "%Y-%m-%d %H:%M:%S", &now_tm);

    log << "[TimeSync] "
        << "LOG_TIME=" << now_buf
        << ", SOURCE=" << source
        << ", OLD=" << old_buf
        << ", NEW=" << new_buf
        << ", DIFF=" << diff << " sec"
        << "\n";

    log.close();
}

///////////////////////////////////////////////////////////////////////////////
// Serial Task
///////////////////////////////////////////////////////////////////////////////

#if defined(USE_SERIAL) && defined(USE_DVR)

    static int s_fd_serial = -1;

    ///////////////////////////////////////////////////////////////////////////
    // Token Validators
    ///////////////////////////////////////////////////////////////////////////

    static bool isValidUTC(const UTC& utc)
    {
        if (utc.YY < 25 || utc.YY > 99) return false;  // 2025~2099
        if (utc.MM < 1  || utc.MM > 12) return false;
        if (utc.DD < 1  || utc.DD > 31) return false;
        if (utc.hh > 23)                return false;
        if (utc.mm > 59)                return false;
        if (utc.ss > 59)                return false;
        return true;
    }

    static bool IsDigitsOnly(const std::string& s)
    {
        if (s.empty()) return false;
        for (char c : s)
        {
            if (!isdigit((unsigned char)c)) return false;
        }
        return true;
    }

    static bool IsSignedFloat(const std::string& s)
    {
        if (s.empty()) return false;
        int i = 0;
        if (s[i] == '+' || s[i] == '-') i++;
        bool hasDot = false;
        for (; i < (int)s.size(); i++)
        {
            if (s[i] == '.')
            {
                if (hasDot) return false;
                hasDot = true;
            }
            else if (!isdigit((unsigned char)s[i]))
            {
                return false;
            }
        }
        return true;
    }

    static bool IsDateFormat(const std::string& s)
    {
        // "YY-MM-DD" : 8
        if (s.size() != 8) return false;
        if (s[2] != '-' || s[5] != '-') return false;
        for (int i : {0, 1, 3, 4, 6, 7})
        {
            if (!isdigit((unsigned char)s[i])) return false;
        }
        return true;
    }

    static bool IsTimeFormat(const std::string& s)
    {
        // "hh:mm:ss" : 8
        if (s.size() != 8) return false;
        if (s[2] != ':' || s[5] != ':') return false;
        for (int i : {0, 1, 3, 4, 6, 7})
        {
            if (!isdigit((unsigned char)s[i])) return false;
        }
        return true;
    }

    static bool IsLatFormat(const std::string& s)
    {
        // "DDMM.MMMMMM" : minimize 10, signed float
        if (s.size() < 10) return false;
        return IsSignedFloat(s);
    }

    static bool IsLonFormat(const std::string& s)
    {
        // "DDDMM.MMMMMM" : minimize 11, signed float
        if (s.size() < 11) return false;
        return IsSignedFloat(s);
    }

    static bool ValidateDrSafeTokens(const std::vector<std::string>& tokens)
    {
        // minimize 16 tokens
        if (tokens.size() < 16)
        {
            // printf("[DrSafe] Token count too small: %zu\n", tokens.size());
            return false;
        }

        // [0] Header : "$"
        if (tokens[0] != "$")
        {
            // printf("[DrSafe] Invalid header: %s\n", tokens[0].c_str());
            return false;
        }

        // [1] seq
        if (!IsDigitsOnly(tokens[1]))
        {
            // printf("[DrSafe] Invalid seq: %s\n", tokens[1].c_str());
            return false;
        }

        // [2] YY-MM-DD
        if (!IsDateFormat(tokens[2]))
        {
            // printf("[DrSafe] Invalid date: %s\n", tokens[2].c_str());
            return false;
        }

        // [3] hh:mm:ss
        if (!IsTimeFormat(tokens[3]))
        {
            // printf("[DrSafe] Invalid time: %s\n", tokens[3].c_str());
            return false;
        }

        // [4] ErrorCode
        if (!IsDigitsOnly(tokens[4]))
        {
            // printf("[DrSafe] Invalid ErrorCode: %s\n", tokens[4].c_str());
            return false;
        }

        // [5] lat
        if (!IsLatFormat(tokens[5]))
        {
            // printf("[DrSafe] Invalid lat: %s\n", tokens[5].c_str());
            return false;
        }

        // [6] lon
        if (!IsLonFormat(tokens[6]))
        {
            // printf("[DrSafe] Invalid lon: %s\n", tokens[6].c_str());
            return false;
        }

        // [7]~[12] acc_x~gyro_z : signed float
        for (int i = 7; i <= 12; i++)
        {
            if (!IsSignedFloat(tokens[i]))
            {
                // printf("[DrSafe] Invalid sensor[%d]: %s\n", i, tokens[i].c_str());
                return false;
            }
        }

        // [13] Speed, [14] Rpm, [15] Flag
        for (int i = 13; i <= 15; i++)
        {
            if (!IsDigitsOnly(tokens[i]))
            {
                // printf("[DrSafe] Invalid field[%d]: %s\n", i, tokens[i].c_str());
                return false;
            }
        }

        return true;
    }

    ///////////////////////////////////////////////////////////////////////////
    // Serial Port Configure
    ///////////////////////////////////////////////////////////////////////////

    int configure_serial_port(const char* device, int baud_rate)
    {
        // printf("[Serial] Configuring %s\n", device);

        s_fd_serial = open(device, O_RDWR | O_NOCTTY | O_NDELAY);
        if (s_fd_serial < 0)
        {
            // perror("[Serial] Open failed");
            return -1;
        }

        if (fcntl(s_fd_serial, F_SETFL, 0) < 0)
        {
            // perror("[Serial] Set blocking mode failed");
            close(s_fd_serial);
            return -1;
        }

        struct termios options;
        if (tcgetattr(s_fd_serial, &options) < 0)
        {
            // perror("[Serial] Get attributes failed");
            close(s_fd_serial);
            return -1;
        }

        cfsetispeed(&options, baud_rate);
        cfsetospeed(&options, baud_rate);

        options.c_cflag &= ~(PARENB | CSTOPB | CSIZE | CRTSCTS);
        options.c_cflag |=  CS8 | CREAD | CLOCAL;
        options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
        options.c_iflag &= ~(IXON | IXOFF | IXANY);
        options.c_oflag &= ~OPOST;

        if (tcsetattr(s_fd_serial, TCSAFLUSH, &options) < 0)
        {
            // perror("[Serial] Set attributes failed");
            close(s_fd_serial);
            return -1;
        }

        return s_fd_serial;
    }

    ///////////////////////////////////////////////////////////////////////////
    // Serial Dispatch
    ///////////////////////////////////////////////////////////////////////////

    #if defined(USE_BTO) && !defined(USE_TAXI) // USE_TAXI Condition does not using BTO (Send Data from STH else)

        int write_uart_command(const char* command)
        {
            if (s_fd_serial < 0)
            {
                fprintf(stderr, "[Serial] Device not open\n");
                return -1;
            }

            int bytes_written = write(s_fd_serial, command, strlen(command));
            if (bytes_written < 0)
            {
                perror("[Serial] Write failed");
                return -1;
            }
            return bytes_written;
        }

    #endif

    #ifndef USE_TAXI

        static void ParseSerialDataforSafeView(const char* data, int length)
        {
            char buffer[256];
            strncpy(buffer, data, length);
            buffer[length] = '\0';

            char* token = strtok(buffer, ";");
            if (!token || strncmp(token, "$", 3) != 0) return;

            token = strtok(token, ",");
            int count = 0;

            while (token != NULL)
            {
                token = strtok(NULL, ",");
                if (token == NULL) break;

                try
                {
                    int value = std::stoi(token);
                    switch (count)
                    {
                        case 2: APP::bto_status = value; break;
                        case 3: APP::aps_level  = value; break;
                        default: break;
                    }
                }
                catch (const std::exception& e) {}

                count++;
            }
        }

    #endif

    static void SetTimeFromDrSafe(const UTC& utc)
    {
        static bool s_timeSynced = false;

        if (s_timeSynced) return;

        if (!isValidUTC(utc))
        {
            printf("[Parser] UTC invalid value, rejected\r\n");
            return;
        }

        static UTCBuffer s_buf;

        if (s_buf.isOutlier(utc)) { s_buf.clear(); }

        s_buf.push(utc);

        printf("[Parser] UTC buffer [%d/%d] : %02d-%02d-%02d %02d:%02d:%02d\n", s_buf.count, REQUIRED, utc.YY, utc.MM, utc.DD, utc.hh, utc.mm, utc.ss);

        if (!s_buf.full()) return;

        const UTC& final_utc = s_buf.data[REQUIRED - 1];

        struct tm tm{};
        tm.tm_year = final_utc.YY + 100;
        tm.tm_mon  = final_utc.MM - 1;
        tm.tm_mday = final_utc.DD;
        tm.tm_hour = final_utc.hh;
        tm.tm_min  = final_utc.mm;
        tm.tm_sec  = final_utc.ss;

        time_t   gps_time = mktime(&tm);
        time_t   sys_time = time(nullptr);
        long int diff     = difftime(gps_time + time_offset, sys_time);

        if (diff > 120 || diff < -120)
        {
            time_t new_time = gps_time + time_offset;

            LogTimeSync("DrSafe", sys_time, new_time, diff);

            struct timeval tv{};
            tv.tv_sec  = new_time;
            tv.tv_usec = 0;

            settimeofday(&tv, nullptr);
            if (system("hwclock --systohc &"));
        }
        s_timeSynced = true;
        printf("[DrSafe] Time synced: %02d-%02d-%02d %02d:%02d:%02d\n", final_utc.YY, final_utc.MM, final_utc.DD, final_utc.hh, final_utc.mm, final_utc.ss);
    }

    static void DispatchReceivedData(const char* data, int length)
    {
        // printf("[Serial] RAW packet (len=%d): %.*s\n", length, length, data);

        #ifdef USE_TAXI
            Process_DrSafe_Data(data, length);
        #else
            ParseSerialDataforSafeView(data, length);
        #endif
    }

    ///////////////////////////////////////////////////////////////////////////
    // Serial Receive Task
    ///////////////////////////////////////////////////////////////////////////

    void SerialDataReceiveTask()
    {
        #if defined(DEVICE_RK3588)
            const char* device = "/dev/ttyS2";
        #elif defined(DEVICE_RK3576)
            const char* device = "/dev/ttyS5";
        #else
            #error "Unsupported device: define DEVICE_RK3588 or DEVICE_RK3576"
        #endif

        s_fd_serial = configure_serial_port(device, B115200);

        char        chunk[256];
        std::string accumulator;

        while (APP::app_running)
        {
            memset(chunk, 0, sizeof(chunk));

            int nbytes = read(s_fd_serial, chunk, sizeof(chunk) - 1);
            if (nbytes < 0)
            {
                printf("[Serial] Read failed\n");
                continue;
            }

            if (nbytes == 0) continue;

            accumulator.append(chunk, nbytes);

            size_t start = accumulator.find('$');
            if (start == std::string::npos)
            {
                accumulator.clear();
                continue;
            }

            if (start > 0)
            {
                accumulator.erase(0, start);
            }

            size_t pos;
            while ((pos = accumulator.find(';')) != std::string::npos)
            {
                std::string packet = accumulator.substr(0, pos + 1);
                accumulator.erase(0, pos + 1);

                if (packet.front() != '$')
                {
                    continue;
                }

                size_t extra_dollar = packet.find('$', 1);
                if (extra_dollar != std::string::npos)
                {
                    packet = packet.substr(extra_dollar);
                }

                if (packet.size() < 20)
                {
                    continue;
                }

                DispatchReceivedData(packet.c_str(), (int)packet.size());
            }

            // if there are more than two '$', it determine missing ';' in the previous packet
            size_t first_string  = accumulator.find('$');
            size_t second_string = (first_string != std::string::npos) ? accumulator.find('$', first_string + 1) : std::string::npos;

            if (second_string != std::string::npos)
            {
                // printf("[Serial] Missing ';' detected, discarding incomplete packet: %s\n",accumulator.substr(0, second_string).c_str());
                accumulator.erase(0, second_string);
            }

            if (accumulator.size() > 1024)
            {
                accumulator.clear();
            }
        }

        if (close(s_fd_serial) < 0)
        {
            perror("[Serial] Close failed");
        }

        printf("[Serial] Task Exit\n");
    }

#endif  // USE_SERIAL

// Dr.Safe Data Parser (USE_TAXI)

#if defined(USE_TAXI)
    /*
    $,  seq,  YY-MM-DD,  hh:mm:ss,  ErrorCode,  lat,  lon,  acc_x,  acc_y,  acc_z,  gyro_x,  gyro_y,  gyro_z,  Speed,  Rpm,  Flag
    0    1       2           3           4         5     6      7       8       9       10       11       12       13      14    15
    */

    extern void DrSafePUAisActivated();

    void Process_DrSafe_Data(const char* data, int length)
    {
        if ((data == nullptr) || (length <= 0)) return;

        std::string raw(data, length);
        std::string packet = raw.substr(0, raw.find(';'));

        std::vector<std::string> tokens;
        std::stringstream ss(packet);
        std::string token;

        while (std::getline(ss, token, ','))
        {
            tokens.push_back(token);
        }

        if (ValidateDrSafeTokens(tokens) == false)  // call validate func
        {
            return;
        } 

        int tmp_pua = 0;
        try
        {
            std::string tmp_gps    = tokens[5] + "," + tokens[6];
            std::string tmp_sensor = " " + tokens[7]  + " " + tokens[8]  + " " + tokens[9]
                                   + " " + tokens[10] + " " + tokens[11] + " " + tokens[12];

            // int tmp_speed = std::stoi(tokens[13]);
            // int tmp_rpm   = std::stoi(tokens[14]);
            tmp_pua   = std::stoi(tokens[15]);

            if (tmp_gps.size() < 3 || tmp_sensor.size() < 10) 
            {
                return; 
            }

            {
                std::lock_guard<std::mutex> lock(APP::subtitle_data_mutex);

                APP::pua_flag = tmp_pua;

                DataUtils::gps_update(APP::g_gps, std::stof(tokens[5]), std::stof(tokens[6]));
                DataUtils::imu_update(APP::g_imu, std::stof(tokens[7]), std::stof(tokens[8]), std::stof(tokens[9]), std::stof(tokens[10]), std::stof(tokens[11]), std::stof(tokens[12]));
                DataUtils::obd_set_speed(APP::g_obd, std::stoi(tokens[13]));
                DataUtils::obd_set_rpm(APP::g_obd, std::stoi(tokens[14]));

                // printf("[DrSafe] token[5]  lat    : %s\n", tokens[5].c_str());
                // printf("[DrSafe] token[6]  lon    : %s\n", tokens[6].c_str());
                // printf("[DrSafe] token[7]  acc_x  : %s\n", tokens[7].c_str());
                // printf("[DrSafe] token[8]  acc_y  : %s\n", tokens[8].c_str());
                // printf("[DrSafe] token[9]  acc_z  : %s\n", tokens[9].c_str());
                // printf("[DrSafe] token[10] gyro_x : %s\n", tokens[10].c_str());
                // printf("[DrSafe] token[11] gyro_y : %s\n", tokens[11].c_str());
                // printf("[DrSafe] token[12] gyro_z : %s\n", tokens[12].c_str());
                // printf("[DrSafe] token[13] speed  : %s\n", tokens[13].c_str());
                // printf("[DrSafe] token[14] rpm    : %s\n", tokens[14].c_str());
                // printf("[DrSafe] token[15] pua    : %s\n", tokens[15].c_str());
            }
        }
        catch (const std::exception& e)
        {
            return;
        }

        CheckCollisionDetected();

        if (tokens.size() > 3) // timestamp Sync
        {
            std::string datetime = tokens[2] + " " + tokens[3];

            UTC utc{};

            if (sscanf(datetime.c_str(), "%hhd-%hhd-%hhd %hhd:%hhd:%hhd", &utc.YY, &utc.MM, &utc.DD, &utc.hh, &utc.mm, &utc.ss) == 6)
            {
                if (isValidUTC(utc) == false) { return; }
                SetTimeFromDrSafe(utc);
            }
        }
        
        #if (0)
            if (tmp_pua == 1) 
            {
                DrSafePUAisActivated(); 
            }
        #else
            static std::chrono::steady_clock::time_point s_last_pua_time{};
            static bool s_pua_ever_called = false;

            {
                auto now = std::chrono::steady_clock::now();
                bool should_call = !s_pua_ever_called || std::chrono::duration_cast<std::chrono::seconds>(now - s_last_pua_time).count() >= 60;

                if (should_call)
                {
                    s_last_pua_time  = now;
                    s_pua_ever_called = true;
                    DrSafePUAisActivated();
                }
            }
        #endif

    }

#endif  // USE_TAXI