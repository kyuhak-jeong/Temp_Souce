#ifndef UI3D_CORE_H
#define UI3D_CORE_H

#include "constants.h"
#include "ui_core.h"
#include <cmath>
#include <vector>
#include <memory>
#include <functional>
#include <set>

namespace APP
{

namespace UI3D
{

// ============================================================================
// Math
// ============================================================================

namespace Math
{

struct Vec3
{
    float x, y, z;

    Vec3() : x(0.0f), y(0.0f), z(0.0f) {}
    Vec3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}
    explicit Vec3(float v) : x(v), y(v), z(v) {}

    Vec3  operator+(const Vec3& o) const { return Vec3(x + o.x, y + o.y, z + o.z); }
    Vec3  operator-(const Vec3& o) const { return Vec3(x - o.x, y - o.y, z - o.z); }
    Vec3  operator*(float s)       const { return Vec3(x * s,   y * s,   z * s);   }
    Vec3  operator/(float s)       const { return Vec3(x / s,   y / s,   z / s);   }
    Vec3  operator-()              const { return Vec3(-x, -y, -z);                  }
    Vec3& operator+=(const Vec3& o)      { x += o.x; y += o.y; z += o.z; return *this; }
    Vec3& operator-=(const Vec3& o)      { x -= o.x; y -= o.y; z -= o.z; return *this; }
    Vec3& operator*=(float s)            { x *= s;   y *= s;   z *= s;   return *this; }

    float length()        const { return std::sqrt(x*x + y*y + z*z); }
    float lengthSquared() const { return x*x + y*y + z*z; }
    float dot(const Vec3& o)   const { return x*o.x + y*o.y + z*o.z; }

    Vec3 normalized() const { float len = length(); return (len > 0.0f) ? (*this / len) : Vec3(0.0f); }
    Vec3 cross(const Vec3& o) const
        { return Vec3(y*o.z - z*o.y, z*o.x - x*o.z, x*o.y - y*o.x); }
    Vec3 lerp(const Vec3& target, float t) const
        { return Vec3(x + (target.x - x)*t, y + (target.y - y)*t, z + (target.z - z)*t); }

    static Vec3 zero()    { return Vec3(0.0f, 0.0f, 0.0f); }
    static Vec3 one()     { return Vec3(1.0f, 1.0f, 1.0f); }
    static Vec3 up()      { return Vec3(0.0f, 1.0f, 0.0f); }
    static Vec3 forward() { return Vec3(0.0f, 0.0f,-1.0f); }
    static Vec3 right()   { return Vec3(1.0f, 0.0f, 0.0f); }
};

struct Matrix4x4
{
    float m[16];

    Matrix4x4() { identity(); }

    void identity()
    {
        m[0]=1; m[4]=0; m[8]=0;  m[12]=0;
        m[1]=0; m[5]=1; m[9]=0;  m[13]=0;
        m[2]=0; m[6]=0; m[10]=1; m[14]=0;
        m[3]=0; m[7]=0; m[11]=0; m[15]=1;
    }

    float&       at(int row, int col)       { return m[col * 4 + row]; }
    const float& at(int row, int col) const { return m[col * 4 + row]; }

    Matrix4x4 operator*(const Matrix4x4& o) const
    {
        Matrix4x4 r;
        for (int col = 0; col < 4; ++col)
            for (int row = 0; row < 4; ++row)
                r.at(row, col) = at(row,0)*o.at(0,col) + at(row,1)*o.at(1,col)
                               + at(row,2)*o.at(2,col) + at(row,3)*o.at(3,col);
        return r;
    }

    Vec3 transformPoint(const Vec3& v) const
    {
        float w = at(3,0)*v.x + at(3,1)*v.y + at(3,2)*v.z + at(3,3);
        float x = at(0,0)*v.x + at(0,1)*v.y + at(0,2)*v.z + at(0,3);
        float y = at(1,0)*v.x + at(1,1)*v.y + at(1,2)*v.z + at(1,3);
        float z = at(2,0)*v.x + at(2,1)*v.y + at(2,2)*v.z + at(2,3);
        return (w != 0.0f && w != 1.0f) ? Vec3(x/w, y/w, z/w) : Vec3(x, y, z);
    }

