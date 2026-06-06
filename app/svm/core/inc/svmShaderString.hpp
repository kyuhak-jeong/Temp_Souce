#ifndef _SVM_SHADERSTRING_HPP_
#define _SVM_SHADERSTRING_HPP_

///////////////////////////////////////////////////////////////////////////
//-------------------------- Calibration shaders --------------------------
///////////////////////////////////////////////////////////////////////////

//-------------------------- Drawing images --------------------------
static const char vs_calib_image[] =
#if defined(_WIN32) || defined(_WIN64)
	"#version 450 core \n "
#else
	"#version 300 es \n"
	"precision mediump float; \n"
#endif
	"layout(location = 0) in vec4 vPosition; \n "
	"layout(location = 1) in vec2 vTexCoord; \n "
	"out vec2 TexCoord; \n "
	"void main() \n "
	"{ \n "
		"gl_Position = vPosition; \n "
		"TexCoord = vTexCoord; \n "
	"} \n ";

static const char fs_calib_image[] =
#if defined(_WIN32) || defined(_WIN64)
	"#version 450 core \n "
#else
	"#version 300 es \n"
	"precision mediump float; \n"
#endif
	"in vec2 TexCoord; \n "
	"out vec4 fragColor; \n "
	"uniform sampler2D src_img; \n "
	"void main() \n "
	"{\n "
		"fragColor = vec4(texture(src_img, TexCoord).bgr, 1.0f); \n "
	"}\n ";


//-------------------------- Drawing contour --------------------------
static const char vs_calib_contour[] =
#if defined(_WIN32) || defined(_WIN64)
	"#version 450 core \n "
#else
	"#version 300 es \n"
	"precision mediump float; \n"
#endif
	"layout(location = 0) in vec4 vPosition; \n "
	"void main() \n "
	"{ \n "
		"gl_Position = vPosition; \n "
	"} \n ";


static const char fs_calib_contour[] =
#if defined(_WIN32) || defined(_WIN64)
	"#version 450 core \n "
#else
	"#version 300 es \n"
	"precision mediump float; \n"
#endif
	"layout(location = 0) out vec4 fragColor;\n "
	"void main()\n"
	"{\n"
		"fragColor = vec4(1.0, 1.0, 0.0, 1.0);\n"
	"}\n";


//-------------------------- drawing grid --------------------------

static const char vs_calib_grid[] =
#if defined(_WIN32) || defined(_WIN64)
	"#version 450 core \n "
#else
	"#version 300 es \n"
	"precision mediump float; \n"
#endif
	"layout(location = 0) in vec4 vPosition; \n "
	"void main() \n "
	"{ \n "
		"gl_Position = vPosition; \n "
		"gl_PointSize = 3.0f;\n"
	"} \n ";


static const char fs_calib_grid[] =
#if defined(_WIN32) || defined(_WIN64)
	"#version 450 core \n "
#else
	"#version 300 es \n"
	"precision mediump float; \n"
#endif
	"layout(location = 0) out vec4 fragColor;\n "
	"void main()\n"
	"{\n"
		"fragColor = vec4(1.0, 1.0, 0.0, 1.0);\n"
	"}\n";


//-------------------------- drawing masks --------------------------
static const char vs_calib_mask[] =
#if defined(_WIN32) || defined(_WIN64)
	"#version 450 core \n "
#else
	"#version 300 es \n"
	"precision mediump float; \n"
#endif
	"layout(location = 0) in vec4 vPosition; \n "
	"layout(location = 1) in vec2 vTexCoord; \n "
	"out vec2 TexCoord; \n "
	"void main() \n "
	"{ \n "
		"gl_Position = vPosition; \n "
		"TexCoord = vTexCoord; \n "
	"} \n ";

static const char fs_calib_mask[] =
#if defined(_WIN32) || defined(_WIN64)
	"#version 450 core \n "
#else
	"#version 300 es \n"
	"precision mediump float; \n"
