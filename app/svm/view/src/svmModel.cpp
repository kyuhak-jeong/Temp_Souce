#include "assimp_glm_helpers.hpp"
#include "svmModel.hpp"
#include "svmFromFile.hpp"
#include "svmIO.hpp"
#include "svmVABT.hpp"

sanModel::sanModel(sanXML* pxml, VEHICLESIGNAL *vehicleSignal, string model_full_filename)
{	
	m_pxml = pxml;
	m_pvehicleSignal = vehicleSignal;
	m_model_full_filename = model_full_filename;
	m_boneCounter = 0;

	unsigned int options = MODEL_IMPORT_OPTIMIZED | MODEL_IMPORT_REMOVED;
	unsigned int processing_flags = 
							aiProcess_JoinIdenticalVertices |	// join identical vertices/ optimize indexing
							aiProcess_Triangulate |				// Ensure all verticies are triangulated (each 3 vertices are triangle)
							aiProcess_GenSmoothNormals |		// normal vectices
							aiProcess_CalcTangentSpace |		// calculate tangents and bitangents if possible
							aiProcess_SortByPType |				// ?
							aiProcess_ValidateDataStructure |	// perform a full validation of the loader's output
							aiProcess_ImproveCacheLocality |	// improve the cache locality of the output vertices	
							aiProcess_RemoveRedundantMaterials |// remove redundant materials
							aiProcess_FindDegenerates |			// remove degenerated polygons from the import
							aiProcess_GenUVCoords |				// convert spherical, cylindrical, box and planar mapping to proper UVs							
							aiProcess_TransformUVCoords |		// preprocess UV transformations (scaling, translation ...)
							aiProcess_FindInstances |			// search for instanced meshes and remove them by references to one master
							aiProcess_LimitBoneWeights |		// limit bone weights to 4 per vertex
							aiProcess_SplitByBoneCount |		// split meshes with too many bones. Necessary for our (limited) hardware skinning shader
							//aiProcess_PreTransformVertices |
							0;

	unsigned int removal_flags = 0;
	if (options & MODEL_IMPORT_REMOVED)
		removal_flags |= aiComponent_COLORS | aiComponent_LIGHTS | aiComponent_CAMERAS;
	else noop;

	if (options & MODEL_IMPORT_OPTIMIZED)
		processing_flags |=  aiProcess_OptimizeMeshes | aiProcess_RemoveComponent;
	else noop;
	
	try
	{
		m_importer.SetPropertyInteger(AI_CONFIG_PP_SBP_REMOVE, aiPrimitiveType_POINT | aiPrimitiveType_LINE);
		m_importer.SetPropertyInteger(AI_CONFIG_PP_LBW_MAX_WEIGHTS, 4);
		m_importer.SetPropertyInteger(AI_CONFIG_PP_RVC_FLAGS, removal_flags);
		m_scene = (aiScene*)m_importer.ReadFile(model_full_filename, processing_flags);

		if (m_scene && m_scene->mRootNode)
		{
			#if (MODEL_DEBUGGING)
				cout << endl << "------------------------------------" << " for meshes" << endl;
			#endif

			if (!m_scene->HasAnimations())
			{
				processing_flags |= aiProcess_PreTransformVertices;
				m_importer.SetPropertyBool(AI_CONFIG_PP_PTV_KEEP_HIERARCHY, true);
				m_scene = m_importer.ApplyPostProcessing(aiProcess_PreTransformVertices);
			}
			else noop;

			m_boneCounter = 0;
			processAnimation(m_scene, m_animations /*[out]*/, m_boneAnimationMap /*[out]*/);
			processNode(m_scene, m_scene->mRootNode, m_rootNode, m_rootNode /*[out]*/);

			#if (MODEL_DEBUGGING) // for debugging
				sanIO::print_map<string, Bone>(m_boneAnimationMap);
			#endif

			//cout << "Loaded model succesfully. " << (this->m_fastRender ? "Fast render!" : "Normal render!") << endl;
		}
		else
			throw runtime_error(string("cannot import the model file - ") + model_full_filename);
	}
	catch (exception& e)
	{
		throw runtime_error(__FUNCTION__ + delimiter(string(e.what())));
	}
}



