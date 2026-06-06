#ifndef SVMMODELANIMATOR_HPP_
#define SVMMODELANIMATOR_HPP_

#include <glm/glm.hpp>
#include <map>
#include <vector>
#include <assimp/scene.h>
#include <assimp/Importer.hpp>
#include <iomanip>

#include "svmXML.hpp"
#include "svmShaderString.hpp"
#include "svmModel.hpp"
#include "svmIO.hpp"

#define DEBUGGING_MODEL_ANIMATOR		(0)
#define N_POINT_LIGHTS					(5)

class sanModelAnimator
{
public:
	sanXML* m_pxml;
	VEHICLESIGNAL* m_pvehicleSignal;
	PM* m_ppm;	// projection matrix
	glm::mat4							m_mm;	// model matrix

	sanShader* m_pShader;
	sanModel* m_pModel;

	std::map<unsigned int, glm::mat4>	m_localToBonesTransformationsMap;
	std::map<string, float>				m_animationTimestampMap;

public:
	sanModelAnimator(sanXML* pxml, PM* ppm, VEHICLESIGNAL* vehicleSignal);
	~sanModelAnimator();

	void initialize();
	void updateModelAnimation(float steering_angle, float vehicle_velocity);
	void calculateNodeTransformation(NODEDATA nodeData, glm::mat4 parentTransform);
	glm::mat4 calcModelMatrix();
	glm::vec3 getPositionFromViewMatrix(glm::mat4 view_matrix);
	void drawLayout0(glm::mat4& view_matrix);
	void drawLayout1(glm::mat4& view_matrix);

	void renderModelAnimator(sanShader* pShader, glm::mat4& model_matrix, glm::mat4& view_matrix, glm::mat4& projection_matrix);

private:

	typedef enum { VIEW_LIGHT = 0, DIR_LIGHT = 1, POINT_LIGHTS = 2,} LIGHTTYPE;
	typedef enum {WORLD_COORD = 0, VIEW_COORD = 1,} RENDERMODE;

	glm::vec3 lightPositions[N_POINT_LIGHTS] = {
		glm::vec3(0.0f,   0.0f,  4.0f),
		glm::vec3(0.6f,   1.2f,  0.5f),
		glm::vec3(0.6f,  -1.2f,  0.5f),
		glm::vec3(-0.6f,  1.2f,  0.5f),
		glm::vec3(-0.6f, -1.2f,  0.5f),
	};

	glm::vec4 lightColors[N_POINT_LIGHTS] = {
		glm::vec4(1.00f,  1.00f,  1.00f,  1.00f),
		glm::vec4(1.00f,  1.00f,  1.00f,  1.00f),
		glm::vec4(1.00f,  1.00f,  1.00f,  1.00f),
		glm::vec4(1.00f,  1.00f,  1.00f,  1.00f),
		glm::vec4(1.00f,  1.00f,  1.00f,  1.00f),
	};

	float getScaleFactor(float lastTimeStamp, float nextTimeStamp, float animationTime); // ms
	int getPositionIndex(BoneAnimation boneAnimation, float animationTime);
	int getRotationIndex(BoneAnimation boneAnimation, float animationTime);
	int getScaleIndex(BoneAnimation boneAnimation, float animationTime);
	glm::mat4 getInterpolatedPosition(BoneAnimation boneAnimation, float animationTime);
	glm::mat4 getInterpolatedRotation(BoneAnimation boneAnimation, float animationTime);
	glm::mat4 getInterpolatedScaling(BoneAnimation boneAnimation, float animationTime);
};

#endif