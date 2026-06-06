#include "svmModelAnimator.hpp"
#include "svmError.hpp"

sanModelAnimator::sanModelAnimator(sanXML* pxml, PM* ppm, VEHICLESIGNAL* vehicleSignal)
{
	m_mm = glm::mat4(1.0f);
	m_pShader = nullptr;
	m_pModel = nullptr;
	m_pxml = pxml;
	m_ppm = ppm;
	m_pvehicleSignal = vehicleSignal;
}

sanModelAnimator::~sanModelAnimator()
{
	if (!m_localToBonesTransformationsMap.empty())  m_localToBonesTransformationsMap.clear(); else noop;
	if (!m_animationTimestampMap.empty()) m_animationTimestampMap.clear(); else noop;

	if (m_pModel != nullptr)
	{
		delete m_pModel;
		m_pModel = nullptr;
	} else noop;

	if (m_pShader != nullptr)
	{
		delete m_pShader;
		m_pShader = nullptr;
	} else noop;	
}


void sanModelAnimator::initialize()
{
	try
	{
		string model_file_fullname = string(_MODELS_PATH_) + string("/") + string(m_pxml->m_model.model_file);

		if(m_pShader == nullptr)
		{
			m_pShader = new sanShader(vs_view_model, NULL, fs_view_model);
		}

		if(m_pModel == nullptr)
		{
			m_pModel = new sanModel(m_pxml, m_pvehicleSignal, model_file_fullname);
		}

		this->m_mm = calcModelMatrix() * this->m_pModel->m_rootNode.transformation;

		for (const ANIMATIONDATA& m : this->m_pModel->m_animations)
			m_animationTimestampMap[m.animationName] = 0.0f;

		// light uniform for model shading
		int lightType = LIGHTTYPE::VIEW_LIGHT;
		m_pShader->use();
		m_pShader->setInt("v_lightType", lightType);		// point lights: 2; dir light: 1; view light: 0 
		m_pShader->setInt("v_renderMode", RENDERMODE::VIEW_COORD);	// in world coordinate: 0; in view (camera) coordinate: 1;
		m_pShader->setInt("lightType", lightType);		// point lights: 2; dir light: 1; view light: 0 
		m_pShader->setInt("renderMode", RENDERMODE::VIEW_COORD);	// in world coordinate: 0; in view (camera) coordinate: 1;

		switch (lightType)
		{
		case LIGHTTYPE::VIEW_LIGHT:
			m_pShader->setVec4("viewLight.ambient", 0.30f, 0.30f, 0.30f, 1.00f);
			m_pShader->setVec4("viewLight.diffuse", 1.00f, 1.00f, 1.00f, 1.00f);
			m_pShader->setVec4("viewLight.specular", 0.20f, 0.20f, 0.20f, 1.00f);
			m_pShader->setFloat("viewLight.constant", 0.50f); // 1.0f
			m_pShader->setFloat("viewLight.linear", 0.090f); // 0.09f
			m_pShader->setFloat("viewLight.quadratic", 0.032f); //0.032f
			m_pShader->setBool("viewLight.attenuate", true);
			break;
		case LIGHTTYPE::DIR_LIGHT:
			m_pShader->setVec4("dirLight.ambient", 0.05f, 0.05f, 0.05f, 1.00f);
			m_pShader->setVec4("dirLight.diffuse", 1.00f, 1.00f, 1.00f, 1.00f);
			m_pShader->setVec4("dirLight.specular", 0.30f, 0.30f, 0.30f, 1.00f);
			m_pShader->setVec3("dirLight.direction", 0.3f, -0.1f, -0.3f);
			break;
		case LIGHTTYPE::POINT_LIGHTS:
			for (unsigned int i = 0; i < N_POINT_LIGHTS; i++)
			{
				std::string attribute_name = "pointLights[" + std::to_string(i) + "]";
				m_pShader->setVec3(attribute_name + ".position", lightPositions[i]);
				m_pShader->setVec4(attribute_name + ".ambient", 0.05f, 0.05f, 0.05f, 1.00f);
				m_pShader->setVec4(attribute_name + ".diffuse", lightColors[i]);
				m_pShader->setVec4(attribute_name + ".specular", 0.3f, 0.3f, 0.3f, 1.00f);
				m_pShader->setFloat(attribute_name + ".constant", 1.0f);
				m_pShader->setFloat(attribute_name + ".linear", 0.09f);
				m_pShader->setFloat(attribute_name + ".quadratic", 0.032f);
			}
			break;
		default:
			throw runtime_error("light type wrong");
			break;
		}

	}
	catch (exception& e)
	{
		throw logger.svm_fatal("C2109001", __FUNCTION__ + delimiter(string(e.what())));
	}
}

