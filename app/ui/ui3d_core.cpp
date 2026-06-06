#include "ui3d_core.h"
#include "constants.h"
#include "logger.h"

#if defined(USE_GLES) || defined(USE_IMGUI)
    #include <GLES3/gl3.h>
#endif // USE_GLES || USE_IMGUI

#include <cstring>
#include <algorithm>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <stdexcept>

#ifndef STB_IMAGE_IMPLEMENTATION
    #include "stb/stb_image.h"
#endif

namespace APP
{

namespace UI3D
{

// ============================================================================
// Shader Sources
// ============================================================================

#if defined(USE_GLES) || defined(USE_IMGUI)

static const char* PBR_VERTEX_SHADER = R"(#version 300 es
precision highp float;
layout(location=0) in vec3 a_position;
layout(location=1) in vec3 a_normal;
layout(location=2) in vec2 a_texCoord;
layout(location=3) in vec4 a_color;
uniform mat4 u_mvp;
uniform mat4 u_model;
uniform mat3 u_normalMat;
out vec3 v_worldPos;
out vec3 v_normal;
out vec2 v_texCoord;
out vec4 v_color;
void main() {
    vec4 worldPos = u_model * vec4(a_position, 1.0);
    v_worldPos = worldPos.xyz;
    v_normal = normalize(u_normalMat * a_normal);
    v_texCoord = a_texCoord;
    v_color = a_color;
    gl_Position = u_mvp * vec4(a_position, 1.0);
}
)";

static const char* PBR_FRAGMENT_SHADER = R"(#version 300 es
precision highp float;
#define MAX_LIGHTS 4
#define PI 3.14159265359
in vec3 v_worldPos;
in vec3 v_normal;
in vec2 v_texCoord;
in vec4 v_color;
uniform vec4 u_albedoColor;
uniform sampler2D u_albedoTex;
uniform bool u_hasAlbedoTex;
uniform float u_metallic;
uniform float u_roughness;
uniform float u_ao;
uniform vec4 u_emissiveColor;
uniform vec3 u_cameraPos;
uniform vec4 u_ambient;
uniform int u_lightCount;
uniform int u_lightType[MAX_LIGHTS];
uniform vec3 u_lightPos[MAX_LIGHTS];
uniform vec3 u_lightDir[MAX_LIGHTS];
uniform vec4 u_lightColor[MAX_LIGHTS];
uniform float u_lightIntensity[MAX_LIGHTS];
out vec4 fragColor;

vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}
float distributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float denom = (NdotH * NdotH * (a2 - 1.0) + 1.0);
    return a2 / (PI * denom * denom);
}
float geometrySchlickGGX(float NdotV, float roughness) {
    float k = (roughness + 1.0) * (roughness + 1.0) / 8.0;
    return NdotV / (NdotV * (1.0 - k) + k);
}
float geometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    return geometrySchlickGGX(max(dot(N,V),0.0), roughness)
         * geometrySchlickGGX(max(dot(N,L),0.0), roughness);
}
void main() {
    vec4 albedo = u_albedoColor;
    if (u_hasAlbedoTex) albedo *= texture(u_albedoTex, v_texCoord);
    vec3 N = normalize(v_normal);
    vec3 V = normalize(u_cameraPos - v_worldPos);
    vec3 F0 = mix(vec3(0.04), albedo.rgb, u_metallic);
    vec3 Lo = vec3(0.0);
    for (int i = 0; i < u_lightCount && i < MAX_LIGHTS; ++i) {
        vec3 L;
        float attenuation = 1.0;
        if (u_lightType[i] == 0) {
            L = normalize(-u_lightDir[i]);
        } else {
            vec3 lightVec = u_lightPos[i] - v_worldPos;
            float distance = length(lightVec);
            L = lightVec / distance;
            attenuation = 1.0 / (distance * distance);
        }
        vec3 H = normalize(V + L);
        vec3 radiance = u_lightColor[i].rgb * u_lightIntensity[i] * attenuation;
        vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);
        vec3 kD = (vec3(1.0) - F) * (1.0 - u_metallic);
        float NDF = distributionGGX(N, H, u_roughness);
        float G   = geometrySmith(N, V, L, u_roughness);
        vec3 specular = (NDF * G * F) / (4.0 * max(dot(N,V),0.0) * max(dot(N,L),0.0) + 0.0001);
        Lo += (kD * albedo.rgb / PI + specular) * radiance * max(dot(N, L), 0.0);
    }
    vec3 color = u_ambient.rgb * albedo.rgb * u_ao + Lo + u_emissiveColor.rgb;
    color = pow(color / (color + vec3(1.0)), vec3(1.0 / 2.2));
    fragColor = vec4(color, albedo.a);
}
)";

static const char* UNLIT_VERTEX_SHADER = R"(#version 300 es
precision highp float;
layout(location=0) in vec3 a_position;
layout(location=1) in vec3 a_normal;
layout(location=2) in vec2 a_texCoord;
layout(location=3) in vec4 a_color;
uniform mat4 u_mvp;
out vec2 v_texCoord;
out vec4 v_color;
void main() {
    v_texCoord = a_texCoord;
    v_color = a_color;
    gl_Position = u_mvp * vec4(a_position, 1.0);
}
)";

static const char* UNLIT_FRAGMENT_SHADER = R"(#version 300 es
precision highp float;
in vec2 v_texCoord;
in vec4 v_color;
uniform vec4 u_color;
uniform sampler2D u_texture;
uniform bool u_hasTexture;
out vec4 fragColor;
void main() {
    vec4 color = u_color;
    if (u_hasTexture) color *= texture(u_texture, v_texCoord);
    fragColor = color * v_color;
}
)";

static const char* LINE_VERTEX_SHADER = R"(#version 300 es
precision highp float;
layout(location=0) in vec3 a_position;
layout(location=1) in vec4 a_color;
uniform mat4 u_mvp;
out vec4 v_color;
void main() {
    v_color = a_color;
    gl_Position = u_mvp * vec4(a_position, 1.0);
}
)";

static const char* LINE_FRAGMENT_SHADER = R"(#version 300 es
precision highp float;
in vec4 v_color;
out vec4 fragColor;
void main() { fragColor = v_color; }
)";

static uint32_t compileShader(uint32_t type, const char* source)
{
    uint32_t shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    int success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (success == 0) { glDeleteShader(shader); return 0; }
    return shader;
}

static uint32_t linkProgram(uint32_t vs, uint32_t fs)
{
    uint32_t program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);
    int success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (success == 0) { glDeleteProgram(program); return 0; }
    return program;
}

#endif // USE_GLES || USE_IMGUI

// ============================================================================
// Camera
// ============================================================================

Camera::Camera()
    : m_position(0.0f, 0.0f, 5.0f)
    , m_target(0.0f, 0.0f, 0.0f)
    , m_up(0.0f, 1.0f, 0.0f)
    , m_fov(45.0f * 3.14159f / 180.0f)
    , m_aspect(16.0f / 9.0f)
    , m_nearPlane(0.1f)
    , m_farPlane(100.0f)
    , m_projectionType(ProjectionType::PERSPECTIVE)
{
    updateMatrices();
}

void Camera::setPosition   (const Vec3& pos)    { m_position = pos;    updateMatrices(); }
void Camera::setTarget     (const Vec3& target) { m_target   = target; updateMatrices(); }
void Camera::setUp         (const Vec3& up)     { m_up       = up;     updateMatrices(); }
void Camera::setAspectRatio(float aspect)       { m_aspect   = aspect; updateMatrices(); }

void Camera::setPerspective(float fovRad, float aspect, float nearPlane, float farPlane)
{
    m_projectionType = ProjectionType::PERSPECTIVE;
    m_fov = fovRad; m_aspect = aspect; m_nearPlane = nearPlane; m_farPlane = farPlane;
    updateMatrices();
}

void Camera::orbit(float deltaYaw, float deltaPitch)
{
    Vec3  offset = m_position - m_target;
    float radius = offset.length();
    if (radius < 0.001f) return;

    float theta = std::atan2(offset.x, offset.z) + deltaYaw;
    float phi   = std::clamp(std::acos(offset.y / radius) + deltaPitch, 0.01f, 3.14f - 0.01f);

    m_position = m_target + Vec3(
        radius * std::sin(phi) * std::sin(theta),
        radius * std::cos(phi),
        radius * std::sin(phi) * std::cos(theta));
    updateMatrices();
}

void Camera::pan(float deltaX, float deltaY)
{
    Vec3 offset = getRight() * deltaX + m_up * deltaY;
    m_position += offset;
    m_target   += offset;
    updateMatrices();
}

void Camera::zoom(float delta)
{
    m_position += getForward() * delta;
    updateMatrices();
}

void Camera::updateMatrices()
{
    m_viewMatrix       = Matrix4x4::makeLookAt(m_position, m_target, m_up);
    m_projectionMatrix = Matrix4x4::makePerspective(m_fov, m_aspect, m_nearPlane, m_farPlane);
}

// ============================================================================
// Light
// ============================================================================

Light::Light()
    : type(Type::DIRECTIONAL)
    , position(0.0f, 10.0f, 0.0f)
    , direction(0.0f, -1.0f, 0.0f)
    , color(UI::Color::White)
    , intensity(1.0f)
    , constantAtten(1.0f)
    , linearAtten(0.09f)
    , quadraticAtten(0.032f)
    , spotInnerCutoff(12.5f * 3.14159f / 180.0f)
    , spotOuterCutoff(17.5f * 3.14159f / 180.0f)
{}

Light Light::makeDirectional(const Vec3& dir, const UI::Color4& col, float intensity)
{
    Light l; l.type = Type::DIRECTIONAL; l.direction = dir.normalized(); l.color = col; l.intensity = intensity;
    return l;
}

Light Light::makePoint(const Vec3& pos, const UI::Color4& col, float intensity)
{
    Light l; l.type = Type::POINT; l.position = pos; l.color = col; l.intensity = intensity;
    return l;
}

Light Light::makeSpot(const Vec3& pos, const Vec3& dir, const UI::Color4& col,
                      float intensity, float innerCutoff, float outerCutoff)
{
    Light l;
    l.type            = Type::SPOT;
    l.position        = pos;
    l.direction       = dir.normalized();
    l.color           = col;
    l.intensity       = intensity;
    l.spotInnerCutoff = innerCutoff * 3.14159f / 180.0f;
    l.spotOuterCutoff = outerCutoff * 3.14159f / 180.0f;
    return l;
}

// ============================================================================
// Material
// ============================================================================

Material::Material()
    : albedoColor(UI::Color::White)
    , albedoTexture(nullptr)
    , normalTexture(nullptr)
    , normalStrength(1.0f)
    , metallic(0.0f)
    , roughness(0.5f)
    , metallicRoughnessTexture(nullptr)
    , ao(1.0f)
    , aoTexture(nullptr)
    , emissiveColor(UI::Color::Transparent)
    , emissiveTexture(nullptr)
    , emissiveStrength(1.0f)
    , clearcoat(0.0f)
    , clearcoatRoughness(0.0f)
    , clearcoatTexture(nullptr)
    , clearcoatRoughnessTexture(nullptr)
    , clearcoatNormalTexture(nullptr)
    , sheenColor(UI::Color::Transparent)
    , sheenRoughness(0.0f)
    , transmission(0.0f)
    , ior(1.5f)
    , alphaMode(AlphaMode::OPAQUE)
    , alphaCutoff(0.5f)
    , doubleSided(false)
    , wireframe(false)
    , unlit(false)
{}

Material Material::makePBR(const UI::Color4& albedo, float metallic, float roughness)
    { Material m; m.albedoColor = albedo; m.metallic = metallic; m.roughness = roughness; return m; }

Material Material::makeUnlit(const UI::Color4& color)
    { Material m; m.albedoColor = color; m.emissiveColor = color; m.unlit = true; return m; }

Material Material::makeGlass(const UI::Color4& tint, float transmission, float ior)
{
    Material m;
    m.albedoColor = tint; m.transmission = transmission; m.ior = ior;
    m.roughness = 0.0f; m.metallic = 0.0f; m.alphaMode = AlphaMode::BLEND;
    return m;
}

Material Material::makeFabric(const UI::Color4& albedo, const UI::Color4& sheen)
{
    Material m;
    m.albedoColor = albedo; m.sheenColor = sheen;
    m.sheenRoughness = 0.5f; m.roughness = 0.8f; m.metallic = 0.0f;
    return m;
}