    static Matrix4x4 makeIdentity()                { Matrix4x4 m; m.identity(); return m; }
    static Matrix4x4 makeTranslation(const Vec3& t)
        { Matrix4x4 m; m.m[12]=t.x; m.m[13]=t.y; m.m[14]=t.z; return m; }
    static Matrix4x4 makeScale(const Vec3& s)
        { Matrix4x4 m; m.m[0]=s.x; m.m[5]=s.y; m.m[10]=s.z; return m; }

    static Matrix4x4 makeRotationX(float rad)
    {
        Matrix4x4 m; float c=std::cos(rad), s=std::sin(rad);
        m.m[5]=c; m.m[6]=s; m.m[9]=-s; m.m[10]=c; return m;
    }
    static Matrix4x4 makeRotationY(float rad)
    {
        Matrix4x4 m; float c=std::cos(rad), s=std::sin(rad);
        m.m[0]=c; m.m[2]=-s; m.m[8]=s; m.m[10]=c; return m;
    }
    static Matrix4x4 makeRotationZ(float rad)
    {
        Matrix4x4 m; float c=std::cos(rad), s=std::sin(rad);
        m.m[0]=c; m.m[1]=s; m.m[4]=-s; m.m[5]=c; return m;
    }
    static Matrix4x4 makePerspective(float fovY, float aspect, float near, float far)
    {
        Matrix4x4 m; float tanHalf = std::tan(fovY * 0.5f);
        m.m[0]  =  1.0f / (aspect * tanHalf);
        m.m[5]  =  1.0f / tanHalf;
        m.m[10] = -(far + near) / (far - near);
        m.m[11] = -1.0f;
        m.m[14] = -(2.0f * far * near) / (far - near);
        m.m[15] =  0.0f;
        return m;
    }
    static Matrix4x4 makeLookAt(const Vec3& eye, const Vec3& center, const Vec3& up)
    {
        Vec3 f = (center - eye).normalized();
        Vec3 s = f.cross(up).normalized();
        Vec3 u = s.cross(f);
        Matrix4x4 m;
        m.m[0]=s.x; m.m[4]=s.y; m.m[8] = s.z; m.m[12]=-s.dot(eye);
        m.m[1]=u.x; m.m[5]=u.y; m.m[9] = u.z; m.m[13]=-u.dot(eye);
        m.m[2]=-f.x;m.m[6]=-f.y;m.m[10]=-f.z; m.m[14]= f.dot(eye);
        return m;
    }
};

struct Quaternion
{
    float x, y, z, w;

    Quaternion() : x(0.0f), y(0.0f), z(0.0f), w(1.0f) {}
    Quaternion(float x_, float y_, float z_, float w_) : x(x_), y(y_), z(z_), w(w_) {}

    static Quaternion identity() { return Quaternion(0, 0, 0, 1); }

    static Quaternion fromAxisAngle(const Vec3& axis, float rad)
    {
        float half = rad * 0.5f, s = std::sin(half);
        Vec3 n = axis.normalized();
        return Quaternion(n.x*s, n.y*s, n.z*s, std::cos(half));
    }
    static Quaternion fromEuler(float pitch, float yaw, float roll)
    {
        float cy=std::cos(yaw*0.5f),  sy=std::sin(yaw*0.5f);
        float cp=std::cos(pitch*0.5f),sp=std::sin(pitch*0.5f);
        float cr=std::cos(roll*0.5f), sr=std::sin(roll*0.5f);
        return Quaternion(sr*cp*cy - cr*sp*sy, cr*sp*cy + sr*cp*sy,
                          cr*cp*sy - sr*sp*cy, cr*cp*cy + sr*sp*sy);
    }

