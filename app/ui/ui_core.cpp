#include "ui_core.h"
#include "ui_focus.h"
#include "io_platform.h"

#include <GLES3/gl3.h>
#include <GLES2/gl2.h>
#define GL_GLEXT_PROTOTYPES
#include <GLES2/gl2ext.h>
#include <EGL/eglext.h>

#define STB_IMAGE_IMPLEMENTATION
	#include "stb/stb_image.h"
#undef STB_IMAGE_IMPLEMENTATION

#define STB_TRUETYPE_IMPLEMENTATION
	#include "stb/stb_truetype.h"
#undef STB_TRUETYPE_IMPLEMENTATION

#include <cmath>
#include <algorithm>
#include <limits>
#include <cstring>
#include <fstream>
#include <iostream>
#include <mutex>
#include <thread>

#ifdef USE_IMGUI
    #include "imgui.h"
    #include "imgui_impl_opengl3.h"
#endif

namespace APP
{

namespace UI
{

// Forward declaration
class ViewGroup;

// ============================================================================
// Texture Implementation
// ============================================================================

Texture::Texture() : id(0), width(0), height(0), isExtTexture(false) {}

Texture::~Texture() { releaseGPU(); }

Texture::Texture(Texture&& other) noexcept
    : id(other.id), width(other.width), height(other.height), imageData(std::move(other.imageData)), isExtTexture(other.isExtTexture)
{
    other.id = 0;
    other.isExtTexture = false;
    other.width = 0;
    other.height = 0;
}

Texture& Texture::operator=(Texture&& other) noexcept
{
    if (this != &other)
    {
        releaseGPU();
        id = other.id;
        width = other.width;
        height = other.height;
        imageData = std::move(other.imageData);
        isExtTexture = other.isExtTexture;
        other.id = 0;
        other.isExtTexture = false;
        other.width = 0;
        other.height = 0;
    }
    return *this;
}

bool Texture::isValid() const { return id != 0 || imageData.empty() == false; }
bool Texture::hasGLTexture() const { return id != 0; }
bool Texture::hasImageData() const { return imageData.empty() == false; }

bool Texture::loadFromMemory(const uint8_t* data, int w, int h, int channels)
{
    if (data == nullptr || w <= 0 || h <= 0 || channels <= 0) return false;
    
    width = w;
    height = h;
    size_t pixelCount = width * height;
    imageData.resize(pixelCount * 4);
    
    if (channels == 4) { std::memcpy(imageData.data(), data, pixelCount * 4); }
    else if (channels == 3)
    {
        for (size_t i = 0; i < pixelCount; ++i)
        {
            imageData[i * 4 + 0] = data[i * 3 + 0];
            imageData[i * 4 + 1] = data[i * 3 + 1];
            imageData[i * 4 + 2] = data[i * 3 + 2];
            imageData[i * 4 + 3] = 255;
        }
    }
    else if (channels == 1)
    {
        for (size_t i = 0; i < pixelCount; ++i)
        {
            uint8_t gray = data[i];
            imageData[i * 4 + 0] = gray;
            imageData[i * 4 + 1] = gray;
            imageData[i * 4 + 2] = gray;
            imageData[i * 4 + 3] = 255;
        }
    }
    else { return false; }
    
    return uploadToGPU();
}

bool Texture::uploadToGPU()
{
    if (imageData.empty() == true || width <= 0 || height <= 0) return false;
    if (id != 0 && isExtTexture == false) glDeleteTextures(1, &id);
    
    glGenTextures(1, &id);
    if (id == 0) return false;
    
    glBindTexture(GL_TEXTURE_2D, id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, imageData.data());
    glBindTexture(GL_TEXTURE_2D, 0);
    
    if (glGetError() != GL_NO_ERROR)
    {
        glDeleteTextures(1, &id);
        id = 0;
        return false;
    }
    
    isExtTexture = false;
    std::vector<uint8_t>().swap(imageData);
    return true;
}

bool Texture::updateGPU(const uint8_t* data, int w, int h)
{
    if (data == nullptr || w <= 0 || h <= 0) return false;
    
    if (id == 0 || width != w || height != h)
    {
        width = w;
        height = h;
        size_t dataSize = width * height * 4;
        imageData.resize(dataSize);
        std::memcpy(imageData.data(), data, dataSize);
        return uploadToGPU();
    }
    
    if (isExtTexture == true) return false;
    
    glBindTexture(GL_TEXTURE_2D, id);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, data);
    glBindTexture(GL_TEXTURE_2D, 0);
    
    return glGetError() == GL_NO_ERROR;
}

void Texture::setExternalTexture(uint32_t textureId, int w, int h)
{
    if (id != 0 && isExtTexture == false) glDeleteTextures(1, &id);
    id = textureId;
    width = w;
    height = h;
    isExtTexture = true;
    if (imageData.empty() == false)
    {
        imageData.clear();
        std::vector<uint8_t>().swap(imageData);
    }
}

void Texture::releaseGPU()
{
    if (id != 0 && isExtTexture == false)
    {
        glDeleteTextures(1, &id);
        id = 0;
    }

    if (imageData.empty() == false)
    {
        imageData.clear();
        std::vector<uint8_t>().swap(imageData);
    }
}

TextureInfo::TextureInfo() : name(), id(), texture(nullptr) {}

// ============================================================================
// TextureManager Implementation
// ============================================================================

TextureManager::TextureManager() {}

TextureManager::~TextureManager() { clearAll(); }

TextureManager& TextureManager::getInstance()
{
    static TextureManager instance;
    return instance;
}

Texture* TextureManager::loadTexture(const std::string& path)
{
    auto it = m_textures.find(path);
    if (it != m_textures.end()) return &it->second;
    
    int width, height, channels;
    stbi_set_flip_vertically_on_load(false);
    unsigned char* data = stbi_load(path.c_str(), &width, &height, &channels, 4);
    
    if (data == nullptr) return nullptr;
    
    Texture texture;
    if (texture.loadFromMemory(data, width, height, 4) == false)
    {
        stbi_image_free(data);
        return nullptr;
    }
    stbi_image_free(data);
    
    auto result = m_textures.emplace(path, std::move(texture));
    return &result.first->second;
}

Texture* TextureManager::loadTextureFromMemory(const uint8_t* data, int width, int height, int channels)
{
    if (data == nullptr || width <= 0 || height <= 0) return nullptr;
    
    IO::Platform::getInstance().makeGLContextCurrent();
    
    Texture texture;
    if (texture.loadFromMemory(data, width, height, channels) == false) return nullptr;
    
    static int counter = 0;
    std::string key = "__memory_" + std::to_string(counter++);
    auto result = m_textures.emplace(key, std::move(texture));
    return &result.first->second;
}

void TextureManager::releaseTexture(const std::string& path)
{
    auto it = m_textures.find(path);
    if (it != m_textures.end())
    {
        IO::Platform::getInstance().makeGLContextCurrent();
        it->second.releaseGPU();
        m_textures.erase(it);
    }
}

Texture* TextureManager::getTexture(const std::string& path)
{
    auto it = m_textures.find(path);
    return it != m_textures.end() ? &it->second : nullptr;
}

bool TextureManager::loadTextureCollection(const std::string& directory, const std::string& manifestFile, bool forceReload)
{
    std::string collectionName = directory;
    size_t lastSlash = collectionName.find_last_of("/\\");
    if (lastSlash != std::string::npos) collectionName = collectionName.substr(lastSlash + 1);
    
    // Check if collection is already loaded (unless force reload)
    if (forceReload == false)
    {
        auto collIt = m_collections.find(collectionName);
        if (collIt != m_collections.end())
        {
            std::cerr << "Warning: Collection '" << collectionName << "' already loaded" << std::endl;
            return true;
        }
    }
    else
    {
        // Clear existing collection first before reload
        releaseTextureCollection(collectionName);
    }

    std::string manifestPath = directory + "/" + manifestFile;
    std::ifstream manifest(manifestPath);
    if (manifest.is_open() == false)
    {
        std::cerr << "Failed to open manifest: " << manifestPath << std::endl;
        return false;
    }
    
    std::vector<std::string> loadedIds;
    std::string line;
    int lineNum = 0;
    
    while (std::getline(manifest, line))
    {
        lineNum++;
        if (line.empty() == true || line[0] == '#') continue;
        
        size_t start = line.find_first_not_of(" \t\r\n");
        size_t end = line.find_last_not_of(" \t\r\n");
        if (start == std::string::npos) continue;
        line = line.substr(start, end - start + 1);
        
        std::string id, filename, name;
        size_t firstColon = line.find(':');
        
        if (firstColon != std::string::npos)
        {
            id = line.substr(0, firstColon);
            size_t secondColon = line.find(':', firstColon + 1);
            
            if (secondColon != std::string::npos)
            {
                filename = line.substr(firstColon + 1, secondColon - firstColon - 1);
                name = line.substr(secondColon + 1);
            }
            else { filename = line.substr(firstColon + 1); name = id; }
        }
        else
        {
            filename = line;
            size_t dotPos = filename.find_last_of('.');
            id = dotPos != std::string::npos ? filename.substr(0, dotPos) : filename;
            name = id;
        }
        
        std::string texturePath = directory + "/" + filename;
        Texture* texture = loadTexture(texturePath);
        
        if (texture == nullptr)
        {
            std::cerr << "Warning: Failed to load texture '" << texturePath << "' from line " << lineNum << std::endl;
            continue;
        }
        // else
        // {
        //     std::cout << "Loaded texture " << name << ", collection: " << collectionName << ", id: " << id << std::endl;
        // }
        
        std::string fullId = generateTextureId(id, collectionName);
        TextureInfo info;
        info.name = name;
        info.id = fullId;
        info.texture = texture;
        m_textureInfoById[fullId] = info;
        m_nameToId[name] = fullId;
        loadedIds.push_back(fullId);
    }
    
    if (loadedIds.empty() == false)
    {
        m_collections[collectionName] = loadedIds;
        std::cout << "Loaded collection '" << collectionName << "' with " << loadedIds.size() << " textures" << std::endl;
        return true;
    }
    
    return false;
}

void TextureManager::releaseTextureCollection(const std::string& collectionName)
{
    auto collIt = m_collections.find(collectionName);
    if (collIt == m_collections.end()) return;
    
    for (const std::string& id : collIt->second)
    {
        auto infoIt = m_textureInfoById.find(id);
        if (infoIt != m_textureInfoById.end())
        {
            m_nameToId.erase(infoIt->second.name);
            m_textureInfoById.erase(infoIt);
        }
    }
    m_collections.erase(collIt);
}

const TextureInfo* TextureManager::getTextureInfo(const std::string& id) const
{
    auto it = m_textureInfoById.find(id);
    return it != m_textureInfoById.end() ? &it->second : nullptr;
}

const TextureInfo* TextureManager::getTextureInfoByName(const std::string& name) const
{
    auto nameIt = m_nameToId.find(name);
    return nameIt == m_nameToId.end() ? nullptr : getTextureInfo(nameIt->second);
}

Texture* TextureManager::getTextureById(const std::string& id)
{
    const TextureInfo* info = getTextureInfo(id);
    return info != nullptr ? info->texture : nullptr;
}

Texture* TextureManager::getTextureByName(const std::string& name)
{
    const TextureInfo* info = getTextureInfoByName(name);
    return info != nullptr ? info->texture : nullptr;
}

bool TextureManager::hasTexture(const std::string& id) const { return m_textureInfoById.find(id) != m_textureInfoById.end(); }

bool TextureManager::hasTextureName(const std::string& name) const { return m_nameToId.find(name) != m_nameToId.end(); }

Vec2 TextureManager::getTextureSize(const std::string& id) const
{
    const TextureInfo* info = getTextureInfo(id);
    return info != nullptr ? Vec2(static_cast<float>(info->texture->width), static_cast<float>(info->texture->height)) : Vec2(0.0f, 0.0f);
}

Vec2 TextureManager::getTextureSizeByName(const std::string& name) const
{
    const TextureInfo* info = getTextureInfoByName(name);
    return info != nullptr ? Vec2(static_cast<float>(info->texture->width), static_cast<float>(info->texture->height)) : Vec2(0.0f, 0.0f);
}

std::vector<std::string> TextureManager::getTextureIds() const
{
    std::vector<std::string> ids;
    ids.reserve(m_textureInfoById.size());
    for (const auto& pair : m_textureInfoById) ids.push_back(pair.first);
    return ids;
}

std::vector<std::string> TextureManager::getTextureNames() const
{
    std::vector<std::string> names;
    names.reserve(m_nameToId.size());
    for (const auto& pair : m_nameToId) names.push_back(pair.first);
    return names;
}

std::vector<TextureInfo> TextureManager::getAllTextureInfo() const
{
    std::vector<TextureInfo> infos;
    infos.reserve(m_textureInfoById.size());
    for (const auto& pair : m_textureInfoById) infos.push_back(pair.second);
    return infos;
}

void TextureManager::clearAll()
{
    if (m_textures.empty()) return;

    for (auto& pair : m_textures) pair.second.releaseGPU();
    m_textures.clear();
    m_textureInfoById.clear();
    m_nameToId.clear();
    m_collections.clear();
    std::cout << "\n TextureManager clear completed\n" << std::endl;
}

std::string TextureManager::generateTextureId(const std::string& name, const std::string& collection)
{
    return collection.empty() == true ? name : collection + "::" + name;
}

// ============================================================================
// Framebuffer Implementation
// ============================================================================

Framebuffer::Framebuffer() : fbo(0), depthRbo(0), texture(nullptr), m_savedViewport{0, 0, 0, 0} {}

Framebuffer::~Framebuffer() { release(); }

Framebuffer::Framebuffer(Framebuffer&& other) noexcept
    : fbo(other.fbo), depthRbo(other.depthRbo), texture(std::move(other.texture))
{
    other.fbo = 0;
    other.depthRbo = 0;
}

Framebuffer& Framebuffer::operator=(Framebuffer&& other) noexcept
{
    if (this != &other)
    {
        release();
        fbo = other.fbo;
        depthRbo = other.depthRbo;
        texture = std::move(other.texture);
        other.fbo = 0;
        other.depthRbo = 0;
    }
    return *this;
}

bool Framebuffer::create(int w, int h)
{
    if (w <= 0 || h <= 0) return false;
    release();

    texture = std::make_unique<Texture>();
    
    glGenTextures(1, &texture->id);
    glBindTexture(GL_TEXTURE_2D, texture->id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);

    texture->width = w;
    texture->height = h;
    texture->isExtTexture = false;
    
    glGenFramebuffers(1, &fbo);
    if (fbo == 0)
    {
        glDeleteTextures(1, &texture->id);
        texture->id = 0;
        return false;
    }
    
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture->id, 0);