// ============================================================================
// Mesh
// ============================================================================

Mesh::Mesh() : m_vbo(0), m_ibo(0), m_vao(0), m_uploaded(false) {}
Mesh::~Mesh() { release(); }

void Mesh::generateNormals()
{
    for (size_t i = 0; i < indices.size(); i += 3)
    {
        Vertex3D& v0 = vertices[indices[i]];
        Vertex3D& v1 = vertices[indices[i + 1]];
        Vertex3D& v2 = vertices[indices[i + 2]];
        Vec3 normal = (v1.position - v0.position).cross(v2.position - v0.position).normalized();
        v0.normal = v1.normal = v2.normal = normal;
    }
    m_uploaded = false;
}

void Mesh::generateTangents() { m_uploaded = false; }

void Mesh::calculateBounds(Vec3& outMin, Vec3& outMax) const
{
    if (vertices.empty() == true) { outMin = outMax = Vec3::zero(); return; }
    outMin = outMax = vertices[0].position;
    for (const auto& v : vertices)
    {
        outMin.x = std::min(outMin.x, v.position.x);
        outMin.y = std::min(outMin.y, v.position.y);
        outMin.z = std::min(outMin.z, v.position.z);
        outMax.x = std::max(outMax.x, v.position.x);
        outMax.y = std::max(outMax.y, v.position.y);
        outMax.z = std::max(outMax.z, v.position.z);
    }
}

#if defined(USE_GLES) || defined(USE_IMGUI)

bool Mesh::uploadToGPU()
{
    if (vertices.empty() == true || indices.empty() == true) return false;

    if (m_vbo == 0) glGenBuffers(1, &m_vbo);
    if (m_ibo == 0) glGenBuffers(1, &m_ibo);
    if (m_vao == 0) glGenVertexArrays(1, &m_vao);

    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex3D), vertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(uint32_t), indices.data(), GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex3D), (void*)offsetof(Vertex3D, position));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex3D), (void*)offsetof(Vertex3D, normal));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex3D), (void*)offsetof(Vertex3D, texCoord));
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex3D), (void*)offsetof(Vertex3D, color));

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    m_uploaded = true;
    return true;
}

void Mesh::release()
{
    if (m_vao != 0) { glDeleteVertexArrays(1, &m_vao); m_vao = 0; }
    if (m_vbo != 0) { glDeleteBuffers(1, &m_vbo);      m_vbo = 0; }
    if (m_ibo != 0) { glDeleteBuffers(1, &m_ibo);      m_ibo = 0; }
    m_uploaded = false;
}

#else
bool Mesh::uploadToGPU() { return false; }
void Mesh::release()     {}
#endif // USE_GLES || USE_IMGUI

Mesh* Mesh::createCube(float size)
{
    Mesh*  mesh = new Mesh();
    float  hs   = size * 0.5f;
    Vec3   pos[8] = {
        Vec3(-hs,-hs,-hs), Vec3(hs,-hs,-hs), Vec3(hs,hs,-hs), Vec3(-hs,hs,-hs),
        Vec3(-hs,-hs, hs), Vec3(hs,-hs, hs), Vec3(hs,hs, hs), Vec3(-hs,hs, hs)
    };
    auto addFace = [&](int i0, int i1, int i2, int i3, const Vec3& n)
    {
        uint32_t base = (uint32_t)mesh->vertices.size();
        mesh->addVertex(Vertex3D(pos[i0], n, UI::Vec2(0,0)));
        mesh->addVertex(Vertex3D(pos[i1], n, UI::Vec2(1,0)));
        mesh->addVertex(Vertex3D(pos[i2], n, UI::Vec2(1,1)));
        mesh->addVertex(Vertex3D(pos[i3], n, UI::Vec2(0,1)));
        mesh->addTriangle(base, base+1, base+2);
        mesh->addTriangle(base, base+2, base+3);
    };
    addFace(4,5,6,7, Vec3( 0, 0, 1));
    addFace(1,0,3,2, Vec3( 0, 0,-1));
    addFace(5,1,2,6, Vec3( 1, 0, 0));
    addFace(0,4,7,3, Vec3(-1, 0, 0));
    addFace(7,6,2,3, Vec3( 0, 1, 0));
    addFace(0,1,5,4, Vec3( 0,-1, 0));
    return mesh;
}

Mesh* Mesh::createSphere(float radius, int segments, int rings)
{
    Mesh* mesh = new Mesh();
    for (int ring = 0; ring <= rings; ++ring)
    {
        float phi        = (float)ring / rings * 3.14159f;
        float y          = std::cos(phi) * radius;
        float ringRadius = std::sin(phi) * radius;
        for (int seg = 0; seg <= segments; ++seg)
        {
            float theta = (float)seg / segments * 2.0f * 3.14159f;
            Vec3  p(std::cos(theta) * ringRadius, y, std::sin(theta) * ringRadius);
            mesh->addVertex(Vertex3D(p, p.normalized(),
                UI::Vec2((float)seg / segments, (float)ring / rings)));
        }
    }
    for (int ring = 0; ring < rings; ++ring)
    {
        for (int seg = 0; seg < segments; ++seg)
        {
            int cur  = ring * (segments + 1) + seg;
            int next = cur + segments + 1;
            mesh->addTriangle(cur, next, cur + 1);
            mesh->addTriangle(cur + 1, next, next + 1);
        }
    }
    return mesh;
}

Mesh* Mesh::createPlane(float width, float height)
{
    Mesh* mesh = new Mesh();
    float hw = width * 0.5f, hh = height * 0.5f;
    mesh->addVertex(Vertex3D(Vec3(-hw,0,-hh), Vec3(0,1,0), UI::Vec2(0,0)));
    mesh->addVertex(Vertex3D(Vec3( hw,0,-hh), Vec3(0,1,0), UI::Vec2(1,0)));
    mesh->addVertex(Vertex3D(Vec3( hw,0, hh), Vec3(0,1,0), UI::Vec2(1,1)));
    mesh->addVertex(Vertex3D(Vec3(-hw,0, hh), Vec3(0,1,0), UI::Vec2(0,1)));
    mesh->addTriangle(0,1,2); mesh->addTriangle(0,2,3);
    return mesh;
}

Mesh* Mesh::createCylinder(float radius, float height, int segments)
{
    Mesh* mesh = new Mesh();
    float hh = height * 0.5f;

    int topCenter    = (int)mesh->vertices.size();
    mesh->addVertex(Vertex3D(Vec3(0, hh, 0), Vec3(0, 1,0), UI::Vec2(0.5f,0.5f)));
    int bottomCenter = (int)mesh->vertices.size();
    mesh->addVertex(Vertex3D(Vec3(0,-hh, 0), Vec3(0,-1,0), UI::Vec2(0.5f,0.5f)));

    for (int i = 0; i <= segments; ++i)
    {
        float theta = (float)i / segments * 2.0f * 3.14159f;
        float x = std::cos(theta) * radius, z = std::sin(theta) * radius;
        Vec3  n = Vec3(x, 0, z).normalized();
        mesh->addVertex(Vertex3D(Vec3(x, hh, z), n, UI::Vec2((float)i/segments, 0)));
        mesh->addVertex(Vertex3D(Vec3(x,-hh, z), n, UI::Vec2((float)i/segments, 1)));
    }

    int off = 2;
    for (int i = 0; i < segments; ++i)
    {
        int tc=off+i*2, tn=off+(i+1)*2, bc=tc+1, bn=tn+1;
        mesh->addTriangle(tc, bc, tn); mesh->addTriangle(tn, bc, bn);
        mesh->addTriangle(topCenter, tn, tc);
        mesh->addTriangle(bottomCenter, bc, bn);
    }
    return mesh;
}

Mesh* Mesh::createCone(float radius, float height, int segments)
{
    Mesh* mesh = new Mesh();
    float hh = height * 0.5f;

    int apex         = (int)mesh->vertices.size();
    mesh->addVertex(Vertex3D(Vec3(0, hh, 0), Vec3(0,1,0), UI::Vec2(0.5f,0)));
    int bottomCenter = (int)mesh->vertices.size();
    mesh->addVertex(Vertex3D(Vec3(0,-hh, 0), Vec3(0,-1,0), UI::Vec2(0.5f,0.5f)));

    for (int i = 0; i <= segments; ++i)
    {
        float theta = (float)i / segments * 2.0f * 3.14159f;
        float x = std::cos(theta) * radius, z = std::sin(theta) * radius;
        Vec3  tangent(-z, 0, x);
        Vec3  toApex  = Vec3(0, hh, 0) - Vec3(x, -hh, z);
        Vec3  normal  = tangent.cross(toApex).normalized();
        mesh->addVertex(Vertex3D(Vec3(x,-hh,z), normal, UI::Vec2((float)i/segments, 1)));
    }

    int off = 2;
    for (int i = 0; i < segments; ++i)
    {
        mesh->addTriangle(apex,         off+i,   off+i+1);
        mesh->addTriangle(bottomCenter, off+i+1, off+i);
    }
    return mesh;
}

// ============================================================================
// RenderSettings
// ============================================================================

RenderSettings::RenderSettings()
    : depthTest(true)
    , backfaceCulling(true)
    , wireframe(false)
    , showGrid(false)
    , clearColor(UI::Color4(0.1f, 0.12f, 0.15f, 1.0f))
    , ambientLight(UI::Color4(0.1f, 0.1f, 0.1f, 1.0f))
    , shadowBias(0.005f)
    , enableShadows(false)
    , shadowMapSize(1024)
    , enableBloom(false)
    , enableSSAO(false)
    , enableFXAA(false)
    , bloomThreshold(1.0f)
    , bloomIntensity(0.5f)
    , enableMRT(false)
{}

// ============================================================================
// Skeletal Animation
// ============================================================================

Transform AnimationChannel::interpolate(float time) const
{
    if (keyframes.empty() == true)   return Transform();
    if (keyframes.size() == 1) return Transform(keyframes[0].position, keyframes[0].rotation, keyframes[0].scale);

    size_t nextIndex = 0;
    for (size_t i = 0; i < keyframes.size(); ++i)
        if (keyframes[i].time > time) { nextIndex = i; break; }

    if (nextIndex == 0) return Transform(keyframes[0].position, keyframes[0].rotation, keyframes[0].scale);

    const AnimationKeyframe& prev = keyframes[nextIndex - 1];
    const AnimationKeyframe& next = keyframes[nextIndex];
    float duration = next.time - prev.time;
    float t = std::clamp((duration > 0.0f) ? ((time - prev.time) / duration) : 0.0f, 0.0f, 1.0f);

    Transform result;
    result.position = prev.position.lerp(next.position, t);
    result.scale    = prev.scale.lerp(next.scale, t);

    Quaternion qa = prev.rotation, qb = next.rotation;
    float dot = qa.x*qb.x + qa.y*qb.y + qa.z*qb.z + qa.w*qb.w;
    if (dot < 0.0f) { qb = Quaternion(-qb.x,-qb.y,-qb.z,-qb.w); dot = -dot; }

    if (dot > 0.9995f)
    {
        result.rotation = Quaternion(
            qa.x + (qb.x-qa.x)*t, qa.y + (qb.y-qa.y)*t,
            qa.z + (qb.z-qa.z)*t, qa.w + (qb.w-qa.w)*t).normalized();
    }
    else
    {
        float theta0 = std::acos(dot);
        float theta  = theta0 * t;
        float sa = std::sin(theta0 - theta) / std::sin(theta0);
        float sb = std::sin(theta)          / std::sin(theta0);
        result.rotation = Quaternion(
            sa*qa.x+sb*qb.x, sa*qa.y+sb*qb.y,
            sa*qa.z+sb*qb.z, sa*qa.w+sb*qb.w).normalized();
    }
    return result;
}

void AnimationClip::sample(float time, std::vector<Transform>& outJointTransforms) const
{
    for (const auto& channel : channels)
    {
        if (channel.jointIndex < 0 || channel.jointIndex >= (int)outJointTransforms.size()) continue;
        Transform sampled = channel.interpolate(time);
        Transform& out    = outJointTransforms[channel.jointIndex];
        switch (channel.path)
        {
            case AnimationChannel::Path::Translation: out.position = sampled.position; break;
            case AnimationChannel::Path::Rotation:    out.rotation = sampled.rotation; break;
            case AnimationChannel::Path::Scale:       out.scale    = sampled.scale;    break;
        }
    }
}