sanModel::~sanModel()
{
	if (!m_boneAnimationMap.empty()) m_boneAnimationMap.clear(); else noop;
	
	for (int i = 0; i < (int)m_loadedTextures.size(); i++)
	{
		if(m_loadedTextures[i].textureID)
		{
			glDeleteTextures(1, &m_loadedTextures[i].textureID);
			m_loadedTextures[i].textureID = 0;
		}
	}

	if (!m_loadedTextures.empty())
	{
		m_loadedTextures.clear(); 
		vector<TEXTUREDATA>().swap(m_loadedTextures);
	}
	else noop;

	if (!m_meshes.empty())
	{
		for(int i = 0; i < (int)m_meshes.size(); i++)
		{
			for (int j = 0; j < (int)m_meshes[i].textures.size(); j++)
			{
				if(m_meshes[i].textures[j].textureID)
				{
					glDeleteTextures(1, &m_meshes[i].textures[j].textureID);
					m_meshes[i].textures[j].textureID = 0;
				}
			}

			if (!m_meshes[i].textures.empty())
			{
				m_meshes[i].textures.clear(); 
				vector<TEXTUREDATA>().swap(m_meshes[i].textures);
			}

			if(!m_meshes[i].indices.empty()) m_meshes[i].indices.clear();
			if(!m_meshes[i].vertices.empty()) m_meshes[i].vertices.clear();


			if(m_meshes[i].vbo) 
			{
				glDeleteBuffers(1, &m_meshes[i].vbo);
				m_meshes[i].vbo = 0;
			}

			if(m_meshes[i].ebo)
			{
				glDeleteBuffers(1, &m_meshes[i].ebo);
				m_meshes[i].ebo = 0;
			}

			if(m_meshes[i].vao) 
			{
				glDeleteVertexArrays(1, &m_meshes[i].vao);
				m_meshes[i].vao = 0;
			}
			
		}

		m_meshes.clear(); 
		vector<MESHDATA>().swap(m_meshes);
	}
	else noop;
	if (!m_transparent_meshes.empty()) 
	{
		for(int i = 0; i < (int)m_transparent_meshes.size(); i++)
		{
			for (int j = 0; j < (int)m_transparent_meshes[i].textures.size(); j++)
			{
				if(m_transparent_meshes[i].textures[j].textureID)
				{
					glDeleteTextures(1, &m_transparent_meshes[i].textures[j].textureID);
					m_transparent_meshes[i].textures[j].textureID = 0;
				}
			}

			if (!m_transparent_meshes[i].textures.empty())
			{
				m_transparent_meshes[i].textures.clear(); 
				vector<TEXTUREDATA>().swap(m_transparent_meshes[i].textures);
			}

			if(!m_transparent_meshes[i].indices.empty()) m_transparent_meshes[i].indices.clear();
			if(!m_transparent_meshes[i].vertices.empty()) m_transparent_meshes[i].vertices.clear();

			if(m_transparent_meshes[i].vbo) 
			{
				glDeleteBuffers(1, &m_transparent_meshes[i].vbo);
				m_transparent_meshes[i].vbo = 0;
			}

			if(m_transparent_meshes[i].ebo)
			{
				glDeleteBuffers(1, &m_transparent_meshes[i].ebo);
				m_transparent_meshes[i].ebo = 0;
			}

			if(m_transparent_meshes[i].vao) 
			{
				glDeleteVertexArrays(1, &m_transparent_meshes[i].vao);
				m_transparent_meshes[i].vao = 0;
			}
		}

		m_transparent_meshes.clear(); 
		vector<MESHDATA>().swap(m_transparent_meshes);
	}
	else noop;

	if (!m_animations.empty()) 
	{
		m_animations.clear(); 
		vector<ANIMATIONDATA>().swap(m_animations);
	}
	else noop;

	m_importer.FreeScene();

}

bool compareAnimationByName(const ANIMATIONDATA& a, const ANIMATIONDATA& b)
{
	// return larger value
	return a.animationName > b.animationName;
}