#endif
	"in vec2 TexCoord; \n "
	"out vec3 fragColor; \n "
	"uniform sampler2D mask_img; \n "
	"void main() \n "
	"{\n "
		"fragColor = vec3(texture(mask_img, TexCoord).r); \n "
	"}\n ";



//-------------------------- drawing bowl --------------------------
static const char vs_calib_bowl[] =
#if defined(_WIN32) || defined(_WIN64)
	"#version 450 core \n "
#else
	"#version 300 es \n"
	"precision mediump float; \n"
#endif
	"layout(location = 0) in vec4 vPosition;\n"
	"layout(location = 1) in vec2 vTexCoord;\n"
	"out vec2 texCoord;\n"
	"uniform mat4 mv; \n "
	"void main()\n"
	"{\n"
		"gl_Position = mv * vec4(vPosition.xyz, 1.0f);\n"
		"texCoord = vTexCoord;\n"
	"}\n";

static const char fs_calib_bowl[] =
#if defined(_WIN32) || defined(_WIN64)
	"#version 450 core \n "
#else
	"#version 300 es \n"
	"precision mediump float; \n"
#endif
	"in vec2 texCoord;\n"
	"out vec4 fragColor;\n"
	"uniform sampler2D src_img;\n"
	"uniform sampler2D msk_img;\n"
	"uniform vec4 compensate;\n"
	"void main()\n"
	"{\n"
		"fragColor = vec4(texture(src_img, texCoord).bgr, texture(msk_img,texCoord).r) + compensate;\n"
		"//fragColor = vec4(texture(src_img, texCoord).bgr, 0.5);\n"
	"}\n";


///////////////////////////////////////////////////////////////////////////
//-------------------------- View shaders --------------------------
///////////////////////////////////////////////////////////////////////////

//-------------------------- Drawing bowl with blending (overlap) --------------------------
static const char vs_view_bowl_b[] =
#if defined(_WIN32) || defined(_WIN64)
	" #version 450 core \n "
#else
	"#version 300 es \n"
	"precision mediump float; \n"
#endif
	" layout(location = 0) in vec3 vxposit; \n "
	" layout(location = 1) in vec2 txcoord; \n "
	" out vec2 txc; \n "
	" uniform mat4 mvp; \n "
	" void main() \n "
	" { \n "
		" txc = txcoord; \n "
		" gl_Position = mvp * vec4(vxposit, 1.0f); \n "
	" } \n ";


static const char fs_view_bowl_b[] =
	#if defined(_WIN32) || defined(_WIN64)
	" #version 450 core \n "
	#else
	" #version 300 es \n "
	" precision mediump float; \n "
	#endif
	" in vec2 txc; \n "
	" out vec4 fragColor; \n "
	" uniform sampler2D img; \n "
	" uniform sampler2D msk; \n "
	" void main() \n "
	" { \n "
	#if defined(_WIN32) || defined(_WIN64)
		" fragColor = vec4(texture(img, txc).bgr, texture(msk, txc).r); \n "
	#else
		" fragColor = vec4(texture(img, txc).rgb, texture(msk, txc).r); \n "
	#endif
	" } \n ";


//-------------------------- Drawing bowl without blending (non-overlap) --------------------------
static const char vs_view_bowl[] =
	#if defined(_WIN32) || defined(_WIN64)
	" #version 450 core \n "
	#else
	" #version 300 es \n "
	" precision mediump float; \n "
	#endif
	" layout(location = 0) in vec3 vxposit; \n "
	" layout(location = 1) in vec2 txcoord; \n "
	" out vec2 tx; \n "
	" uniform mat4 mvp; \n "
	" void main() \n "
	" { \n "
		" tx = txcoord; \n "
		" gl_Position = mvp * vec4(vxposit, 1.0f); \n "
	" } \n ";

