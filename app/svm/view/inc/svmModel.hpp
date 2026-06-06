#ifndef SVMMODEL_HPP_
#define SVMMODEL_HPP_

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include "svmCore.hpp"
#include "svmXML.hpp"
#include "svmShaderString.hpp"
#include "svmLogger.hpp"
#include "svmShader.hpp"
#include "svmError.hpp"

#define MODEL_DEBUGGING			(0)

#define MODEL_IMPORT_OPTIMIZED	(0x01)
#define MODEL_IMPORT_REMOVED	(0x02)

#define MAX_BONES_PER_VERTEX    (4)
#define MAX_BONES				(100)

#define COLOR_VERTEX            (0)
#define COLOR_MATERIAL          (1)
#define COLOR_TEXTURE           (2)

const vector<string> gTextureTypeNames = {
	string("texture_none"),     string("texture_diffuse"),      string("texture_specular"), string("texture_ambient"),
	string("texture_emissive"), string("texture_height"),	    string("texture_normal"),   string("texture_shininess"),
	string("texture_opacity"),  string("texture_displacement"), string("texture_laghtmap"), string("texture_refelection")
};

// Mesh data: including Vertex and Bone
struct Vertex
{
	glm::vec3 position;
	glm::vec3 normal;
	glm::vec4 color;
	glm::vec2 texcoords;
	glm::vec3 tangent;
	glm::vec3 bitangent;
	int boneIDs[MAX_BONES_PER_VERTEX]; 				//bone indexes which will influence this vertex
	float weights[MAX_BONES_PER_VERTEX]; 			//weights from each bone
};

// Material data of the model or scene
struct MATERIALDATA
{
	// name
	string name;

	// mode
	int twosided;
	int enable_wireframe;
	int blend_func;
	int shading_model;

	// color
	glm::vec4 emissive;
	glm::vec4 ambient;
	glm::vec4 diffuse;
	glm::vec4 specular;
	glm::vec4 color_reflective;
	glm::vec4 color_transparent;

	// intencity
	float shininess;
	float shininess_strength;
	float opacity;
	float refracti;
	float reflectivity;
};

// Texture data of the model or scene
struct TEXTUREDATA
{
	unsigned int textureID;
	std::string textureName = std::string();
	std::string texturePath;
};

struct MESHDATA
{
	std::string nodeName = std::string();
	std::string meshName = std::string();
	std::vector<Vertex> vertices;
	std::vector<unsigned int> indices; // face indices
	int colorType;
	std::vector<TEXTUREDATA> textures;
	MATERIALDATA material;

	GLuint vao;
	GLuint vbo;
	GLuint ebo;
};

// Node data: including Node and meshes
struct NODEDATA
{
	std::string nodeName = std::string();
	int numChildren = 0;
	std::vector<NODEDATA> children;
	glm::mat4 transformation = glm::mat4(1.0f);
	glm::mat4 parentTransformation = glm::mat4(1.0f);
	glm::mat4 globalTransformation = glm::mat4(1.0f);
};

// Animation data
struct KeyPosition
{
	glm::vec3 position;
	float timeStamp;
};

struct KeyRotation
{
	glm::quat rotation;
	float timeStamp;
};

struct KeyScale
{
	glm::vec3 scale;
	float timeStamp;
};

struct BoneAnimation
{
	std::string animationName = std::string();
	std::string boneName = std::string();
	int numPositionKeys = 0;
	int numRotationKeys = 0;
	int numScalingKeys = 0;
	std::vector<KeyPosition> positionKeys;
	std::vector<KeyRotation> rotationKeys;
	std::vector<KeyScale> scaleKeys;
};

struct Bone
{
	std::string meshName = std::string();
	unsigned int id;
	glm::mat4 offsetMatrix;							// transformation from mesh to bone
	std::vector<BoneAnimation> boneAnimations;		// store for multiple animations in 1 scene
	NODEDATA node;
};

struct ANIMATIONDATA
{
	std::string animationName = std::string();
	float duration;
	int ticksPerSecond;
	int numChannels;
	std::vector<BoneAnimation> channels;			// store all boneAnimation for this animation only
};


class sanModel
{
public:
	string						m_model_full_filename;
	sanXML* m_pxml;
	VEHICLESIGNAL* m_pvehicleSignal;
	Assimp::Importer			m_importer;
	const aiScene* m_scene;

	NODEDATA														m_rootNode;
	std::vector<ANIMATIONDATA>										m_animations;
	std::vector<TEXTUREDATA>										m_loadedTextures;
	std::map<std::string, Bone>										m_boneAnimationMap;				// map the channel(bone) name to its data
	int																m_boneCounter;
	std::vector<MESHDATA>											m_meshes;
	std::vector<MESHDATA>											m_transparent_meshes;

	bool															m_fastRender = true;

public:
	sanModel(sanXML* pxml, VEHICLESIGNAL* vehicleSignal, string model_file_fullname);
	~sanModel();

	void processAnimation(const aiScene* scene, std::vector<ANIMATIONDATA>& animations /*[out]*/, std::map<std::string, Bone>& boneAnimationMap /*[out]*/);
	void processNode(const aiScene* scene, aiNode* one_node, NODEDATA parentData, NODEDATA& nodeData /*[out]*/);
	void processMesh(const aiScene* scene, aiMesh* one_mesh, NODEDATA nodeData);

	void renderModel(sanShader* shader, glm::mat4& model_matrix, glm::mat4& view_matrix, glm::mat4& projection_matrix);
	void renderMeshes(sanShader* shader, std::vector<MESHDATA>& meshes);

	glm::vec4 decide_material_emissive(TURN_SIGNAL turnSignal, string meshName, glm::vec4 default_emissive);
	std::vector<TEXTUREDATA> loadMaterialTextures(aiMaterial* material, aiTextureType textureType, std::string textureName);
};

#endif