    // Depth+stencil renderbuffer — required for 3D rendering with depth test
    glGenRenderbuffers(1, &depthRbo);
    glBindRenderbuffer(GL_RENDERBUFFER, depthRbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, w, h);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, depthRbo);

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    
    if (status != GL_FRAMEBUFFER_COMPLETE)
    {
        std::cerr << "Framebuffer incomplete: " << status << std::endl;
        release();
        return false;
    }
    
    return true;
}

void Framebuffer::release()
{
    if (isValid()) 
    { 
        if(texture != nullptr && texture->isValid() == true)
        {
            texture->releaseGPU();
            texture.reset();
        }

        if (depthRbo != 0) { glDeleteRenderbuffers(1, &depthRbo); depthRbo = 0; }
        glDeleteFramebuffers(1, &fbo);
        fbo = 0;
    }
}

bool Framebuffer::bind()
{
    if (isValid() == false) return false;
    glGetIntegerv(GL_VIEWPORT, m_savedViewport);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glViewport(0, 0, texture->width, texture->height);
    return true;
}

void Framebuffer::unbind() 
{ 
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(m_savedViewport[0], m_savedViewport[1], m_savedViewport[2], m_savedViewport[3]);
}

bool Framebuffer::isValid() const { return fbo != 0 && texture != nullptr && texture->isValid(); }

Texture* Framebuffer::getTexture() const { return texture.get(); }

// ============================================================================
// GLFont Implementation
// ============================================================================
#ifdef USE_GLES

GLFont::GLFont()
    : m_fontSize(16.0f), m_lineHeight(0.0f), m_ascent(0.0f), m_descent(0.0f)
    , m_atlasWidth(1024), m_atlasHeight(1024), m_currentX(2), m_currentY(2), m_rowHeight(0)
{
}

GLFont::~GLFont() { cleanup(); }

bool GLFont::loadFromConfig(const FontConfig& config)
{
    std::ifstream file(config.path, std::ios::binary | std::ios::ate);
    if (file.is_open() == false)
    {
        std::cerr << "GLFont: Failed to open font file: " << config.path << std::endl;
        return false;
    }
    
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    m_ttfData.resize(size);
    
    if (file.read(reinterpret_cast<char*>(m_ttfData.data()), size).fail() == true)
    {
        std::cerr << "GLFont: Failed to read font file: " << config.path << std::endl;
        return false;
    }
    
    std::vector<CodepointRange> ranges = config.customRanges.empty() == false ? config.customRanges : getCharacterSetRanges(config.charset);
    return buildAtlas(m_ttfData.data(), m_ttfData.size(), config.size, ranges, config.atlasWidth, config.atlasHeight);
}

void GLFont::cleanup()
{
    m_glyphs.clear();
    m_atlasTexture.releaseGPU();
    m_ttfData.clear();
}

Texture* GLFont::getTexture() { return &m_atlasTexture; }
const Texture* GLFont::getTexture() const { return &m_atlasTexture; }
float GLFont::getFontSize() const { return m_fontSize; }
float GLFont::getLineHeight() const { return m_lineHeight; }
float GLFont::getAscent() const { return m_ascent; }
float GLFont::getDescent() const { return m_descent; }

Vec2 GLFont::measureText(const std::string& text, float fontSize) const
{
    float scale = fontSize / m_fontSize;
    float x = 0.0f;
    float maxX = 0.0f;
    float maxY = fontSize;
    
    const char* str = text.c_str();
    while (*str != '\0')
    {
        if (*str == '\n') 
        { 
            maxX = std::max(maxX, x);
            maxY += fontSize; 
            x = 0.0f; 
            str++; 
            continue; 
        }
        
        uint32_t codepoint = utf8ToCodepoint(str);
        const GlyphInfo* glyph = getGlyph(codepoint);
        
        if (glyph == nullptr && m_ttfData.empty() == false)
        {
            const_cast<GLFont*>(this)->addGlyphOnDemand(codepoint);
            glyph = getGlyph(codepoint);
        }
        
        if (glyph != nullptr) x += glyph->xadvance * scale;
        else x += fontSize * 0.5f;
    }
    
    maxX = std::max(maxX, x);
    
    return Vec2(maxX, maxY);
}

const GlyphInfo* GLFont::getGlyph(uint32_t codepoint) const
{
    auto it = m_glyphs.find(codepoint);
    return it != m_glyphs.end() ? &it->second : nullptr;
}

bool GLFont::hasGlyph(uint32_t codepoint) const { return m_glyphs.find(codepoint) != m_glyphs.end(); }

void GLFont::addGlyphOnDemand(uint32_t codepoint)
{
    if (hasGlyph(codepoint) == true || m_ttfData.empty() == true) return;
    
    stbtt_fontinfo font;
    if (stbtt_InitFont(&font, m_ttfData.data(), 0) == 0) return;
    
    float scale = stbtt_ScaleForPixelHeight(&font, m_fontSize);
    int glyphIndex = stbtt_FindGlyphIndex(&font, codepoint);
    
    if (glyphIndex == 0)
    {
        GlyphInfo glyph = { codepoint, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, m_fontSize * 0.5f };
        m_glyphs[codepoint] = glyph;
        return;
    }
    
    int x0, y0, x1, y1;
    stbtt_GetGlyphBitmapBox(&font, glyphIndex, scale, scale, &x0, &y0, &x1, &y1);
    int glyphWidth = x1 - x0, glyphHeight = y1 - y0;
    
    if (glyphWidth <= 0 || glyphHeight <= 0)
    {
        int advanceWidth, leftSideBearing;
        stbtt_GetGlyphHMetrics(&font, glyphIndex, &advanceWidth, &leftSideBearing);
        GlyphInfo glyph = { codepoint, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, advanceWidth * scale };
        m_glyphs[codepoint] = glyph;
        return;
    }
    
    if (m_currentX + glyphWidth + 2 >= m_atlasWidth) { m_currentX = 2; m_currentY += m_rowHeight + 2; m_rowHeight = 0; }
    
    if (m_currentY + glyphHeight + 2 >= m_atlasHeight)
    {
        static std::unordered_map<uint32_t, bool> warnedCodepoints;
        if (warnedCodepoints.find(codepoint) == warnedCodepoints.end())
        {
            std::cerr << "GLFont: Atlas full, cannot add glyph U+" << std::hex << codepoint << std::dec << std::endl;
            warnedCodepoints[codepoint] = true;
        }
        GlyphInfo glyph = { codepoint, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, m_fontSize * 0.5f };
        m_glyphs[codepoint] = glyph;
        return;
    }
    
    std::vector<uint8_t> glyphBitmap(glyphWidth * glyphHeight);
    stbtt_MakeGlyphBitmap(&font, glyphBitmap.data(), glyphWidth, glyphHeight, glyphWidth, scale, scale, glyphIndex);
    
    std::vector<uint8_t> rgbaData(glyphWidth * glyphHeight * 4);
    for (int i = 0; i < glyphWidth * glyphHeight; ++i)
    {
        rgbaData[i * 4 + 0] = 255;
        rgbaData[i * 4 + 1] = 255;
        rgbaData[i * 4 + 2] = 255;
        rgbaData[i * 4 + 3] = glyphBitmap[i];
    }
    
    if (m_atlasTexture.hasGLTexture() == true)
    {
        glBindTexture(GL_TEXTURE_2D, m_atlasTexture.id);
        glTexSubImage2D(GL_TEXTURE_2D, 0, m_currentX, m_currentY, glyphWidth, glyphHeight, GL_RGBA, GL_UNSIGNED_BYTE, rgbaData.data());
        glBindTexture(GL_TEXTURE_2D, 0);
    }
    
    int advanceWidth, leftSideBearing;
    stbtt_GetGlyphHMetrics(&font, glyphIndex, &advanceWidth, &leftSideBearing);
    
    GlyphInfo glyph;
    glyph.codepoint = codepoint;
    glyph.x0 = static_cast<float>(m_currentX) / m_atlasWidth;
    glyph.y0 = static_cast<float>(m_currentY) / m_atlasHeight;
    glyph.x1 = static_cast<float>(m_currentX + glyphWidth) / m_atlasWidth;
    glyph.y1 = static_cast<float>(m_currentY + glyphHeight) / m_atlasHeight;
    glyph.xoff = static_cast<float>(x0);
    glyph.yoff = static_cast<float>(y0);
    glyph.xadvance = advanceWidth * scale;
    m_glyphs[codepoint] = glyph;
    
    m_currentX += glyphWidth + 2;
    m_rowHeight = std::max(m_rowHeight, glyphHeight);
}

std::vector<CodepointRange> GLFont::getCharacterSetRanges(CharacterSet charset) const
{
    std::vector<CodepointRange> ranges;
    
    switch (charset)
    {
        case CharacterSet::ASCII:
            ranges.push_back(CodepointRange(0x0020, 0x007E));
            break;
        case CharacterSet::LATIN_EXT:
            ranges.push_back(CodepointRange(0x0020, 0x007E));
            ranges.push_back(CodepointRange(0x00A0, 0x00FF));
            ranges.push_back(CodepointRange(0x0100, 0x017F));
            ranges.push_back(CodepointRange(0x0180, 0x024F));
            break;
        case CharacterSet::CJK_BASIC:
        case CharacterSet::CJK_FULL:
        case CharacterSet::KOREAN:
        case CharacterSet::CUSTOM:
        default:
            ranges.push_back(CodepointRange(0x0020, 0x007E));
            break;
    }
    
    return ranges;
}

std::vector<uint32_t> GLFont::collectCodepoints(const std::vector<CodepointRange>& ranges) const
{
    std::vector<uint32_t> codepoints;
    const size_t MAX_INITIAL_GLYPHS = 512;
    size_t totalCount = 0;
    
    for (const CodepointRange& range : ranges)
    {
        for (uint32_t cp = range.first; cp <= range.last && totalCount < MAX_INITIAL_GLYPHS; ++cp)
        {
            codepoints.push_back(cp);
            totalCount++;
        }
        if (totalCount >= MAX_INITIAL_GLYPHS) break;
    }
    
    return codepoints;
}

bool GLFont::buildAtlas(const uint8_t* ttfData, size_t ttfSize, float fontSize, const std::vector<CodepointRange>& ranges, int atlasWidth, int atlasHeight)
{
    m_fontSize = fontSize;
    m_atlasWidth = atlasWidth;
    m_atlasHeight = atlasHeight;
    
    stbtt_fontinfo font;
    if (stbtt_InitFont(&font, ttfData, 0) == 0)
    {
        std::cerr << "GLFont: Failed to initialize font" << std::endl;
        return false;
    }
    
    float scale = stbtt_ScaleForPixelHeight(&font, fontSize);
    int ascent, descent, lineGap;
    stbtt_GetFontVMetrics(&font, &ascent, &descent, &lineGap);
    m_ascent = ascent * scale;
    m_descent = descent * scale;
    m_lineHeight = (ascent - descent + lineGap) * scale;
    
    std::vector<uint8_t> atlasData(atlasWidth * atlasHeight, 0);
    m_currentX = 2;
    m_currentY = 2;
    m_rowHeight = 0;
    
    std::vector<uint32_t> codepoints = collectCodepoints(ranges);
    
    for (uint32_t codepoint : codepoints)
    {
        int glyphIndex = stbtt_FindGlyphIndex(&font, codepoint);
        if (glyphIndex == 0) continue;
        
        int x0, y0, x1, y1;
        stbtt_GetGlyphBitmapBox(&font, glyphIndex, scale, scale, &x0, &y0, &x1, &y1);
        int glyphWidth = x1 - x0, glyphHeight = y1 - y0;
        
        if (glyphWidth == 0 || glyphHeight == 0)
        {
            int advanceWidth, leftSideBearing;
            stbtt_GetGlyphHMetrics(&font, glyphIndex, &advanceWidth, &leftSideBearing);
            GlyphInfo glyph = { codepoint, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, advanceWidth * scale };
            m_glyphs[codepoint] = glyph;
            continue;
        }
        
        if (m_currentX + glyphWidth + 2 >= atlasWidth) { m_currentX = 2; m_currentY += m_rowHeight + 2; m_rowHeight = 0; }
        if (m_currentY + glyphHeight + 2 >= atlasHeight) break;
        
        std::vector<uint8_t> glyphBitmap(glyphWidth * glyphHeight);
        stbtt_MakeGlyphBitmap(&font, glyphBitmap.data(), glyphWidth, glyphHeight, glyphWidth, scale, scale, glyphIndex);
        
        for (int gy = 0; gy < glyphHeight; ++gy)
        {
            for (int gx = 0; gx < glyphWidth; ++gx)
                atlasData[(m_currentY + gy) * atlasWidth + (m_currentX + gx)] = glyphBitmap[gy * glyphWidth + gx];
        }
        
        int advanceWidth, leftSideBearing;
        stbtt_GetGlyphHMetrics(&font, glyphIndex, &advanceWidth, &leftSideBearing);
        
        GlyphInfo glyph;
        glyph.codepoint = codepoint;
        glyph.x0 = static_cast<float>(m_currentX) / atlasWidth;
        glyph.y0 = static_cast<float>(m_currentY) / atlasHeight;
        glyph.x1 = static_cast<float>(m_currentX + glyphWidth) / atlasWidth;
        glyph.y1 = static_cast<float>(m_currentY + glyphHeight) / atlasHeight;
        glyph.xoff = static_cast<float>(x0);
        glyph.yoff = static_cast<float>(y0);
        glyph.xadvance = advanceWidth * scale;
        m_glyphs[codepoint] = glyph;
        
        m_currentX += glyphWidth + 2;
        m_rowHeight = std::max(m_rowHeight, glyphHeight);
    }
    
    std::vector<uint8_t> rgbaData(atlasWidth * atlasHeight * 4);
    for (int i = 0; i < atlasWidth * atlasHeight; ++i)
    {
        rgbaData[i * 4 + 0] = 255;
        rgbaData[i * 4 + 1] = 255;
        rgbaData[i * 4 + 2] = 255;
        rgbaData[i * 4 + 3] = atlasData[i];
    }
    
    return m_atlasTexture.loadFromMemory(rgbaData.data(), atlasWidth, atlasHeight, 4);
}

uint32_t GLFont::utf8ToCodepoint(const char*& str) const
{
    uint32_t codepoint = 0;
    uint8_t c = static_cast<uint8_t>(*str);
    
    if (c < 0x80) { codepoint = c; str += 1; }
    else if ((c & 0xE0) == 0xC0) { codepoint = ((c & 0x1F) << 6) | (static_cast<uint8_t>(str[1]) & 0x3F); str += 2; }
    else if ((c & 0xF0) == 0xE0) { codepoint = ((c & 0x0F) << 12) | ((static_cast<uint8_t>(str[1]) & 0x3F) << 6) | (static_cast<uint8_t>(str[2]) & 0x3F); str += 3; }
    else if ((c & 0xF8) == 0xF0) { codepoint = ((c & 0x07) << 18) | ((static_cast<uint8_t>(str[1]) & 0x3F) << 12) | ((static_cast<uint8_t>(str[2]) & 0x3F) << 6) | (static_cast<uint8_t>(str[3]) & 0x3F); str += 4; }
    else { str += 1; }
    
    return codepoint;
}