static const char fs_view_bowl[] =
	#if defined(_WIN32) || defined(_WIN64)
	" #version 450 core \n "
	#else
	" #version 300 es \n "
	" precision mediump float; \n "
	#endif
	" in vec2 tx; \n "
	" out vec4 fragColor; \n "
	" uniform sampler2D img; \n "
	" uniform vec4 compensate; \n "
	" void main() \n "
	" { \n "
	
	#if defined(_WIN32) || defined(_WIN64)
		" fragColor = vec4(texture(img, tx).bgr, 1.0f) + compensate; \n "
	#else
		" fragColor = vec4(texture(img, tx).rgb, 1.0f) + compensate; \n "
	#endif
	" } \n ";

//-------------------------- Drawing Osd --------------------------
static const char vs_view_osd[] =
	#if defined(_WIN32) || defined(_WIN64)
	" #version 450 core \n "
	#else
	" #version 300 es \n "
	" precision mediump float; \n "
	#endif
	" layout(location = 0) in vec3 vxposit; \n "
	" layout(location = 1) in vec2 txcoord; \n "
	" out vec2 txc; \n "
	" void main() \n "
	" { \n "
		" txc = txcoord; \n "
		" gl_Position = vec4(vxposit, 1.0f); \n "
	" } \n ";

static const char fs_view_osd[] =
	#if defined(_WIN32) || defined(_WIN64)
	" #version 450 core \n "
	#else
	" #version 300 es \n "
	" precision mediump float; \n "
	#endif
	" in vec2 txc; \n "
	" out vec4 fragColor; \n "
	" uniform sampler2D img; \n "
	" void main() \n "
	" { \n "
		" if (texture(img, txc).a < 0.5f) \n "
		" { \n "
			" fragColor = vec4(texture(img, txc).rgb, 1.0f); \n "
		" } \n "
		" else \n "
		" { \n "
			" fragColor = vec4(texture(img, txc).rgb, 0.0f); \n "
		" } \n "
	" } \n ";


//-------------------------- Drawing CamView --------------------------
static const char vs_view_c2d[] =
	#if defined(_WIN32) || defined(_WIN64)
	" #version 450 core \n "
	#else
	" #version 300 es \n "
	" precision mediump float; \n "
	#endif
	" layout(location = 0) in vec3 vxposit; \n "
	" layout(location = 1) in vec2 txcoord; \n "
	" uniform mat4 mvp; \n "
	" out vec2 txc; \n "
	" void main() \n "
	" { \n "
		" txc = txcoord; \n "
		" gl_Position = mvp * vec4(vxposit, 1.0f); \n "
	" } \n ";

static const char fs_view_c2d[] =
	#if defined(_WIN32) || defined(_WIN64)
	" #version 450 core \n "
	#else
	" #version 300 es \n "
	" precision mediump float; \n "
	#endif
	" in vec2 txc; \n "
	" out vec4 fragColor; \n "
	" uniform sampler2D img; \n "
	" void main() \n "
	" { \n "
	#if defined(_WIN32) || defined(_WIN64)
		" fragColor = vec4(texture(img, txc).bgr, 1.0f); \n "
	#else
		" fragColor = vec4(texture(img, txc).rgb, 1.0f); \n "
	#endif
	" } \n ";