    Vec3 rotate(const Vec3& v) const
    {
        Vec3 u(x, y, z);
        return u * (2.0f * u.dot(v)) + v * (w*w - u.dot(u)) + u.cross(v) * (2.0f * w);
    }
    Quaternion normalized() const
    {
        float len = std::sqrt(x*x + y*y + z*z + w*w);
        return (len > 0.0f) ? Quaternion(x/len, y/len, z/len, w/len) : identity();
    }
    Matrix4x4 toMatrix() const
    {
        float xx=x*x, yy=y*y, zz=z*z, xy=x*y, xz=x*z, yz=y*z, wx=w*x, wy=w*y, wz=w*z;
        Matrix4x4 m;
        m.m[0]=1-2*(yy+zz); m.m[4]=2*(xy-wz);   m.m[8] =2*(xz+wy);
        m.m[1]=2*(xy+wz);   m.m[5]=1-2*(xx+zz); m.m[9] =2*(yz-wx);
        m.m[2]=2*(xz-wy);   m.m[6]=2*(yz+wx);   m.m[10]=1-2*(xx+yy);
        return m;
    }
};

struct Transform
{
    Vec3       position;
    Quaternion rotation;
    Vec3       scale;

    Transform() : position(Vec3::zero()), rotation(Quaternion::identity()), scale(Vec3::one()) {}
    Transform(const Vec3& pos, const Quaternion& rot = Quaternion::identity(), const Vec3& scl = Vec3::one())
        : position(pos), rotation(rot), scale(scl) {}

    Matrix4x4 toMatrix() const
        { return Matrix4x4::makeTranslation(position) * rotation.toMatrix() * Matrix4x4::makeScale(scale); }
};

} // namespace Math

// ============================================================================
// Core type aliases
// ============================================================================

using Math::Vec3;
using Math::Matrix4x4;
using Math::Quaternion;
using Math::Transform;

// ============================================================================
// Camera
// ============================================================================

class Camera
{
public:
    enum class ProjectionType { PERSPECTIVE, ORTHOGRAPHIC };

    Camera();

    void setPosition(const Vec3& pos);
    void setTarget(const Vec3& target);
    void setUp(const Vec3& up);
    void setPerspective(float fovRad, float aspect, float nearPlane, float farPlane);
    void setAspectRatio(float aspect);

    const Vec3& getPosition() const { return m_position; }
    const Vec3& getTarget()   const { return m_target; }
    Vec3        getForward()  const { return (m_target - m_position).normalized(); }
    Vec3        getRight()    const { return getForward().cross(m_up).normalized(); }

    const Matrix4x4& getViewMatrix()           const { return m_viewMatrix; }
    const Matrix4x4& getProjectionMatrix()     const { return m_projectionMatrix; }
    Matrix4x4        getViewProjectionMatrix() const { return m_projectionMatrix * m_viewMatrix; }

    void orbit(float deltaYaw, float deltaPitch);
    void pan(float deltaX, float deltaY);
    void zoom(float delta);

private:
    void updateMatrices();

    Vec3           m_position, m_target, m_up;
    float          m_fov, m_aspect, m_nearPlane, m_farPlane;
    ProjectionType m_projectionType;
    Matrix4x4      m_viewMatrix, m_projectionMatrix;
};

// ============================================================================
// Light
// ============================================================================

struct Light
{
    enum class Type { DIRECTIONAL, POINT, SPOT };

    Type       type;
    Vec3       position;
    Vec3       direction;
    UI::Color4 color;
    float      intensity;
    float      constantAtten, linearAtten, quadraticAtten;
    float      spotInnerCutoff, spotOuterCutoff;

    Light();

    static Light makeDirectional(const Vec3& dir, const UI::Color4& col = UI::Color::White, float intensity = 1.0f);
    static Light makePoint      (const Vec3& pos, const UI::Color4& col = UI::Color::White, float intensity = 1.0f);
    static Light makeSpot       (const Vec3& pos, const Vec3& dir, const UI::Color4& col = UI::Color::White,
                                  float intensity = 1.0f, float innerCutoff = 12.5f, float outerCutoff = 17.5f);
};

// ============================================================================
// Material
// ============================================================================

struct Material
{
    // Albedo
    UI::Color4   albedoColor;
    UI::Texture* albedoTexture;