#endif // USE_GLES

// ============================================================================
// IGFont Implementation
// ============================================================================
#ifdef USE_IMGUI

IGFont::IGFont() : m_imFont(nullptr), m_fontSize(16.0f), m_ownsFont(false) {}

IGFont::~IGFont() {}

bool IGFont::loadFromConfig(const FontConfig& config)
{
    ImGuiIO& io = ImGui::GetIO();
    ImFontConfig imConfig;
    imConfig.SizePixels = config.size;
    
    ImFont* font = nullptr;
    
    if (config.customRanges.empty() == false)
    {
        static ImVector<ImWchar> ranges;
        ranges.clear();
        for (const CodepointRange& range : config.customRanges)
        {
            ranges.push_back(static_cast<ImWchar>(range.first));
            ranges.push_back(static_cast<ImWchar>(range.last));
        }
        ranges.push_back(0);
        font = io.Fonts->AddFontFromFileTTF(config.path.c_str(), config.size, &imConfig, ranges.Data);
    }
    else
    {
        if (config.charset == CharacterSet::ASCII) { font = io.Fonts->AddFontFromFileTTF(config.path.c_str(), config.size, &imConfig); }
        else if (config.charset == CharacterSet::KOREAN) { font = io.Fonts->AddFontFromFileTTF(config.path.c_str(), config.size, &imConfig, io.Fonts->GetGlyphRangesKorean()); }
        else if (config.charset == CharacterSet::CJK_BASIC || config.charset == CharacterSet::CJK_FULL) { font = io.Fonts->AddFontFromFileTTF(config.path.c_str(), config.size, &imConfig, io.Fonts->GetGlyphRangesChineseFull()); }
        else { font = io.Fonts->AddFontFromFileTTF(config.path.c_str(), config.size, &imConfig); }
    }
    
    if (font == nullptr)
    {
        std::cerr << "IGFont: Failed to load font: " << config.path << std::endl;
        return false;
    }
    
    io.Fonts->Build();
    ImGui_ImplOpenGL3_DestroyFontsTexture();
    ImGui_ImplOpenGL3_CreateFontsTexture();
    
    m_imFont = font;
    m_fontSize = config.size;
    m_ownsFont = true;
    
    return true;
}

void IGFont::setNativeFont(void* imFont, float fontSize)
{
    m_imFont = imFont;
    m_fontSize = fontSize;
    m_ownsFont = false;
}

float IGFont::getFontSize() const { return m_fontSize; }

float IGFont::getLineHeight() const
{
    if (m_imFont == nullptr) return m_fontSize;
    return static_cast<ImFont*>(m_imFont)->FontSize;
}

float IGFont::getAscent() const
{
    if (m_imFont == nullptr) return m_fontSize * 0.8f;
    return static_cast<ImFont*>(m_imFont)->Ascent;
}

float IGFont::getDescent() const
{
    if (m_imFont == nullptr) return m_fontSize * 0.2f;
    return static_cast<ImFont*>(m_imFont)->Descent;
}

Vec2 IGFont::measureText(const std::string& text, float fontSize) const
{
    if (m_imFont == nullptr) return Vec2(text.length() * fontSize * 0.5f, fontSize);
    ImFont* font = static_cast<ImFont*>(m_imFont);
    ImVec2 size = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, text.c_str());
    return Vec2(size.x, size.y);
}

void* IGFont::getNativeHandle() { return m_imFont; }

const void* IGFont::getNativeHandle() const { return m_imFont; }

#endif // USE_IMGUI

// ============================================================================
// FontManager Implementation
// ============================================================================

FontManager::FontManager() : m_defaultFontId("default"), m_initialized(false) {}

FontManager::~FontManager() { cleanup(); }

FontManager& FontManager::getInstance()
{
    static FontManager instance;
    return instance;
}

bool FontManager::initialize()
{
    if (m_initialized == true) return true;
    m_initialized = true;
    return true;
}

void FontManager::cleanup()
{
    if (m_initialized == false) return;
    
    #ifdef USE_GLES
        m_glFonts.clear();
    #endif
    
    #ifdef USE_IMGUI
        m_igFonts.clear();
        m_contextFonts.clear();
        m_contextDefaultFonts.clear();
    #endif
    
    m_defaultFontId = "default";
    m_initialized = false;
}

bool FontManager::loadFont(const FontConfig& config)
{
    if (m_initialized == false)
    {
        std::cerr << "FontManager: Not initialized" << std::endl;
        return false;
    }
    
    bool glSuccess = false;
    bool igSuccess = false;
    
    #ifdef USE_GLES
    {
        auto glFont = std::make_unique<GLFont>();
        FontConfig glConfig = config;
        
        if (config.charset == CharacterSet::KOREAN || config.charset == CharacterSet::CJK_BASIC || config.charset == CharacterSet::CJK_FULL)
        {
            std::vector<CodepointRange> asciiOnly;
            asciiOnly.push_back(CodepointRange(0x0020, 0x007E));
            glConfig.customRanges = asciiOnly;
        }
        
        if (glFont->loadFromConfig(glConfig) == true)
        {
            m_glFonts[config.id] = std::move(glFont);
            glSuccess = true;
        }
        else { std::cerr << "  ✗ Failed to load GLFont: " << config.name << std::endl; }
    }
    #endif
    
    #ifdef USE_IMGUI
    {
        auto igFont = std::make_unique<IGFont>();
        if (igFont->loadFromConfig(config) == true)
        {
            m_igFonts[config.id] = std::move(igFont);
            igSuccess = true;
        }
        else { std::cerr << "  ✗ Failed to load IGFont: " << config.name << std::endl; }
    }
    #endif
    
    return glSuccess || igSuccess;
}

void FontManager::setDefaultFont(const std::string& id)
{
    bool exists = false;
    
    #ifdef USE_GLES
        if (m_glFonts.find(id) != m_glFonts.end()) exists = true;
    #endif
    
    #ifdef USE_IMGUI
        if (m_igFonts.find(id) != m_igFonts.end()) 
        {
            exists = true;
        }
        else
        {
            // Check if font exists in any context
            void* currentContext = ImGui::GetCurrentContext();
            if (currentContext != nullptr)
            {
                auto contextIt = m_contextFonts.find(currentContext);
                if (contextIt != m_contextFonts.end())
                {
                    if (contextIt->second.find(id) != contextIt->second.end())
                    {
                        exists = true;
                        m_contextDefaultFonts[currentContext] = id;
                    }
                }
            }
        }
    #endif
    
    if (exists == true)
    {
        m_defaultFontId = id;
    }
    else 
    { 
        std::cerr << "FontManager: Cannot set default font - " << id << " not found" << std::endl; 
    }
}

#ifdef USE_IMGUI
bool FontManager::setDefaultFontForContext(const std::string& id, void* imguiContext)
{
    if (imguiContext == nullptr) return false;
    
    auto contextIt = m_contextFonts.find(imguiContext);
    if (contextIt != m_contextFonts.end())
    {
        if (contextIt->second.find(id) != contextIt->second.end())
        {
            m_contextDefaultFonts[imguiContext] = id;
            return true;
        }
    }
    
    return false;
}
#endif

Vec2 FontManager::measureText(const std::string& text, const Paint& paint)
{
    if (text.empty()) return Vec2(0.0f, 0.0f);
    
    #ifdef USE_GLES
        GLFont* glFont = getGLFont(paint.textProps.fontId);
        if (glFont == nullptr) glFont = getDefaultGLFont();
        if (glFont != nullptr) return glFont->measureText(text, paint.textProps.size);
    #endif
    
    #ifdef USE_IMGUI
        // Try to get current ImGui context (will be set during rendering)
        void* currentContext = ImGui::GetCurrentContext();
        if (currentContext != nullptr)
        {
            // Use context-aware font lookup
            IGFont* igFont = getIGFontForContext(paint.textProps.fontId, currentContext);
            if (igFont == nullptr) igFont = getDefaultIGFontForContext(currentContext);
            
            if (igFont != nullptr)
            {
                return igFont->measureText(text, paint.textProps.size);
            }
            
            // Fallback: use ImGui's default font for this context
            ImFont* defaultFont = ImGui::GetFont();
            if (defaultFont != nullptr)
            {
                ImVec2 size = defaultFont->CalcTextSizeA(paint.textProps.size, FLT_MAX, 0.0f, text.c_str());
                return Vec2(size.x, size.y);
            }
        }
        
        // No current context - try global fonts (shouldn't happen in multi-context)
        IGFont* igFont = getIGFont(paint.textProps.fontId);
        if (igFont == nullptr) igFont = getDefaultIGFont();
        if (igFont != nullptr) return igFont->measureText(text, paint.textProps.size);
    #endif
    
    // Ultimate fallback
    return Vec2(text.length() * paint.textProps.size * 0.5f, paint.textProps.size);
}

#ifdef USE_IMGUI

bool FontManager::loadFontForContext(const FontConfig& config, void* imguiContext)
{
    if (imguiContext == nullptr) return false;
    
    // Save current context
    ImGuiContext* prevContext = ImGui::GetCurrentContext();
    
    // Switch to target context
    ImGui::SetCurrentContext(static_cast<ImGuiContext*>(imguiContext));
    
    // Load font in this context
    auto igFont = std::make_unique<IGFont>();
    bool success = igFont->loadFromConfig(config);
    
    if (success)
    {
        m_contextFonts[imguiContext][config.id] = std::move(igFont);
        
        // Set as default if this is the first font for this context
        if (m_contextDefaultFonts.find(imguiContext) == m_contextDefaultFonts.end())
        {
            m_contextDefaultFonts[imguiContext] = config.id;
        }
    }
    
    // Restore previous context
    ImGui::SetCurrentContext(prevContext);
    
    return success;
}

IGFont* FontManager::getIGFontForContext(const std::string& id, void* imguiContext)
{
    if (imguiContext == nullptr) return nullptr;
    
    auto contextIt = m_contextFonts.find(imguiContext);
    if (contextIt != m_contextFonts.end())
    {
        auto fontIt = contextIt->second.find(id);
        if (fontIt != contextIt->second.end())
        {
            return fontIt->second.get();
        }
    }
    
    return nullptr;
}

IGFont* FontManager::getDefaultIGFontForContext(void* imguiContext)
{
    if (imguiContext == nullptr) return nullptr;
    
    auto defaultIt = m_contextDefaultFonts.find(imguiContext);
    if (defaultIt != m_contextDefaultFonts.end())
    {
        return getIGFontForContext(defaultIt->second, imguiContext);
    }
    
    return nullptr;
}

void FontManager::cleanupContextFonts(void* imguiContext)
{
    if (imguiContext == nullptr) return;
    
    m_contextFonts.erase(imguiContext);
    m_contextDefaultFonts.erase(imguiContext);
}

#endif // USE_IMGUI


#ifdef USE_GLES

GLFont* FontManager::getGLFont(const std::string& id)
{
    auto it = m_glFonts.find(id);
    return it != m_glFonts.end() ? it->second.get() : nullptr;
}

GLFont* FontManager::getDefaultGLFont() { return getGLFont(m_defaultFontId); }

#endif // USE_GLES

#ifdef USE_IMGUI

IGFont* FontManager::getIGFont(const std::string& id)
{
    auto it = m_igFonts.find(id);
    return it != m_igFonts.end() ? it->second.get() : nullptr;
}

IGFont* FontManager::getDefaultIGFont() { return getIGFont(m_defaultFontId); }

#endif // USE_IMGUI

// ============================================================================
// IGCanvas Implementation
// ============================================================================
#ifdef USE_IMGUI

IGCanvas::IGCanvas() 
    : m_drawList(nullptr), m_imguiContext(nullptr), m_screenWidth(1920), m_screenHeight(1080), m_initialized(false) , m_frameActive (false)
{
}

IGCanvas::~IGCanvas() { shutdown(); }

bool IGCanvas::initialize(int width, int height)
{
    if (m_initialized == true) return true;
    
    m_screenWidth = width;
    m_screenHeight = height;
    
    IMGUI_CHECKVERSION();
    m_imguiContext = ImGui::CreateContext();
    
    if (m_imguiContext == nullptr)
    {
        printf("IGCanvas: Failed to create ImGui context\n");
        return false;
    }
    
    ImGui::SetCurrentContext(static_cast<ImGuiContext*>(m_imguiContext));
    
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2(static_cast<float>(width), static_cast<float>(height));
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename = nullptr;
    ImGui::StyleColorsDark();
    
    if (ImGui_ImplOpenGL3_Init("#version 300 es") == false)
    {
        printf("IGCanvas: Failed to initialize ImGui OpenGL3 implementation\n");
        ImGui::DestroyContext(static_cast<ImGuiContext*>(m_imguiContext));
        m_imguiContext = nullptr;
        return false;
    }

    m_initialized = true;
    printf("IGCanvas: Initialized (context=%p, size=%dx%d)\n", m_imguiContext, width, height);
    return true;
}

void IGCanvas::shutdown()
{
    if (m_initialized == false) return;
    printf("IGCanvas: Shutting down (context=%p)\n", m_imguiContext);
    activateContext();
    
    if (m_frameActive == true)
    {
        ImGui::EndFrame();
        m_frameActive = false;
    }
    
    // Clean up context-specific fonts BEFORE destroying ImGui
    FontManager::getInstance().cleanupContextFonts(m_imguiContext);
    ImGui_ImplOpenGL3_DestroyFontsTexture();
    
    // Clean up ImGui fonts for this context
    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->Clear();
    
    ImGui_ImplOpenGL3_Shutdown();
    
    if (m_imguiContext != nullptr)
    {
        ImGui::DestroyContext(static_cast<ImGuiContext*>(m_imguiContext));
        m_imguiContext = nullptr;
    }
    
    m_initialized = false;
}

bool IGCanvas::isInitialized() const { return m_initialized; }

void IGCanvas::activateContext()
{
    if (m_imguiContext != nullptr) ImGui::SetCurrentContext(static_cast<ImGuiContext*>(m_imguiContext));
}

void IGCanvas::deactivateContext()
{
    ImGui::SetCurrentContext(nullptr);
}

void* IGCanvas::getCurrentContext() { return m_imguiContext; }

