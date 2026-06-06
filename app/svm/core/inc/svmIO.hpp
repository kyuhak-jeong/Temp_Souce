#ifndef SVMIO_HPP_
#define SVMIO_HPP_

#include "svmCore.hpp"

class sanIO
{
public:
	static void print_vec3(string vec_name, glm::vec3 v)
	{
		cout << vec_name;
		printf(":\t%8.4f %8.4f %8.4f\n", v[0], v[1], v[2]);
	}

	static void print_vec4(string vec_name, glm::vec4 v)
	{
		cout << vec_name;
		printf(":\t%8.4f %8.4f %8.4f %8.4f\n", v[0], v[1], v[2], v[3]);
	}

	static void print_mat3(string matrix_name, glm::mat3 m)
	{
		cout << matrix_name << endl;
		printf("%8.4f %8.4f %8.4f\n", m[0][0], m[1][0], m[2][0]);
		printf("%8.4f %8.4f %8.4f\n", m[0][1], m[1][1], m[2][1]);
		printf("%8.4f %8.4f %8.4f\n", m[0][2], m[1][2], m[2][2]);
	}

	static void print_mat4(string matrix_name, glm::mat4 m)
	{
		cout << matrix_name << endl;
		printf("%8.4f %8.4f %8.4f %8.4f\n", m[0][0], m[1][0], m[2][0], m[3][0]);
		printf("%8.4f %8.4f %8.4f %8.4f\n", m[0][1], m[1][1], m[2][1], m[3][1]);
		printf("%8.4f %8.4f %8.4f %8.4f\n", m[0][2], m[1][2], m[2][2], m[3][2]);
		printf("%8.4f %8.4f %8.4f %8.4f\n", m[0][3], m[1][3], m[2][3], m[3][3]);
	}

	static void print_cvMat(string matrix_name, Mat m)
	{
		cout << matrix_name << endl;
		Mat1f mm(m);

		for (int i = 0; i < mm.size().height; i++)
		{
			for (int j = 0; j < mm.size().width; j++)
			{
				printf("%8.4f ", mm(i, j));
			}
			printf("\n");
		}
	}

	template <typename K, typename V>
	static void print_map(std::map<K, V>& m) 
	{
		for (typename std::map<K, V>::iterator itr = m.begin(); itr != m.end(); ++itr)
		{
			std::cout <<"key: " << itr->first << endl;

			if (1)
			{
				std::cout << "id: " << itr->second.id << endl;
				print_mat4("offset matrix: ", itr->second.offsetMatrix);
			}
			else
			{
				print_mat4("matrix: ", itr->second);
			}
			
		}
	}
};

#endif 