    // Normal
    UI::Texture* normalTexture;
    float        normalStrength;

    // Metallic / Roughness
    float        metallic, roughness;
    UI::Texture* metallicRoughnessTexture;

    // Occlusion
    float        ao;
    UI::Texture* aoTexture;

    // Emissive
    UI::Color4   emissiveColor;
    UI::Texture* emissiveTexture;
    float        emissiveStrength;

    // Clearcoat
    float        clearcoat, clearcoatRoughness;
    UI::Texture* clearcoatTexture;
    UI::Texture* clearcoatRoughnessTexture;
    UI::Texture* clearcoatNormalTexture;

    // Sheen
    UI::Color4 sheenColor;
    float      sheenRoughness;

    // Transmission
    float transmission;
    float ior;

    // Alpha
    enum class AlphaMode { OPAQUE, MASK, BLEND };
    AlphaMode alphaMode;
    float     alphaCutoff;

    // Flags
    bool doubleSided;
    bool wireframe;
    bool unlit;

    Material();

    static Material makePBR    (const UI::Color4& albedo, float metallic = 0.0f, float roughness = 0.5f);
    static Material makeUnlit  (const UI::Color4& color);
    static Material makeGlass  (const UI::Color4& tint = UI::Color::White, float transmission = 1.0f, float ior = 1.5f);
    static Material makeFabric (const UI::Color4& albedo, const UI::Color4& sheen = UI::Color::White);
};

// ============================================================================
// Vertex / Mesh
// ============================================================================

struct Vertex3D
{
    Vec3       position;
    Vec3       normal;
    UI::Vec2   texCoord;
    UI::Color4 color;

    Vertex3D() : position(Vec3::zero()), normal(Vec3::up()), texCoord(0,0), color(UI::Color::White) {}
    Vertex3D(const Vec3& pos, const Vec3& norm = Vec3::up(), const UI::Vec2& uv = UI::Vec2(0,0),
             const UI::Color4& col = UI::Color::White)
        : position(pos), normal(norm), texCoord(uv), color(col) {}
};

class Mesh
{
public:
    Mesh();
    ~Mesh();

    std::vector<Vertex3D> vertices;
    std::vector<uint32_t> indices;

    void addVertex(const Vertex3D& v)                          { vertices.push_back(v); m_uploaded = false; }
    void addTriangle(uint32_t i0, uint32_t i1, uint32_t i2)
        { indices.push_back(i0); indices.push_back(i1); indices.push_back(i2); m_uploaded = false; }

    void generateNormals();
    void generateTangents();
    void calculateBounds(Vec3& outMin, Vec3& outMax) const;

    bool uploadToGPU();
    void release();
    bool     isUploaded() const { return m_uploaded; }
    uint32_t getVBO()     const { return m_vbo; }
    uint32_t getIBO()     const { return m_ibo; }
    uint32_t getVAO()     const { return m_vao; }

    static Mesh* createCube    (float size = 1.0f);
    static Mesh* createSphere  (float radius = 1.0f, int segments = 32, int rings = 16);
    static Mesh* createPlane   (float width = 1.0f, float height = 1.0f);
    static Mesh* createCylinder(float radius = 1.0f, float height = 2.0f, int segments = 32);
    static Mesh* createCone    (float radius = 1.0f, float height = 2.0f, int segments = 32);

private:
    uint32_t m_vbo, m_ibo, m_vao;
    bool     m_uploaded;
};

// ============================================================================
// Skeletal Animation
// ============================================================================

struct Joint
{
    std::string name;
    int         parentIndex;
    Transform   localTransform;
    Matrix4x4   inverseBindMatrix;
};

struct AnimationKeyframe
{
    float      time;
    Vec3       position;
    Quaternion rotation;
    Vec3       scale;
};

struct AnimationChannel
{
    enum class Path { Translation, Rotation, Scale };

    int                            jointIndex;
    Path                           path = Path::Translation;
    std::vector<AnimationKeyframe> keyframes;