void IGCanvas::loadFontsForContext(const std::vector<FontConfig>& configs)
{
    if (m_imguiContext == nullptr || m_initialized == false)
    {
        std::cerr << "IGCanvas::loadFontsForContext: Canvas not initialized" << std::endl;
        return;
    }
    
    activateContext();
    
    for (const auto& config : configs)
    {
        if (FontManager::getInstance().loadFontForContext(config, m_imguiContext))
        {
            std::cout << "IGCanvas: Loaded font '" << config.name << "' for context " << m_imguiContext << std::endl;
        }
        else
        {
            std::cerr << "IGCanvas: Failed to load font '" << config.name << "' for context " << m_imguiContext << std::endl;
        }
    }
}

bool IGCanvas::setDefaultFont(const std::string& fontId)
{
    if (m_imguiContext == nullptr || m_initialized == false)
    {
        std::cerr << "IGCanvas::setDefaultFont: Canvas not initialized" << std::endl;
        return false;
    }
    
    activateContext();
    return FontManager::getInstance().setDefaultFontForContext(fontId, m_imguiContext);
}

void IGCanvas::begin()
{
    if (m_initialized == false) return;
    activateContext();
    if (m_frameActive == true)
    {
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        m_frameActive = false;
    }
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2(static_cast<float>(m_screenWidth), static_cast<float>(m_screenHeight));
    ImGui_ImplOpenGL3_NewFrame();
    ImGui::NewFrame();
    m_frameActive = true;
    m_drawList = ImGui::GetBackgroundDrawList();
}

void IGCanvas::end()
{
    if (m_initialized == false || m_frameActive == false) return;
    activateContext();
    m_drawList = nullptr;
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    m_frameActive = false;
    // deactivateContext();
}

void IGCanvas::setScreenSize(int width, int height)
{
    m_screenWidth = width;
    m_screenHeight = height;
    
    if (m_initialized == true && m_imguiContext != nullptr)
    {
        activateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.DisplaySize = ImVec2(static_cast<float>(width), static_cast<float>(height));
    }
}

void IGCanvas::drawRect(const RectF& rect, const Paint& paint)
{
    if (m_drawList == nullptr || m_frameActive == false || rect.width() <= 0.0f || rect.height() <= 0.0f) return;
    activateContext();
    ImDrawList* drawList = static_cast<ImDrawList*>(m_drawList);
    ImVec2 min(rect.left, rect.top);
    ImVec2 max(rect.right, rect.bottom);
    
    Color4 bgColor = paint.getEffectiveBgColor();
    Color4 fgColor = paint.getEffectiveFgColor();
    
    if (paint.filled == true && bgColor.a > 0.0f)
    {
        if (paint.cornerRadius > 0.0f) { drawList->AddRectFilled(min, max, bgColor.toImU32(), paint.cornerRadius); }
        else { drawList->AddRectFilled(min, max, bgColor.toImU32()); }
    }
    
    if (paint.strokeWidth > 0.0f && fgColor.a > 0.0f)
    {
        if (paint.cornerRadius > 0.0f) { drawList->AddRect(min, max, fgColor.toImU32(), paint.cornerRadius, 0, paint.strokeWidth); }
        else { drawList->AddRect(min, max, fgColor.toImU32(), 0, 0, paint.strokeWidth); }
    }
}

void IGCanvas::drawCircle(const Vec2& center, float radius, const Paint& paint)
{
    if (m_drawList == nullptr || m_frameActive == false) return;
    activateContext();
    ImDrawList* drawList = static_cast<ImDrawList*>(m_drawList);
    ImVec2 centerIm(center.x, center.y);
    Color4 bgColor = paint.getEffectiveBgColor();
    Color4 fgColor = paint.getEffectiveFgColor();
    
    if (paint.filled == true && bgColor.a > 0.0f) { drawList->AddCircleFilled(centerIm, radius, bgColor.toImU32()); }
    if (paint.strokeWidth > 0.0f && fgColor.a > 0.0f) { drawList->AddCircle(centerIm, radius, fgColor.toImU32(), 0, paint.strokeWidth); }
}

void IGCanvas::drawLine(const Vec2& start, const Vec2& end, const Paint& paint)
{
    if (m_drawList == nullptr || m_frameActive == false) return;
    activateContext();
    ImDrawList* drawList = static_cast<ImDrawList*>(m_drawList);
    Color4 color = paint.getEffectiveFgColor();
    if (color.a <= 0.0f) color = paint.getEffectiveBgColor();
    drawList->AddLine(start.toImGui(), end.toImGui(), color.toImU32(), paint.strokeWidth);
}

void IGCanvas::drawPolygon(const std::vector<Vec2>& points, const Paint& paint)
{
    if (m_drawList == nullptr || m_frameActive == false || points.size() < 3) return;
    activateContext();
    ImDrawList* drawList = static_cast<ImDrawList*>(m_drawList);

    std::vector<ImVec2> imPts;
    imPts.reserve(points.size());
    for (const Vec2& p : points) imPts.push_back(ImVec2(p.x, p.y));

    Color4 bgColor = paint.getEffectiveBgColor();
    Color4 fgColor = paint.getEffectiveFgColor();

    if (paint.filled == true && bgColor.a > 0.0f)
        drawList->AddConvexPolyFilled(imPts.data(), static_cast<int>(imPts.size()), bgColor.toImU32());

    if (paint.strokeWidth > 0.0f && fgColor.a > 0.0f)
        drawList->AddPolyline(imPts.data(), static_cast<int>(imPts.size()), fgColor.toImU32(), ImDrawFlags_Closed, paint.strokeWidth);
}

void IGCanvas::drawTexture(Texture* texture, const RectF& srcRect, const RectF& dstRect, const Paint& paint)
{
    if (m_drawList == nullptr || m_frameActive == false || texture == nullptr) return;
    activateContext();    
    if (texture->hasGLTexture() == false)
    {
        if (texture->hasImageData() == true)
        {
            IO::Platform::getInstance().makeGLContextCurrent();
            const_cast<Texture*>(texture)->uploadToGPU();
        }
    }
    
    if (texture->hasGLTexture() == false) return;

    ImDrawList* drawList = static_cast<ImDrawList*>(m_drawList);
    ImVec2 min(dstRect.left, dstRect.top);
    ImVec2 max(dstRect.right, dstRect.bottom);
    ImVec2 uvMin(srcRect.left / texture->width, srcRect.top / texture->height);
    ImVec2 uvMax(srcRect.right / texture->width, srcRect.bottom / texture->height);
    Color4 tintColor = paint.getEffectiveBgColor();
    
    if (paint.cornerRadius > 0.0f) { drawList->AddImageRounded((ImTextureID)(intptr_t)texture->id, min, max, uvMin, uvMax, tintColor.toImU32(), paint.cornerRadius, ImDrawFlags_RoundCornersAll); }
    else { drawList->AddImage((ImTextureID)(intptr_t)texture->id, min, max, uvMin, uvMax, tintColor.toImU32()); }
}

void IGCanvas::drawText(const std::string& text, const Vec2& position, const Paint& paint)
{
    if (m_drawList == nullptr || m_frameActive == false || text.empty() == true) return;
    activateContext();
    ImDrawList* drawList = static_cast<ImDrawList*>(m_drawList);
    Color4 textColor = paint.getEffectiveFgColor();
    if (textColor.a <= 0.0f) textColor = paint.getEffectiveBgColor();

    void* fontHandle = getImGuiFontFromPaint(paint);
    ImFont* imFont = static_cast<ImFont*>(fontHandle);
    
    if (imFont != nullptr)
    {
        float fontSize = paint.textProps.size;
        
        ImGuiIO& io = ImGui::GetIO();
        if (imFont->ContainerAtlas == io.Fonts)
        {
            drawList->AddText(imFont, fontSize, position.toImGui(), textColor.toImU32(), text.c_str());
        }
        else
        {
            // Fall back to default font for this context
            drawList->AddText(position.toImGui(), textColor.toImU32(), text.c_str());
        }
    }
    else 
    {
        drawList->AddText(position.toImGui(), textColor.toImU32(), text.c_str());
    }
}

Vec2 IGCanvas::measureText(const std::string& text, const Paint& paint)
{
    if (text.empty()) return Vec2(0.0f, 0.0f);
    
    activateContext();
    
    void* fontHandle = getImGuiFontFromPaint(paint);
    ImFont* imFont = static_cast<ImFont*>(fontHandle);
    
    if (imFont != nullptr)
    {
        float fontSize = paint.textProps.size;
        
        ImGuiIO& io = ImGui::GetIO();
        if (imFont->ContainerAtlas == io.Fonts)
        {
            ImVec2 size = imFont->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, text.c_str());
            return Vec2(size.x, size.y);
        }
    }
    
    ImFont* defaultFont = ImGui::GetFont();
    if (defaultFont != nullptr)
    {
        float fontSize = paint.textProps.size;
        ImVec2 size = defaultFont->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, text.c_str());
        return Vec2(size.x, size.y);
    }
    
    // Ultimate fallback
    return Vec2(text.length() * paint.textProps.size * 0.5f, paint.textProps.size);
}

void IGCanvas::resetForAppSwitch()
{
    if (m_imguiContext == nullptr || m_initialized == false)
    {
        return;
    }
    
    activateContext();
    
    // End any pending frame to ensure clean state
    if (m_frameActive == true)
    {
        ImGui::EndFrame();
        m_frameActive = false;
    }
    
    rebuildFontAtlas();
    
    ImGuiIO& io = ImGui::GetIO();
    io.ClearInputCharacters();
    memset(io.MouseDown, 0, sizeof(io.MouseDown));
    
    printf("IGCanvas: Reset for app switch (context=%p)\n", m_imguiContext);
}

void IGCanvas::reloadFonts(const std::vector<FontConfig>& configs)
{
    if (m_imguiContext == nullptr || m_initialized == false)
    {
        std::cerr << "IGCanvas::reloadFonts: Canvas not initialized" << std::endl;
        return;
    }
    
    activateContext();
    
    // Ensure we're not in a frame
    if (m_frameActive == true)
    {
        ImGui::EndFrame();
        m_frameActive = false;
    }
    
    // Clear existing fonts
    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->Clear();
    
    // Load each font config
    for (const auto& config : configs)
    {
        if (FontManager::getInstance().loadFontForContext(config, m_imguiContext) == true)
        {
            std::cout << "IGCanvas: Reloaded font '" << config.name << "' for context " << m_imguiContext << std::endl;
        }
        else
        {
            std::cerr << "IGCanvas: Failed to reload font '" << config.name << "' for context " << m_imguiContext << std::endl;
        }
    }
    
    // Rebuild the font atlas texture
    ImGui_ImplOpenGL3_DestroyFontsTexture();
    ImGui_ImplOpenGL3_CreateFontsTexture();
}

void IGCanvas::rebuildFontAtlas()
{
    ImGuiIO& io = ImGui::GetIO();
    
    // Clear all fonts from the atlas
    io.Fonts->Clear();
    
    // Add default font back
    io.Fonts->AddFontDefault();
    
    ImGui_ImplOpenGL3_DestroyFontsTexture();
    ImGui_ImplOpenGL3_CreateFontsTexture();
    
    printf("IGCanvas: Font atlas rebuilt with default font\n");
}

void IGCanvas::pushClip(const RectF& rect)
{
    if (m_drawList == nullptr || m_frameActive == false) return;
    activateContext();
    ImDrawList* drawList = static_cast<ImDrawList*>(m_drawList);
    ImVec2 min(rect.left, rect.top);
    ImVec2 max(rect.right, rect.bottom);
    drawList->PushClipRect(min, max, true);
}

void IGCanvas::popClip()
{
    if (m_drawList == nullptr || m_frameActive == false) return;
    activateContext();
    ImDrawList* drawList = static_cast<ImDrawList*>(m_drawList);
    drawList->PopClipRect();
}

bool IGCanvas::renderToFramebuffer(Framebuffer* fb, View* view)
{
    if (fb == nullptr || view == nullptr || fb->isValid() == false) return false;
    activateContext();    
    GLint previousFBO, viewport[4];
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &previousFBO);
    glGetIntegerv(GL_VIEWPORT, viewport);
    
    ImDrawList* tempDrawList = IM_NEW(ImDrawList)(ImGui::GetDrawListSharedData());
    tempDrawList->_ResetForNewFrame();
    tempDrawList->PushTextureID(ImGui::GetIO().Fonts->TexID);
    tempDrawList->PushClipRect(ImVec2(0.0f, 0.0f), ImVec2(static_cast<float>(fb->getTexture()->width), static_cast<float>(fb->getTexture()->height)), false);
    
    void* savedDrawList = m_drawList;
    m_drawList = tempDrawList;
    
    ViewGroup* viewGroup = dynamic_cast<ViewGroup*>(view);
    if (viewGroup != nullptr) { viewGroup->drawViewGroupContent(*this); }
    else { view->drawViewContent(*this); }
    
    tempDrawList->PopClipRect();
    tempDrawList->PopTextureID();
    m_drawList = savedDrawList;
    
    if (fb->bind() == true)
    {
        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        
        ImDrawData drawData;
        drawData.Valid = true;
        drawData.CmdListsCount = 1;
        drawData.CmdLists.resize(1);
        drawData.CmdLists[0] = tempDrawList;
        drawData.TotalVtxCount = tempDrawList->VtxBuffer.Size;
        drawData.TotalIdxCount = tempDrawList->IdxBuffer.Size;
        drawData.DisplayPos = ImVec2(0.0f, 0.0f);
        drawData.DisplaySize = ImVec2(static_cast<float>(fb->getTexture()->width), static_cast<float>(fb->getTexture()->height));
        drawData.FramebufferScale = ImVec2(1.0f, 1.0f);
        
        ImGui_ImplOpenGL3_RenderDrawData(&drawData);
        fb->unbind();
    }
    
    IM_DELETE(tempDrawList);
    glBindFramebuffer(GL_FRAMEBUFFER, previousFBO);
    glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
    
    return true;
}

void* IGCanvas::getImGuiFontFromPaint(const Paint& paint)
{
    activateContext();
    
    // First try to get font for THIS specific context
    IGFont* font = FontManager::getInstance().getIGFontForContext(
        paint.textProps.fontId, 
        m_imguiContext
    );
    
    if (font == nullptr)
    {
        // Try default font for this context
        font = FontManager::getInstance().getDefaultIGFontForContext(m_imguiContext);
    }
    
    if (font != nullptr)
    {
        void* nativeHandle = font->getNativeHandle();
        if (nativeHandle != nullptr) return nativeHandle;
    }
    
    // Ultimate fallback: use ImGui's built-in default font for this context
    return ImGui::GetFont();
}

#endif // USE_IMGUI

#ifdef USE_GLES
// ============================================================================
// GLCanvas Shaders
// ============================================================================

namespace
{
    const char* SHAPE_VERTEX_SHADER = R"(#version 300 es
        precision highp float;
        layout(location = 0) in vec2 a_position;
        layout(location = 1) in vec2 a_texcoord;
        layout(location = 2) in vec4 a_color;
        uniform mat4 u_projection;
        out vec2 v_texcoord;
        out vec4 v_color;