void Skeleton::updateMatrices(const std::vector<Transform>& localTransforms)
{
    if (joints.empty() == true) return;
    jointMatrices.resize(joints.size());
    std::vector<Matrix4x4> globalTransforms(joints.size());

    for (size_t i = 0; i < joints.size(); ++i)
    {
        Matrix4x4 local = (i < localTransforms.size())
            ? localTransforms[i].toMatrix()
            : joints[i].localTransform.toMatrix();

        globalTransforms[i] = (joints[i].parentIndex >= 0)
            ? globalTransforms[joints[i].parentIndex] * local
            : local;

        jointMatrices[i] = globalTransforms[i] * joints[i].inverseBindMatrix;
    }
}

int Skeleton::findJointIndex(const std::string& name) const
{
    for (size_t i = 0; i < joints.size(); ++i)
        if (joints[i].name == name) return (int)i;
    return -1;
}

bool SkinnedMesh::uploadToGPU() { return Mesh::uploadToGPU(); }

AnimationController::AnimationController()
    : m_currentTime(0.0f), m_isPlaying(false), m_loop(false)
{}

void AnimationController::addClip(const std::string& name, const AnimationClip& clip)
    { m_clips[name] = clip; }

void AnimationController::play(const std::string& name, bool loop)
{
    if (m_clips.find(name) == m_clips.end()) return;
    m_currentClip = name; m_currentTime = 0.0f; m_isPlaying = true; m_loop = loop;
}

void AnimationController::stop() { m_isPlaying = false; m_currentTime = 0.0f; }

void AnimationController::update(float deltaTime)
{
    if (m_isPlaying == false || m_clips.find(m_currentClip) == m_clips.end()) return;

    const AnimationClip& clip = m_clips[m_currentClip];
    m_currentTime += deltaTime;
    if (m_currentTime >= clip.duration)
    {
        if (m_loop == true) m_currentTime = std::fmod(m_currentTime, clip.duration);
        else { m_currentTime = clip.duration; m_isPlaying = false; }
    }

    std::vector<Transform> jointTransforms(m_skeleton.joints.size());
    for (size_t i = 0; i < jointTransforms.size(); ++i)
        jointTransforms[i] = m_skeleton.joints[i].localTransform;

    clip.sample(m_currentTime, jointTransforms);
    m_skeleton.updateMatrices(jointTransforms);
}

// ============================================================================
// Model
// ============================================================================

Model::Model() : rootNodeIndex(0) {}
Model::~Model() { release(); }

void Model::release() { clearMeshes(); materials.clear(); nodes.clear(); animations.clear(); }
void Model::clearMeshes()
{
    for (Mesh* mesh : meshes)
        if (mesh != nullptr) delete mesh;
    meshes.clear();
}

// ── GLTF Helpers ─────────────────────────────────────────────────────────────

static constexpr int GLTF_BYTE           = 5120;
static constexpr int GLTF_UNSIGNED_BYTE  = 5121;
static constexpr int GLTF_SHORT          = 5122;
static constexpr int GLTF_UNSIGNED_SHORT = 5123;
static constexpr int GLTF_UNSIGNED_INT   = 5125;
static constexpr int GLTF_FLOAT          = 5126;

struct GltfBuffer { std::vector<uint8_t> data; };

static float gltfReadFloat(const uint8_t* p, int componentType)
{
    switch (componentType)
    {
        case GLTF_FLOAT:          { float v;    memcpy(&v, p, 4); return v; }
        case GLTF_UNSIGNED_BYTE:  return p[0] / 255.0f;
        case GLTF_BYTE:           return std::max((int8_t)p[0] / 127.0f, -1.0f);
        case GLTF_UNSIGNED_SHORT: { uint16_t v; memcpy(&v, p, 2); return v / 65535.0f; }
        case GLTF_SHORT:          { int16_t  v; memcpy(&v, p, 2); return std::max(v / 32767.0f, -1.0f); }
        default: return 0.0f;
    }
}

static uint32_t gltfReadIndex(const uint8_t* p, int componentType)
{
    switch (componentType)
    {
        case GLTF_UNSIGNED_BYTE:  return p[0];
        case GLTF_UNSIGNED_SHORT: { uint16_t v; memcpy(&v, p, 2); return v; }
        case GLTF_UNSIGNED_INT:   { uint32_t v; memcpy(&v, p, 4); return v; }
        default: return 0;
    }
}

static int gltfComponentSize(int componentType)
{
    switch (componentType)
    {
        case GLTF_BYTE: case GLTF_UNSIGNED_BYTE:   return 1;
        case GLTF_SHORT: case GLTF_UNSIGNED_SHORT: return 2;
        case GLTF_UNSIGNED_INT: case GLTF_FLOAT:   return 4;
        default: return 4;
    }
}

static int gltfTypeCount(const std::string& type)
{
    if (type == "SCALAR") return 1;
    if (type == "VEC2")   return 2;
    if (type == "VEC3")   return 3;
    if (type == "VEC4")   return 4;
    if (type == "MAT4")   return 16;
    return 1;
}

static std::vector<uint8_t> gltfBase64Decode(const std::string& b64)
{
    static const char* table = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::vector<uint8_t> out;
    out.reserve(b64.size() * 3 / 4);
    int val = 0, bits = -8;
    for (unsigned char c : b64)
    {
        const char* pos = strchr(table, c);
        if (pos == nullptr) { if (c == '=') break; continue; }
        val = (val << 6) | (int)(pos - table);
        bits += 6;
        if (bits >= 0) { out.push_back((val >> bits) & 0xFF); bits -= 8; }
    }
    return out;
}

struct GltfAccessorView
{
    const uint8_t* data;
    size_t         count;
    int            stride;
    int            componentType;
    int            numComponents;
};

static GltfAccessorView gltfGetAccessor(
    const nlohmann::json& j, const std::vector<GltfBuffer>& buffers, int accessorIdx)
{
    GltfAccessorView view{};
    if (accessorIdx < 0) return view;

    const auto& acc   = j["accessors"][accessorIdx];
    const int   bvIdx = acc.value("bufferView", -1);
    if (bvIdx < 0) return view;

    const auto& bv     = j["bufferViews"][bvIdx];
    const int   bufIdx = bv.value("buffer", 0);
    if (bufIdx >= (int)buffers.size()) return view;

    view.componentType = acc.value("componentType", GLTF_FLOAT);
    view.numComponents = gltfTypeCount(acc.value("type", std::string("SCALAR")));
    view.count         = acc.value("count", size_t(0));

    const size_t bvOffset  = bv.value("byteOffset",  size_t(0));
    const size_t accOffset = acc.value("byteOffset", size_t(0));
    const int    compSize  = gltfComponentSize(view.componentType);
    const int    elemSize  = compSize * view.numComponents;
    view.stride            = bv.value("byteStride", elemSize);
    view.data              = buffers[bufIdx].data.data() + bvOffset + accOffset;
    return view;
}

static UI::Texture* gltfLoadTexture(
    const nlohmann::json& j, const std::vector<GltfBuffer>& buffers,
    const std::string& baseDir, int texIdx)
{
    if (texIdx < 0 || j.contains("textures") == false) return nullptr;
    const auto& texNode = j["textures"][texIdx];
    const int   srcIdx  = texNode.value("source", -1);
    if (srcIdx < 0 || j.contains("images") == false) return nullptr;

    const auto&       imgNode = j["images"][srcIdx];
    const std::string uri     = imgNode.value("uri", std::string(""));
    const int         bvIdx   = imgNode.value("bufferView", -1);

    int w = 0, h = 0, ch = 0;
    stbi_uc* pixels = nullptr;

    if (bvIdx >= 0)
    {
        const auto&  bv   = j["bufferViews"][bvIdx];
        const int    bufI = bv.value("buffer", 0);
        const size_t off  = bv.value("byteOffset", size_t(0));
        const size_t len  = bv.value("byteLength", size_t(0));
        if (bufI < (int)buffers.size() && len > 0)
            pixels = stbi_load_from_memory(buffers[bufI].data.data() + off, (int)len, &w, &h, &ch, 4);
    }
    else if (uri.empty() == false)
    {
        if (uri.substr(0, 5) == "data:")
        {
            const size_t comma = uri.find(',');
            if (comma != std::string::npos)
            {
                auto decoded = gltfBase64Decode(uri.substr(comma + 1));
                pixels = stbi_load_from_memory(decoded.data(), (int)decoded.size(), &w, &h, &ch, 4);
            }
        }
        else
        {
            pixels = stbi_load((baseDir + uri).c_str(), &w, &h, &ch, 4);
        }
    }

    if (pixels == nullptr) return nullptr;
    UI::Texture* tex = new UI::Texture();
    tex->loadFromMemory(pixels, w, h, 4);
    stbi_image_free(pixels);
    return tex;
}

static Matrix4x4 gltfNodeMatrix(const nlohmann::json& node)
{
    if (node.contains("matrix") == true)
    {
        Matrix4x4 m;
        const auto& arr = node["matrix"];
        for (int i = 0; i < 16; ++i) m.m[i] = arr[i].get<float>();
        return m;
    }
    Vec3       translation(0.0f, 0.0f, 0.0f);
    Quaternion rotation = Quaternion::identity();
    Vec3       scale(1.0f, 1.0f, 1.0f);

    if (node.contains("translation") == true)
    {
        const auto& tr = node["translation"];
        translation = Vec3(tr[0].get<float>(), tr[1].get<float>(), tr[2].get<float>());
    }
    if (node.contains("rotation") == true)
    {
        const auto& r = node["rotation"];
        rotation = Quaternion(r[0].get<float>(), r[1].get<float>(),
                              r[2].get<float>(), r[3].get<float>()).normalized();
    }
    if (node.contains("scale") == true)
    {
        const auto& s = node["scale"];
        scale = Vec3(s[0].get<float>(), s[1].get<float>(), s[2].get<float>());
    }
    return Matrix4x4::makeTranslation(translation) * rotation.toMatrix() * Matrix4x4::makeScale(scale);
}

static Transform matrixToTransform(const Matrix4x4& m)
{
    Transform t;
    t.position = Vec3(m.m[12], m.m[13], m.m[14]);

    Vec3 col0(m.m[0], m.m[1], m.m[2]);
    Vec3 col1(m.m[4], m.m[5], m.m[6]);
    Vec3 col2(m.m[8], m.m[9], m.m[10]);
    t.scale = Vec3(col0.length(), col1.length(), col2.length());

    if (t.scale.x > 1e-6f) col0 = col0 * (1.0f / t.scale.x);
    if (t.scale.y > 1e-6f) col1 = col1 * (1.0f / t.scale.y);
    if (t.scale.z > 1e-6f) col2 = col2 * (1.0f / t.scale.z);

    float trace = col0.x + col1.y + col2.z;
    if (trace > 0.0f)
    {
        float s = 0.5f / std::sqrt(trace + 1.0f);
        t.rotation.w = 0.25f / s;
        t.rotation.x = (col1.z - col2.y) * s;
        t.rotation.y = (col2.x - col0.z) * s;
        t.rotation.z = (col0.y - col1.x) * s;
    }
    else if (col0.x > col1.y && col0.x > col2.z)
    {
        float s = 2.0f * std::sqrt(1.0f + col0.x - col1.y - col2.z);
        t.rotation.w = (col1.z - col2.y) / s; t.rotation.x = 0.25f * s;
        t.rotation.y = (col1.x + col0.y) / s; t.rotation.z = (col2.x + col0.z) / s;
    }
    else if (col1.y > col2.z)
    {
        float s = 2.0f * std::sqrt(1.0f + col1.y - col0.x - col2.z);
        t.rotation.w = (col2.x - col0.z) / s; t.rotation.x = (col1.x + col0.y) / s;
        t.rotation.y = 0.25f * s;             t.rotation.z = (col2.y + col1.z) / s;
    }
    else
    {
        float s = 2.0f * std::sqrt(1.0f + col2.z - col0.x - col1.y);
        t.rotation.w = (col0.y - col1.x) / s; t.rotation.x = (col2.x + col0.z) / s;
        t.rotation.y = (col2.y + col1.z) / s; t.rotation.z = 0.25f * s;
    }
    t.rotation = t.rotation.normalized();
    return t;
}

static Transform gltfNodeTransform(const nlohmann::json& node) { return matrixToTransform(gltfNodeMatrix(node)); }

