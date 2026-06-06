#ifndef SVMCAMVEC_HPP_
#define SVMCAMVEC_HPP_

#define _USE_MATH_DEFINES
#include <math.h>
#include <float.h>
#include <memory.h>
#include <malloc.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "svmCore.hpp"

#define PF               (10000000000.0F)  // precision factor
#define TINY			 (1.0e-20)

glm::mat4 VM4x4(glm::vec3 vcamPos/*[in]*/, glm::vec3 vcamDir/*[in]*/);
glm::mat4 invVM4x4(glm::vec3 vcamPos/*[in]*/, glm::vec3 vcamDir/*[in]*/);
glm::mat4 LookAt(glm::vec3 camera_position/*[in]*/, glm::vec3 target_position/*[in]*/);
glm::vec3 get_intersection_with_ground(glm::vec3 camera_pos, glm::vec3 camera_dir); /* extrinsic parameter */
glm::mat3 rotate_around_axis(glm::vec3 rotation_vec /*unit vector*/, float rotation_ang /* radian */);

glm::mat4 EPtoVM4x4(glm::vec3 vcamPos/*[in]*/, glm::vec3 vcamDir/*[in]*/);
void VM4x4toEP(glm::mat4 Vwc/* [in]*/, glm::vec3& camPos/*[out]*/, glm::vec3& camDir/* [out] */);

#endif