    Transform interpolate(float time) const;
};

class AnimationClip
{
public:
    std::string                    name;
    float                          duration;
    std::vector<AnimationChannel>  channels;

    void sample(float time, std::vector<Transform>& outJointTransforms) const;
};

class Skeleton
{
public:
    std::vector<Joint>     joints;
    std::vector<Matrix4x4> jointMatrices;

    void updateMatrices(const std::vector<Transform>& localTransforms);
    int  findJointIndex(const std::string& name) const;
};

class SkinnedMesh : public Mesh
{
public:
    static constexpr int MAX_BONE_INFLUENCE = 4;

    struct VertexWeight
    {
        int   jointIndices[MAX_BONE_INFLUENCE];
        float weights[MAX_BONE_INFLUENCE];
        VertexWeight() { for (int i = 0; i < MAX_BONE_INFLUENCE; ++i) { jointIndices[i]=0; weights[i]=0.0f; } }
    };

    std::vector<VertexWeight> vertexWeights;
    Skeleton                  skeleton;

    bool uploadToGPU();
};

class AnimationController
{
public:
    AnimationController();

    void addClip(const std::string& name, const AnimationClip& clip);
    void play(const std::string& name, bool loop = true);
    void stop();
    void update(float deltaTime);

    const std::vector<Matrix4x4>& getJointMatrices() const { return m_skeleton.jointMatrices; }
    void  setSkeleton(const Skeleton& skeleton) { m_skeleton = skeleton; }
    bool  isPlaying()        const { return m_isPlaying; }
    float getCurrentTime()   const { return m_currentTime; }

private:
    std::unordered_map<std::string, AnimationClip> m_clips;
    Skeleton    m_skeleton;
    std::string m_currentClip;
    float       m_currentTime;
    bool        m_isPlaying;
    bool        m_loop;
};

// ============================================================================
// Model / Loading
// ============================================================================

struct ModelNode
{
    std::string      name;
    Transform        transform;
    std::vector<int> meshIndices;
    std::vector<int> childIndices;
    int              parentIndex;

    ModelNode() : parentIndex(-1) {}
};

class Model
{
public:
    Model();
    ~Model();

    std::vector<Mesh*>         meshes;
    std::vector<Material>      materials;
    std::vector<ModelNode>     nodes;
    int                        rootNodeIndex;
    std::vector<AnimationClip> animations;
    Skeleton                   skeleton;

    bool hasAnimations() const { return !animations.empty(); }
    void release();

private:
    void clearMeshes();
};

class ModelLoader
{
public:
    enum class Format { AUTO, GLTF, OBJ };

    static Model* loadFromFile(const std::string& path, Format format = Format::AUTO);
    static Model* loadGLTF    (const std::string& path);
    static Model* loadOBJ     (const std::string& path);

private:
    static Format detectFormat(const std::string& path);
};

namespace TransformHelper
{
    Transform fromMatrix(const Matrix4x4& m);
}

// ============================================================================
// Render Settings / MRT
// ============================================================================

struct RenderSettings
{
    bool       depthTest;
    bool       backfaceCulling;
    bool       wireframe;
    bool       showGrid;
    UI::Color4 clearColor;
    UI::Color4 ambientLight;

    float shadowBias;
    bool  enableShadows;
    int   shadowMapSize;

    bool  enableBloom;
    bool  enableSSAO;
    bool  enableFXAA;
    float bloomThreshold;
    float bloomIntensity;

    bool enableMRT;

    RenderSettings();
};

enum class MRTAttachment { COLOR_0, COLOR_1, COLOR_2, COLOR_3, DEPTH_STENCIL };

struct MRTFramebuffer
{
    uint32_t fbo;
    uint32_t colorTextures[4];
    uint32_t depthTexture;
    int      width, height;
    int      colorAttachmentCount;

    MRTFramebuffer();
    ~MRTFramebuffer();