namespace TransformHelper { Transform fromMatrix(const Matrix4x4& m) { return matrixToTransform(m); } }

// ============================================================================
// ModelInstance
// ============================================================================

ModelInstance::ModelInstance()
    : m_model(nullptr)
    , m_viewport(nullptr)
    , m_positionOffset(0.0f, 0.0f, 0.0f)
    , m_halfExtents(0.0f, 0.0f, 0.0f)
    , m_groundLift(0.0f)
    , m_activeClip(-1)
    , m_animTime(0.0f)
{}

ModelInstance::~ModelInstance() { unload(); }

void ModelInstance::buildWorldList(std::vector<RenderEntry>& out, Vec3& bMin, Vec3& bMax) const
{
    if (m_model == nullptr) return;

    struct NodeWork { int idx; Matrix4x4 worldMat; };
    std::vector<NodeWork> stack;
    stack.push_back({ m_model->rootNodeIndex, Matrix4x4() });
    for (int ni = 0; ni < (int)m_model->nodes.size(); ++ni)
        if (m_model->nodes[ni].parentIndex == -1 && ni != m_model->rootNodeIndex)
            stack.push_back({ ni, Matrix4x4() });

    while (stack.empty() == false)
    {
        NodeWork nw = stack.back(); stack.pop_back();
        if (nw.idx < 0 || nw.idx >= (int)m_model->nodes.size()) continue;

        const ModelNode& node  = m_model->nodes[nw.idx];
        Matrix4x4        world = nw.worldMat * node.transform.toMatrix();

        for (int mi : node.meshIndices)
        {
            if (mi < 0 || mi >= (int)m_model->meshes.size()) continue;
            const Mesh* mesh = m_model->meshes[mi];
            if (mesh == nullptr || mesh->vertices.empty() == true) continue;

            out.push_back({ mi, nw.idx, world });

            Vec3 lMin, lMax;
            mesh->calculateBounds(lMin, lMax);
            const Vec3 corners[8] = {
                {lMin.x,lMin.y,lMin.z},{lMax.x,lMin.y,lMin.z},
                {lMax.x,lMax.y,lMin.z},{lMin.x,lMax.y,lMin.z},
                {lMin.x,lMin.y,lMax.z},{lMax.x,lMin.y,lMax.z},
                {lMax.x,lMax.y,lMax.z},{lMin.x,lMax.y,lMax.z},
            };
            for (const auto& c : corners)
            {
                Vec3 wc = world.transformPoint(c);
                bMin.x = std::min(bMin.x, wc.x); bMin.y = std::min(bMin.y, wc.y); bMin.z = std::min(bMin.z, wc.z);
                bMax.x = std::max(bMax.x, wc.x); bMax.y = std::max(bMax.y, wc.y); bMax.z = std::max(bMax.z, wc.z);
            }
        }
        for (int ci : node.childIndices)
            stack.push_back({ ci, world });
    }
}

void ModelInstance::computeWorldMatrices(const std::vector<Transform>& local, std::vector<Matrix4x4>& worldMats) const
{
    if (m_model == nullptr) return;

    struct NodeWork { int idx; Matrix4x4 parentMat; };
    std::vector<NodeWork> stack;
    stack.push_back({ m_model->rootNodeIndex, Matrix4x4() });
    for (int ni = 0; ni < (int)m_model->nodes.size(); ++ni)
        if (m_model->nodes[ni].parentIndex == -1 && ni != m_model->rootNodeIndex)
            stack.push_back({ ni, Matrix4x4() });

    while (stack.empty() == false)
    {
        NodeWork nw = stack.back(); stack.pop_back();
        if (nw.idx < 0 || nw.idx >= (int)worldMats.size()) continue;
        worldMats[nw.idx] = nw.parentMat * local[nw.idx].toMatrix();
        for (int ci : m_model->nodes[nw.idx].childIndices)
            stack.push_back({ ci, worldMats[nw.idx] });
    }
}

bool ModelInstance::load(Model* model, Viewport& viewport)
{
    unload();
    if (model == nullptr) return false;

    m_model    = model;
    m_viewport = &viewport;

    Vec3 bMin( 1e9f, 1e9f, 1e9f), bMax(-1e9f,-1e9f,-1e9f);
    std::vector<RenderEntry> entries;
    buildWorldList(entries, bMin, bMax);

    if (entries.empty() == true)
    {
        for (int mi = 0; mi < (int)model->meshes.size(); ++mi)
        {
            if (model->meshes[mi] == nullptr || model->meshes[mi]->vertices.empty() == true) continue;
            Vec3 lMin, lMax;
            model->meshes[mi]->calculateBounds(lMin, lMax);
            bMin.x = std::min(bMin.x,lMin.x); bMin.y = std::min(bMin.y,lMin.y); bMin.z = std::min(bMin.z,lMin.z);
            bMax.x = std::max(bMax.x,lMax.x); bMax.y = std::max(bMax.y,lMax.y); bMax.z = std::max(bMax.z,lMax.z);
            entries.push_back({ mi, -1, Matrix4x4() });
        }
    }
    if (entries.empty() == true) return false;

    const Vec3  center   = (bMin + bMax) * 0.5f;
    const float extent   = std::max<float>({ bMax.x-bMin.x, bMax.y-bMin.y, bMax.z-bMin.z });
    const float uniScale = (extent > 0.0f) ? (3.0f / extent) : 1.0f;
    m_normMat = Matrix4x4::makeScale(Vec3(uniScale,uniScale,uniScale)) *
                Matrix4x4::makeTranslation(Vec3(-center.x,-center.y,-center.z));

    m_halfExtents  = Vec3((bMax.x - bMin.x) * uniScale * 0.5f,
                          (bMax.y - bMin.y) * uniScale * 0.5f,
                          (bMax.z - bMin.z) * uniScale * 0.5f);
    m_groundLift   = m_halfExtents.y;   // move up so bottom sits at Y=0
    m_positionOffset = Vec3(0.0f, 0.0f, 0.0f);
    m_offsetMat    = Matrix4x4();  // default ctor = identity

    const Material defaultMat;
    m_objNodeIdx.clear();
    m_baseTransforms.clear();
    for (const auto& e : entries)
    {
        const Material& mat = (e.mi < (int)model->materials.size()) ? model->materials[e.mi] : defaultMat;
        Matrix4x4 baseMat = m_normMat * e.worldMat;
        m_baseTransforms.push_back(baseMat);        // cache for setPosition()
        Transform t = matrixToTransform(m_offsetMat * baseMat);
        viewport.getScene().addObject(model->meshes[e.mi], mat, t);
        m_objNodeIdx.push_back(e.nodeIdx);
    }

    m_activeClip = model->animations.empty() ? -1 : 0;
    m_animTime   = 0.0f;
    return true;
}

void ModelInstance::update(float deltaTime)
{
    if (m_model == nullptr || m_viewport == nullptr) return;
    if (m_activeClip < 0 || m_activeClip >= (int)m_model->animations.size()) return;
    if (m_objNodeIdx.empty() == true) return;

    const AnimationClip& clip = m_model->animations[m_activeClip];
    if (clip.duration > 0.0f)
    {
        m_animTime += deltaTime;
        if (m_animTime > clip.duration) m_animTime = std::fmod(m_animTime, clip.duration);
    }

    const int nodeCount = (int)m_model->nodes.size();
    std::vector<Transform> local(nodeCount);
    for (int i = 0; i < nodeCount; ++i) local[i] = m_model->nodes[i].transform;
    clip.sample(m_animTime, local);

    std::vector<Matrix4x4> worldMats(nodeCount);
    computeWorldMatrices(local, worldMats);

    auto& objects = m_viewport->getScene().getObjects();
    const int objCount = std::min((int)objects.size(), (int)m_objNodeIdx.size());
    for (int oi = 0; oi < objCount; ++oi)
    {
        const int ni = m_objNodeIdx[oi];
        if (ni < 0 || ni >= nodeCount) continue;
        Matrix4x4 baseMat = m_normMat * worldMats[ni];
        if (oi < (int)m_baseTransforms.size()) m_baseTransforms[oi] = baseMat;
        objects[oi].transform = matrixToTransform(m_offsetMat * baseMat);
    }
}

void ModelInstance::setPosition(const Vec3& pos)
{
    m_positionOffset = pos;
    m_offsetMat      = Matrix4x4::makeTranslation(pos);

    if (m_viewport == nullptr || m_objNodeIdx.empty()) return;
    auto& objects = m_viewport->getScene().getObjects();
    const int objCount = std::min((int)objects.size(), (int)m_objNodeIdx.size());

    for (int oi = 0; oi < objCount && oi < (int)m_baseTransforms.size(); ++oi)
    {
        Matrix4x4 final = m_offsetMat * m_baseTransforms[oi];
        objects[oi].transform = matrixToTransform(final);
    }
}

void ModelInstance::unload()
{
    if (m_viewport != nullptr && m_objNodeIdx.empty() == false)
        m_viewport->getScene().clear();
    m_model      = nullptr;
    m_viewport   = nullptr;
    m_activeClip = -1;
    m_animTime   = 0.0f;
    m_objNodeIdx.clear();
}

// ── GLTF Parser ──────────────────────────────────────────────────────────────