void sanModelAnimator::updateModelAnimation(float steering_angle, float vehicle_velocity)
{
	for (const ANIMATIONDATA& m : this->m_pModel->m_animations)
	{
		if (m.animationName.find("Rotating") != std::string::npos && m_pxml->m_activation.model_animation)
		{
			m_animationTimestampMap[m.animationName] += (3600.0f * vehicle_velocity) / m.ticksPerSecond;
			m_animationTimestampMap[m.animationName] = fmod(m_animationTimestampMap[m.animationName], m.duration);
		}
		else if (m.animationName.find("Turning") != std::string::npos)
		{
			m_animationTimestampMap[m.animationName] = m.duration * (steering_angle - m_pxml->m_vehicle_spec.min_steering_angle) / (m_pxml->m_vehicle_spec.max_steering_angle - m_pxml->m_vehicle_spec.min_steering_angle);
		}
		else m_animationTimestampMap[m.animationName] = 0.0f;
	}

	calculateNodeTransformation(this->m_pModel->m_rootNode, this->m_pModel->m_rootNode.parentTransformation);
}

void sanModelAnimator::calculateNodeTransformation(NODEDATA nodeData, glm::mat4 parentTransform)
{
	//glm::mat4 localTransformation = nodeData.transformation
	glm::mat4 localTransformation = glm::mat4(1.0f);

	#if(DEBUGGING_MODEL_ANIMATOR)
		printf("------------------------------------------------------\n");
		sanIO::print_mat4("parent in global", parentTransform);
		sanIO::print_mat4(nodeData.nodeName + " in local (w.r.t parent)", nodeData.transformation);
	#endif
	// get bone with same name with node name in the map
	// if not existed, then no local animation of this bone
	// if existed, this bone has animation, and vertex position and norm calculated w.r.t this bone

	if (this->m_pModel->m_boneAnimationMap.find(nodeData.nodeName) != this->m_pModel->m_boneAnimationMap.end())
	{
		Bone one_bone = this->m_pModel->m_boneAnimationMap[nodeData.nodeName];
		bool translated = false;

		for (ANIMATIONDATA m : this->m_pModel->m_animations)
		{
			string animationName = m.animationName;
			// check again the animation name of this bone
			auto it = find_if(one_bone.boneAnimations.begin(), one_bone.boneAnimations.end(), [&animationName](const BoneAnimation& obj) {return obj.animationName == animationName; });
			if (it != one_bone.boneAnimations.end())
			{
				BoneAnimation boneAnimation = one_bone.boneAnimations[std::distance(one_bone.boneAnimations.begin(), it)];
				glm::mat4 translation = getInterpolatedPosition(boneAnimation, m_animationTimestampMap[m.animationName]);
				glm::mat4 rotation = getInterpolatedRotation(boneAnimation, m_animationTimestampMap[m.animationName]);
				glm::mat4 scale = getInterpolatedScaling(boneAnimation, m_animationTimestampMap[m.animationName]);

				#if(DEBUGGING_MODEL_ANIMATOR)
					cout << "=====================================" << endl;
					cout << nodeData.nodeName << " - " << animationName << endl;
					sanIO::print_mat4("translation", translation);
					sanIO::print_mat4("rotation", rotation);
					sanIO::print_mat4("scale", scale);
				#endif

				//localTransformation = translation * rotation * scale;
				localTransformation *= (translated ? glm::mat4(1.0f) : translation) * rotation * scale;
				translated = true;
			}
			else noop;
		}
	}
	else noop;

	glm::mat4 hierarchicalTransformation = parentTransform * localTransformation;

	#if(DEBUGGING_MODEL_ANIMATOR)
		sanIO::print_mat4("local (by animation)", localTransformation);
		sanIO::print_mat4("hiera (w.r.t global) = parent*local", hierarchicalTransformation);
	#endif

	// update localToBonesTransformation matrix for shader
	if (this->m_pModel->m_boneAnimationMap.find(nodeData.nodeName) != this->m_pModel->m_boneAnimationMap.end())
	{
		Bone one_bone = this->m_pModel->m_boneAnimationMap[nodeData.nodeName];
		m_localToBonesTransformationsMap[one_bone.id] = hierarchicalTransformation * one_bone.offsetMatrix;
	}
	else noop;

	// for node child
	for (int i = 0; i < nodeData.numChildren; i++)
	{
		calculateNodeTransformation(nodeData.children[i], hierarchicalTransformation);
	}
}