    bool create (int w, int h, int numColorAttachments = 1);
    void release();
    bool bind();
    static void unbind();
    bool     isValid()              const;
    uint32_t getColorTexture(int index) const;
    uint32_t getDepthTexture()          const;
};

// ============================================================================
// Renderer Interface
// ============================================================================

class I3DCanvas
{
public:
    virtual ~I3DCanvas() = default;

    virtual bool initialize(int width, int height) = 0;
    virtual void shutdown() = 0;

    virtual void begin() = 0;
    virtual void end()   = 0;
    virtual void clear(const UI::Color4& color) = 0;

    virtual void setViewport      (int x, int y, int width, int height) = 0;
    virtual void setCamera        (const Camera& camera) = 0;
    virtual void setRenderSettings(const RenderSettings& settings) = 0;

    virtual void drawMesh        (const Mesh& mesh, const Transform& transform, const Material& material) = 0;
    virtual void drawSkinnedMesh (const SkinnedMesh& mesh, const Transform& transform, const Material& material,
                                  const AnimationController* animController = nullptr) = 0;
    virtual void drawModel       (const Model& model, const Transform& transform) = 0;

    virtual void drawLine  (const Vec3& start, const Vec3& end, const UI::Color4& color) = 0;
    virtual void drawGrid  (float size, int divisions, const UI::Color4& color) = 0;
    virtual void drawAxis  (float size) = 0;
    virtual void drawBounds(const Vec3& min, const Vec3& max, const UI::Color4& color) = 0;

    virtual void addLight  (const Light& light) = 0;
    virtual void clearLights() = 0;

    virtual bool renderToFramebuffer(UI::Framebuffer* fb) = 0;
    virtual bool renderToMRT        (MRTFramebuffer* mrt) = 0;
    virtual void setMRTEnabled      (bool enabled) = 0;
};

#if defined(USE_GLES) || defined(USE_IMGUI)

// ============================================================================
// OpenGL ES3 Renderer
// ============================================================================

class GL3DCanvas : public I3DCanvas
{
public:
    GL3DCanvas();
    ~GL3DCanvas();

    bool initialize(int width, int height) override;
    void shutdown() override;

    void begin() override;
    void end()   override;
    void clear(const UI::Color4& color) override;

    void setViewport      (int x, int y, int width, int height) override;
    void setCamera        (const Camera& camera) override;
    void setRenderSettings(const RenderSettings& settings) override;

    void drawMesh       (const Mesh& mesh, const Transform& transform, const Material& material) override;
    void drawSkinnedMesh(const SkinnedMesh& mesh, const Transform& transform, const Material& material,
                         const AnimationController* animController = nullptr) override;
    void drawModel      (const Model& model, const Transform& transform) override;

    void drawLine  (const Vec3& start, const Vec3& end, const UI::Color4& color) override;
    void drawGrid  (float size, int divisions, const UI::Color4& color) override;
    void drawAxis  (float size) override;
    void drawBounds(const Vec3& min, const Vec3& max, const UI::Color4& color) override;

    void addLight  (const Light& light) override;
    void clearLights() override;

    bool renderToFramebuffer(UI::Framebuffer* fb) override;
    bool renderToMRT        (MRTFramebuffer* mrt) override;
    void setMRTEnabled      (bool enabled) override;

private:
    bool createShaders();
    void releaseShaders();
    void applyRenderSettings();
    void setShaderLights(uint32_t shader);
    void renderMeshInternal(const Mesh& mesh, const Matrix4x4& mvp, const Matrix4x4& model, const Material& mat);

    struct LineVertex { Vec3 position; UI::Color4 color; };
    void flushLines();
    void addLineInternal(const Vec3& start, const Vec3& end, const UI::Color4& color);

    bool m_initialized;
    int  m_viewportX, m_viewportY, m_viewportWidth, m_viewportHeight;
    Camera         m_camera;
    RenderSettings m_settings;
    std::vector<Light> m_lights;

    uint32_t m_pbrShader, m_unlitShader, m_lineShader;
    uint32_t m_pbrAdvancedShader, m_skinnedShader, m_mrtShader;