//-------------------------- Drawing 3D Model --------------------------
static const char vs_view_model[] =
	#if defined(_WIN32) || defined(_WIN64)
	" #version 450 core \n "
	#else
	" #version 300 es \n "
	" precision highp float; \n"
	#endif
	" layout(location = 0) in vec3 position; \n"
	" layout(location = 1) in vec3 normal; \n"
	" layout(location = 2) in vec4 color; \n"
	" layout(location = 3) in vec2 texcoords; \n"
	" layout(location = 4) in vec3 tangent; \n"
	" layout(location = 5) in vec3 bitangent; \n"
	" layout(location = 6) in ivec4 boneIDs;  \n"
	" layout(location = 7) in vec4 weights; \n"
	" \n"
	" out vec3 FragPos; \n"
	" out vec3 FragNormal; \n"
	" out vec4 FragColor; \n"
	" out vec2 TexCoords; \n"
	" out mat3 TBN; \n"
	" \n"
	" uniform mat4 model; \n"
	" uniform mat4 v_view; \n"
	" uniform mat4 projection; \n"
	" \n"
	" uniform int v_renderMode; \n"
	" uniform int v_lightType; \n"
	" \n"
	" const int MAX_BONES_PER_VERTEX = 4; \n"
	" const int MAX_ANIMATED_BONES = 100; \n"
	" uniform int boneIdList[MAX_ANIMATED_BONES]; \n"
	" uniform mat4 localToBonesTransformations[MAX_ANIMATED_BONES]; \n"
	" \n"
	" int findBoneIndex(int boneID) \n"
	" { \n"
		" for(int j = 0; j < MAX_ANIMATED_BONES; j++) \n"
		" { \n"
			" if(boneIdList[j] == boneID) \n"
			" return j; \n"
		" } \n"
	" \n"
		" return 0; \n"
	" } \n"
	" \n"
	" void main() \n"
	" { \n"
		" vec4 totalPosition = vec4(0.0f); \n"
		" vec4 totalNormal = vec4(0.0f); \n"
		" vec4 totalTangent = vec4(0.0f); \n"
		" \n"
		" for(int i = 0; i < MAX_BONES_PER_VERTEX; i++) \n"
		" { \n"
			" if(-1 < boneIDs[i]) \n"
			" { \n"
				" if(boneIDs[i] < MAX_ANIMATED_BONES) \n"
				" { \n"
					" int boneIndex = findBoneIndex(boneIDs[i]); \n"
					" vec4 localPosition = localToBonesTransformations[boneIndex] * vec4(position, 1.0f); \n"
					" totalPosition += localPosition * weights[i]; \n"
					" vec4 localNormal = localToBonesTransformations[boneIndex] * vec4(normal, 0.0f); \n"
					" totalNormal += localNormal * weights[i]; \n"
					" vec4 localTangent = localToBonesTransformations[boneIndex] * vec4(tangent, 0.0f); \n"
					" totalTangent += localTangent * weights[i]; \n"
				" } \n"
				" else \n"
				" { \n"
					" totalPosition = vec4(position, 1.0f); \n"
					" totalNormal = vec4(normal, 0.0f); \n"
					" totalTangent = vec4(tangent, 0.0f); \n"
				" } \n"
				" \n"
				" continue; \n"
			" } \n"
			" \n"
			" if(boneIDs[i] == -1) \n"
			" { \n"
				" if(i == 0) \n"
				" {    \n"
					" totalPosition = vec4(position, 1.0f); \n"
					" totalNormal = vec4(normal, 0.0f); \n"
					" totalTangent = vec4(tangent, 0.0f); \n"
				" } \n"
				" break; \n"
			" } \n"
			" \n"
		" } \n"
		" \n"
		" mat4 transform = (v_renderMode == 0 || v_lightType == 1) ? model : v_view * model; // dir Light only in world coordinate \n"
		" FragPos = vec3(transform * totalPosition); \n"
		" TexCoords = texcoords; \n"
		" FragColor = color; \n"
		" \n"
		" mat3 normalMatrix = transpose(inverse(mat3(transform))); \n"
		" vec3 T = normalize(normalMatrix * vec3(totalTangent)); \n"
		" vec3 N = normalize(normalMatrix * vec3(totalNormal)); \n"
		" T = normalize(T - dot(T, N) * N); \n"
		" vec3 B = cross(N, T); \n"
		" \n"
		" TBN = transpose(mat3(T, B, N)); \n"
		" \n"
		" FragNormal = normalMatrix * vec3(totalNormal); \n"
		" \n"
		" gl_Position =  projection * v_view * model * totalPosition; \n"
	" } \n";