void sanModel::processAnimation(const aiScene* scene,
	std::vector<ANIMATIONDATA>& animations /*[out]*/,
	std::map<std::string, Bone>& boneAnimationMap /*[out]*/)
{
	for (unsigned int animationID = 0; animationID < scene->mNumAnimations; animationID++)
	{
		aiAnimation* one_animation = scene->mAnimations[animationID];
		ANIMATIONDATA animationData;
		animationData.animationName = one_animation->mName.data;
		animationData.duration = (float)one_animation->mDuration;
		animationData.ticksPerSecond = (int)one_animation->mTicksPerSecond;
		animationData.numChannels = (int)one_animation->mNumChannels;

		for (unsigned int channelID = 0; channelID < one_animation->mNumChannels; channelID++)
		{
			/*extract channel animation data*/
			aiNodeAnim* one_channel = one_animation->mChannels[channelID];
			BoneAnimation boneAnimation;
			boneAnimation.animationName = animationData.animationName;
			boneAnimation.boneName = one_channel->mNodeName.data;

			// translation
			boneAnimation.numPositionKeys = one_channel->mNumPositionKeys;
			for (int positionID = 0; positionID < boneAnimation.numPositionKeys; positionID++)
			{
				KeyPosition data;
				data.position = AssimpGLMHelpers::GetGLMVec(one_channel->mPositionKeys[positionID].mValue);
				data.timeStamp = (float)one_channel->mPositionKeys[positionID].mTime; // ms
				boneAnimation.positionKeys.push_back(data);
			}
			// rotation
			boneAnimation.numRotationKeys = one_channel->mNumRotationKeys;
			for (int rotationID = 0; rotationID < boneAnimation.numRotationKeys; rotationID++)
			{
				KeyRotation data;
				data.rotation = AssimpGLMHelpers::GetGLMQuat(one_channel->mRotationKeys[rotationID].mValue);
				data.timeStamp = (float)one_channel->mRotationKeys[rotationID].mTime; // ms
				boneAnimation.rotationKeys.push_back(data);
			}
			// scaling
			boneAnimation.numScalingKeys = one_channel->mNumScalingKeys;
			for (int scaleID = 0; scaleID < boneAnimation.numScalingKeys; scaleID++)
			{
				KeyScale data;
				data.scale = AssimpGLMHelpers::GetGLMVec(one_channel->mScalingKeys[scaleID].mValue);
				data.timeStamp = (float)one_channel->mScalingKeys[scaleID].mTime; // ms
				boneAnimation.scaleKeys.push_back(data);
			}

			/*this channel is animation of a bone => update boneAnimationMap*/
			//if (boneAnimationMap.find(boneAnimation.boneName) == boneAnimationMap.end())
			{
				boneAnimationMap[boneAnimation.boneName].id = m_boneCounter++;
				boneAnimationMap[boneAnimation.boneName].boneAnimations.push_back(boneAnimation);
			}

			/*add channel back to animation*/
			animationData.channels.push_back(boneAnimation);
		}

		/*add animation back */
		bool existed = false;
		for (ANIMATIONDATA& m : animations)
		{
			// check if existed then merge
			if (animationData.animationName == m.animationName)
			{
				m.numChannels += animationData.numChannels;
				m.channels.insert(m.channels.end(), animationData.channels.begin(), animationData.channels.end());
				existed = true;
			}
			else noop;
		}

		if (!existed)
			animations.push_back(animationData);
		else noop;
	}

	/* Sort animation by name: decrease to multiply former matrix first => rotate later */
	std::sort(animations.begin(), animations.end(), compareAnimationByName);
}