        void main()
        {
            gl_Position = u_projection * vec4(a_position, 0.0, 1.0);
            v_texcoord = a_texcoord;
            v_color = a_color;
        }
    )";
    
    const char* SHAPE_FRAGMENT_SHADER = R"(#version 300 es
        precision highp float;
        in vec2 v_texcoord;
        in vec4 v_color;
        uniform vec2 u_center;
        uniform vec2 u_size;
        uniform float u_cornerRadius;
        uniform float u_strokeWidth;
        uniform vec4 u_fillColor;
        uniform vec4 u_strokeColor;
        out vec4 fragColor;
        
        float sdRoundedBox(vec2 p, vec2 b, float r)
        {
            vec2 q = abs(p) - b + r;
            return min(max(q.x, q.y), 0.0) + length(max(q, 0.0)) - r;
        }
        
        void main()
        {
            vec2 halfSize = u_size * 0.5;
            vec2 localPos = v_texcoord - u_center;
            float dist = sdRoundedBox(localPos, halfSize, u_cornerRadius);
            float fillAlpha = 1.0 - smoothstep(-0.5, 0.5, dist);
            vec4 fillCol = u_fillColor * fillAlpha;
            vec4 finalColor = fillCol;
            
            if (u_strokeWidth > 0.0) {
                float strokeInner = -u_strokeWidth * 0.5;
                float strokeOuter = u_strokeWidth * 0.5;
                float strokeAlpha = smoothstep(strokeOuter + 0.5, strokeOuter - 0.5, abs(dist + strokeInner));
                vec4 strokeCol = u_strokeColor * strokeAlpha;
                finalColor = mix(fillCol, strokeCol, strokeAlpha);
            }
            fragColor = finalColor * v_color;
        }
    )";
    
    const char* TEXTURE_VERTEX_SHADER = R"(#version 300 es
        precision highp float;
        layout(location = 0) in vec2 a_position;
        layout(location = 1) in vec2 a_texcoord;
        layout(location = 2) in vec4 a_color;
        uniform mat4 u_projection;
        out vec2 v_texcoord;
        out vec2 v_position;
        out vec4 v_color;

        void main()
        {
            gl_Position = u_projection * vec4(a_position, 0.0, 1.0);
            v_texcoord = a_texcoord;
            v_position = a_position;
            v_color = a_color;
        }
    )";
    
    const char* TEXTURE_FRAGMENT_SHADER = R"(#version 300 es
        precision highp float;
        in vec2 v_texcoord;
        in vec2 v_position;
        in vec4 v_color;
        uniform sampler2D u_texture;
        uniform vec4 u_rect;
        uniform float u_cornerRadius;
        out vec4 fragColor;

        float sdRoundedBox(vec2 p, vec2 b, float r)
        {
            vec2 q = abs(p) - b + r;
            return min(max(q.x, q.y), 0.0) + length(max(q, 0.0)) - r;
        }

        void main()
        {
            vec4 texColor = texture(u_texture, v_texcoord);
            if (u_cornerRadius > 0.0) {
                vec2 center = vec2((u_rect.x + u_rect.z) * 0.5, (u_rect.y + u_rect.w) * 0.5);
                vec2 halfSize = vec2((u_rect.z - u_rect.x) * 0.5, (u_rect.w - u_rect.y) * 0.5);
                vec2 localPos = v_position - center;
                float dist = sdRoundedBox(localPos, halfSize, u_cornerRadius);
                float alpha = 1.0 - smoothstep(-0.5, 0.5, dist);
                fragColor = texColor * v_color * alpha;
            } else { fragColor = texColor * v_color; }
        }
    )";
    
    const char* TEXT_VERTEX_SHADER = R"(#version 300 es
        precision highp float;
        layout(location = 0) in vec2 a_position;
        layout(location = 1) in vec2 a_texcoord;
        layout(location = 2) in vec4 a_color;
        uniform mat4 u_projection;
        out vec2 v_texcoord;
        out vec4 v_color;

        void main()
        {
            gl_Position = u_projection * vec4(a_position, 0.0, 1.0);
            v_texcoord = a_texcoord;
            v_color = a_color;
        }
    )";
    
    const char* TEXT_FRAGMENT_SHADER = R"(#version 300 es
        precision highp float;
        in vec2 v_texcoord;
        in vec4 v_color;
        uniform sampler2D u_texture;
        out vec4 fragColor;

        void main()
        {
            float alpha = texture(u_texture, v_texcoord).a;
            fragColor = vec4(v_color.rgb, v_color.a * alpha);
        }
    )";

    // Flat shader: renders vertex color directly, no SDF, no texture.
    // Used by drawPolygon (triangle-fan fill) and drawLine (quad fill).
    const char* FLAT_VERTEX_SHADER = R"(#version 300 es
        precision highp float;
        layout(location = 0) in vec2 a_position;
        layout(location = 1) in vec2 a_texcoord;
        layout(location = 2) in vec4 a_color;
        uniform mat4 u_projection;
        out vec4 v_color;

        void main()
        {
            gl_Position = u_projection * vec4(a_position, 0.0, 1.0);
            v_color = a_color;
        }
    )";

    const char* FLAT_FRAGMENT_SHADER = R"(#version 300 es
        precision highp float;
        in vec4 v_color;
        out vec4 fragColor;

        void main()
        {
            fragColor = v_color;
        }
    )";
    
    uint32_t compileShader(uint32_t type, const char* source)
    {
        uint32_t shader = glCreateShader(type);
        glShaderSource(shader, 1, &source, nullptr);
        glCompileShader(shader);
        
        int success;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (success == GL_FALSE)
        {
            char infoLog[512];
            glGetShaderInfoLog(shader, 512, nullptr, infoLog);
            std::cerr << "Shader compilation error: " << infoLog << std::endl;
            glDeleteShader(shader);
            return 0;
        }
        return shader;
    }
    
    uint32_t linkProgram(uint32_t vertexShader, uint32_t fragmentShader)
    {
        uint32_t program = glCreateProgram();
        glAttachShader(program, vertexShader);
        glAttachShader(program, fragmentShader);
        glLinkProgram(program);
        
        int success;
        glGetProgramiv(program, GL_LINK_STATUS, &success);
        if (success == GL_FALSE)
        {
            char infoLog[512];
            glGetProgramInfoLog(program, 512, nullptr, infoLog);
            std::cerr << "Program linking error: " << infoLog << std::endl;
            glDeleteProgram(program);
            return 0;
        }
        return program;
    }
}

// ============================================================================
// GLCanvas Implementation
// ============================================================================

GLCanvas::GLCanvas()
    : m_screenWidth(1920), m_screenHeight(1080), m_initialized(false)
    , m_shapeShader(0), m_textureShader(0), m_textShader(0), m_flatShader(0), m_vbo(0), m_ibo(0)
    , m_shapeProjLoc(-1), m_shapeCenterLoc(-1), m_shapeSizeLoc(-1), m_shapeRadiusLoc(-1)
    , m_shapeStrokeWidthLoc(-1), m_shapeFillColorLoc(-1), m_shapeStrokeColorLoc(-1)
    , m_textureProjLoc(-1), m_textureTexLoc(-1), m_textureRectLoc(-1), m_textureRadiusLoc(-1)
    , m_textProjLoc(-1), m_textTexLoc(-1), m_flatProjLoc(-1)
    , m_batchVertexCount(0), m_batchIndexCount(0), m_currentTexture(0), m_currentShader(0)
{
}

GLCanvas::~GLCanvas() { shutdown(); }

bool GLCanvas::initialize(int width, int height)
{
    if (m_initialized == true) return true;
    
    m_screenWidth = width;
    m_screenHeight = height;
    
    if (createShaders() == false) return false;
    if (createBuffers() == false)
    {
        releaseShaders();
        return false;
    }
    
    m_initialized = true;
    return true;
}

void GLCanvas::shutdown()
{
    if (m_initialized == false) return;
    releaseBuffers();
    releaseShaders();
    m_initialized = false;
}

bool GLCanvas::isInitialized() const { return m_initialized; }

void GLCanvas::begin()
{
    if (m_initialized == false) return;
    
    // Reset batch state
    m_vertices.clear();
    m_indices.clear();
    m_batchVertexCount = 0;
    m_batchIndexCount = 0;
    m_currentTexture = 0;
    m_currentShader = 0;
    
    // Reset OpenGL state
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, m_screenWidth, m_screenHeight);
    glUseProgram(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindVertexArray(0);

    for (GLuint i = 0; i < 16; i++) glDisableVertexAttribArray(i);
    
    // Set rendering state
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_SCISSOR_TEST);
}

void GLCanvas::end()
{
    if (m_initialized == false) return;
    flushBatch();
}

void GLCanvas::setScreenSize(int width, int height)
{
    m_screenWidth = width;
    m_screenHeight = height;
}

void GLCanvas::drawRect(const RectF& rect, const Paint& paint)
{
    if (m_initialized == false || rect.width() <= 0.0f || rect.height() <= 0.0f) return;
    drawRectSDF(rect, paint);
}

void GLCanvas::drawCircle(const Vec2& center, float radius, const Paint& paint)
{
    if (m_initialized == false || radius <= 0.0f) return;
    drawCircleSDF(center, radius, paint);
}

void GLCanvas::drawLine(const Vec2& start, const Vec2& end, const Paint& paint)
{
    if (m_initialized == false) return;
    
    Vec2 delta = end - start;
    float length = delta.length();
    if (length <= 0.0f) return;

    Color4 color = paint.getEffectiveFgColor();
    if (color.a <= 0.0f) color = paint.getEffectiveBgColor();
    if (color.a <= 0.0f) return;

    // Switch to flat shader so the rotated-quad vertices are rendered correctly.
    if (m_currentShader != m_flatShader)
    {
        flushBatch();
        m_currentShader = m_flatShader;
        m_currentTexture = 0;
        glUseProgram(m_flatShader);

        float proj[16] = {
             2.0f / m_screenWidth,  0.0f,  0.0f, 0.0f,
             0.0f, -2.0f / m_screenHeight, 0.0f, 0.0f,
             0.0f,  0.0f, -1.0f, 0.0f,
            -1.0f,  1.0f,  0.0f, 1.0f
        };
        glUniformMatrix4fv(m_flatProjLoc, 1, GL_FALSE, proj);
    }

    // Build four corners of the oriented rectangle.
    Vec2 dir = delta.normalized();
    Vec2 normal(-dir.y, dir.x);
    float halfWidth = std::max(paint.strokeWidth, 1.0f) * 0.5f;

    Vec2 p0 = start - normal * halfWidth;  // start left
    Vec2 p1 = start + normal * halfWidth;  // start right
    Vec2 p2 = end   + normal * halfWidth;  // end   right
    Vec2 p3 = end   - normal * halfWidth;  // end   left

    ensureBatchCapacity(4, 6);
    uint16_t base = static_cast<uint16_t>(m_vertices.size());
    m_vertices.push_back({ p0.x, p0.y, 0.f, 0.f, color.r, color.g, color.b, color.a });
    m_vertices.push_back({ p1.x, p1.y, 0.f, 0.f, color.r, color.g, color.b, color.a });
    m_vertices.push_back({ p2.x, p2.y, 0.f, 0.f, color.r, color.g, color.b, color.a });
    m_vertices.push_back({ p3.x, p3.y, 0.f, 0.f, color.r, color.g, color.b, color.a });
    m_indices.push_back(base + 0); m_indices.push_back(base + 1); m_indices.push_back(base + 2);
    m_indices.push_back(base + 0); m_indices.push_back(base + 2); m_indices.push_back(base + 3);
}

void GLCanvas::drawPolygon(const std::vector<Vec2>& points, const Paint& paint)
{
    if (m_initialized == false || points.size() < 3) return;

    if (paint.filled == true)
    {
        Color4 fillColor = paint.getEffectiveBgColor();
        if (fillColor.a > 0.0f)
        {
            if (m_currentShader != m_flatShader)
            {
                flushBatch();
                m_currentShader = m_flatShader;
                m_currentTexture = 0;
                glUseProgram(m_flatShader);

                float proj[16] = {
                     2.0f / m_screenWidth,  0.0f,  0.0f, 0.0f,
                     0.0f, -2.0f / m_screenHeight, 0.0f, 0.0f,
                     0.0f,  0.0f, -1.0f, 0.0f,
                    -1.0f,  1.0f,  0.0f, 1.0f
                };
                glUniformMatrix4fv(m_flatProjLoc, 1, GL_FALSE, proj);
            }

            addPolygonFan(points, fillColor);
            flushBatch();
        }
    }

    if (paint.strokeWidth > 0.0f)
    {
        Color4 strokeColor = paint.getEffectiveFgColor();
        if (strokeColor.a > 0.0f)
        {
            Paint linePaint;
            linePaint.strokeWidth = paint.strokeWidth;
            linePaint.fgColor     = paint.fgColor;
            linePaint.opacity     = paint.opacity;
            linePaint.filled      = false;

            const size_t n = points.size();
            for (size_t i = 0; i < n; ++i)
                drawLine(points[i], points[(i + 1) % n], linePaint);
        }
    }
}

void GLCanvas::drawTexture(Texture* texture, const RectF& srcRect, const RectF& dstRect, const Paint& paint)
{
    if (m_initialized == false || texture == nullptr) return;
    
    if (texture->hasGLTexture() == false && texture->hasImageData() == true)
        const_cast<Texture*>(texture)->uploadToGPU();
    
    if (texture->hasGLTexture() == false) return;
    
    if (m_currentShader != m_textureShader || m_currentTexture != texture->id)
    {
        flushBatch();
        m_currentShader = m_textureShader;
        m_currentTexture = texture->id;
        
        glUseProgram(m_textureShader);
        
        float projMatrix[16] = {
            2.0f / m_screenWidth, 0.0f, 0.0f, 0.0f,
            0.0f, -2.0f / m_screenHeight, 0.0f, 0.0f,
            0.0f, 0.0f, -1.0f, 0.0f,
            -1.0f, 1.0f, 0.0f, 1.0f
        };
        
        glUniformMatrix4fv(m_textureProjLoc, 1, GL_FALSE, projMatrix);
        glUniform1i(m_textureTexLoc, 0);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture->id);
    }
    
    glUniform4f(m_textureRectLoc, dstRect.left, dstRect.top, dstRect.right, dstRect.bottom);
    glUniform1f(m_textureRadiusLoc, paint.cornerRadius);
    
    float u0 = srcRect.left / texture->width;
    float v0 = srcRect.top / texture->height;
    float u1 = srcRect.right / texture->width;
    float v1 = srcRect.bottom / texture->height;
    
    Color4 tintColor = paint.getEffectiveBgColor();
    addQuad(dstRect, tintColor, u0, v0, u1, v1);
    flushBatch();
}