static const char fs_view_model[] =
	#if defined(_WIN32) || defined(_WIN64)
	" #version 450 core \n "
	#else
	" #version 300 es \n "
	" precision mediump float; \n "
	#endif
	" out vec4 fragColor; \n"
	" \n"
	" in vec3 FragPos; \n"
	" in vec3 FragNormal; \n"
	" in vec4 FragColor; \n"
	" in vec2 TexCoords; \n"
	" in mat3 TBN; \n"
	" \n"
	" struct PointLight { \n"
		" vec3 position; \n"
		" \n"
		" float constant; \n"
		" float linear; \n"
		" float quadratic; \n"
		" \n"
		" vec4 ambient; \n"
		" vec4 diffuse; \n"
		" vec4 specular; \n"
	" }; \n"
	" \n"
	" struct DirLight { \n"
		" vec3 direction; \n"
		" \n"
		" vec4 ambient; \n"
		" vec4 diffuse; \n"
		" vec4 specular; \n"
	" }; \n"
	" \n"
	" struct ViewLight { \n"
		" vec4 ambient; \n"
		" vec4 diffuse; \n"
		" vec4 specular; \n"
		" \n"
		" float constant; \n"
		" float linear; \n"
		" float quadratic; \n"
		" \n"
		" bool attenuate;\n"
	" }; \n"
	" \n"
	" // from material \n"
	" uniform vec4 material_emissive; \n"
	" uniform vec4 material_ambient; \n"
	" uniform vec4 material_diffuse; \n"
	" uniform vec4 material_specular; \n"
	" uniform float material_shininess; \n"
	" uniform float material_opacity; \n"
	" \n"
	" // from texture \n"
	" uniform sampler2D texture_emissive1; \n"
	" uniform sampler2D texture_diffuse1; \n"
	" uniform sampler2D texture_specular1; \n"
	" uniform sampler2D texture_normal1; \n"
	" \n"
	" uniform int colorType; \n"
	" uniform vec3 viewPosition; \n"
	" uniform int lightType; \n"
	" \n"
	" uniform mat4 view; \n"
	" uniform int renderMode; \n"
	" \n"
	" #define N_POINT_LIGHTS 5 \n"
	" uniform PointLight pointLights[N_POINT_LIGHTS]; \n"
	" uniform DirLight dirLight; \n"
	" uniform ViewLight viewLight; \n"
	" \n"
	" vec4 CalcPointLight(PointLight light, vec3 normal, vec3 fragPos, mat3 transform, mat4 renderTransform); \n"
	" vec4 CalcDirLight(DirLight light, vec3 normal, vec3 fragPos, mat3 transform); \n"
	" vec4 CalcViewLight(ViewLight light, vec3 normal, vec3 fragPos, mat3 transform, mat4 renderTransform); \n"
	" \n"
	" void main() \n"
	" { \n"
		" vec4 result; \n"
		" vec3 normal; \n"
		" mat3 transform; \n"
		" float opacity; \n"
		" \n"
		" mat4 renderTransform = (renderMode == 0) ? mat4(1.0) : view; \n"
		" \n"
		" if(colorType == 2)          // texture \n"
		" { \n"
			" normal = vec3(texture(texture_normal1, TexCoords)); \n"
			" normal = normalize(normal * 2.0 - 1.0); \n"
			" \n"
			" transform = TBN; \n"
			" \n"
			" opacity = 0.0; \n"
		" } \n"
		" else if(colorType == 1)     // material \n"
		" { \n"
			" normal = normalize(FragNormal); \n"
			" \n"
			" transform = mat3(1.0); \n"
			" \n"
			" opacity = material_opacity; \n"
		" } \n"
		" \n"
		" if(colorType == 0)          // vertex color \n"
		" { \n"
			" fragColor = FragColor; \n"
		" } \n"
		" else \n"
		" { \n"
			" if(lightType == 0)		// view light \n"
			" { \n"
				" result = CalcViewLight(viewLight, normal, FragPos, transform, renderTransform); \n"
			" } \n"
			" else if(lightType == 1)	// dir light \n"
			" { \n"
				" result = CalcDirLight(dirLight, normal, FragPos, transform); \n"
			" } \n"
			" else if(lightType == 2)	// point lights \n"
			" { \n"
				" for(int i = 0; i < N_POINT_LIGHTS; i++) \n"
					" result += CalcPointLight(pointLights[i], normal, FragPos, transform, renderTransform);  \n"
					" \n"
			" } \n"
			" \n"
			" fragColor = vec4(result.r, result.g, result.b, opacity); \n"
		" } \n"
	" } \n"
	" \n"
	" // calculates the color when using a point light. \n"
	" vec4 CalcPointLight(PointLight light, vec3 normal, vec3 fragPos, mat3 transform, mat4 renderTransform) \n"
	" { \n"
		" vec3 lightDir = normalize(transform * (vec3(renderTransform * vec4(light.position,1.0)) - fragPos)); \n"
		" // diffuse shading \n"
		" float diff = max(dot(normal, lightDir), 0.0); \n"
		" // specular shading \n"
		" vec3 viewDir = normalize(transform * (vec3(renderTransform * vec4(viewPosition,1.0)) - fragPos)); \n"
		" vec3 reflectDir = reflect(-lightDir, normal); \n"
		" // attenuation \n"
		" float distance = length(transform * (vec3(renderTransform * vec4(light.position,1.0)) - fragPos)); \n"
		" float attenuation = 1.0 / (light.constant + light.linear * distance + light.quadratic * (distance * distance));     \n"
		" // combine results \n"
		" vec4 emissive; \n"
		" vec4 ambient; \n"
		" vec4 diffuse; \n"
		" vec4 specular; \n"
		" \n"
		" if(colorType == 2) \n"
		" { \n"
			" emissive = texture(texture_emissive1, TexCoords); \n"
			" ambient = attenuation * light.ambient * texture(texture_diffuse1, TexCoords); \n"
			" diffuse = attenuation * light.diffuse * diff * texture(texture_diffuse1, TexCoords); \n"
			" float spec = pow(max(dot(viewDir, reflectDir), 0.0), 128.0); \n"
			" specular = attenuation * light.specular * spec * texture(texture_specular1, TexCoords); \n"
		" } \n"
		" else if(colorType == 1) \n"
		" { \n"
			" emissive = material_emissive; \n"
			" ambient = attenuation * light.ambient * material_diffuse; \n"
			" diffuse = attenuation * light.diffuse * diff * material_diffuse; \n"
			" float spec = pow(max(dot(viewDir, reflectDir), 0.0), material_shininess); \n"
			" specular = attenuation * light.specular * spec * material_specular; \n"
		" } \n"
		" \n"
		" return (emissive + ambient + diffuse + specular); \n"
	" } \n"
	" // calculates the color when using a directional light. only render in world coordinate \n"
	" vec4 CalcDirLight(DirLight light, vec3 normal, vec3 fragPos, mat3 transform) \n"
	" { \n"
		" vec3 lightDir = normalize(-transform * light.direction); \n"
		" // diffuse shading \n"
		" float diff = max(dot(normal, lightDir), 0.0); \n"
		" // specular shading \n"
		" vec3 viewDir = normalize(transform * (viewPosition - fragPos)); \n"
		" vec3 reflectDir = reflect(-lightDir, normal); \n"
		" // combine results \n"
		" vec4 emissive; \n"
		" vec4 ambient; \n"
		" vec4 diffuse; \n"
		" vec4 specular; \n"
		" if(colorType == 2) \n"
		" { \n"
			" emissive = texture(texture_emissive1, TexCoords); \n"
			" ambient = light.ambient * texture(texture_diffuse1, TexCoords); \n"
			" diffuse = light.diffuse * diff * texture(texture_diffuse1, TexCoords); \n"
			" float spec = pow(max(dot(viewDir, reflectDir), 0.0), 128.0); \n"
			" specular = light.specular * spec * texture(texture_specular1, TexCoords); \n"
		" } \n"
		" else if(colorType == 1) \n"
		" { \n"
			" emissive = material_emissive; \n"
			" ambient = light.ambient * material_diffuse; \n"
			" diffuse = light.diffuse * diff * material_diffuse; \n"
			" float spec = pow(max(dot(viewDir, reflectDir), 0.0), material_shininess); \n"
			" specular = light.specular * spec * material_specular; \n"
		" } \n"
		" return (emissive + ambient + diffuse + specular); \n"
	" } \n"
	" \n"
	" // point light at the camera (view): use viewPosition instead of light.position \n"
	" // and different in function to calculate diff and spec: use abs instead of max \n"
	" vec4 CalcViewLight(ViewLight light, vec3 normal, vec3 fragPos, mat3 transform, mat4 renderTransform) \n"
	" { \n"
		" vec3 lightDir = normalize(transform * (vec3(renderTransform * vec4(viewPosition,1.0)) - fragPos)); \n"
		" // diffuse shading \n"
		" float diff = abs(dot(normal, lightDir)); \n"
		" // specular shading \n"
		" vec3 viewDir = normalize(transform * (vec3(renderTransform * vec4(viewPosition,1.0)) - fragPos)); \n"
		" vec3 reflectDir = reflect(-lightDir, normal); \n"
		" // attenuation \n"
		" float distance = length(transform * (vec3(renderTransform * vec4(viewPosition,1.0)) - fragPos)); \n"
		" float attenuation = light.attenuate ? 1.0 / (light.constant + light.linear * distance + light.quadratic * (distance * distance)) : 1.0; \n"
		" // combine results \n"
		" vec4 emissive; \n"
		" vec4 ambient; \n"
		" vec4 diffuse; \n"
		" vec4 specular; \n"
		" \n"
		" if(colorType == 2) \n"
		" { \n"
			" emissive = texture(texture_emissive1, TexCoords); \n"
			" ambient = light.ambient * texture(texture_diffuse1, TexCoords); \n"
			" diffuse = light.diffuse * diff * texture(texture_diffuse1, TexCoords); \n"
			" float spec = pow(abs(dot(viewDir, reflectDir)), 128.0); \n"
			" specular = light.specular * spec * texture(texture_specular1, TexCoords); \n"
		" } \n"
		" else if(colorType == 1) \n"
		" { \n"
			" emissive = material_emissive; \n"
			" ambient = attenuation * light.ambient * material_diffuse; \n"
			" diffuse = attenuation * light.diffuse * diff * material_diffuse; \n"
			" float spec = pow(abs(dot(viewDir, reflectDir)), material_shininess); \n"
			" specular = attenuation * light.specular * spec * material_specular; \n"
		" } \n"
		" \n"
		" return (emissive + ambient + diffuse + specular); \n"
	" } \n";