    int m_pbrMVPLoc, m_pbrModelLoc, m_pbrNormalMatLoc;
    int m_pbrAlbedoColorLoc, m_pbrAlbedoTexLoc, m_pbrHasAlbedoTexLoc;
    int m_pbrMetallicLoc, m_pbrRoughnessLoc, m_pbrAOLoc;
    int m_pbrEmissiveColorLoc, m_pbrCameraPosLoc, m_pbrAmbientLoc, m_pbrLightCountLoc;
    std::vector<int> m_pbrLightTypeLocs, m_pbrLightPosLocs, m_pbrLightDirLocs;
    std::vector<int> m_pbrLightColorLocs, m_pbrLightIntensityLocs;

    int m_pbrNormalTexLoc, m_pbrNormalStrengthLoc, m_pbrHasNormalTexLoc;
    int m_pbrClearcoatLoc, m_pbrClearcoatRoughnessLoc;
    int m_pbrTransmissionLoc, m_pbrIORLoc;

    int m_skinnedMVPLoc, m_skinnedModelLoc, m_skinnedJointMatricesLoc;

    int m_unlitMVPLoc, m_unlitColorLoc, m_unlitTexLoc, m_unlitHasTexLoc;
    int m_lineMVPLoc;

    std::vector<LineVertex> m_lineVertices;
    std::vector<uint16_t>   m_lineIndices;
    uint32_t m_lineVBO, m_lineIBO;

    bool             m_mrtEnabled;
    MRTFramebuffer*  m_currentMRT;
};

#endif // USE_GLES || USE_IMGUI

// ============================================================================
// Scene
// ============================================================================

struct SceneObject
{
    Mesh*      mesh;
    Material   material;
    Transform  transform;
    bool       visible;
    std::string name;
    void*      userData;

    SceneObject() : mesh(nullptr), visible(true), userData(nullptr) {}
    SceneObject(Mesh* m, const Material& mat = Material(), const Transform& trans = Transform())
        : mesh(m), material(mat), transform(trans), visible(true), userData(nullptr) {}
};

class Scene
{
public:
    Scene();
    ~Scene();

    SceneObject* addObject     (Mesh* mesh, const Material& material = Material(), const Transform& transform = Transform());
    SceneObject* addOwnedObject(Mesh* mesh, const Material& material = Material(), const Transform& transform = Transform());
    void         removeObject  (SceneObject* obj);
    void         clear();

    size_t             getObjectCount()        const { return m_objects.size(); }
    SceneObject*       getObject(size_t index);
    const SceneObject* getObject(size_t index) const;

    std::vector<SceneObject>&       getObjects()       { return m_objects; }
    const std::vector<SceneObject>& getObjects() const { return m_objects; }

    void                     addLight (const Light& light) { m_lights.push_back(light); }
    void                     clearLights()                 { m_lights.clear(); }
    std::vector<Light>&       getLights()       { return m_lights; }
    const std::vector<Light>& getLights() const { return m_lights; }

    void           setCamera(const Camera& camera) { m_camera = camera; }
    Camera&        getCamera()       { return m_camera; }
    const Camera&  getCamera() const { return m_camera; }

private:
    std::vector<SceneObject> m_objects;
    std::vector<Light>       m_lights;
    Camera                   m_camera;
    std::set<Mesh*>          m_ownedMeshes;
};

// ============================================================================
// Viewport
// ============================================================================

class Viewport : public UI::View
{
public:
    enum class InteractionMode { NONE, ORBIT, PAN, ZOOM };

    Viewport();
    virtual ~Viewport();

    void onMeasure    (UI::MeasureSpec widthSpec, UI::MeasureSpec heightSpec) override;
    void onLayout     (const UI::RectF& bounds) override;
    void onDraw       (UI::ICanvas& canvas) override;
    bool onMotionEvent(const IO::MotionEvent& event) override;

    Scene&       getScene()       { return m_scene; }
    const Scene& getScene() const { return m_scene; }