void sanModel::processNode(const aiScene* scene, aiNode* one_node, NODEDATA parentData, NODEDATA& nodeData /*[out]*/)
{
	// node base data
	nodeData.nodeName = one_node->mName.data;
	nodeData.numChildren = one_node->mNumChildren;
	nodeData.transformation = AssimpGLMHelpers::ConvertMatrixToGLMFormat(one_node->mTransformation);

	if (one_node->mParent != NULL)
	{
		nodeData.parentTransformation = parentData.globalTransformation;
		nodeData.globalTransformation = nodeData.parentTransformation * nodeData.transformation;
	}
	else
	{
		nodeData.parentTransformation = AssimpGLMHelpers::ConvertMatrixToGLMFormat(scene->mRootNode->mTransformation.Inverse());
		nodeData.globalTransformation = glm::mat4(1.0f);
	}


	#if(MODEL_DEBUGGING)
		printf("--------------------------------------------------\n");
		sanIO::print_mat4(nodeData.nodeName, nodeData.transformation);
		sanIO::print_mat4("parent", nodeData.parentTransformation);
		sanIO::print_mat4("global", nodeData.globalTransformation);
	#endif

	// process mesh data
	for (unsigned int meshID = 0; meshID < one_node->mNumMeshes; meshID++)
	{
		aiMesh* one_mesh = scene->mMeshes[one_node->mMeshes[meshID]];
		try
		{
			processMesh(scene, one_mesh, nodeData);
		}
		catch (exception& e)
		{
			throw runtime_error(__FUNCTION__ + delimiter(string(e.what())));
		}
	}

	// process child nodes
	for (unsigned int j = 0; j < one_node->mNumChildren; j++)
	{
		NODEDATA childNodeData;
		processNode(scene, one_node->mChildren[j], nodeData, childNodeData);
		nodeData.children.push_back(childNodeData);
	}

}