//-------------------------- Drawing PGS --------------------------
static const char vs_view_pgs[] =
	#if defined(_WIN32) || defined(_WIN64)
	" #version 450 core \n "
	#else
	" #version 300 es \n "
	" precision mediump float; \n "
	#endif
	" layout(location = 0) in vec3 aPosition; \n "
	" layout(location = 1) in vec2 aTexCoords; \n "
	" out vec2 TexCoords; \n"
	" \n"
	" uniform vec3 scale; \n"
	" uniform vec3 translate; \n"
	" uniform mat4 mvp; \n"
	" void main() \n "
	" { \n "
	"     TexCoords = aTexCoords; \n "
	"     gl_Position = mvp * vec4(scale * aPosition + translate, 1.0); \n "
	" } \n ";

static const char fs_view_pgs[] =
	#if defined(_WIN32) || defined(_WIN64)
	" #version 450 core \n "
	#else
	" #version 300 es \n "
	" precision mediump float; \n "
	#endif
	" in vec2 TexCoords; \n"
	" uniform int colorType; \n "
	" uniform sampler2D texture_image; \n "
	" uniform vec3 color; \n "
	" uniform float alpha; \n"
	" out vec4 fragColor; \n "
	" void main() \n "
	" { \n "
	"     if(colorType == 0) \n "
	"     { \n "
	"		fragColor = vec4(color, alpha); \n "
	"     } \n "
	"     else if(colorType == 1) \n "
	"     { \n "
	"		vec4 result = texture(texture_image, TexCoords); \n "
	"		fragColor = vec4(result.rgb, max(result.a, 0.1)); \n "
	"     } \n "
	" } \n ";


