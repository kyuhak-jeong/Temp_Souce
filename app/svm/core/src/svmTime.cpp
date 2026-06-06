#include "svmTime.hpp"

string sanTime::timezone(time_t when)
{
    std::ostringstream os;

    if (when == -1) when = std::time(nullptr);
    else noop;

#if defined(_WIN32) || defined(_WIN64)
    struct tm stime;
    localtime_s(&stime, &when);
    os << std::put_time(&stime, "%z");
#else
    struct tm* stime = localtime(&when);
    os << std::put_time(stime, "%z");
#endif

    std::string s = os.str();
    return "UTC" + s.substr(0, 3) + ":" + s.substr(3);
}

string sanTime::get_string_time(int time_format_type, bool with_timezone)
{
    char str_time[256];
    time_t now = time(NULL);
    memset(str_time, 0x00, sizeof(str_time));
    string time_format = "%G-%m-%d %H:%M:%S";

    switch (time_format_type)
    {
    case 0:time_format = "%G-%m-%d %H:%M:%S"; break;
    case 1:time_format = "%G%m%d"; break;
    default: break;
    }

#if defined(_WIN32) || defined(_WIN64)
    struct tm stime;
    localtime_s(&stime, &now);
    std::strftime(str_time, sizeof(str_time), time_format.c_str(), &stime);
#else
    struct tm* pstime = localtime(&now);
    std::strftime(str_time, sizeof(str_time), time_format.c_str(), pstime);
#endif

    return (with_timezone)? (string(str_time) + string(" ") + sanTime::timezone(now)): string(str_time);
}