void sanModel::processMesh(const aiScene* scene, aiMesh* one_mesh, NODEDATA nodeData)
{
	MESHDATA meshData;
	meshData.nodeName = nodeData.nodeName;
	meshData.meshName = one_mesh->mName.data;

	// extract vertices
	std::vector<Vertex> vertices;

	for (unsigned int i = 0; i < one_mesh->mNumVertices; i++)
	{
		Vertex vertex;
		vertex.position = AssimpGLMHelpers::GetGLMVec(one_mesh->mVertices[i]);
		vertex.normal = AssimpGLMHelpers::GetGLMVec(one_mesh->mNormals[i]);

		if (one_mesh->HasVertexColors(0))
			vertex.color = glm::vec4(one_mesh->mColors[0][i].r, one_mesh->mColors[0][i].g, one_mesh->mColors[0][i].b, one_mesh->mColors[0][i].a);
		else noop;

		if (one_mesh->HasTextureCoords(0))
			vertex.texcoords = glm::vec2(one_mesh->mTextureCoords[0][i].x, one_mesh->mTextureCoords[0][i].y);
		else noop;

		if (one_mesh->HasTangentsAndBitangents())
		{
			vertex.tangent = AssimpGLMHelpers::GetGLMVec(one_mesh->mTangents[i]);
			vertex.bitangent = AssimpGLMHelpers::GetGLMVec(one_mesh->mBitangents[i]);
		}
		else noop;

		for (int k = 0; k < MAX_BONES_PER_VERTEX; k++)		// initialization
		{
			vertex.boneIDs[k] = -1;
			vertex.weights[k] = 0.0f;
		}

		// push back into vertices
		vertices.push_back(vertex);
	}

	// extract bone weights for vertices in this mesh if existed
	if (one_mesh->mNumBones)
	{
		this->m_fastRender = false;

		for (unsigned int boneIndex = 0; boneIndex < one_mesh->mNumBones; boneIndex++)
		{
			// update mesh name and offset matrix for this bone from mesh data
			this->m_boneAnimationMap[one_mesh->mBones[boneIndex]->mName.data].meshName = one_mesh->mName.data;
			this->m_boneAnimationMap[one_mesh->mBones[boneIndex]->mName.data].offsetMatrix =
				AssimpGLMHelpers::ConvertMatrixToGLMFormat(one_mesh->mBones[boneIndex]->mOffsetMatrix);
			this->m_boneAnimationMap[one_mesh->mBones[boneIndex]->mName.data].node = nodeData;

			// update vertices bone ID and weight dependences
			for (unsigned int weightIndex = 0; weightIndex < one_mesh->mBones[boneIndex]->mNumWeights; weightIndex++)
			{
				unsigned int vertexID = one_mesh->mBones[boneIndex]->mWeights[weightIndex].mVertexId;
				for (int i = 0; i < MAX_BONES_PER_VERTEX; i++)
				{
					if (vertices[vertexID].boneIDs[i] == -1)
					{
						vertices[vertexID].boneIDs[i] = this->m_boneAnimationMap[one_mesh->mBones[boneIndex]->mName.data].id;
						vertices[vertexID].weights[i] = one_mesh->mBones[boneIndex]->mWeights[weightIndex].mWeight;
					}
					else noop;
				}
			}
		}
	}
	else
	{
		// this mesh has no bone, check if the node has same name with bone in animation data
		if (this->m_boneAnimationMap.find(nodeData.nodeName) != this->m_boneAnimationMap.end())
		{
			if(nodeData.numChildren) this->m_fastRender = false; else noop;

			// this mesh has no bone, but the node contains this mesh has an animation data.
			// => can assume this bone is as the mesh, centered, and aligned with the mesh itself
			// with offset matrix is identity

			this->m_boneAnimationMap[nodeData.nodeName].meshName = one_mesh->mName.data;
			this->m_boneAnimationMap[nodeData.nodeName].offsetMatrix = glm::mat4(1.0f);
			this->m_boneAnimationMap[nodeData.nodeName].node = nodeData;

			// update vertices bone data
			for (int i = 0; i < (int)vertices.size(); i++)
			{
				vertices[i].boneIDs[0] = this->m_boneAnimationMap[nodeData.nodeName].id;
				vertices[i].weights[0] = 1.0f;
			}
		}
		else // no animation at all for this mesh. need to convert vertices position to absolute
		{
			/*need to implement here or in model animation
			transform nodes data to absolute instead of local, because hierarchicalTransformation is not sent to shader*/
			/*one note is normal is not depends on translation, only rotation*/

			for (int i = 0; i < (int)vertices.size(); i++)
			{
				vertices[i].position = glm::vec3(nodeData.globalTransformation * glm::vec4(vertices[i].position, 1.0f));
				vertices[i].normal = glm::vec3(nodeData.globalTransformation * glm::vec4(vertices[i].normal, 0.0f));
				//vertices[i].normal = glm::mat3(nodeData.globalTransformation) * vertices[i].normal;
			}

		}
	}


	meshData.vertices = vertices;
	// done vertices

	// face indices
	std::vector<unsigned int> indices;
	for (unsigned int i = 0; i < one_mesh->mNumFaces; i++)
	{
		aiFace one_face = one_mesh->mFaces[i];
		for (unsigned int j = 0; j < one_face.mNumIndices; j++)
			indices.push_back(one_face.mIndices[j]);
	}
	meshData.indices = indices;

	// textures and materials
	int  colorType = (one_mesh->HasVertexColors(0)) ? COLOR_VERTEX : COLOR_MATERIAL;

	std::vector<TEXTUREDATA> textures;
	aiMaterial* aimaterial = scene->mMaterials[one_mesh->mMaterialIndex];
	try
	{
		for (unsigned int i = 0; i < gTextureTypeNames.size(); i++)					// texture name
		{
			std::vector<TEXTUREDATA> texture = loadMaterialTextures(aimaterial, (aiTextureType)i, gTextureTypeNames[i]);
			textures.insert(textures.end(), texture.begin(), texture.end());
		}
	}
	catch (exception& e)
	{
		throw runtime_error(__FUNCTION__ + delimiter(string(e.what())));
	}
	meshData.textures = textures;

	// materials
	MATERIALDATA material;
	if (0 < textures.size())
		colorType = COLOR_TEXTURE;
	else
	{
		aiMaterial* aimaterial = scene->mMaterials[one_mesh->mMaterialIndex];

		aiString material_name;
		aiReturn name_check = aimaterial->Get(AI_MATKEY_NAME, material_name);
		material.name = (name_check == aiReturn_SUCCESS) ? material_name.data : string();

		int twosided = 0;
		aiReturn twosided_check = aimaterial->Get(AI_MATKEY_TWOSIDED, twosided);
		material.twosided = (twosided_check == aiReturn_SUCCESS) ? twosided : 0;

		int enable_wireframe = 0;
		aiReturn enable_wireframe_check = aimaterial->Get(AI_MATKEY_ENABLE_WIREFRAME, enable_wireframe);
		material.enable_wireframe = (enable_wireframe_check == aiReturn_SUCCESS) ? enable_wireframe : 0;

		int blend_func = aiBlendMode_Default;
		aiReturn blend_func_check = aimaterial->Get(AI_MATKEY_BLEND_FUNC, blend_func);
		material.blend_func = (blend_func_check == aiReturn_SUCCESS) ? blend_func : aiBlendMode_Default;


		int shading_model = 0;
		aiReturn shading_model_check = aimaterial->Get(AI_MATKEY_SHADING_MODEL, shading_model);
		material.shading_model = (shading_model_check == aiReturn_SUCCESS) ? shading_model : 0;

		// color properties
		aiColor4D emissive(0.0f);
		aiReturn emissive_check = aimaterial->Get(AI_MATKEY_COLOR_EMISSIVE, emissive);
		material.emissive = (emissive_check == aiReturn_SUCCESS) ? glm::vec4(emissive.r, emissive.g, emissive.b, emissive.a) : glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);

		aiColor4D ambient(0.0f);
		aiReturn ambient_check = aimaterial->Get(AI_MATKEY_COLOR_AMBIENT, ambient);
		material.ambient = (ambient_check == aiReturn_SUCCESS) ? glm::vec4(ambient.r, ambient.g, ambient.b, ambient.a) : glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);

		aiColor4D diffuse(0.0f);
		aiReturn diffuse_check = aimaterial->Get(AI_MATKEY_COLOR_DIFFUSE, diffuse);
		material.diffuse = (diffuse_check == aiReturn_SUCCESS) ? glm::vec4(diffuse.r, diffuse.g, diffuse.b, diffuse.a) : glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);

		aiColor4D specular(0.0f);
		aiReturn specular_check = aimaterial->Get(AI_MATKEY_COLOR_SPECULAR, specular);
		material.specular = (specular_check == aiReturn_SUCCESS) ? glm::vec4(specular.r, specular.g, specular.b, specular.a) : glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);

		aiColor4D color_reflective(0.0f);
		aiReturn color_reflective_check = aimaterial->Get(AI_MATKEY_COLOR_REFLECTIVE, color_reflective);
		material.color_reflective = (color_reflective_check == aiReturn_SUCCESS) ? glm::vec4(color_reflective.r, color_reflective.g, color_reflective.b, color_reflective.a) : glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);

		aiColor4D color_transparent(0.0f);
		aiReturn color_transparent_check = aimaterial->Get(AI_MATKEY_COLOR_TRANSPARENT, color_transparent);
		material.color_transparent = (color_transparent_check == aiReturn_SUCCESS) ? glm::vec4(color_transparent.r, color_transparent.g, color_transparent.b, color_transparent.a) : glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);

		// intencity properties
		float shininess = 0.0f;
		aiReturn shininess_check = aimaterial->Get(AI_MATKEY_SHININESS, shininess);
		material.shininess = (shininess_check == aiReturn_SUCCESS) ? glm::max(shininess, 128.0f) : 0.0f;

		float shininess_strength = 1.0f;
		aiReturn shininess_strength_check = aimaterial->Get(AI_MATKEY_SHININESS_STRENGTH, shininess_strength);
		material.shininess_strength = (shininess_strength_check == aiReturn_SUCCESS) ? shininess_strength : 1.0f;

		float opacity = 0.0f;
		aiReturn opacity_check = aimaterial->Get(AI_MATKEY_OPACITY, opacity);
		material.opacity = (opacity_check == aiReturn_SUCCESS) ? 1.0f - opacity : 0.0f;



		float refracti = 0.0f;
		aiReturn refracti_check = aimaterial->Get(AI_MATKEY_REFRACTI, refracti);
		material.refracti = (refracti_check == aiReturn_SUCCESS) ? refracti : 0.0f;

		float reflectivity = 0.0f;
		aiReturn reflectivity_check = aimaterial->Get(AI_MATKEY_REFLECTIVITY, reflectivity);
		material.reflectivity = (reflectivity_check == aiReturn_SUCCESS) ? reflectivity : 0.0f;			
	}

	meshData.material = material;

	meshData.colorType = colorType;


	// Buffer objects
	GLuint m_vao = 0;
	GLuint m_vbo = 0;
	GLuint m_ebo = 0;

	glGenVertexArrays(1, &m_vao);
	glGenBuffers(1, &m_vbo);
	glGenBuffers(1, &m_ebo);

	// binding and sending
	glBindVertexArray(m_vao);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, meshData.indices.size() * sizeof(unsigned int), &meshData.indices[0], GL_STATIC_DRAW);

	//binding and sending
	glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
	glBufferData(GL_ARRAY_BUFFER, meshData.vertices.size() * sizeof(Vertex), &meshData.vertices[0], GL_STATIC_DRAW);

	// position 
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, position));

	// normal vector
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));

	// vertex color
	glEnableVertexAttribArray(2);
	glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, color));

	// texture coordinates
	glEnableVertexAttribArray(3);
	glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, texcoords));

	// tangent
	glEnableVertexAttribArray(4);
	glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, tangent));

	// bitanent
	glEnableVertexAttribArray(5);
	glVertexAttribPointer(5, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, bitangent));

	// bone IDs
	glEnableVertexAttribArray(6);

	#if defined(_WIN32) || defined(_WIN64)  // GL_INT not supported in GLES 3.0   
		glVertexAttribIPointer(6, 4, GL_INT, sizeof(Vertex), (void*)offsetof(Vertex, boneIDs));
	#else // esseo_debug, Jan. 15th, 2024
		glVertexAttribPointer(6, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, boneIDs));
	#endif


	// bone weights
	glEnableVertexAttribArray(7);
	glVertexAttribPointer(7, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, weights));

	glBindVertexArray(0);


	meshData.vao = m_vao;
	meshData.vbo = m_vbo;
	meshData.ebo = m_ebo;


	// pushback this meshData
	if (material.opacity == 0.0f)
		this->m_meshes.push_back(meshData);
	else
		this->m_transparent_meshes.push_back(meshData);
}


