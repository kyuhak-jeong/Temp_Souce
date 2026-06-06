#ifndef SVMFROMFILE_HPP_
#define SVMFROMFILE_HPP_

#include "svmCore.hpp"
#include "svmLogger.hpp"

class sanFromFile
{
public:
	static Mat read_image(string full_file_path, ImreadModes cv_image_mode);

	static void write_image(string full_file_path, cv::Mat& image);

	static TEXTURE_INFO read_texture_from_file(string full_file_path, bool hflop=false, bool vflop=false);
	static std::map<float, float> read_map(string full_file_path);

	static int read_mesh(string full_file_path, GLfloat** vertitces /*[out]*/);

	static vector<Point2f> read_point_2d(string full_file_path);
	static vector<Point3f> read_point_3d(string full_file_path);

	static std::string read_text(string full_file_path);
};

#endif