glm::mat4 sanModelAnimator::calcModelMatrix()
{
	glm::mat4 T = glm::translate(glm::mat4(1.0f), glm::vec3(m_pxml->m_model.translation.x, m_pxml->m_model.translation.y, m_pxml->m_model.translation.z));
	glm::mat4 Rx = glm::rotate(glm::mat4(1.0f), glm::radians(m_pxml->m_model.rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
	glm::mat4 RxRz = glm::rotate(Rx, glm::radians(m_pxml->m_model.rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));
	glm::mat4 RxRzRy = glm::rotate(RxRz, glm::radians(m_pxml->m_model.rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
	glm::mat4 S = glm::scale(glm::mat4(1.0f), glm::vec3(m_pxml->m_model.scale.x, m_pxml->m_model.scale.y, m_pxml->m_model.scale.z));

	return (T * S * RxRzRy);
}

glm::vec3 sanModelAnimator::getPositionFromViewMatrix(glm::mat4 view_matrix)
{
	// Xc= Rwc * Xw + Tc, Tw = - inv(Rwc)*Tc
	float Tc_x = view_matrix[3][0];
	float Tc_y = view_matrix[3][1];
	float Tc_z = view_matrix[3][2];
	float Tw_x = -1.0f * (view_matrix[0][0] * Tc_x + view_matrix[0][1] * Tc_y + view_matrix[0][2] * Tc_z);
	float Tw_y = -1.0f * (view_matrix[1][0] * Tc_x + view_matrix[1][1] * Tc_y + view_matrix[1][2] * Tc_z);
	float Tw_z = -1.0f * (view_matrix[2][0] * Tc_x + view_matrix[2][1] * Tc_y + view_matrix[2][2] * Tc_z);

	return glm::vec3(Tw_x, Tw_y, Tw_z);
}

void sanModelAnimator::renderModelAnimator(sanShader* pShader, glm::mat4& model_matrix, glm::mat4& view_matrix, glm::mat4& projection_matrix)
{
	sanError::glClearError();

	glEnable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	glBlendFunc(GL_ONE_MINUS_SRC_ALPHA, GL_SRC_ALPHA);
	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);

	try
	{
		m_pModel->renderModel(pShader, model_matrix, view_matrix, projection_matrix);
	}
	catch (exception& e)
	{
		throw runtime_error(__FUNCTION__ + delimiter(string(e.what())));
	}

	glDisable(GL_CULL_FACE);
	glDisable(GL_BLEND);
	glDisable(GL_DEPTH_TEST);

	sanError::glCheckError(__FUNCTION__);
}


void sanModelAnimator::drawLayout0(glm::mat4& view_matrix)
{
	try
	{
		sanError::glClearError();
		
		glViewport(m_pxml->m_layout[0].x, m_pxml->m_layout[0].y, m_pxml->m_layout[0].width, m_pxml->m_layout[0].height);

		updateModelAnimation(m_pvehicleSignal->m_steering_angle, max(-5.0f, (m_pvehicleSignal->m_trigger.gear == GEAR_REVERSE ? -1.0f : 1.0f) * m_pvehicleSignal->m_vehicle_velocity));

		m_pShader->use();
		m_pShader->setFloat("viewLight.constant", 1.00f); // 1.0f
		m_pShader->setVec3("viewPosition", getPositionFromViewMatrix(view_matrix));

		int count = 0;
		for (auto& m : m_localToBonesTransformationsMap)
		{
			std::string attribute_name = "boneIdList[" + std::to_string(count) + "]";
			m_pShader->setInt(attribute_name, m.first);

			attribute_name = "localToBonesTransformations[" + std::to_string(count) + "]";
			m_pShader->setMat4(attribute_name, m.second);

			count++;
		}

		renderModelAnimator(m_pShader, m_mm, view_matrix, m_ppm->opm);

		sanError::glCheckError();
	}
	catch (exception& e)
	{
		if (m_pxml->m_activation.model_animation)
		{
			m_pxml->m_activation.model_animation = false;
			runtime_error  err_msg = logger.svm_fatal("C2209101", __FUNCTION__ + delimiter(string(e.what())) + string(". Then it was automatically disable."));
			logger.record_message(err_msg.what());
		}
		else noop;
	}
}

void sanModelAnimator::drawLayout1(glm::mat4& view_matrix)
{
	try
	{
		switch (m_pxml->m_layout[1].view_mode)
		{
		case CAMVIEW3D_FRONT:
		case CAMVIEW3D_RIGHT:
		case CAMVIEW3D_REAR:
		case CAMVIEW3D_LEFT:
		{
			sanError::glClearError();

			glViewport(m_pxml->m_layout[1].x, m_pxml->m_layout[1].y, m_pxml->m_layout[1].width, m_pxml->m_layout[1].height);

			m_pShader->use();
			m_pShader->setFloat("viewLight.constant", 0.50f); // 1.0f
			int count = 0;
			for (auto& m : m_localToBonesTransformationsMap)
			{
				std::string attribute_name = "boneIdList[" + std::to_string(count) + "]";
				m_pShader->setInt(attribute_name, m.first);

				attribute_name = "localToBonesTransformations[" + std::to_string(count) + "]";
				m_pShader->setMat4(attribute_name, m.second);

				count++;
			}

			m_pShader->setVec3("viewPosition", getPositionFromViewMatrix(view_matrix));
			renderModelAnimator(m_pShader, m_mm, view_matrix, m_ppm->ppm);
			sanError::glCheckError();
			break;
		}
		case CAMVIEW2D_FRONT:
		case CAMVIEW2D_RIGHT:
		case CAMVIEW2D_REAR:
		case CAMVIEW2D_LEFT:
		case CAMVIEW2D_ADD0:
			noop;
			break;
		default:
			throw runtime_error("view_mode wrong");
			break;
		}
	}
	catch (exception& e)
	{
		if (m_pxml->m_activation.model_animation)
		{
			m_pxml->m_activation.model_animation = false;
			runtime_error  err_msg = logger.svm_fatal("C2209201", __FUNCTION__ + delimiter(string(e.what())) + string(". Then it was automatically disable."));
			logger.record_message(err_msg.what());
		}
		else noop;
	}
}

float sanModelAnimator::getScaleFactor(float lastTimeStamp, float nextTimeStamp, float animationTime) // ms
{
	float midWayLength = animationTime - lastTimeStamp;
	float framesDifference = nextTimeStamp - lastTimeStamp;

	return (fabs(framesDifference) <= FLT_EPSILON)? midWayLength: (midWayLength / framesDifference);
}

int sanModelAnimator::getPositionIndex(BoneAnimation boneAnimation, float animationTime)
{
	int result_index = 0;

	for (int index = 0; index < boneAnimation.numPositionKeys - 1; ++index)
	{
		if (animationTime < boneAnimation.positionKeys[index + 1].timeStamp)
		{
			result_index = index;
			break;
		}
		else noop;
	}
	return result_index;
}

int sanModelAnimator::getRotationIndex(BoneAnimation boneAnimation, float animationTime)
{
	int result_index = 0;

	for (int index = 0; index < boneAnimation.numRotationKeys - 1; ++index)
	{
		if (animationTime < boneAnimation.rotationKeys[index + 1].timeStamp)
		{
			result_index = index;
			break;
		}
		else noop;
	}
	return result_index;
}

int sanModelAnimator::getScaleIndex(BoneAnimation boneAnimation, float animationTime)
{
	int result_index = 0;

	for (int index = 0; index < boneAnimation.numScalingKeys - 1; ++index)
	{
		if (animationTime < boneAnimation.scaleKeys[index + 1].timeStamp)
		{
			result_index = index;
			break;
		}
		else noop;
	}
	return result_index;
}

glm::mat4 sanModelAnimator::getInterpolatedPosition(BoneAnimation boneAnimation, float animationTime)
{
	glm::mat4 translationMatrix(1.0f);

	if (1 == boneAnimation.numPositionKeys)
		translationMatrix = glm::translate(glm::mat4(1.0f), boneAnimation.positionKeys[0].position);
	else
	{
		int p0Index = getPositionIndex(boneAnimation, animationTime);
		int p1Index = p0Index + 1;
		float scaleFactor = getScaleFactor(boneAnimation.positionKeys[p0Index].timeStamp, boneAnimation.positionKeys[p1Index].timeStamp, animationTime);
		glm::vec3 finalPosition = glm::mix(boneAnimation.positionKeys[p0Index].position, boneAnimation.positionKeys[p1Index].position, scaleFactor); // glm::mix() - linear interpolation

		translationMatrix = glm::translate(glm::mat4(1.0f), finalPosition);
	}
	return translationMatrix;
}

glm::mat4 sanModelAnimator::getInterpolatedRotation(BoneAnimation boneAnimation, float animationTime)
{
	glm::mat4 rotationMatrix(1.0f);

	if (1 == boneAnimation.numRotationKeys)
		rotationMatrix = glm::toMat4(glm::normalize(boneAnimation.rotationKeys[0].rotation));
	else
	{
		int p0Index = getRotationIndex(boneAnimation, animationTime);
		int p1Index = p0Index + 1;
		float scaleFactor = getScaleFactor(boneAnimation.rotationKeys[p0Index].timeStamp, boneAnimation.rotationKeys[p1Index].timeStamp, animationTime);
		glm::quat finalRotation = glm::slerp(boneAnimation.rotationKeys[p0Index].rotation, boneAnimation.rotationKeys[p1Index].rotation, scaleFactor); // glm::slerp() - Spherical Linear Interpolation(Quarternion)
		finalRotation = glm::normalize(finalRotation);
		rotationMatrix = glm::toMat4(finalRotation);
	}
	return rotationMatrix;
}

glm::mat4 sanModelAnimator::getInterpolatedScaling(BoneAnimation boneAnimation, float animationTime)
{
	glm::mat4 scalingMatrix(1.0f);

	if (1 == boneAnimation.numScalingKeys)
		scalingMatrix = glm::scale(glm::mat4(1.0f), boneAnimation.scaleKeys[0].scale);
	else
	{
		int p0Index = getScaleIndex(boneAnimation, animationTime);
		int p1Index = p0Index + 1;
		float scaleFactor = getScaleFactor(boneAnimation.scaleKeys[p0Index].timeStamp, boneAnimation.scaleKeys[p1Index].timeStamp, animationTime);
		glm::vec3 finalScale = glm::mix(boneAnimation.scaleKeys[p0Index].scale, boneAnimation.scaleKeys[p1Index].scale, scaleFactor);
		scalingMatrix = glm::scale(glm::mat4(1.0f), finalScale);
	}
	return scalingMatrix;
}