void sanModel::renderModel(sanShader* shader, glm::mat4& model_matrix, glm::mat4& view_matrix, glm::mat4& projection_matrix)
{
	try
	{
		shader->use();
		shader->setMat4("model", model_matrix);
		shader->setMat4("v_view", view_matrix);
		shader->setMat4("view", view_matrix);
		shader->setMat4("projection", projection_matrix);

		renderMeshes(shader, this->m_meshes);
		renderMeshes(shader, this->m_transparent_meshes);
	}
	catch (exception& e)
	{
		throw runtime_error(__FUNCTION__ + delimiter(string(e.what())));
	}
}

void sanModel::renderMeshes(sanShader* shader, std::vector<MESHDATA>& meshes)
{
	sanError::glClearError();

	static clock_t sclock = clock();
	shader->use();

	for (int i = 0; i < (int)meshes.size(); i++)
	{
		MESHDATA& one_mesh = meshes[i];

		shader->setInt("colorType", one_mesh.colorType);

		if (one_mesh.colorType == COLOR_MATERIAL)
		{
			clock_t eclock = clock();
			float duration_second = (float)(eclock - sclock) / (float)CLOCKS_PER_SEC;
			bool toggle_flag = false;
			if (duration_second < 0.5f) toggle_flag = true;
			else if (1.0f < duration_second) sclock = eclock;
			else noop;

			// opacity
			if (m_pxml->m_activation.model_transparency) shader->setFloat("material_opacity", m_pxml->m_model_transparency_parameter.rate); // transparency
			else shader->setFloat("material_opacity", one_mesh.material.opacity); // non-transparency 

			// emissive for vehicle lamps
			glm::vec4 new_material_emissive = one_mesh.material.emissive;
			if (toggle_flag)
				new_material_emissive = decide_material_emissive(m_pvehicleSignal->m_trigger.turn_signal, one_mesh.nodeName, one_mesh.material.emissive);
			else noop;

			shader->setVec4("material_emissive", new_material_emissive);
			shader->setVec4("material_ambient", one_mesh.material.ambient);
			shader->setVec4("material_diffuse", one_mesh.material.diffuse);
			shader->setVec4("material_specular", one_mesh.material.specular);
			shader->setFloat("material_shininess", one_mesh.material.shininess);
		}
		else if (one_mesh.colorType == COLOR_TEXTURE)
		{
			int* ptextureTypeCounter = (int*)new int[(int)gTextureTypeNames.size()](); // zero-based initialization
			for (unsigned int i = 0; i < one_mesh.textures.size(); i++)
			{
				std::string strIndex;
				std::string current_textureTypeName = one_mesh.textures[i].textureName;

				for (int k = 0; k < (int)gTextureTypeNames.size(); k++)
				{
					if (current_textureTypeName == gTextureTypeNames[k])
					{
						strIndex = std::to_string(++ptextureTypeCounter[k]); // to start from 1                       
						break;
					}
					else noop;
				}

				glActiveTexture(GL_TEXTURE0 + i);
				shader->setInt((current_textureTypeName + strIndex).c_str(), i);
				glBindTexture(GL_TEXTURE_2D, one_mesh.textures[i].textureID);
			}
			delete[] ptextureTypeCounter;
		}

		glBindVertexArray(one_mesh.vao);
		glDrawElements(GL_TRIANGLES, static_cast<unsigned int>(one_mesh.indices.size()), GL_UNSIGNED_INT, 0);
		glBindVertexArray(0);
	}

	sanError::glCheckError(__FUNCTION__);
}