void GLCanvas::drawText(const std::string& text, const Vec2& position, const Paint& paint)
{
    if (m_initialized == false || text.empty() == true) return;
    GLFont* glFont = getGLFontFromPaint(paint);
    if (glFont == nullptr) return;
    drawTextWithGLFont(text, position, paint, glFont);
}

Vec2 GLCanvas::measureText(const std::string& text, const Paint& paint)
{
    if (text.empty()) return Vec2(0.0f, 0.0f);
    
    GLFont* glFont = getGLFontFromPaint(paint);
    if (glFont != nullptr)
    {
        return glFont->measureText(text, paint.textProps.size);
    }
    
    // Fallback
    return Vec2(text.length() * paint.textProps.size * 0.5f, paint.textProps.size);
}

void GLCanvas::pushClip(const RectF& rect)
{
    if (m_initialized == false) return;
    flushBatch();
    m_clipStack.push_back(rect);
    glEnable(GL_SCISSOR_TEST);
    glScissor(static_cast<int>(rect.left), static_cast<int>(m_screenHeight - rect.bottom), 
              static_cast<int>(rect.width()), static_cast<int>(rect.height()));
}

void GLCanvas::popClip()
{
    if (m_initialized == false) return;
    flushBatch();
    
    if (m_clipStack.empty() == false) m_clipStack.pop_back();
    
    if (m_clipStack.empty() == true) { glDisable(GL_SCISSOR_TEST); }
    else
    {
        const RectF& rect = m_clipStack.back();
        glScissor(static_cast<int>(rect.left), static_cast<int>(m_screenHeight - rect.bottom),
                  static_cast<int>(rect.width()), static_cast<int>(rect.height()));
    }
}

bool GLCanvas::renderToFramebuffer(Framebuffer* fb, View* view)
{
    if (fb == nullptr || view == nullptr || fb->isValid() == false) return false;
    
    GLint previousFBO, viewport[4];
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &previousFBO);
    glGetIntegerv(GL_VIEWPORT, viewport);
    
    if (fb->bind() == false) return false;
    
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    
    int originalWidth = m_screenWidth;
    int originalHeight = m_screenHeight;
    m_screenWidth = fb->getTexture()->width;
    m_screenHeight = fb->getTexture()->height;
    
    m_vertices.clear();
    m_indices.clear();
    m_batchVertexCount = 0;
    m_batchIndexCount = 0;
    m_currentTexture = 0;
    m_currentShader = 0;
    
    glViewport(0, 0, fb->getTexture()->width, fb->getTexture()->height);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    
    ViewGroup* viewGroup = dynamic_cast<ViewGroup*>(view);
    if (viewGroup != nullptr) { viewGroup->drawViewGroupContent(*this); }
    else { view->drawViewContent(*this); }
    
    flushBatch();
    
    m_screenWidth = originalWidth;
    m_screenHeight = originalHeight;
    
    fb->unbind();
    glBindFramebuffer(GL_FRAMEBUFFER, previousFBO);
    glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
    
    resetState();
    return true;
}

void GLCanvas::resetState()
{
    if (m_initialized == false) return;

    flushBatch();
    m_currentShader  = 0;
    m_currentTexture = 0;

    if (m_clipStack.empty() == false)
    {
        m_clipStack.clear();
        glDisable(GL_SCISSOR_TEST);
    }

    glViewport(0, 0, m_screenWidth, m_screenHeight);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glUseProgram(0);
}

void GLCanvas::reloadFonts(const std::vector<FontConfig>& configs)
{
    if (m_initialized == false) return;

    FontManager& fm = FontManager::getInstance();
    fm.cleanup();
    fm.initialize();
    while (glGetError() != GL_NO_ERROR) {}

    bool foundDefault = false;
    for (const FontConfig& cfg : configs)
    {
        if (fm.loadFont(cfg) == false) { std::cerr << "GLCanvas: Failed to reload font: " << cfg.name << std::endl; continue; }
        if (foundDefault == false)     { fm.setDefaultFont(cfg.id); foundDefault = true; }
    }
}

// ============================================================================
// GLCanvas Private Helpers
// ============================================================================

bool GLCanvas::createShaders()
{
    uint32_t shapeVS = compileShader(GL_VERTEX_SHADER, SHAPE_VERTEX_SHADER);
    uint32_t shapeFS = compileShader(GL_FRAGMENT_SHADER, SHAPE_FRAGMENT_SHADER);
    if (shapeVS == 0 || shapeFS == 0) return false;
    
    m_shapeShader = linkProgram(shapeVS, shapeFS);
    glDeleteShader(shapeVS);
    glDeleteShader(shapeFS);
    if (m_shapeShader == 0) return false;
    
    m_shapeProjLoc = glGetUniformLocation(m_shapeShader, "u_projection");
    m_shapeCenterLoc = glGetUniformLocation(m_shapeShader, "u_center");
    m_shapeSizeLoc = glGetUniformLocation(m_shapeShader, "u_size");
    m_shapeRadiusLoc = glGetUniformLocation(m_shapeShader, "u_cornerRadius");
    m_shapeStrokeWidthLoc = glGetUniformLocation(m_shapeShader, "u_strokeWidth");
    m_shapeFillColorLoc = glGetUniformLocation(m_shapeShader, "u_fillColor");
    m_shapeStrokeColorLoc = glGetUniformLocation(m_shapeShader, "u_strokeColor");
    
    uint32_t textureVS = compileShader(GL_VERTEX_SHADER, TEXTURE_VERTEX_SHADER);
    uint32_t textureFS = compileShader(GL_FRAGMENT_SHADER, TEXTURE_FRAGMENT_SHADER);
    if (textureVS == 0 || textureFS == 0) return false;
    
    m_textureShader = linkProgram(textureVS, textureFS);
    glDeleteShader(textureVS);
    glDeleteShader(textureFS);
    if (m_textureShader == 0) return false;
    
    m_textureProjLoc = glGetUniformLocation(m_textureShader, "u_projection");
    m_textureTexLoc = glGetUniformLocation(m_textureShader, "u_texture");
    m_textureRectLoc = glGetUniformLocation(m_textureShader, "u_rect");
    m_textureRadiusLoc = glGetUniformLocation(m_textureShader, "u_cornerRadius");
    
    uint32_t textVS = compileShader(GL_VERTEX_SHADER, TEXT_VERTEX_SHADER);
    uint32_t textFS = compileShader(GL_FRAGMENT_SHADER, TEXT_FRAGMENT_SHADER);
    if (textVS == 0 || textFS == 0) return false;
    
    m_textShader = linkProgram(textVS, textFS);
    glDeleteShader(textVS);
    glDeleteShader(textFS);
    if (m_textShader == 0) return false;
    
    m_textProjLoc = glGetUniformLocation(m_textShader, "u_projection");
    m_textTexLoc = glGetUniformLocation(m_textShader, "u_texture");
    
    uint32_t flatVS = compileShader(GL_VERTEX_SHADER, FLAT_VERTEX_SHADER);
    uint32_t flatFS = compileShader(GL_FRAGMENT_SHADER, FLAT_FRAGMENT_SHADER);
    if (flatVS == 0 || flatFS == 0) return false;
    
    m_flatShader = linkProgram(flatVS, flatFS);
    glDeleteShader(flatVS);
    glDeleteShader(flatFS);
    if (m_flatShader == 0) return false;
    
    m_flatProjLoc = glGetUniformLocation(m_flatShader, "u_projection");
    
    return true;
}

bool GLCanvas::createBuffers()
{
    glGenBuffers(1, &m_vbo);
    glGenBuffers(1, &m_ibo);
    
    if (m_vbo == 0 || m_ibo == 0)
    {
        if (m_vbo != 0) glDeleteBuffers(1, &m_vbo);
        if (m_ibo != 0) glDeleteBuffers(1, &m_ibo);
        return false;
    }
    
    m_vertices.reserve(4096);
    m_indices.reserve(6144);
    return true;
}

void GLCanvas::releaseShaders()
{
    if (m_shapeShader != 0) { glDeleteProgram(m_shapeShader); m_shapeShader = 0; }
    if (m_textureShader != 0) { glDeleteProgram(m_textureShader); m_textureShader = 0; }
    if (m_textShader != 0) { glDeleteProgram(m_textShader); m_textShader = 0; }
    if (m_flatShader != 0) { glDeleteProgram(m_flatShader); m_flatShader = 0; }
}

void GLCanvas::releaseBuffers()
{
    if (m_vbo != 0) { glDeleteBuffers(1, &m_vbo); m_vbo = 0; }
    if (m_ibo != 0) { glDeleteBuffers(1, &m_ibo); m_ibo = 0; }
    m_vertices.clear();
    m_indices.clear();
}

void GLCanvas::flushBatch()
{
    if (m_vertices.empty() == true || m_indices.empty() == true) return;
    
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, m_vertices.size() * sizeof(Vertex), m_vertices.data(), GL_STREAM_DRAW);
    
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, m_indices.size() * sizeof(uint16_t), m_indices.data(), GL_STREAM_DRAW);
    
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);
    
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, x));
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, u));
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, r));
    
    glDrawElements(GL_TRIANGLES, m_indices.size(), GL_UNSIGNED_SHORT, nullptr);
    
    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);
    glDisableVertexAttribArray(2);
    
    m_vertices.clear();
    m_indices.clear();
    m_batchVertexCount = 0;
    m_batchIndexCount = 0;
}

void GLCanvas::ensureBatchCapacity(size_t vertexCount, size_t indexCount)
{
    if (m_vertices.size() + vertexCount > 65536 || m_indices.size() + indexCount > 65536) flushBatch();
}

void GLCanvas::addQuad(const RectF& rect, const Color4& color, const RectF& uv)
{
    addQuad(rect, color, uv.left, uv.top, uv.right, uv.bottom);
}

void GLCanvas::addQuad(const RectF& rect, const Color4& color, float u0, float v0, float u1, float v1)
{
    ensureBatchCapacity(4, 6);
    
    uint16_t baseVertex = static_cast<uint16_t>(m_vertices.size());
    
    m_vertices.push_back({ rect.left, rect.top, u0, v0, color.r, color.g, color.b, color.a });
    m_vertices.push_back({ rect.right, rect.top, u1, v0, color.r, color.g, color.b, color.a });
    m_vertices.push_back({ rect.right, rect.bottom, u1, v1, color.r, color.g, color.b, color.a });
    m_vertices.push_back({ rect.left, rect.bottom, u0, v1, color.r, color.g, color.b, color.a });
    
    m_indices.push_back(baseVertex + 0);
    m_indices.push_back(baseVertex + 1);
    m_indices.push_back(baseVertex + 2);
    m_indices.push_back(baseVertex + 0);
    m_indices.push_back(baseVertex + 2);
    m_indices.push_back(baseVertex + 3);
}

void GLCanvas::addPolygonFan(const std::vector<Vec2>& points, const Color4& color)
{
    // A polygon with N points produces N-2 triangles -> 3*(N-2) indices, N vertices
    const size_t n = points.size();
    if (n < 3) return;

    ensureBatchCapacity(n, 3 * (n - 2));

    uint16_t base = static_cast<uint16_t>(m_vertices.size());

    for (const Vec2& p : points)
        m_vertices.push_back({ p.x, p.y, 0.0f, 0.0f, color.r, color.g, color.b, color.a });

    for (size_t i = 1; i + 1 < n; ++i)
    {
        m_indices.push_back(base);
        m_indices.push_back(static_cast<uint16_t>(base + i));
        m_indices.push_back(static_cast<uint16_t>(base + i + 1));
    }
}

void GLCanvas::drawRectSDF(const RectF& rect, const Paint& paint)
{
    if (m_currentShader != m_shapeShader)
    {
        flushBatch();
        m_currentShader = m_shapeShader;
        m_currentTexture = 0;
        glUseProgram(m_shapeShader);
        
        float projMatrix[16] = {
            2.0f / m_screenWidth, 0.0f, 0.0f, 0.0f,
            0.0f, -2.0f / m_screenHeight, 0.0f, 0.0f,
            0.0f, 0.0f, -1.0f, 0.0f,
            -1.0f, 1.0f, 0.0f, 1.0f
        };
        glUniformMatrix4fv(m_shapeProjLoc, 1, GL_FALSE, projMatrix);
    }
    
    Vec2 center = rect.center();
    Vec2 size = rect.size();
    glUniform2f(m_shapeCenterLoc, center.x, center.y);
    glUniform2f(m_shapeSizeLoc, size.x, size.y);
    glUniform1f(m_shapeRadiusLoc, paint.cornerRadius);
    glUniform1f(m_shapeStrokeWidthLoc, paint.strokeWidth);
    
    Color4 fillColor = paint.getEffectiveBgColor();
    Color4 strokeColor = paint.getEffectiveFgColor();

    if (paint.filled == false) fillColor.a = 0.0f;
    glUniform4f(m_shapeFillColorLoc, fillColor.r, fillColor.g, fillColor.b, fillColor.a);
    glUniform4f(m_shapeStrokeColorLoc, strokeColor.r, strokeColor.g, strokeColor.b, strokeColor.a);
    
    float expand = std::max(paint.cornerRadius, paint.strokeWidth) + 2.0f;
    RectF expandedRect = rect.inset(-expand);
    addQuad(expandedRect, Color4(1, 1, 1, 1), expandedRect);
    flushBatch();
}

void GLCanvas::drawCircleSDF(const Vec2& center, float radius, const Paint& paint)
{
    if (m_currentShader != m_shapeShader)
    {
        flushBatch();
        m_currentShader = m_shapeShader;
        m_currentTexture = 0;
        glUseProgram(m_shapeShader);
        
        float projMatrix[16] = {
            2.0f / m_screenWidth, 0.0f, 0.0f, 0.0f,
            0.0f, -2.0f / m_screenHeight, 0.0f, 0.0f,
            0.0f, 0.0f, -1.0f, 0.0f,
            -1.0f, 1.0f, 0.0f, 1.0f
        };
        glUniformMatrix4fv(m_shapeProjLoc, 1, GL_FALSE, projMatrix);
    }
    
    glUniform2f(m_shapeCenterLoc, center.x, center.y);
    glUniform2f(m_shapeSizeLoc, radius * 2.0f, radius * 2.0f);
    glUniform1f(m_shapeRadiusLoc, radius);
    glUniform1f(m_shapeStrokeWidthLoc, paint.strokeWidth);
    
    Color4 fillColor = paint.getEffectiveBgColor();
    Color4 strokeColor = paint.getEffectiveFgColor();

    if (paint.filled == false) fillColor.a = 0.0f;

    glUniform4f(m_shapeFillColorLoc, fillColor.r, fillColor.g, fillColor.b, fillColor.a);
    glUniform4f(m_shapeStrokeColorLoc, strokeColor.r, strokeColor.g, strokeColor.b, strokeColor.a);
    
    float expand = radius + paint.strokeWidth + 2.0f;
    RectF bounds = RectF::fromXYWH(center.x - expand, center.y - expand, expand * 2.0f, expand * 2.0f);
    addQuad(bounds, Color4(1, 1, 1, 1), bounds);
    flushBatch();
}