static Model* parseGLTF(const nlohmann::json& j, const std::vector<GltfBuffer>& buffers, const std::string& baseDir)
{
    Model* model = new Model();

    // Textures / Materials
    std::vector<UI::Texture*> loadedTextures;
    if (j.contains("textures") == true)
        for (int ti = 0; ti < (int)j["textures"].size(); ++ti)
            loadedTextures.push_back(gltfLoadTexture(j, buffers, baseDir, ti));

    if (j.contains("materials") == true)
    {
        for (const auto& mat : j["materials"])
        {
            Material m;
            m.doubleSided = mat.value("doubleSided", false);

            const std::string alphaMode = mat.value("alphaMode", std::string("OPAQUE"));
            if      (alphaMode == "BLEND") m.alphaMode = Material::AlphaMode::BLEND;
            else if (alphaMode == "MASK")  m.alphaMode = Material::AlphaMode::MASK;
            else                           m.alphaMode = Material::AlphaMode::OPAQUE;
            m.alphaCutoff = mat.value("alphaCutoff", 0.5f);

            m.unlit = mat.contains("extensions") == true && mat["extensions"].contains("KHR_materials_unlit") == true;

            if (mat.contains("pbrMetallicRoughness") == true)
            {
                const auto& pbr = mat["pbrMetallicRoughness"];
                if (pbr.contains("baseColorFactor") == true)
                {
                    const auto& c = pbr["baseColorFactor"];
                    m.albedoColor = UI::Color4(c[0].get<float>(), c[1].get<float>(),
                                               c[2].get<float>(), c[3].get<float>());
                }
                if (pbr.contains("baseColorTexture") == true)
                {
                    const int idx = pbr["baseColorTexture"].value("index", -1);
                    if (idx >= 0 && idx < (int)loadedTextures.size())
                        m.albedoTexture = loadedTextures[idx];
                }
                m.metallic  = pbr.value("metallicFactor",  0.0f);
                m.roughness = pbr.value("roughnessFactor", 0.5f);
            }
            if (mat.contains("normalTexture") == true)
            {
                const int idx = mat["normalTexture"].value("index", -1);
                if (idx >= 0 && idx < (int)loadedTextures.size())
                    m.normalTexture = loadedTextures[idx];
                m.normalStrength = mat["normalTexture"].value("scale", 1.0f);
            }
            if (mat.contains("emissiveFactor") == true)
            {
                const auto& e = mat["emissiveFactor"];
                m.emissiveColor = UI::Color4(e[0].get<float>(), e[1].get<float>(), e[2].get<float>(), 1.0f);
            }
            model->materials.push_back(m);
        }
    }

    // Meshes
    std::vector<std::vector<std::pair<int,int>>> gltfMeshToPrims;
    std::vector<int> primMaterials;

    if (j.contains("meshes") == true)
    {
        gltfMeshToPrims.resize(j["meshes"].size());

        for (int gmi = 0; gmi < (int)j["meshes"].size(); ++gmi)
        {
            const auto& meshNode = j["meshes"][gmi];
            if (meshNode.contains("primitives") == false) continue;

            for (const auto& prim : meshNode["primitives"])
            {
                const auto& attrs = prim["attributes"];
                if (attrs.contains("POSITION") == false) continue;

                Mesh* mesh = new Mesh();
                auto posView    = gltfGetAccessor(j, buffers, attrs["POSITION"].get<int>());
                GltfAccessorView normalView{}, uvView{}, colorView{};
                if (attrs.contains("NORMAL") == true)     normalView = gltfGetAccessor(j, buffers, attrs["NORMAL"].get<int>());
                if (attrs.contains("TEXCOORD_0") == true) uvView     = gltfGetAccessor(j, buffers, attrs["TEXCOORD_0"].get<int>());
                if (attrs.contains("COLOR_0") == true)    colorView  = gltfGetAccessor(j, buffers, attrs["COLOR_0"].get<int>());

                for (size_t vi = 0; vi < posView.count; ++vi)
                {
                    Vertex3D       v;
                    const uint8_t* pp = posView.data + vi * posView.stride;
                    float px, py, pz;
                    memcpy(&px, pp, 4); memcpy(&py, pp+4, 4); memcpy(&pz, pp+8, 4);
                    v.position = Vec3(px, py, pz);

                    if (normalView.data != nullptr && vi < normalView.count)
                    {
                        const uint8_t* np = normalView.data + vi * normalView.stride;
                        v.normal = Vec3(gltfReadFloat(np,     normalView.componentType),
                                        gltfReadFloat(np + 4, normalView.componentType),
                                        gltfReadFloat(np + 8, normalView.componentType));
                    }
                    if (uvView.data != nullptr && vi < uvView.count)
                    {
                        const uint8_t* up = uvView.data + vi * uvView.stride;
                        v.texCoord = UI::Vec2(gltfReadFloat(up,     uvView.componentType),
                                              gltfReadFloat(up + 4, uvView.componentType));
                    }
                    if (colorView.data != nullptr && vi < colorView.count)
                    {
                        const uint8_t* cp = colorView.data + vi * colorView.stride;
                        const int      cs = gltfComponentSize(colorView.componentType);
                        v.color = UI::Color4(
                            gltfReadFloat(cp,       colorView.componentType),
                            gltfReadFloat(cp + cs,  colorView.componentType),
                            gltfReadFloat(cp+cs*2,  colorView.componentType),
                            colorView.numComponents >= 4
                                ? gltfReadFloat(cp+cs*3, colorView.componentType)
                                : 1.0f);
                    }
                    mesh->vertices.push_back(v);
                }

                if (prim.contains("indices") == true)
                {
                    auto idxView = gltfGetAccessor(j, buffers, prim["indices"].get<int>());
                    for (size_t ii = 0; ii < idxView.count; ++ii)
                        mesh->indices.push_back(gltfReadIndex(
                            idxView.data + ii * idxView.stride, idxView.componentType));
                }
                else
                {
                    for (uint32_t ii = 0; ii < (uint32_t)mesh->vertices.size(); ++ii)
                        mesh->indices.push_back(ii);
                }

                if (mesh->vertices.empty() == true) { delete mesh; continue; }
                if (normalView.data == nullptr) mesh->generateNormals();

                const int flatIdx = (int)model->meshes.size();
                const int matIdx  = prim.value("material", -1);
                gltfMeshToPrims[gmi].emplace_back(flatIdx, matIdx);
                primMaterials.push_back(matIdx);
                model->meshes.push_back(mesh);
            }
        }
    }

    // Nodes
    if (j.contains("nodes") == true)
    {
        for (const auto& nodeJson : j["nodes"])
        {
            ModelNode mn;
            mn.name      = nodeJson.value("name", std::string(""));
            mn.transform = gltfNodeTransform(nodeJson);
            if (nodeJson.contains("mesh") == true)
            {
                const int gmi = nodeJson["mesh"].get<int>();
                if (gmi >= 0 && gmi < (int)gltfMeshToPrims.size())
                    for (const auto& p : gltfMeshToPrims[gmi])
                        mn.meshIndices.push_back(p.first);
            }
            if (nodeJson.contains("children") == true)
                for (int c : nodeJson["children"]) mn.childIndices.push_back(c);
            model->nodes.push_back(mn);
        }
    }

    // Align materials with mesh array
    {
        std::vector<Material> flatMaterials(model->meshes.size());
        for (int fi = 0; fi < (int)primMaterials.size() && fi < (int)model->meshes.size(); ++fi)
        {
            const int matIdx = primMaterials[fi];
            if (matIdx >= 0 && matIdx < (int)model->materials.size())
                flatMaterials[fi] = model->materials[matIdx];
        }
        model->materials = std::move(flatMaterials);
    }

    // Animations
    if (j.contains("animations") == true && j.contains("accessors") == true)
    {
        for (const auto& animJson : j["animations"])
        {
            AnimationClip clip;
            clip.name     = animJson.value("name", std::string(""));
            clip.duration = 0.0f;

            if (animJson.contains("channels") == false || animJson.contains("samplers") == false) continue;
            const auto& samplers = animJson["samplers"];

            for (const auto& chanJson : animJson["channels"])
            {
                const int samplerIdx = chanJson.value("sampler", -1);
                if (samplerIdx < 0 || samplerIdx >= (int)samplers.size()) continue;
                if (chanJson.contains("target") == false) continue;

                const auto& target     = chanJson["target"];
                const int   nodeIdx    = target.value("node", -1);
                const std::string path = target.value("path", std::string(""));

                const auto& sampler   = samplers[samplerIdx];
                const int   inputAcc  = sampler.value("input",  -1);
                const int   outputAcc = sampler.value("output", -1);
                if (inputAcc < 0 || outputAcc < 0) continue;

                auto timeView = gltfGetAccessor(j, buffers, inputAcc);
                auto valView  = gltfGetAccessor(j, buffers, outputAcc);
                if (timeView.data == nullptr || valView.data == nullptr) continue;

                std::vector<float> times(timeView.count);
                for (size_t k = 0; k < timeView.count; ++k)
                {
                    float t; memcpy(&t, timeView.data + k * timeView.stride, 4);
                    times[k] = t;
                    if (t > clip.duration) clip.duration = t;
                }

                AnimationChannel channel;
                channel.jointIndex = nodeIdx;
                if      (path == "translation") channel.path = AnimationChannel::Path::Translation;
                else if (path == "rotation")    channel.path = AnimationChannel::Path::Rotation;
                else if (path == "scale")       channel.path = AnimationChannel::Path::Scale;
                else continue;

                for (size_t k = 0; k < timeView.count; ++k)
                {
                    AnimationKeyframe kf;
                    kf.time  = times[k];
                    kf.scale = Vec3(1.0f, 1.0f, 1.0f);

                    const uint8_t* vp = valView.data + k * valView.stride;
                    switch (channel.path)
                    {
                        case AnimationChannel::Path::Translation:
                        {
                            float x, y, z;
                            memcpy(&x, vp, 4); memcpy(&y, vp+4, 4); memcpy(&z, vp+8, 4);
                            kf.position = Vec3(x, y, z);
                            break;
                        }
                        case AnimationChannel::Path::Rotation:
                        {
                            float x, y, z, w;
                            memcpy(&x, vp, 4); memcpy(&y, vp+4, 4);
                            memcpy(&z, vp+8, 4); memcpy(&w, vp+12, 4);
                            kf.rotation = Quaternion(x, y, z, w).normalized();
                            break;
                        }
                        case AnimationChannel::Path::Scale:
                        {
                            float x, y, z;
                            memcpy(&x, vp, 4); memcpy(&y, vp+4, 4); memcpy(&z, vp+8, 4);
                            kf.scale = Vec3(x, y, z);
                            break;
                        }
                    }
                    channel.keyframes.push_back(kf);
                }
                if (channel.keyframes.empty() == false) clip.channels.push_back(std::move(channel));
            }
            if (clip.channels.empty() == false) model->animations.push_back(std::move(clip));
        }
    }

    // Scene root
    model->rootNodeIndex = 0;
    if (j.contains("scene") == true && j.contains("scenes") == true)
    {
        const int   sceneIdx = j["scene"].get<int>();
        const auto& scene    = j["scenes"][sceneIdx];
        if (scene.contains("nodes") == true && scene["nodes"].empty() == false)
            model->rootNodeIndex = scene["nodes"][0].get<int>();
    }
    return model;
}

// ── ModelLoader ──────────────────────────────────────────────────────────────

Model* ModelLoader::loadFromFile(const std::string& path, Format format)
{
    if (format == Format::AUTO) format = detectFormat(path);
    switch (format)
    {
        case Format::GLTF: return loadGLTF(path);
        case Format::OBJ:  return loadOBJ(path);
        default:           return nullptr;
    }
}

ModelLoader::Format ModelLoader::detectFormat(const std::string& path)
{
    size_t dotPos = path.find_last_of('.');
    if (dotPos == std::string::npos) return Format::AUTO;
    std::string ext = path.substr(dotPos + 1);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    if (ext == "gltf" || ext == "glb") return Format::GLTF;
    if (ext == "obj")                  return Format::OBJ;
    return Format::AUTO;
}

Model* ModelLoader::loadGLTF(const std::string& path)
{
    std::ifstream file(path, std::ios::binary);
    if (file.is_open() == false)
    {
        LOG_APP_WARNINGF("ModelLoader: Cannot open '%s'", path.c_str());
        return nullptr;
    }
    std::vector<uint8_t> raw(
        (std::istreambuf_iterator<char>(file)),
        std::istreambuf_iterator<char>());
    file.close();

    const bool isGLB = (raw.size() >= 12 &&
                        raw[0]==0x67 && raw[1]==0x6C && raw[2]==0x54 && raw[3]==0x46);

    nlohmann::json          j;
    std::vector<GltfBuffer> buffers;

    if (isGLB == true)
    {
        if (raw.size() < 28) return nullptr;
        uint32_t jsonLen; memcpy(&jsonLen, raw.data() + 12, 4);
        std::string jsonStr(raw.begin() + 20, raw.begin() + 20 + jsonLen);
        j = nlohmann::json::parse(jsonStr, nullptr, false);
        if (j.is_discarded() == true)
        {
            LOG_APP_WARNINGF("ModelLoader: GLB JSON parse failed '%s'", path.c_str());
            return nullptr;
        }
        const size_t binChunkStart = 20 + jsonLen;
        if (binChunkStart + 8 <= raw.size())
        {
            uint32_t binLen; memcpy(&binLen, raw.data() + binChunkStart, 4);
            GltfBuffer buf;
            buf.data.assign(raw.begin() + binChunkStart + 8,
                            raw.begin() + binChunkStart + 8 + binLen);
            buffers.push_back(std::move(buf));
        }
    }
    else
    {
        std::string jsonStr(raw.begin(), raw.end());
        j = nlohmann::json::parse(jsonStr, nullptr, false);
        if (j.is_discarded() == true)
        {
            LOG_APP_WARNINGF("ModelLoader: GLTF JSON parse failed '%s'", path.c_str());
            return nullptr;
        }
        const std::string baseDir = path.substr(0, path.find_last_of("/\\") + 1);
        if (j.contains("buffers") == true)
        {
            for (const auto& bufNode : j["buffers"])
            {
                GltfBuffer        buf;
                const std::string uri = bufNode.value("uri", std::string(""));
                if (uri.substr(0, 5) == "data:")
                {
                    const size_t comma = uri.find(',');
                    if (comma != std::string::npos)
                        buf.data = gltfBase64Decode(uri.substr(comma + 1));
                }
                else if (uri.empty() == false)
                {
                    std::ifstream bf(baseDir + uri, std::ios::binary);
                    if (bf) buf.data.assign(
                        std::istreambuf_iterator<char>(bf),
                        std::istreambuf_iterator<char>());
                }
                buffers.push_back(std::move(buf));
            }
        }
    }

    const std::string baseDir = path.substr(0, path.find_last_of("/\\") + 1);
    return parseGLTF(j, buffers, baseDir);
}