    void addCube  (const Vec3& pos, float size   = 1.0f, const Material& mat = Material());
    void addSphere(const Vec3& pos, float radius = 1.0f, const Material& mat = Material());
    void addPlane (const Vec3& pos, float width  = 10.0f, float height = 10.0f, const Material& mat = Material());

    Camera&        getCamera()       { return m_scene.getCamera(); }
    const Camera&  getCamera() const { return m_scene.getCamera(); }
    void           setCamera(const Camera& camera) { m_scene.setCamera(camera); }

    void addLight  (const Light& light) { m_scene.addLight(light); }
    void clearLights()                  { m_scene.clearLights(); }

    void                  setRenderSettings(const RenderSettings& settings) { m_renderSettings = settings; }
    const RenderSettings& getRenderSettings() const { return m_renderSettings; }

    void setShowGrid          (bool show)              { m_renderSettings.showGrid = show; }
    void setShowAxis          (bool show)              { m_showAxis = show; }
    void setBackgroundColor   (const UI::Color4& color){ m_renderSettings.clearColor = color; }
    void setInteractionEnabled(bool enabled)           { m_interactionEnabled = enabled; }
    void setOrbitSensitivity  (float s)                { m_orbitSensitivity = s; }
    void setPanSensitivity    (float s)                { m_panSensitivity = s; }
    void setZoomSensitivity   (float s)                { m_zoomSensitivity = s; }
    void setAutoSpin          (float speed)            { m_autoSpinSpeed = speed; }

    void update(float deltaTime);

    using ObjectClickCallback = std::function<void(SceneObject*)>;
    void setOnObjectClickListener(ObjectClickCallback callback) { m_onObjectClick = callback; }

protected:
    void initializeRenderer();
    void renderScene();
    void handleCameraInteraction(const IO::MotionEvent& event);

private:
    Scene                            m_scene;
    std::unique_ptr<I3DCanvas>       m_renderer;
    std::unique_ptr<UI::Framebuffer> m_framebuffer;
    RenderSettings                   m_renderSettings;

    bool            m_interactionEnabled;
    bool            m_isDragging;
    InteractionMode m_currentMode;
    UI::Vec2        m_lastMousePos;

    float m_orbitSensitivity;
    float m_panSensitivity;
    float m_zoomSensitivity;
    float m_autoSpinSpeed;

    bool     m_touchGestureInit;
    UI::Vec2 m_touchPrevPos[2];
    float    m_touchPrevSpan;
    UI::Vec2 m_touchPrevMid;

    bool                m_showAxis;
    ObjectClickCallback m_onObjectClick;
    bool                m_rendererInitialized;
};

// ============================================================================
// ModelInstance
// ============================================================================

class ModelInstance
{
public:
    ModelInstance();
    ~ModelInstance();

    bool load  (Model* model, Viewport& viewport);
    void update(float deltaTime);
    void unload();

    bool isLoaded()      const { return m_model != nullptr; }
    bool hasAnimations() const { return m_model != nullptr && !m_model->animations.empty(); }

    void  setPosition(const Vec3& pos);
    Vec3  getPosition()    const { return m_positionOffset; }
    float getGroundLift()  const { return m_groundLift; }
    Vec3  getHalfExtents() const { return m_halfExtents; }

private:
    struct RenderEntry { int mi; int nodeIdx; Matrix4x4 worldMat; };

    void buildWorldList      (std::vector<RenderEntry>& out, Vec3& bMin, Vec3& bMax) const;
    void computeWorldMatrices(const std::vector<Transform>& local, std::vector<Matrix4x4>& worldMats) const;

    Model*    m_model    = nullptr;
    Viewport* m_viewport = nullptr;

    Matrix4x4              m_normMat;
    Matrix4x4              m_offsetMat;
    std::vector<Matrix4x4> m_baseTransforms;
    std::vector<int>       m_objNodeIdx;

    Vec3  m_positionOffset;
    Vec3  m_halfExtents;
    float m_groundLift;

    int   m_activeClip = -1;
    float m_animTime   = 0.0f;
};

} // namespace UI3D

} // namespace APP

#endif // UI3D_CORE_H