//-------------------------- Drawing PGS / MOBS / DGS / OD / VBC --------------------------
static const char vs_view_primitive2d[] =
	#if defined(_WIN32) || defined(_WIN64)
	" #version 450 core \n "
	#else
	" #version 300 es \n "
	" precision mediump float; \n "
	#endif	
	" layout(location=0) in vec3 vxposit; \n"
	" uniform mat4 mvp; \n "
	" void main() \n "
	" { \n "
		" gl_Position = mvp * vec4(vxposit, 1.0f); \n "
	" } \n ";

static const char fs_view_primitive2d[] =
	#if defined(_WIN32) || defined(_WIN64)
	" #version 450 core \n "
	#else
	" #version 300 es \n "
	" precision mediump float; \n "
	#endif	
	" uniform vec3 color; \n"
	" uniform float alpha; \n"
	" out vec4 fragColor; \n "
	" void main() \n "
	" { \n "
		" fragColor = vec4(color, alpha); \n "
	" } \n ";





///////////////////////////////////////////////////////////////////////////
//-------------------------- Common shaders --------------------------
///////////////////////////////////////////////////////////////////////////

//-------------------------- drawing 2D text --------------------------
static const char vs_common_text[] =
	#if defined(_WIN32) || defined(_WIN64)
	" #version 450 core \n "
	#else
	" #version 300 es \n "
	" precision mediump float; \n "
	#endif	
	" layout (location = 0) in vec3 aPosition; \n "
	" layout (location = 1) in vec2 aTexCoords; \n "
	" out vec2 TexCoords; \n "
	" uniform mat4 mvp; \n "
	" void main() \n "
	" { \n "
		" gl_Position = mvp * vec4(aPosition, 1.0); \n "
		" TexCoords = aTexCoords; \n "
	" } \n ";