Model* ModelLoader::loadOBJ(const std::string& path)
{
    std::ifstream file(path);
    if (file.is_open() == false)
    {
        LOG_APP_WARNINGF("ModelLoader: Cannot open OBJ '%s'", path.c_str());
        return nullptr;
    }

    Model*                model = new Model();
    Mesh*                 mesh  = new Mesh();
    std::vector<Vec3>     positions, normals;
    std::vector<UI::Vec2> uvs;

    std::string line;
    while (std::getline(file, line))
    {
        if (line.empty() == true || line[0] == '#') continue;
        std::istringstream ss(line);
        std::string token; ss >> token;

        if (token == "v")
            { float x,y,z; ss>>x>>y>>z; positions.push_back(Vec3(x,y,z)); }
        else if (token == "vn")
            { float x,y,z; ss>>x>>y>>z; normals.push_back(Vec3(x,y,z)); }
        else if (token == "vt")
            { float u,v; ss>>u>>v; uvs.push_back(UI::Vec2(u, 1.0f-v)); }
        else if (token == "f")
        {
            std::vector<Vertex3D> faceVerts;
            std::string faceToken;
            while (ss >> faceToken)
            {
                Vertex3D vert;
                int pi = 0, ti = 0, ni = 0;
                int fields = sscanf(faceToken.c_str(), "%d/%d/%d", &pi, &ti, &ni);
                if (fields < 2) sscanf(faceToken.c_str(), "%d//%d", &pi, &ni);
                if (pi > 0 && pi <= (int)positions.size()) vert.position = positions[pi-1];
                if (ti > 0 && ti <= (int)uvs.size())       vert.texCoord = uvs[ti-1];
                if (ni > 0 && ni <= (int)normals.size())   vert.normal   = normals[ni-1];
                faceVerts.push_back(vert);
            }
            uint32_t base = (uint32_t)mesh->vertices.size();
            for (auto& fv : faceVerts) mesh->vertices.push_back(fv);
            for (size_t fi = 1; fi + 1 < faceVerts.size(); ++fi)
            {
                mesh->indices.push_back(base);
                mesh->indices.push_back(base + (uint32_t)fi);
                mesh->indices.push_back(base + (uint32_t)fi + 1);
            }
        }
    }
    file.close();

    if (mesh->vertices.empty() == true) { delete mesh; delete model; return nullptr; }
    if (normals.empty() == true) mesh->generateNormals();

    model->meshes.push_back(mesh);
    ModelNode root; root.meshIndices.push_back(0);
    model->nodes.push_back(root);
    model->rootNodeIndex = 0;
    model->materials.push_back(Material());
    return model;
}

// ============================================================================
// MRTFramebuffer
// ============================================================================

#if defined(USE_GLES) || defined(USE_IMGUI)

MRTFramebuffer::MRTFramebuffer()
    : fbo(0), depthTexture(0), width(0), height(0), colorAttachmentCount(0)
    { for (int i = 0; i < 4; ++i) colorTextures[i] = 0; }

MRTFramebuffer::~MRTFramebuffer() { release(); }

