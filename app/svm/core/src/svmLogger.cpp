#include "svmCore.hpp"
#include "svmTime.hpp"
#include "svmLogger.hpp"

sanLogger::sanLogger()
{
    this->logLevel = LOG_LEVEL_FATAL;
}

sanLogger::sanLogger(int level = LOG_LEVEL_FATAL)
{
    this->logLevel = level;
}

void sanLogger::record_message(const string msg)
{
    try
    {
        std::string dir = std::string(_LOGS_PATH_);
		if(!std::filesystem::is_directory(dir)) 
		{
            #if defined(_WIN32) || defined(_WIN64)
                bool status = _mkdir(dir.c_str());
            #else
                bool status = mkdir(dir.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
            #endif
			if (status != 0 && errno != EEXIST) 
			{
				throw runtime_error(std::string("Error creating directory: " + dir));
			}
		}

        fstream fs;
        string full_filename = dir + string("/log") + sanTime::get_string_time(1, false) + string(".txt");

        fs.open(full_filename, ios_base::out | ios_base::app);
        if (fs.is_open())
        {
            fs << msg << endl;
            fs.close();
        }
        else
        {
            string errmsg = string("$cannot record a message to the logging file ") + full_filename;
            throw runtime_error(errmsg);
        }
    }
    catch (Exception& e)
    {
        throw runtime_error(__FUNCTION__ + delimiter(string(e.what())));
    }
}


runtime_error sanLogger::packing_message(int log_level, const char* full_file_path, int line_num, string error_code = string(), string msg = string())
{
    char log_data[4000];
    char log_level_name[10];

    memset(log_data, 0x00, sizeof(log_data));
    memset(log_level_name, 0x00, sizeof(log_level_name));

    string full_file_name = string(full_file_path);
    int find = (int)full_file_name.rfind("\\") + 1;
    string pure_file_name = full_file_name.substr(find, full_file_name.length() - find);
    string string_time = sanTime::get_string_time(0, false);

#if defined(_WIN32)||defined(_WIN64)
    switch (log_level)
    {
    case LOG_LEVEL_FATAL: strcpy_s(log_level_name, "[FATAL]"); break; // when a system is down
    case LOG_LEVEL_ERROR: strcpy_s(log_level_name, "[ERROR]"); break; // when an operation or a function makes problem
    case LOG_LEVEL_WARN:  strcpy_s(log_level_name, "[warn ]"); break; // when it has a posibillity of something wrong
    case LOG_LEVEL_INFO:  strcpy_s(log_level_name, "[info ]"); break;
    case LOG_LEVEL_DEBUG: strcpy_s(log_level_name, "[debug]"); break;
    case LOG_LEVEL_TRACE: strcpy_s(log_level_name, "[trace]"); break;
    default: break;
    }
    sprintf_s(log_data, "%7s %s %20s %8d %12s     @%s", log_level_name, string_time.c_str(), pure_file_name.c_str(), line_num, error_code.c_str(), msg.c_str());
#else
    switch(log_level)
    {
	case LOG_LEVEL_FATAL: strcpy(log_level_name, "[FATAL]"); break;
	case LOG_LEVEL_ERROR: strcpy(log_level_name, "[ERROR]"); break;
	case LOG_LEVEL_WARN:  strcpy(log_level_name, "[warn ]"); break;
	case LOG_LEVEL_INFO:  strcpy(log_level_name, "[info ]"); break;
	case LOG_LEVEL_DEBUG: strcpy(log_level_name, "[debug]"); break;
	case LOG_LEVEL_TRACE: strcpy(log_level_name, "[trace]"); break;
    default: break;
    }
    sprintf(log_data, "%7s %s %20s %8d %12s     @%s", log_level_name, string_time.c_str(), pure_file_name.c_str(), line_num, error_code.c_str(), msg.c_str());
#endif

	return runtime_error(log_data);
}