void GLCanvas::drawTextWithGLFont(const std::string& text, const Vec2& position, const Paint& paint, GLFont* font)
{
    if (font == nullptr || font->getTexture() == nullptr) return;
    
    Texture* fontTexture = font->getTexture();
    if (fontTexture->hasGLTexture() == false) return;
    
    if (m_currentShader != m_textShader || m_currentTexture != fontTexture->id)
    {
        flushBatch();
        m_currentShader = m_textShader;
        m_currentTexture = fontTexture->id;
        
        glUseProgram(m_textShader);
        
        float projMatrix[16] = {
            2.0f / m_screenWidth, 0.0f, 0.0f, 0.0f,
            0.0f, -2.0f / m_screenHeight, 0.0f, 0.0f,
            0.0f, 0.0f, -1.0f, 0.0f,
            -1.0f, 1.0f, 0.0f, 1.0f
        };
        
        glUniformMatrix4fv(m_textProjLoc, 1, GL_FALSE, projMatrix);
        glUniform1i(m_textTexLoc, 0);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, fontTexture->id);
    }
    
    Color4 textColor = paint.getEffectiveFgColor();
    if (textColor.a <= 0.0f) textColor = paint.getEffectiveBgColor();
    
    float requestedSize = paint.textProps.size;
    float baseFontSize = font->getFontSize();
    float scale = requestedSize / baseFontSize;
    float x = position.x;

    float visualAscent = font->getAscent() * scale;
    float baselineOffset = requestedSize * 0.5f + visualAscent * 0.335f;
    float y = position.y + baselineOffset;

    // float y = position.y + font->getAscent() * scale;
    
    const char* str = text.c_str();
    while (*str != '\0')
    {
        if (*str == '\n')
        {
            x = position.x;
            y += font->getLineHeight() * scale;
            str++;
            continue;
        }
        
        uint32_t codepoint = 0;
        uint8_t c = static_cast<uint8_t>(*str);
        
        if (c < 0x80) { codepoint = c; str += 1; }
        else if ((c & 0xE0) == 0xC0) { codepoint = ((c & 0x1F) << 6) | (static_cast<uint8_t>(str[1]) & 0x3F); str += 2; }
        else if ((c & 0xF0) == 0xE0) { codepoint = ((c & 0x0F) << 12) | ((static_cast<uint8_t>(str[1]) & 0x3F) << 6) | (static_cast<uint8_t>(str[2]) & 0x3F); str += 3; }
        else if ((c & 0xF8) == 0xF0) { codepoint = ((c & 0x07) << 18) | ((static_cast<uint8_t>(str[1]) & 0x3F) << 12) | ((static_cast<uint8_t>(str[2]) & 0x3F) << 6) | (static_cast<uint8_t>(str[3]) & 0x3F); str += 4; }
        else { str += 1; continue; }
        
        const GlyphInfo* glyph = font->getGlyph(codepoint);
        if (glyph == nullptr)
        {
            font->addGlyphOnDemand(codepoint);
            glyph = font->getGlyph(codepoint);
        }
        if (glyph == nullptr) continue;
        
        float x0 = x + glyph->xoff * scale;
        float y0 = y + glyph->yoff * scale;
        float x1 = x0 + (glyph->x1 - glyph->x0) * fontTexture->width * scale;
        float y1 = y0 + (glyph->y1 - glyph->y0) * fontTexture->height * scale;
        
        RectF dstRect = RectF::fromLTRB(x0, y0, x1, y1);
        addQuad(dstRect, textColor, glyph->x0, glyph->y0, glyph->x1, glyph->y1);
        x += glyph->xadvance * scale;
    }
}

GLFont* GLCanvas::getGLFontFromPaint(const Paint& paint)
{
    GLFont* font = FontManager::getInstance().getGLFont(paint.textProps.fontId);
    if (font == nullptr) font = FontManager::getInstance().getDefaultGLFont();
    return font;
}

#endif // USE_GLES

// ============================================================================
// View Implementation
// ============================================================================

View::View()
    : m_parent(nullptr)
    , m_visibility(Visibility::VISIBLE)
    , m_focusable(false)
    , m_hasFocus(false)
    , m_isHovered(false)
    , m_isPressed(false)
    , m_isEnabled(true)
    , m_autoRegisterFocus(true)
    , m_focusManager(nullptr)
    , m_focusContext(nullptr)
    , m_onClickCallback(nullptr)
    , m_fbEnabled(false)
    , m_framebuffer(nullptr)
{
    m_normalPaint.bgColor = Color::Transparent;
    m_normalPaint.filled = true;
    m_normalPaint.cornerRadius = 0.0f;
    m_normalPaint.opacity = 1.0f;
    m_hoverPaint = m_normalPaint;
    m_focusPaint = m_normalPaint;
    m_pressedPaint = m_normalPaint;
    m_disabledPaint = m_normalPaint;
    m_disabledPaint.bgColor = Color::CardBackground.withOpacity(0.15f);
}

View::~View() {}

void View::onAttach()
{
    if (m_autoRegisterFocus == true && m_focusable == true && m_focusManager != nullptr)
    {
        m_focusManager->registerView(this, m_focusContext);
    }
}

void View::onDetach()
{
    if (m_autoRegisterFocus == true && m_focusManager != nullptr)
    {
        m_focusManager->unregisterView(this);
    }
}

// Lifecycle
void View::onMeasure(MeasureSpec widthSpec, MeasureSpec heightSpec)
{
    float width = 0.0f;
    float height = 0.0f;

    if (m_layoutParams.isExactWidth() == true)
    {
        width = m_layoutParams.width;
    }
    else if (m_layoutParams.isMatchParentWidth() == true)
    {
        if (widthSpec.mode == MeasureSpecMode::EXACTLY || widthSpec.mode == MeasureSpecMode::AT_MOST)
        {
            width = widthSpec.size;
        }
        else
        {
            width = 0.0f;
        }
    }
    else
    {
        width = 0.0f;
    }

    if (m_layoutParams.isExactHeight() == true)
    {
        height = m_layoutParams.height;
    }
    else if (m_layoutParams.isMatchParentHeight() == true)
    {
        if (heightSpec.mode == MeasureSpecMode::EXACTLY || heightSpec.mode == MeasureSpecMode::AT_MOST)
        {
            height = heightSpec.size;
        }
        else
        {
            height = 0.0f;
        }
    }
    else
    {
        height = 0.0f;
    }

    m_measuredSize = Vec2(width, height);
}

void View::onLayout(const RectF& bounds)
{
    m_bounds = bounds;

    if (m_fbEnabled == true)
    {
        int fbWidth = static_cast<int>(bounds.width());
        int fbHeight = static_cast<int>(bounds.height());
        updateFramebufferSize(fbWidth, fbHeight);
    }
}

void View::onDraw(ICanvas& canvas)
{
    if (isVisible() == false) return;

    // Render to framebuffer if enabled
    if (m_fbEnabled == true && m_framebuffer != nullptr && m_framebuffer->isValid() == true)
    {
        // Only render to framebuffer, don't draw to screen yet
        // The framebuffer texture will be used when needed
    }

    drawViewContent(canvas);
}

// Hierarchy
void View::setParent(ViewGroup* parent) { m_parent = parent; }
ViewGroup* View::getParent() const { return m_parent; }

// Layout
void View::setLayoutParams(const LayoutParams& params) { m_layoutParams = params; }
LayoutParams& View::getLayoutParams() { return m_layoutParams; }
const LayoutParams& View::getLayoutParams() const { return m_layoutParams; }
void View::setBounds(const RectF& bounds) { m_bounds = bounds; }
const RectF& View::getBounds() const { return m_bounds; }
Vec2 View::getMeasuredSize() const { return m_measuredSize; }
void View::setMeasuredSize(const Vec2& size) { m_measuredSize = size; }

// Visibility
void View::setVisibility(Visibility v) { m_visibility = v; }
Visibility View::getVisibility() const { return m_visibility; }
bool View::isVisible() const { return m_visibility == Visibility::VISIBLE; }

// Paint & appearance - Setters
void View::setNormalPaint(const Paint& paint) { m_normalPaint = paint; }
void View::setHoverPaint(const Paint& paint) { m_hoverPaint = paint; }
void View::setFocusedPaint(const Paint& paint) { m_focusPaint = paint; }
void View::setPressedPaint(const Paint& paint) { m_pressedPaint = paint; }
void View::setDisabledPaint(const Paint& paint) { m_disabledPaint = paint; }

// Paint & appearance - Getters
const Paint& View::getNormalPaint() const { return m_normalPaint; }
const Paint& View::getHoverPaint() const { return m_hoverPaint; }
const Paint& View::getFocusedPaint() const { return m_focusPaint; }
const Paint& View::getPressedPaint() const { return m_pressedPaint; }
const Paint& View::getDisabledPaint() const { return m_disabledPaint; }

// Paint & appearance - Color and style setters
void View::setBackgroundColor(const Color4& color) { m_normalPaint.bgColor = color; }

void View::setCornerRadius(float radius)
{
    m_normalPaint.cornerRadius = radius;
    m_hoverPaint.cornerRadius = radius;
    m_focusPaint.cornerRadius = radius;
    m_pressedPaint.cornerRadius = radius;
    m_disabledPaint.cornerRadius = radius;
}

void View::setOpacity(float opacity)
{
    m_normalPaint.opacity = opacity;
    m_hoverPaint.opacity = opacity;
    m_focusPaint.opacity = opacity;
    m_pressedPaint.opacity = opacity;
    m_disabledPaint.opacity = opacity;
}

void View::setNormalColor(const Color4& color) { m_normalPaint.bgColor = color; }
void View::setHoverColor(const Color4& color) { m_hoverPaint.bgColor = color; }
void View::setFocusedColor(const Color4& color) { m_focusPaint.bgColor = color; }
void View::setPressedColor(const Color4& color) { m_pressedPaint.bgColor = color; }
void View::setDisabledColor(const Color4& color) { m_disabledPaint.bgColor = color; }

void View::setBorderColor(const Color4& color, float width)
{
    m_normalPaint.fgColor = color;
    m_normalPaint.strokeWidth = width;
    m_hoverPaint.fgColor = color;
    m_hoverPaint.strokeWidth = width;
    m_focusPaint.fgColor = color;
    m_focusPaint.strokeWidth = width;
    m_pressedPaint.fgColor = color;
    m_pressedPaint.strokeWidth = width;
    m_disabledPaint.fgColor = color;
    m_disabledPaint.strokeWidth = width;
}

void View::setBorderWidth(float width)
{
    m_normalPaint.strokeWidth = width;
    m_hoverPaint.strokeWidth = width;
    m_focusPaint.strokeWidth = width;
    m_pressedPaint.strokeWidth = width;
    m_disabledPaint.strokeWidth = width;
}

// Paint & appearance - Getters for computed values
Color4 View::getBackgroundColor() const { return m_normalPaint.bgColor; }
float View::getCornerRadius() const { return m_normalPaint.cornerRadius; }
float View::getOpacity() const { return m_normalPaint.opacity; }

// State
bool View::isFocusable() const { return m_focusable; }
void View::setFocusable(bool focusable)
{
    if (m_focusable == focusable) return;
    
    bool wasAttached = (m_parent != nullptr);
    
    if (wasAttached == true && m_autoRegisterFocus == true && m_focusManager != nullptr)
    {
        if (m_focusable == true)
        {
            m_focusManager->unregisterView(this);
        }
    }
    
    m_focusable = focusable;
    
    if (wasAttached == true && m_autoRegisterFocus == true && m_focusManager != nullptr)
    {
        if (m_focusable == true)
        {
            m_focusManager->registerView(this, m_focusContext);
        }
    }
}
bool View::hasFocus() const { return m_hasFocus; }
void View::setHovered(bool hovered) { m_isHovered = hovered; }
bool View::isHovered() const { return m_isHovered; }
void View::setPressed(bool pressed) { m_isPressed = pressed; }
bool View::isPressed() const { return m_isPressed; }
void View::setEnabled(bool enabled) { m_isEnabled = enabled; }
bool View::isEnabled() const { return m_isEnabled; }

// Click handling
void View::setOnClickListener(ClickCallback callback) { m_onClickCallback = callback; }
bool View::hasClickListener() const { return m_onClickCallback != nullptr; }

void View::performClick()
{
    if (m_onClickCallback != nullptr)
    {
        m_onClickCallback(this);
    }
}

void View::setFocusManager(FocusManager* manager)
{
    if (m_focusManager == manager) return;
    
    if (m_focusManager != nullptr && m_autoRegisterFocus == true && m_focusable == true)
    {
        m_focusManager->unregisterView(this);
    }
    
    m_focusManager = manager;
    
    if (m_focusManager != nullptr && m_autoRegisterFocus == true && m_focusable == true && m_parent != nullptr)
    {
        m_focusManager->registerView(this, m_focusContext);
    }
}

FocusManager* View::getFocusManager() const
{
    if (m_focusManager != nullptr) return m_focusManager;
    
    ViewGroup* parent = m_parent;
    while (parent != nullptr)
    {
        FocusManager* parentManager = parent->getFocusManager();
        if (parentManager != nullptr) return parentManager;
        parent = parent->getParent();
    }
    
    return nullptr;
}

void View::setFocusContext(std::shared_ptr<FocusContext> context)
{
    if (m_focusContext == context) return;
    
    FocusManager* manager = getFocusManager();
    
    if (manager != nullptr && m_autoRegisterFocus == true && m_focusable == true && m_parent != nullptr)
    {
        manager->unregisterView(this);
    }
    
    m_focusContext = context;
    
    if (manager != nullptr && m_autoRegisterFocus == true && m_focusable == true && m_parent != nullptr)
    {
        manager->registerView(this, m_focusContext);
    }
}

// Framebuffer
void View::setFramebufferEnabled(bool enabled)
{
    if (m_fbEnabled == enabled) return;
    m_fbEnabled = enabled;

    if (enabled == true)
    {
        if (m_framebuffer == nullptr) m_framebuffer = std::make_unique<Framebuffer>();
        int fbWidth = static_cast<int>(m_bounds.width());
        int fbHeight = static_cast<int>(m_bounds.height());
        if (fbWidth > 0 && fbHeight > 0) m_framebuffer->create(fbWidth, fbHeight);
    }
    else
    {
        m_framebuffer.reset();
    }
}