bool MRTFramebuffer::create(int w, int h, int numColorAttachments)
{
    if (w <= 0 || h <= 0 || numColorAttachments < 1 || numColorAttachments > 4) return false;
    width = w; height = h; colorAttachmentCount = numColorAttachments;

    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    GLenum drawBuffers[4];
    for (int i = 0; i < colorAttachmentCount; ++i)
    {
        glGenTextures(1, &colorTextures[i]);
        glBindTexture(GL_TEXTURE_2D, colorTextures[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, GL_TEXTURE_2D, colorTextures[i], 0);
        drawBuffers[i] = GL_COLOR_ATTACHMENT0 + i;
    }
    glDrawBuffers(colorAttachmentCount, drawBuffers);

    glGenTextures(1, &depthTexture);
    glBindTexture(GL_TEXTURE_2D, depthTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, width, height, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthTexture, 0);

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return (status == GL_FRAMEBUFFER_COMPLETE);
}

void MRTFramebuffer::release()
{
    if (depthTexture != 0) { glDeleteTextures(1, &depthTexture); depthTexture = 0; }
    for (int i = 0; i < 4; ++i)
        if (colorTextures[i] != 0) { glDeleteTextures(1, &colorTextures[i]); colorTextures[i] = 0; }
    colorAttachmentCount = 0;
    if (fbo != 0) { glDeleteFramebuffers(1, &fbo); fbo = 0; }
}

bool     MRTFramebuffer::bind()                          { if (fbo == 0) return false; glBindFramebuffer(GL_FRAMEBUFFER, fbo); return true; }
void     MRTFramebuffer::unbind()                        { glBindFramebuffer(GL_FRAMEBUFFER, 0); }
bool     MRTFramebuffer::isValid()                 const { return (fbo != 0); }
uint32_t MRTFramebuffer::getColorTexture(int index) const { return (index >= 0 && index < colorAttachmentCount) ? colorTextures[index] : 0; }
uint32_t MRTFramebuffer::getDepthTexture()          const { return depthTexture; }

#else
    MRTFramebuffer::MRTFramebuffer() : fbo(0), depthTexture(0), width(0), height(0), colorAttachmentCount(0)
        { for (int i = 0; i < 4; ++i) colorTextures[i] = 0; }
    MRTFramebuffer::~MRTFramebuffer() {}
    bool     MRTFramebuffer::create(int, int, int) { return false; }
    void     MRTFramebuffer::release()             {}
    bool     MRTFramebuffer::bind()                { return false; }
    void     MRTFramebuffer::unbind()              {}
    bool     MRTFramebuffer::isValid()       const { return false; }
    uint32_t MRTFramebuffer::getColorTexture(int)  const { return 0; }
    uint32_t MRTFramebuffer::getDepthTexture()     const { return 0; }
#endif // USE_GLES || USE_IMGUI

// ============================================================================
// GL3DCanvas
// ============================================================================

#if defined(USE_GLES) || defined(USE_IMGUI)

GL3DCanvas::GL3DCanvas()
    : m_initialized(false)
    , m_viewportX(0), m_viewportY(0), m_viewportWidth(800), m_viewportHeight(600)
    , m_pbrShader(0), m_unlitShader(0), m_lineShader(0)
    , m_pbrAdvancedShader(0), m_skinnedShader(0), m_mrtShader(0)
    , m_lineVBO(0), m_lineIBO(0)
    , m_mrtEnabled(false), m_currentMRT(nullptr)
{}

GL3DCanvas::~GL3DCanvas() { shutdown(); }

bool GL3DCanvas::initialize(int width, int height)
{
    if (m_initialized == true) return true;
    m_viewportWidth = width; m_viewportHeight = height;
    if (createShaders() == false) return false;
    glGenBuffers(1, &m_lineVBO);
    glGenBuffers(1, &m_lineIBO);
    m_camera.setAspectRatio((float)width / (float)height);
    m_initialized = true;
    return true;
}

void GL3DCanvas::shutdown()
{
    if (m_initialized == false) return;
    releaseShaders();
    if (m_lineVBO != 0) { glDeleteBuffers(1, &m_lineVBO); m_lineVBO = 0; }
    if (m_lineIBO != 0) { glDeleteBuffers(1, &m_lineIBO); m_lineIBO = 0; }
    m_initialized = false;
}

void GL3DCanvas::begin() { if (m_initialized == true) applyRenderSettings(); }

void GL3DCanvas::end()
{
    flushLines();
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDepthMask(GL_TRUE);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glUseProgram(0);
    glBindVertexArray(0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

void GL3DCanvas::clear(const UI::Color4& color)
    { glClearColor(color.r, color.g, color.b, color.a); glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); }

void GL3DCanvas::setViewport(int x, int y, int width, int height)
{
    m_viewportX = x; m_viewportY = y; m_viewportWidth = width; m_viewportHeight = height;
    glViewport(x, y, width, height);
    m_camera.setAspectRatio((float)width / (float)height);
}

void GL3DCanvas::setCamera        (const Camera& camera)           { m_camera   = camera;   }
void GL3DCanvas::setRenderSettings(const RenderSettings& settings) { m_settings = settings; }

void GL3DCanvas::drawMesh(const Mesh& mesh, const Transform& transform, const Material& material)
{
    if (m_initialized == false || mesh.vertices.empty() == true) return;
    if (mesh.isUploaded() == false) const_cast<Mesh*>(&mesh)->uploadToGPU();
    Matrix4x4 model = transform.toMatrix();
    renderMeshInternal(mesh, m_camera.getViewProjectionMatrix() * model, model, material);
}

void GL3DCanvas::drawSkinnedMesh(const SkinnedMesh& mesh, const Transform& transform,
                                  const Material& material, const AnimationController*)
    { drawMesh(mesh, transform, material); }

void GL3DCanvas::drawModel(const Model& model, const Transform& transform)
{
    if (model.nodes.empty() == true)
    {
        for (size_t i = 0; i < model.meshes.size(); ++i)
        {
            if (model.meshes[i] == nullptr) continue;
            drawMesh(*model.meshes[i], transform,
                     i < model.materials.size() ? model.materials[i] : Material());
        }
        return;
    }

    Matrix4x4 rootMat = transform.toMatrix();
    Matrix4x4 vp      = m_camera.getViewProjectionMatrix();

    struct NodeWork { int idx; Matrix4x4 worldMat; };
    std::vector<NodeWork> stack;
    stack.push_back({ model.rootNodeIndex, rootMat });
    for (int ni = 0; ni < (int)model.nodes.size(); ++ni)
        if (model.nodes[ni].parentIndex == -1 && ni != model.rootNodeIndex)
            stack.push_back({ ni, rootMat });

    while (stack.empty() == false)
    {
        NodeWork nw = stack.back(); stack.pop_back();
        if (nw.idx < 0 || nw.idx >= (int)model.nodes.size()) continue;
        const ModelNode& node     = model.nodes[nw.idx];
        Matrix4x4        worldMat = nw.worldMat * node.transform.toMatrix();

        for (int mi : node.meshIndices)
        {
            if (mi < 0 || mi >= (int)model.meshes.size()) continue;
            Mesh* mesh = model.meshes[mi];
            if (mesh == nullptr || mesh->vertices.empty() == true) continue;
            if (mesh->isUploaded() == false) mesh->uploadToGPU();
            Material mat = (mi < (int)model.materials.size()) ? model.materials[mi] : Material();
            renderMeshInternal(*mesh, vp * worldMat, worldMat, mat);
        }
        for (int ci : node.childIndices)
            stack.push_back({ ci, worldMat });
    }
}

void GL3DCanvas::drawLine  (const Vec3& start, const Vec3& end, const UI::Color4& color) { addLineInternal(start, end, color); }

void GL3DCanvas::drawGrid(float size, int divisions, const UI::Color4& color)
{
    float step = size / divisions, halfSize = size * 0.5f;
    for (int i = 0; i <= divisions; ++i)
    {
        float pos = -halfSize + i * step;
        addLineInternal(Vec3(-halfSize, 0, pos), Vec3(halfSize, 0, pos), color);
        addLineInternal(Vec3(pos, 0, -halfSize), Vec3(pos, 0, halfSize), color);
    }
}

void GL3DCanvas::drawAxis(float size)
{
    addLineInternal(Vec3::zero(), Vec3(size, 0, 0), UI::Color::Red);
    addLineInternal(Vec3::zero(), Vec3(0, size, 0), UI::Color::Green);
    addLineInternal(Vec3::zero(), Vec3(0, 0, size), UI::Color::Blue);
}

void GL3DCanvas::drawBounds(const Vec3& min, const Vec3& max, const UI::Color4& color)
{
    Vec3 v[8] = {
        Vec3(min.x,min.y,min.z), Vec3(max.x,min.y,min.z),
        Vec3(max.x,max.y,min.z), Vec3(min.x,max.y,min.z),
        Vec3(min.x,min.y,max.z), Vec3(max.x,min.y,max.z),
        Vec3(max.x,max.y,max.z), Vec3(min.x,max.y,max.z)
    };
    for (int i = 0; i < 4; ++i)
    {
        addLineInternal(v[i],   v[(i+1)%4],     color);
        addLineInternal(v[i+4], v[((i+1)%4)+4], color);
        addLineInternal(v[i],   v[i+4],          color);
    }
}

void GL3DCanvas::addLight  (const Light& light) { if (m_lights.size() < 4) m_lights.push_back(light); }
void GL3DCanvas::clearLights()                   { m_lights.clear(); }

bool GL3DCanvas::renderToFramebuffer(UI::Framebuffer* fb)
{
    if (fb == nullptr || fb->isValid() == false) return false;
    fb->bind();
    setViewport(0, 0, fb->getTexture()->width, fb->getTexture()->height);
    return true;
}

bool GL3DCanvas::renderToMRT(MRTFramebuffer* mrt)
    { if (mrt == nullptr || mrt->isValid() == false) return false; mrt->bind(); return true; }

void GL3DCanvas::setMRTEnabled(bool enabled) { m_mrtEnabled = enabled; }

bool GL3DCanvas::createShaders()
{
    auto make = [&](const char* vs, const char* fs, uint32_t& prog) -> bool
    {
        uint32_t v = compileShader(GL_VERTEX_SHADER, vs);
        uint32_t f = compileShader(GL_FRAGMENT_SHADER, fs);
        if (v == 0 || f == 0) return false;
        prog = linkProgram(v, f);
        glDeleteShader(v); glDeleteShader(f);
        return (prog != 0);
    };

    if (make(PBR_VERTEX_SHADER,   PBR_FRAGMENT_SHADER,   m_pbrShader)   == false) { LOG_APP_ERROR("GL3D: PBR shader failed");   return false; }
    if (make(UNLIT_VERTEX_SHADER, UNLIT_FRAGMENT_SHADER, m_unlitShader) == false) { LOG_APP_ERROR("GL3D: Unlit shader failed"); return false; }
    if (make(LINE_VERTEX_SHADER,  LINE_FRAGMENT_SHADER,  m_lineShader)  == false) { LOG_APP_ERROR("GL3D: Line shader failed");  return false; }
    LOG_APP_SUCCESS("GL3D: All shaders compiled and linked");

    m_pbrMVPLoc          = glGetUniformLocation(m_pbrShader, "u_mvp");
    m_pbrModelLoc        = glGetUniformLocation(m_pbrShader, "u_model");
    m_pbrNormalMatLoc    = glGetUniformLocation(m_pbrShader, "u_normalMat");
    m_pbrAlbedoColorLoc  = glGetUniformLocation(m_pbrShader, "u_albedoColor");
    m_pbrAlbedoTexLoc    = glGetUniformLocation(m_pbrShader, "u_albedoTex");
    m_pbrHasAlbedoTexLoc = glGetUniformLocation(m_pbrShader, "u_hasAlbedoTex");
    m_pbrMetallicLoc     = glGetUniformLocation(m_pbrShader, "u_metallic");
    m_pbrRoughnessLoc    = glGetUniformLocation(m_pbrShader, "u_roughness");
    m_pbrAOLoc           = glGetUniformLocation(m_pbrShader, "u_ao");
    m_pbrEmissiveColorLoc= glGetUniformLocation(m_pbrShader, "u_emissiveColor");
    m_pbrCameraPosLoc    = glGetUniformLocation(m_pbrShader, "u_cameraPos");
    m_pbrAmbientLoc      = glGetUniformLocation(m_pbrShader, "u_ambient");
    m_pbrLightCountLoc   = glGetUniformLocation(m_pbrShader, "u_lightCount");

    for (int i = 0; i < 4; ++i)
    {
        char name[64];
        std::sprintf(name, "u_lightType[%d]",      i); m_pbrLightTypeLocs.push_back(glGetUniformLocation(m_pbrShader, name));
        std::sprintf(name, "u_lightPos[%d]",       i); m_pbrLightPosLocs.push_back(glGetUniformLocation(m_pbrShader, name));
        std::sprintf(name, "u_lightDir[%d]",       i); m_pbrLightDirLocs.push_back(glGetUniformLocation(m_pbrShader, name));
        std::sprintf(name, "u_lightColor[%d]",     i); m_pbrLightColorLocs.push_back(glGetUniformLocation(m_pbrShader, name));
        std::sprintf(name, "u_lightIntensity[%d]", i); m_pbrLightIntensityLocs.push_back(glGetUniformLocation(m_pbrShader, name));
    }

    m_pbrNormalTexLoc          = glGetUniformLocation(m_pbrShader, "u_normalTex");
    m_pbrNormalStrengthLoc     = glGetUniformLocation(m_pbrShader, "u_normalStrength");
    m_pbrHasNormalTexLoc       = glGetUniformLocation(m_pbrShader, "u_hasNormalTex");
    m_pbrClearcoatLoc          = glGetUniformLocation(m_pbrShader, "u_clearcoat");
    m_pbrClearcoatRoughnessLoc = glGetUniformLocation(m_pbrShader, "u_clearcoatRoughness");
    m_pbrTransmissionLoc       = glGetUniformLocation(m_pbrShader, "u_transmission");
    m_pbrIORLoc                = glGetUniformLocation(m_pbrShader, "u_ior");

    m_unlitMVPLoc    = glGetUniformLocation(m_unlitShader, "u_mvp");
    m_unlitColorLoc  = glGetUniformLocation(m_unlitShader, "u_color");
    m_unlitTexLoc    = glGetUniformLocation(m_unlitShader, "u_texture");
    m_unlitHasTexLoc = glGetUniformLocation(m_unlitShader, "u_hasTexture");
    m_lineMVPLoc     = glGetUniformLocation(m_lineShader,  "u_mvp");
    return true;
}

void GL3DCanvas::releaseShaders()
{
    if (m_pbrShader   != 0) { glDeleteProgram(m_pbrShader);   m_pbrShader   = 0; }
    if (m_unlitShader != 0) { glDeleteProgram(m_unlitShader); m_unlitShader = 0; }
    if (m_lineShader  != 0) { glDeleteProgram(m_lineShader);  m_lineShader  = 0; }
}

void GL3DCanvas::applyRenderSettings()
{
    if (m_settings.depthTest == true)
        { glEnable(GL_DEPTH_TEST); glDepthFunc(GL_LESS); }
    else
        glDisable(GL_DEPTH_TEST);

    if (m_settings.backfaceCulling == true)
        { glEnable(GL_CULL_FACE); glCullFace(GL_BACK); }
    else
        glDisable(GL_CULL_FACE);
}

void GL3DCanvas::setShaderLights(uint32_t shader)
{
    if (shader != m_pbrShader) return;
    glUniform1i(m_pbrLightCountLoc, std::min((int)m_lights.size(), 4));
    for (size_t i = 0; i < std::min(m_lights.size(), size_t(4)); ++i)
    {
        const Light& l = m_lights[i];
        glUniform1i(m_pbrLightTypeLocs[i],      (int)l.type);
        glUniform3f(m_pbrLightPosLocs[i],        l.position.x,  l.position.y,  l.position.z);
        glUniform3f(m_pbrLightDirLocs[i],        l.direction.x, l.direction.y, l.direction.z);
        glUniform4f(m_pbrLightColorLocs[i],      l.color.r,     l.color.g,     l.color.b,    l.color.a);
        glUniform1f(m_pbrLightIntensityLocs[i],  l.intensity);
    }
    glUniform4f(m_pbrAmbientLoc, m_settings.ambientLight.r, m_settings.ambientLight.g,
                m_settings.ambientLight.b, m_settings.ambientLight.a);
}

void GL3DCanvas::renderMeshInternal(const Mesh& mesh, const Matrix4x4& mvp, const Matrix4x4& model, const Material& mat)
{
    uint32_t shader = mat.unlit ? m_unlitShader : m_pbrShader;
    glUseProgram(shader);

    if (mat.alphaMode == Material::AlphaMode::BLEND)
        { glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); glDepthMask(GL_FALSE); }
    else
        { glDisable(GL_BLEND); glDepthMask(GL_TRUE); }

    if (mat.doubleSided == true) glDisable(GL_CULL_FACE);

    auto lazyUpload = [](const UI::Texture* t)
    {
        if (t != nullptr && t->hasGLTexture() == false && t->imageData.empty() == false)
            const_cast<UI::Texture*>(t)->uploadToGPU();
    };

    if (shader == m_pbrShader)
    {
        glUniformMatrix4fv(m_pbrMVPLoc,   1, GL_FALSE, mvp.m);
        glUniformMatrix4fv(m_pbrModelLoc, 1, GL_FALSE, model.m);

        const float a00=model.m[0], a10=model.m[1], a20=model.m[2];
        const float a01=model.m[4], a11=model.m[5], a21=model.m[6];
        const float a02=model.m[8], a12=model.m[9], a22=model.m[10];
        const float n[9] = {
            a11*a22-a21*a12, a20*a12-a10*a22, a10*a21-a20*a11,
            a21*a02-a01*a22, a00*a22-a20*a02, a20*a01-a00*a21,
            a01*a12-a11*a02, a10*a02-a00*a12, a00*a11-a10*a01
        };
        glUniformMatrix3fv(m_pbrNormalMatLoc, 1, GL_TRUE, n);

        glUniform4f(m_pbrAlbedoColorLoc,
            mat.albedoColor.r, mat.albedoColor.g, mat.albedoColor.b, mat.albedoColor.a);

        lazyUpload(mat.albedoTexture);
        const bool hasTex = (mat.albedoTexture != nullptr && mat.albedoTexture->hasGLTexture() == true);
        glUniform1i(m_pbrHasAlbedoTexLoc, hasTex ? 1 : 0);
        if (hasTex == true)
            { glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, mat.albedoTexture->id); glUniform1i(m_pbrAlbedoTexLoc, 0); }

        glUniform1f(m_pbrMetallicLoc,  mat.metallic);
        glUniform1f(m_pbrRoughnessLoc, mat.roughness);
        glUniform1f(m_pbrAOLoc,        mat.ao);
        glUniform4f(m_pbrEmissiveColorLoc,
            mat.emissiveColor.r, mat.emissiveColor.g, mat.emissiveColor.b, mat.emissiveColor.a);

        Vec3 camPos = m_camera.getPosition();
        glUniform3f(m_pbrCameraPosLoc, camPos.x, camPos.y, camPos.z);
        setShaderLights(shader);
    }
    else
    {
        glUniformMatrix4fv(m_unlitMVPLoc, 1, GL_FALSE, mvp.m);
        glUniform4f(m_unlitColorLoc,
            mat.albedoColor.r, mat.albedoColor.g, mat.albedoColor.b, mat.albedoColor.a);

        lazyUpload(mat.albedoTexture);
        const bool hasTex = (mat.albedoTexture != nullptr && mat.albedoTexture->hasGLTexture() == true);
        glUniform1i(m_unlitHasTexLoc, hasTex ? 1 : 0);
        if (hasTex == true)
            { glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, mat.albedoTexture->id); glUniform1i(m_unlitTexLoc, 0); }
    }

    glBindVertexArray(mesh.getVAO());
    glDrawElements(GL_TRIANGLES, (GLsizei)mesh.indices.size(), GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);

    if (mat.alphaMode == Material::AlphaMode::BLEND)                   glDepthMask(GL_TRUE);
    if (mat.doubleSided == true && m_settings.backfaceCulling == true) glEnable(GL_CULL_FACE);
}

void GL3DCanvas::flushLines()
{
    if (m_lineVertices.empty() == true) return;

    glUseProgram(m_lineShader);
    glUniformMatrix4fv(m_lineMVPLoc, 1, GL_FALSE, m_camera.getViewProjectionMatrix().m);

    glBindBuffer(GL_ARRAY_BUFFER, m_lineVBO);
    glBufferData(GL_ARRAY_BUFFER, m_lineVertices.size() * sizeof(LineVertex), m_lineVertices.data(), GL_STREAM_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_lineIBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, m_lineIndices.size() * sizeof(uint16_t), m_lineIndices.data(), GL_STREAM_DRAW);

    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(LineVertex), (void*)offsetof(LineVertex, position));
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(LineVertex), (void*)offsetof(LineVertex, color));

    glDrawElements(GL_LINES, (GLsizei)m_lineIndices.size(), GL_UNSIGNED_SHORT, nullptr);

    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);
    m_lineVertices.clear();
    m_lineIndices.clear();
}

void GL3DCanvas::addLineInternal(const Vec3& start, const Vec3& end, const UI::Color4& color)
{
    uint16_t startIdx = (uint16_t)m_lineVertices.size();
    m_lineVertices.push_back({ start, color });
    m_lineVertices.push_back({ end,   color });
    m_lineIndices.push_back(startIdx);
    m_lineIndices.push_back(startIdx + 1);
}

#endif // USE_GLES || USE_IMGUI

// ============================================================================
// Scene
// ============================================================================

Scene::Scene()
{
    Camera defaultCam;
    defaultCam.setPosition(Vec3(5.0f, 5.0f, 5.0f));
    defaultCam.setTarget(Vec3(0.0f, 0.0f, 0.0f));
    m_camera = defaultCam;
    m_lights.push_back(Light::makeDirectional(Vec3(-0.5f, -1.0f, -0.3f), UI::Color::White, 1.0f));
}

Scene::~Scene() { clear(); }

SceneObject* Scene::addObject(Mesh* mesh, const Material& material, const Transform& transform)
    { m_objects.emplace_back(mesh, material, transform); return &m_objects.back(); }

