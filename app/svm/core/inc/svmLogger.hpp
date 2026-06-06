#ifndef SVMLOGGER_HPP_
#define SVMLOGGER_HPP_

#include <iomanip>
#include <iostream>
#include <fstream>
#include <cstdarg>
#include <ctime>
#include <sstream>
#include <cstring>
#include <cstdio>

#define LOG_LEVEL_OFF     (0)
#define LOG_LEVEL_FATAL  (10)
#define LOG_LEVEL_ERROR  (20)
#define LOG_LEVEL_WARN   (30)
#define LOG_LEVEL_INFO   (40)
#define LOG_LEVEL_DEBUG  (50)
#define LOG_LEVEL_TRACE  (60)
#define LOG_LEVEL_ALL   (100)

#define svm_fatal(ecode, msg) packing_message(LOG_LEVEL_FATAL, __FILE__, __LINE__, ecode, msg)
#define svm_error(ecode, msg) packing_message(LOG_LEVEL_ERROR, __FILE__, __LINE__, ecode, msg)
#define svm_warn(ecode,  msg) packing_message(LOG_LEVEL_WARN,  __FILE__, __LINE__, ecode, msg)
#define svm_inform(ecode,  msg) packing_message(LOG_LEVEL_INFO,  __FILE__, __LINE__, ecode, msg)
#define svm_debug(ecode, msg) packing_message(LOG_LEVEL_DEBUG, __FILE__, __LINE__, ecode, msg)


using namespace std;

class sanLogger 
{
private:
    int logLevel;

public:
    sanLogger();
    sanLogger(int level);
    void record_message(const string msg);
	runtime_error packing_message(int log_level, const char* full_file_path, int line_mum, string error_code, string msg);
};

static sanLogger logger;

#endif