std::vector<TEXTUREDATA> sanModel::loadMaterialTextures(aiMaterial* material, aiTextureType textureType, std::string textureName)
{
	std::vector<TEXTUREDATA> textures;
	try
	{
		for (unsigned int i = 0; i < material->GetTextureCount(textureType); i++)
		{
			bool skip = false;
			aiString texturePath;
			material->GetTexture(textureType, i, &texturePath);

			// check if each texture is loaded or not
			for (unsigned int j = 0; j < this->m_loadedTextures.size(); j++)
			{
				if (this->m_loadedTextures[j].texturePath == texturePath.data)
				{
					textures.push_back(this->m_loadedTextures[j]);
					skip = true;
					break;
				}
				else noop;
			}

			if (!skip)
			{
				TEXTURE_INFO texture_info = sanFromFile::read_texture_from_file(_MODELS_PATH_ + "/" + (std::string)texturePath.data, true, false);

				GLuint textureID = 0;
				sanVABT::generateTexture(GL_TEXTURE0, &textureID);
				sanVABT::updateTexture(GL_TEXTURE0, textureID, texture_info);

				TEXTUREDATA texturedata;
				texturedata.textureID = textureID;
				texturedata.textureName = textureName;
				texturedata.texturePath = texturePath.data;
				textures.push_back(texturedata);
				this->m_loadedTextures.push_back(texturedata);
			}
			else noop;
		}
	}
	catch (exception& e)
	{
		throw runtime_error(__FUNCTION__ + delimiter(string(e.what())));
	}

	return textures;
}