SceneObject* Scene::addOwnedObject(Mesh* mesh, const Material& material, const Transform& transform)
    { m_ownedMeshes.insert(mesh); return addObject(mesh, material, transform); }

void Scene::removeObject(SceneObject* obj)
{
    auto it = std::find_if(m_objects.begin(), m_objects.end(),
        [obj](const SceneObject& o) { return &o == obj; });
    if (it == m_objects.end()) return;
    if (it->mesh != nullptr && m_ownedMeshes.find(it->mesh) != m_ownedMeshes.end())
        { delete it->mesh; m_ownedMeshes.erase(it->mesh); }
    m_objects.erase(it);
}

void Scene::clear()
{
    for (auto& obj : m_objects)
        if (obj.mesh != nullptr && m_ownedMeshes.find(obj.mesh) != m_ownedMeshes.end())
            delete obj.mesh;
    m_objects.clear();
    m_ownedMeshes.clear();
    m_lights.clear();
}

SceneObject*       Scene::getObject(size_t index)       { return (index < m_objects.size()) ? &m_objects[index] : nullptr; }
const SceneObject* Scene::getObject(size_t index) const { return (index < m_objects.size()) ? &m_objects[index] : nullptr; }

// ============================================================================
// Viewport
// ============================================================================

Viewport::Viewport()
    : m_interactionEnabled(true)
    , m_isDragging(false)
    , m_currentMode(InteractionMode::NONE)
    , m_lastMousePos(0.0f, 0.0f)
    , m_orbitSensitivity(0.01f)
    , m_panSensitivity(0.01f)
    , m_zoomSensitivity(0.1f)
    , m_autoSpinSpeed(0.0f)
    , m_touchGestureInit(false)
    , m_touchPrevPos{}
    , m_touchPrevSpan(0.0f)
    , m_touchPrevMid(0.0f, 0.0f)
    , m_showAxis(true)
    , m_rendererInitialized(false)
{
    m_renderSettings.showGrid = true;
}

Viewport::~Viewport() {}

void Viewport::update(float deltaTime)
{
    if (m_autoSpinSpeed != 0.0f && m_isDragging == false)
        m_scene.getCamera().orbit(m_autoSpinSpeed * deltaTime, 0.0f);
}

void Viewport::onMeasure(UI::MeasureSpec widthSpec, UI::MeasureSpec heightSpec)
{
    float width  = (widthSpec.mode  == UI::MeasureSpecMode::EXACTLY) ? widthSpec.size  : 400.0f;
    float height = (heightSpec.mode == UI::MeasureSpecMode::EXACTLY) ? heightSpec.size : 300.0f;
    if (widthSpec.mode  == UI::MeasureSpecMode::AT_MOST) width  = std::min(width,  widthSpec.size);
    if (heightSpec.mode == UI::MeasureSpecMode::AT_MOST) height = std::min(height, heightSpec.size);
    setMeasuredSize(UI::Vec2(width, height));
}

void Viewport::onLayout(const UI::RectF& bounds)
{
    UI::View::onLayout(bounds);
    const int width  = (int)bounds.width();
    const int height = (int)bounds.height();
    if (width <= 0 || height <= 0) return;

    if (m_rendererInitialized == true && m_framebuffer != nullptr)
    {
        UI::Texture* fbTexture = m_framebuffer->getTexture();
        if (fbTexture == nullptr || fbTexture->width != width || fbTexture->height != height)
            m_framebuffer->create(width, height);
    }
    m_scene.getCamera().setAspectRatio((float)width / (float)height);
}

void Viewport::onDraw(UI::ICanvas& canvas)
{
    drawBackground(canvas);
    if (m_rendererInitialized == false) initializeRenderer();

    if (m_renderer != nullptr && m_framebuffer != nullptr && m_framebuffer->isValid() == true)
    {
        renderScene();

        UI::Texture* fbTex = m_framebuffer->getTexture();
        if (fbTex != nullptr)
        {
            const float     w       = static_cast<float>(fbTex->width);
            const float     h       = static_cast<float>(fbTex->height);
            const UI::RectF srcRect(0.0f, h, w, 0.0f);
            UI::Paint       paint;
            paint.bgColor      = UI::Color::White;
            paint.cornerRadius = 24.0f;
            paint.opacity      = 1.0f;
            canvas.drawTexture(fbTex, srcRect, m_bounds, paint);
        }
    }
    drawFocusBorder(canvas);
}

bool Viewport::onMotionEvent(const IO::MotionEvent& event)
{
    if (m_interactionEnabled == false || isEnabled() == false) return false;

    // Mouse
    if (event.isMouse() == true)
    {
        if (event.action == IO::MotionAction::DOWN && m_bounds.contains(event.x, event.y) == true)
        {
            m_isDragging   = true;
            m_lastMousePos = UI::Vec2(event.x, event.y);
            if      (event.button == IO::MouseButton::LEFT)   m_currentMode = InteractionMode::ORBIT;
            else if (event.button == IO::MouseButton::MIDDLE) m_currentMode = InteractionMode::PAN;
            else if (event.button == IO::MouseButton::RIGHT)  m_currentMode = InteractionMode::ZOOM;
            return true;
        }
        if (event.action == IO::MotionAction::MOVE && m_isDragging == true)
            { handleCameraInteraction(event); return true; }
        if (event.action == IO::MotionAction::UP)
            { m_isDragging = false; m_currentMode = InteractionMode::NONE; return true; }
        if (event.action == IO::MotionAction::SCROLL && m_bounds.contains(event.x, event.y) == true)
            { m_scene.getCamera().zoom(-event.scrollY * m_zoomSensitivity); return true; }
        return false;
    }

    // Touch
    if (event.isTouch() == false) return false;

    if (event.action == IO::MotionAction::DOWN && m_bounds.contains(event.x, event.y) == false)
        return false;

    if (event.action == IO::MotionAction::DOWN)
    {
        m_isDragging       = true;
        m_touchGestureInit = false;
        return true;
    }

    if (event.action == IO::MotionAction::UP || event.action == IO::MotionAction::CANCEL)
    {
        m_isDragging       = false;
        m_currentMode      = InteractionMode::NONE;
        m_touchGestureInit = false;
        return true;
    }

    // Finger count changed — reset reference frame
    if (event.action == IO::MotionAction::POINTER_DOWN || event.action == IO::MotionAction::POINTER_UP)
    {
        m_touchGestureInit = false;
        return true;
    }

    if (event.action == IO::MotionAction::MOVE && m_isDragging == true)
        { handleCameraInteraction(event); return true; }

    return false;
}

void Viewport::addCube  (const Vec3& pos, float size,   const Material& mat)     { m_scene.addOwnedObject(Mesh::createCube(size), mat, Transform(pos)); }
void Viewport::addSphere(const Vec3& pos, float radius, const Material& mat)     { m_scene.addOwnedObject(Mesh::createSphere(radius), mat, Transform(pos)); }
void Viewport::addPlane (const Vec3& pos, float w, float h, const Material& mat) { m_scene.addOwnedObject(Mesh::createPlane(w, h), mat, Transform(pos)); }

void Viewport::initializeRenderer()
{
    #if defined(USE_GLES) || defined(USE_IMGUI)
        if (m_rendererInitialized == true) return;
        const int width  = (int)m_bounds.width();
        const int height = (int)m_bounds.height();
        if (width <= 0 || height <= 0) return;

        if (m_renderer == nullptr) m_renderer = std::make_unique<GL3DCanvas>();
        if (m_renderer->initialize(width, height) == false) return;

        m_framebuffer = std::make_unique<UI::Framebuffer>();
        if (m_framebuffer->create(width, height) == true) m_rendererInitialized = true;
    #endif // USE_GLES || USE_IMGUI
}

void Viewport::renderScene()
{
    if (m_renderer == nullptr || m_framebuffer == nullptr || m_framebuffer->isValid() == false) return;

    m_framebuffer->bind();
    m_renderer->setViewport(0, 0, m_framebuffer->getTexture()->width, m_framebuffer->getTexture()->height);
    m_renderer->setCamera(m_scene.getCamera());
    m_renderer->setRenderSettings(m_renderSettings);
    m_renderer->begin();
    m_renderer->clear(m_renderSettings.clearColor);

    m_renderer->clearLights();
    for (const auto& light : m_scene.getLights())
        m_renderer->addLight(light);

    if (m_renderSettings.showGrid == true) m_renderer->drawGrid(10.0f, 10, UI::Color4(0.3f, 0.3f, 0.3f, 0.5f));
    if (m_showAxis == true)                m_renderer->drawAxis(1.0f);

    for (const auto& obj : m_scene.getObjects())
        if (obj.visible == true && obj.mesh != nullptr)
            m_renderer->drawMesh(*obj.mesh, obj.transform, obj.material);

    m_renderer->end();
    m_framebuffer->unbind();
}

void Viewport::handleCameraInteraction(const IO::MotionEvent& event)
{
    Camera& camera = m_scene.getCamera();

    // Mouse
    if (event.isMouse() == true)
    {
        UI::Vec2 delta = UI::Vec2(event.x, event.y) - m_lastMousePos;
        switch (m_currentMode)
        {
            case InteractionMode::ORBIT: camera.orbit(-delta.x * m_orbitSensitivity, -delta.y * m_orbitSensitivity); break;
            case InteractionMode::PAN:   camera.pan  (-delta.x * m_panSensitivity,    delta.y * m_panSensitivity);   break;
            case InteractionMode::ZOOM:  camera.zoom (-delta.y * m_zoomSensitivity * 0.1f);                          break;
            default: break;
        }
        m_lastMousePos = UI::Vec2(event.x, event.y);
        return;
    }

    // Touch
    const int count = event.pointerCount;
    if (count == 0) return;

    // Single finger: orbit (same feel as left-mouse drag)
    if (count == 1)
    {
        UI::Vec2 cur(event.pointers[0].x, event.pointers[0].y);
        if (m_touchGestureInit == true)
        {
            UI::Vec2 delta = cur - m_touchPrevPos[0];
            camera.orbit(-delta.x * m_orbitSensitivity, -delta.y * m_orbitSensitivity);
        }
        m_touchPrevPos[0]  = cur;
        m_touchGestureInit = true;
        return;
    }

    // 2 fingers: pinch-to-zoom (orbit radius, target stays fixed)
    if (count == 2)
    {
        const UI::Vec2 cur0(event.pointers[0].x, event.pointers[0].y);
        const UI::Vec2 cur1(event.pointers[1].x, event.pointers[1].y);
        const float    dx      = cur1.x - cur0.x;
        const float    dy      = cur1.y - cur0.y;
        const float    curSpan = std::sqrt(dx * dx + dy * dy);

        if (m_touchGestureInit == false)
        {
            m_touchPrevPos[0]  = cur0;
            m_touchPrevPos[1]  = cur1;
            m_touchPrevSpan    = (curSpan > 0.0f) ? curSpan : 1.0f;
            m_touchGestureInit = true;
            return;
        }

        const float dSpan = curSpan - m_touchPrevSpan;
        if (std::abs(dSpan) > 0.5f)
        {
            const Vec3  offset    = camera.getPosition() - camera.getTarget();
            const float radius    = offset.length();
            const float newRadius = std::max(0.05f, radius - dSpan * m_zoomSensitivity * 0.08f);
            camera.setPosition(camera.getTarget() + offset.normalized() * newRadius);
        }

        m_touchPrevPos[0] = cur0;
        m_touchPrevPos[1] = cur1;
        m_touchPrevSpan   = curSpan;
        return;
    }

    // 3+ fingers: pan (centroid of all fingers)
    {
        UI::Vec2 curCentroid(0.0f, 0.0f);
        for (int i = 0; i < count; ++i)
            curCentroid = curCentroid + UI::Vec2(event.pointers[i].x, event.pointers[i].y);
        curCentroid = curCentroid * (1.0f / static_cast<float>(count));

        if (m_touchGestureInit == false)
        {
            m_touchPrevMid     = curCentroid;
            m_touchGestureInit = true;
            return;
        }

        const UI::Vec2 dMid = curCentroid - m_touchPrevMid;
        if (std::abs(dMid.x) > 0.5f || std::abs(dMid.y) > 0.5f)
            camera.pan(-dMid.x * m_panSensitivity, dMid.y * m_panSensitivity);

        m_touchPrevMid = curCentroid;
        return;
    }
}

} // namespace UI3D

} // namespace APP