bool View::isFramebufferEnabled() const { return m_fbEnabled; }

Texture* View::getFramebufferTexture()
{
    if (m_fbEnabled == false || m_framebuffer == nullptr) return nullptr;
    return m_framebuffer->getTexture();
}

uint32_t View::getFramebufferTextureId()
{
    if (m_fbEnabled == false || m_framebuffer == nullptr) return 0;
    Texture* fbTexture = m_framebuffer->getTexture();
    if (fbTexture == nullptr) return 0;
    return fbTexture->id;
}

// Drawing utilities (public for framebuffer rendering)
void View::drawViewContent(ICanvas& canvas)
{
    drawBackground(canvas);
    drawFocusBorder(canvas);
}

// Protected methods
const Paint& View::getCurrentPaint() const
{
    if (m_isEnabled == false) return m_disabledPaint;
    if (m_isPressed == true) return m_pressedPaint;
    if (m_hasFocus == true) return m_focusPaint;
    if (m_isHovered == true) return m_hoverPaint;
    return m_normalPaint;
}

void View::drawBackground(ICanvas& canvas)
{
    const Paint& paint = getCurrentPaint();
    Color4 bgColor = paint.getEffectiveBgColor();
    Color4 fgColor = paint.getEffectiveFgColor();

    if (bgColor.a > 0.0f || (paint.strokeWidth > 0.0f && fgColor.a > 0.0f))
    {
        canvas.drawRect(m_bounds, paint);
    }
}

void View::drawFocusBorder(ICanvas& canvas)
{
    if (m_hasFocus == true)
    {
        Paint focusBorderPaint;
        focusBorderPaint.bgColor = Color::Transparent;
        focusBorderPaint.fgColor = Color::BorderFocus;
        focusBorderPaint.filled = false;
        focusBorderPaint.strokeWidth = 5.0f;
        focusBorderPaint.cornerRadius = m_normalPaint.cornerRadius;
        focusBorderPaint.opacity = m_normalPaint.opacity;

        RectF focusRect = m_bounds.inset(0.0f);
        canvas.drawRect(focusRect, focusBorderPaint);
    }
}

RectF View::getContentBounds() const
{
    return RectF::fromLTRB(
        m_bounds.left + m_layoutParams.paddingLeft,
        m_bounds.top + m_layoutParams.paddingTop,
        m_bounds.right - m_layoutParams.paddingRight,
        m_bounds.bottom - m_layoutParams.paddingBottom
    );
}

void View::updateFramebufferSize(int width, int height)
{
    if (m_fbEnabled == false || m_framebuffer == nullptr) return;
    if (width <= 0 || height <= 0) return;

    Texture *fbTexture = m_framebuffer->getTexture();

    if (fbTexture == nullptr || fbTexture->width != width || fbTexture->height != height)
    {
        m_framebuffer->create(width, height);
    }
}

void View::applyGravity(const Vec2& childSize, const RectF& parentBounds, Gravity gravity, const LayoutParams& lp, RectF& outBounds)
{
    float width = childSize.x;
    float height = childSize.y;
    float left = parentBounds.left;
    float top = parentBounds.top;
    
    RectF availableBounds = RectF::fromLTRB(
        parentBounds.left + lp.marginLeft,
        parentBounds.top + lp.marginTop,
        parentBounds.right - lp.marginRight,
        parentBounds.bottom - lp.marginBottom
    );

    if (hasGravity(gravity, Gravity::CENTER_HORIZONTAL) == true)
    {
        left = availableBounds.left + (availableBounds.width() - width) * 0.5f;
    }
    else if (hasGravity(gravity, Gravity::RIGHT) == true)
    {
        left = availableBounds.right - width;
    }
    else if (hasGravity(gravity, Gravity::LEFT) == true)
    {
        left = availableBounds.left;
    }

    if (hasGravity(gravity, Gravity::CENTER_VERTICAL) == true)
    {
        top = availableBounds.top + (availableBounds.height() - height) * 0.5f;
    }
    else if (hasGravity(gravity, Gravity::BOTTOM) == true)
    {
        top = availableBounds.bottom - height;
    }
    else if (hasGravity(gravity, Gravity::TOP) == true)
    {
        top = availableBounds.top;
    }

    outBounds = RectF::fromLTRB(left, top, left + width, top + height);
}

// ============================================================================
// ViewGroup Implementation
// ============================================================================

ViewGroup::ViewGroup() {}

ViewGroup::~ViewGroup()
{
    removeAllViews();
}

// Child management
void ViewGroup::addView(View* child)
{
    if (child == nullptr) return;
    
    child->setParent(this);
    
    FocusManager* manager = getFocusManager();
    if (manager != nullptr && child->getFocusManager() == nullptr)
    {
        child->setFocusManager(manager);
    }
    
    if (m_focusContext != nullptr && child->getFocusContext() == nullptr)
    {
        child->setFocusContext(m_focusContext);
    }
    
    m_children.push_back(child);
    child->onAttach();
}

void ViewGroup::addViewAt(View* child, size_t index)
{
    if (child == nullptr) return;

    child->setParent(this);

    FocusManager* manager = getFocusManager();
    if (manager != nullptr && child->getFocusManager() == nullptr)
        child->setFocusManager(manager);

    if (m_focusContext != nullptr && child->getFocusContext() == nullptr)
        child->setFocusContext(m_focusContext);

    if (index >= m_children.size())
        m_children.push_back(child);
    else
        m_children.insert(m_children.begin() + static_cast<ptrdiff_t>(index), child);

    child->onAttach();
}

void ViewGroup::removeViewNoDelete(View* child)
{
    if (child == nullptr) return;

    for (auto it = m_children.begin(); it != m_children.end(); ++it)
    {
        if (*it == child)
        {
            child->onDetach();
            child->setParent(nullptr);
            m_children.erase(it);
            return;
        }
    }
}

void ViewGroup::removeView(View* child)
{
    if (child == nullptr) return;

    for (auto it = m_children.begin(); it != m_children.end(); ++it)
    {
        if (*it == child)
        {
            child->onDetach();
            child->setParent(nullptr);
            m_children.erase(it);
            delete child;
            return;
        }
    }
}

void ViewGroup::removeAllViews()
{
    for (View *child : m_children)
    {
        child->onDetach();
        child->setParent(nullptr);
        delete child;
    }
    m_children.clear();
}

size_t ViewGroup::getChildCount() const
{
    return m_children.size();
}

View* ViewGroup::getChildAt(size_t index) const
{
    return (index < m_children.size()) ? m_children[index] : nullptr;
}

// Overrides
void ViewGroup::onMeasure(MeasureSpec widthSpec, MeasureSpec heightSpec)
{
    float availableWidth = widthSpec.size - m_layoutParams.getPaddingHorizontal();
    float availableHeight = heightSpec.size - m_layoutParams.getPaddingVertical();
    availableWidth = std::max(0.0f, availableWidth);
    availableHeight = std::max(0.0f, availableHeight);
    
    MeasureSpec childWidthSpec = MeasureSpec::makeAtMost(availableWidth);
    MeasureSpec childHeightSpec = MeasureSpec::makeAtMost(availableHeight);
    measureChildren(childWidthSpec, childHeightSpec);
    
    float width = 0.0f;
    float height = 0.0f;

    if (m_layoutParams.isExactWidth() == true)
    {
        width = m_layoutParams.width;
    }
    else if (m_layoutParams.isMatchParentWidth() == true)
    {
        if (widthSpec.mode == MeasureSpecMode::EXACTLY || widthSpec.mode == MeasureSpecMode::AT_MOST)
        {
            width = widthSpec.size;
        }
    }
    else
    {
        width = 0.0f;
    }

    if (m_layoutParams.isExactHeight() == true)
    {
        height = m_layoutParams.height;
    }
    else if (m_layoutParams.isMatchParentHeight() == true)
    {
        if (heightSpec.mode == MeasureSpecMode::EXACTLY || heightSpec.mode == MeasureSpecMode::AT_MOST)
        {
            height = heightSpec.size;
        }
    }
    else
    {
        height = 0.0f;
    }

    m_measuredSize = Vec2(width, height);
}

void ViewGroup::onLayout(const RectF& bounds)
{
    View::onLayout(bounds);
    layoutChildren();
}

void ViewGroup::onDraw(ICanvas& canvas)
{
    if (isVisible() == false) return;

    if (m_fbEnabled == true && m_framebuffer != nullptr && m_framebuffer->isValid() == true)
    {
        renderToFramebuffer(canvas);
    }

    drawViewGroupContent(canvas);
}

bool ViewGroup::onKeyEvent(const IO::KeyEvent& event)
{
    for (auto it = m_children.rbegin(); it != m_children.rend(); ++it)
    {
        if ((*it)->isVisible() == true && (*it)->onKeyEvent(event) == true)
        {
            return true;
        }
    }
    return View::onKeyEvent(event);
}

void ViewGroup::setFocusManager(FocusManager* manager)
{
    View::setFocusManager(manager);
    
    for (View* child : m_children)
    {
        if (child != nullptr)
        {
            child->setFocusManager(manager);
        }
    }
}

void ViewGroup::setFocusContext(std::shared_ptr<FocusContext> context)
{
    View::setFocusContext(context);
    
    for (View* child : m_children)
    {
        if (child != nullptr)
        {
            if (child->getFocusContext() == nullptr)
            {
                child->setFocusContext(context);
            }
        }
    }
}

View* ViewGroup::findTopChildAt(float x, float y) const
{
    for (auto it = m_children.rbegin(); it != m_children.rend(); ++it)
    {
        View* child = *it;
        if (child != nullptr && child->isVisible() == true)
        {
            if (child->getBounds().contains(x, y) == true)
            {
                return child;
            }
        }
    }
    
    return nullptr;
}

bool ViewGroup::shouldConsumeEvent(const IO::MotionEvent& event) const
{
    return false;
}

bool ViewGroup::onMotionEvent(const IO::MotionEvent& event)
{
    for (auto it = m_children.rbegin(); it != m_children.rend(); ++it)
    {
        View *child = *it;
        if (child->isVisible() == false)
            continue;

        const RectF &bounds = child->getBounds();
        if (bounds.contains(event.x, event.y) == true)
        {
            if (child->onMotionEvent(event) == true)
            {
                return true;
            }
        }
    }
    return View::onMotionEvent(event);
}

// Drawing utilities (public for framebuffer rendering)
void ViewGroup::drawViewGroupContent(ICanvas& canvas)
{
    drawBackground(canvas);

    for (View *child : m_children)
    {
        if (child->isVisible() == true)
        {
            // #ifdef USE_GLES
            //     GLCanvas *glCanvas = dynamic_cast<GLCanvas *>(&canvas);
            //     if (glCanvas != nullptr) glCanvas->resetState();
            // #endif
            child->onDraw(canvas);
        }
    }

    drawFocusBorder(canvas);
}

// Protected methods
void ViewGroup::measureChildren(MeasureSpec widthSpec, MeasureSpec heightSpec)
{
    for (View *child : m_children)
    {
        if (child->getVisibility() == Visibility::GONE) continue;

        const LayoutParams &lp = child->getLayoutParams();
        MeasureSpec childWidthSpec = getChildMeasureSpec(widthSpec, lp, true);
        MeasureSpec childHeightSpec = getChildMeasureSpec(heightSpec, lp, false);
        child->onMeasure(childWidthSpec, childHeightSpec);
    }
}

void ViewGroup::renderToFramebuffer(ICanvas& canvas)
{
    if (m_framebuffer == nullptr || m_framebuffer->isValid() == false) return;
    if (m_bounds.width() <= 0.0f || m_bounds.height() <= 0.0f) return;
    Texture* fbTexture = m_framebuffer->getTexture();
    if (fbTexture == nullptr) return;

    // Save original bounds
    RectF originalBounds = m_bounds;
    std::vector<RectF> childOriginalBounds;
    for (View *child : m_children)
    {
        if (child->isVisible() == true)
        {
            childOriginalBounds.push_back(child->getBounds());
        }
    }

    // Translate to framebuffer space
    Vec2 offset(-m_bounds.left, -m_bounds.top);
    m_bounds = RectF::fromXYWH(0, 0, fbTexture->width, fbTexture->height);

    size_t childIndex = 0;
    for (View *child : m_children)
    {
        if (child->isVisible() == true)
        {
            RectF childBounds = childOriginalBounds[childIndex];
            child->setBounds(childBounds.offset(offset.x, offset.y));
            childIndex++;
        }
    }

    // Render to framebuffer using the canvas's method
    canvas.renderToFramebuffer(m_framebuffer.get(), this);

    // Restore bounds
    m_bounds = originalBounds;
    childIndex = 0;
    for (View *child : m_children)
    {
        if (child->isVisible() == true)
        {
            child->setBounds(childOriginalBounds[childIndex]);
            childIndex++;
        }
    }
}

MeasureSpec ViewGroup::getChildMeasureSpec(MeasureSpec parentSpec, const LayoutParams& lp, bool isWidth)
{
    float parentSize = parentSpec.size;
    MeasureSpecMode parentMode = parentSpec.mode;
    float margin = isWidth ? lp.getMarginHorizontal() : lp.getMarginVertical();
    int childSize = isWidth ? lp.width : lp.height;
    float availableSize = std::max(0.0f, parentSize - margin);

    MeasureSpec resultSpec;

    if (childSize >= 0)
    {
        resultSpec = MeasureSpec::makeExactly(childSize);
    }
    else if (childSize == MATCH_PARENT)
    {
        if (parentMode == MeasureSpecMode::EXACTLY || parentMode == MeasureSpecMode::AT_MOST)
        {
            resultSpec = MeasureSpec::makeExactly(availableSize);
        }
        else
        {
            resultSpec = MeasureSpec::makeUnspecified();
        }
    }
    else
    {
        if (parentMode == MeasureSpecMode::EXACTLY || parentMode == MeasureSpecMode::AT_MOST)
        {
            resultSpec = MeasureSpec::makeAtMost(availableSize);
        }
        else
        {
            resultSpec = MeasureSpec::makeUnspecified();
        }
    }

    return resultSpec;
}

float ViewGroup::getAvailableWidth() const
{
    return m_bounds.width() - m_layoutParams.getPaddingHorizontal();
}

float ViewGroup::getAvailableHeight() const
{
    return m_bounds.height() - m_layoutParams.getPaddingVertical();
}

RectF ViewGroup::getChildLayoutBounds() const
{
    return RectF::fromLTRB(
        m_bounds.left + m_layoutParams.paddingLeft,
        m_bounds.top + m_layoutParams.paddingTop,
        m_bounds.right - m_layoutParams.paddingRight,
        m_bounds.bottom - m_layoutParams.paddingBottom
    );
}

} // namespace UI

} // namespace APP
