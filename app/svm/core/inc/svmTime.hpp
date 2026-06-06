#ifndef SVMTIME_HPP_
#define SVMTIME_HPP_

#include "svmCore.hpp"

class sanTime
{
public:
	static string timezone(time_t when=-1);
	//static string get_string_time();
	static string get_string_time(int time_format_type=0, bool with_timezone=false);
	
};

#endif
