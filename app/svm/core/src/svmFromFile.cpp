#include "svmFromFile.hpp"

cv::Mat sanFromFile::read_image(string full_file_path, ImreadModes cv_image_mode)
{
	cv::Mat image = cv::imread(full_file_path, cv_image_mode);
	if (image.empty())
	{
		string msg = string("$cannot read the image file ") + full_file_path;
		throw runtime_error(__FUNCTION__ + delimiter(msg));
	}
	else noop;

	return image;
}

void sanFromFile::write_image(string full_file_path, cv::Mat& image)
{
	bool check_flag = cv::imwrite(full_file_path, image);
	if (!check_flag)
	{
		string msg = string("$cannot write an image to ") + full_file_path;
		throw runtime_error(__FUNCTION__ + delimiter(msg));
	}
	else noop;
}

int sanFromFile::read_mesh(string full_file_path /*[in]*/, GLfloat** vertices /*[out]*/)
{	
	int vertices_num = 0;
	ifstream mesh_file(full_file_path.c_str());
	if (mesh_file.is_open())
	{
		vertices_num = (int)count(std::istreambuf_iterator<char>(mesh_file), std::istreambuf_iterator<char>(), '\n');
		if (0 < vertices_num)
		{
			mesh_file.clear();
			mesh_file.seekg(0, ios::beg);
			*vertices = (GLfloat*)malloc((size_t)vertices_num * 5 * sizeof(GLfloat));
			if (*vertices == nullptr)
			{
				mesh_file.close();
				string msg = string("$failed to allocate memory");
				throw runtime_error(__FUNCTION__ + delimiter(msg));
			}
			else
			{
				for (int k = 0; k < vertices_num * 5; k++)
					mesh_file >> (*vertices)[k];

				mesh_file.close();
			}
		}
		else
		{
			mesh_file.close();
			string msg = string("$empty file ") + full_file_path;
			throw runtime_error(__FUNCTION__ + delimiter(msg));
		}
	}
	else
	{
		mesh_file.close();
		string msg = string("$cannot read the mesh file ") + full_file_path;
		throw runtime_error(__FUNCTION__ + delimiter(msg));
	}

	return vertices_num;
}

TEXTURE_INFO sanFromFile::read_texture_from_file(string full_file_path, bool hflip, bool vflip)
{
	TEXTURE_INFO texture_info;

	texture_info.texture = cv::imread(full_file_path, cv::IMREAD_UNCHANGED);
	if (texture_info.texture.empty())
	{
		string msg = string("$cannot read the texture file ") + full_file_path;
		throw runtime_error(__FUNCTION__ + delimiter(msg));
	}
	else
	{
		texture_info.width = texture_info.texture.cols;
		texture_info.height = texture_info.texture.rows;
		texture_info.nchannel = texture_info.texture.channels();
		texture_info.color_format = (texture_info.nchannel == 1) ? GL_RED : (texture_info.nchannel == 4) ? GL_RGBA : GL_RGB;

		if (hflip) cv::flip(texture_info.texture, texture_info.texture, 1);
		else noop;

		if (vflip)	cv::flip(texture_info.texture, texture_info.texture, 0);
		else noop;

		switch (texture_info.nchannel)
		{
		case 3:
			cv::cvtColor(texture_info.texture, texture_info.texture, cv::COLOR_BGR2RGB);
			break;
		case 4:
			cv::cvtColor(texture_info.texture, texture_info.texture, cv::COLOR_BGRA2RGBA);
			break;
		default:
			string msg = string("$wrong number of channels in ") + full_file_path;
			throw runtime_error(__FUNCTION__ + delimiter(msg));
			break;
		}
	}

	return texture_info;
}


std::map<float, float> sanFromFile::read_map(string full_file_path)
{
	std::map<float, float> map_data;

	ifstream map_file(full_file_path.c_str());
	if (map_file.is_open())
	{
		float key=0.0f, value=0.0f;
		while (map_file >> key >> value) 
			map_data[key] = value;

		map_file.close();
	}
	else
	{
		string msg = string("$cannot read the file ") + full_file_path;
		throw runtime_error(__FUNCTION__ + delimiter(msg));
	}

	return map_data;
}

vector<Point2f> sanFromFile::read_point_2d(string full_file_path)
{
	vector<Point2f> pt2d;

	ifstream data_file(full_file_path.c_str());
	if (data_file.is_open())
	{
		float x = 0.0f, y = 0.0f;
		while (data_file >> x >> y)
			pt2d.push_back(Point2f(x, y));

		data_file.close();
	}
	else
	{
		string msg = string("$cannot read the file ") + full_file_path;
		throw runtime_error(__FUNCTION__ + delimiter(msg));
	}
	
	return pt2d;
}

vector<Point3f> sanFromFile::read_point_3d(string full_file_path)
{
	vector<Point3f> pt3d;

	ifstream data_file(full_file_path.c_str());
	if (data_file.is_open())
	{
		float x = 0.0f, y = 0.0f, z = 0.0f;
		while (data_file >> x >> y >> z)
			pt3d.push_back(Point3f(x, y, z));

		data_file.close();
	}
	else
	{
		string msg = string("$cannot read the file ") + full_file_path;
		throw runtime_error(__FUNCTION__ + delimiter(msg));
	}

	return pt3d;
}

std::string sanFromFile::read_text(string full_file_path)
{
	std::string ret = "";
	std::ifstream txtFile;
	txtFile.exceptions (std::ifstream::failbit | std::ifstream::badbit);

	try 
	{
		// open files
		txtFile.open(full_file_path);
		std::stringstream txtStream;
		txtStream << txtFile.rdbuf();
		txtFile.close();
		ret = txtStream.str();
	}
	catch (std::ifstream::failure& e)
	{
		std::cout << "can't read a text file: " << e.what() << std::endl;
	}
	
	return ret;
}