glm::vec4 sanModel::decide_material_emissive(TURN_SIGNAL turnSignal, string meshNodeName, glm::vec4 default_emissive)
{
	glm::vec4 new_color = glm::vec4(1.0f, 0.7f, 0.0f, 1.0f);
	glm::vec4 new_material_emissive = default_emissive;

	switch(turnSignal)
	{
	case TURN_SIGNAL_LEFT:
	{
		if (std::find(m_pxml->m_model_lamp_parameter.left_lamp_names.begin(), m_pxml->m_model_lamp_parameter.left_lamp_names.end(), meshNodeName) != m_pxml->m_model_lamp_parameter.left_lamp_names.end())
		{
			new_material_emissive = new_color;
		}
		else noop;
		break;
	}
	case TURN_SIGNAL_RIGHT:
	{
		if (std::find(m_pxml->m_model_lamp_parameter.right_lamp_names.begin(), m_pxml->m_model_lamp_parameter.right_lamp_names.end(), meshNodeName) != m_pxml->m_model_lamp_parameter.right_lamp_names.end())
		{
			new_material_emissive = new_color;
		}
		else noop;
		break;
	}
	case TURN_SIGNAL_EMERGENCY:
	{
		if (std::find(m_pxml->m_model_lamp_parameter.left_lamp_names.begin(), m_pxml->m_model_lamp_parameter.left_lamp_names.end(), meshNodeName) != m_pxml->m_model_lamp_parameter.left_lamp_names.end() ||
			std::find(m_pxml->m_model_lamp_parameter.right_lamp_names.begin(), m_pxml->m_model_lamp_parameter.right_lamp_names.end(), meshNodeName) != m_pxml->m_model_lamp_parameter.right_lamp_names.end())
				new_material_emissive = new_color;
		else noop;
		break;
	}
	case TURN_SIGNAL_OFF:
	default:
		new_material_emissive = default_emissive;
		break;
	};

	return new_material_emissive;
}