static const char fs_common_text[] =
	#if defined(_WIN32) || defined(_WIN64)
	" #version 450 core \n "
	#else
	" #version 300 es \n "
	" precision mediump float; \n "
	#endif	
	" in vec2 TexCoords; \n"
	" out vec4 color; \n"
	" uniform sampler2D text; \n "
	" uniform vec3 textColor; \n "
	" void main() \n "
	" { \n "
		" vec4 sampled = vec4(1.0, 1.0, 1.0, texture(text, TexCoords).r); \n "
		" color = vec4(textColor, 1.0) * sampled; \n "
	" } \n ";


//-------------------------- MRT shader --------------------------
static const char vs_common_mrt[] =
	#if defined(_WIN32) || defined(_WIN64)
	" #version 450 core \n "
	#else
	" #version 300 es \n "
	" precision mediump float; \n "
	#endif	
	" layout(location = 0) in vec2 in_ScreenCoord; \n"
	" layout(location = 1) in vec2 in_TexCoord; \n"
	" out vec2 fragTexCoord; \n"
	" void main() \n"
	" { \n"
	" 	gl_Position = vec4(in_ScreenCoord, 0.0, 1.0); \n"
	" 	fragTexCoord = in_TexCoord; \n"
	" } \n";

static const char fs_common_mrt[] =
	#if defined(_WIN32) || defined(_WIN64)
	" #version 450 core \n "
	#else
	" #version 300 es \n "
	" precision mediump float; \n "
	#endif	
	" uniform sampler2D tex; \n"
	" in vec2 fragTexCoord; \n"
	" out vec4 out_fragData; \n"
	" void main() \n"
	" { \n"
	"    out_fragData = vec4(texture(tex, fragTexCoord).xyz,1.0); \n"
	" } \n";



#endif
