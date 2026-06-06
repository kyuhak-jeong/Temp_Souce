#ifndef UI_CORE_H
#define UI_CORE_H

#include "constants.h"
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <functional>
#include <mutex>
#include <thread>

#include <GLES3/gl3.h>

namespace APP
{

namespace UI
{

// Forward declarations
class View;
class ViewGroup;
class ICanvas;
class FocusManager;
class FocusContext;

// ============================================================================
// Texture - Image data and GPU texture management
// ============================================================================

struct Texture
{
    uint32_t id;
    int width, height;
    std::vector<uint8_t> imageData;
    bool isExtTexture;
    
    Texture();
    ~Texture();
    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;
    Texture(Texture&& other) noexcept;
    Texture& operator=(Texture&& other) noexcept;
    
    bool isValid() const;
    bool hasGLTexture() const;
    bool hasImageData() const;
    bool loadFromMemory(const uint8_t* data, int w, int h, int channels);
    bool uploadToGPU();
    bool updateGPU(const uint8_t* data, int w, int h);
    void setExternalTexture(uint32_t textureId, int w, int h);
    void releaseGPU();
};

struct TextureInfo
{
    std::string name, id;
    Texture* texture;
    TextureInfo();
};

// ============================================================================
// TextureManager - Singleton for texture management
// ============================================================================

class TextureManager
{
public:
    static TextureManager& getInstance();
    
    Texture* loadTexture(const std::string& path);
    Texture* loadTextureFromMemory(const uint8_t* data, int width, int height, int channels);
    void releaseTexture(const std::string& path);
    Texture* getTexture(const std::string& path);
    
    bool loadTextureCollection(const std::string& directory, const std::string& manifestFile = "textures.txt", bool forceReload = false);
    void releaseTextureCollection(const std::string& collectionName);
    
    const TextureInfo* getTextureInfo(const std::string& id) const;
    const TextureInfo* getTextureInfoByName(const std::string& name) const;
    Texture* getTextureById(const std::string& id);
    Texture* getTextureByName(const std::string& name);
    
    bool hasTexture(const std::string& id) const;
    bool hasTextureName(const std::string& name) const;
    Vec2 getTextureSize(const std::string& id) const;
    Vec2 getTextureSizeByName(const std::string& name) const;
    
    std::vector<std::string> getTextureIds() const;
    std::vector<std::string> getTextureNames() const;
    std::vector<TextureInfo> getAllTextureInfo() const;
    
    void clearAll();
    
private:
    TextureManager();
    ~TextureManager();
    TextureManager(const TextureManager&) = delete;
    TextureManager& operator=(const TextureManager&) = delete;
    
    std::string generateTextureId(const std::string& name, const std::string& collection = "");
    
    std::unordered_map<std::string, Texture> m_textures;
    std::unordered_map<std::string, TextureInfo> m_textureInfoById;
    std::unordered_map<std::string, std::string> m_nameToId;
    std::unordered_map<std::string, std::vector<std::string>> m_collections;
};

// ============================================================================
// Framebuffer - Offscreen rendering target
// ============================================================================

struct Framebuffer
{
    uint32_t fbo;
    uint32_t depthRbo;
    std::unique_ptr<Texture> texture;
    int m_savedViewport[4];
    
    Framebuffer();
    ~Framebuffer();
    Framebuffer(const Framebuffer&) = delete;
    Framebuffer& operator=(const Framebuffer&) = delete;
    Framebuffer(Framebuffer&& other) noexcept;
    Framebuffer& operator=(Framebuffer&& other) noexcept;
    
    bool create(int w, int h);
    void release();
    bool bind();
    void unbind();
    bool isValid() const;
    Texture* getTexture() const;
};

// ============================================================================
// Font - Glyph information and rendering
// ============================================================================

struct GlyphInfo
{
    uint32_t codepoint;
    float x0, y0, x1, y1;
    float xoff, yoff, xadvance;
};

#ifdef USE_GLES

class GLFont
{
public:
    GLFont();
    ~GLFont();
    
    bool loadFromConfig(const FontConfig& config);
    void cleanup();
    
    Texture* getTexture();
    const Texture* getTexture() const;
    float getFontSize() const;
    float getLineHeight() const;
    float getAscent() const;
    float getDescent() const;
    
    Vec2 measureText(const std::string& text, float fontSize) const;
    const GlyphInfo* getGlyph(uint32_t codepoint) const;
    bool hasGlyph(uint32_t codepoint) const;
    void addGlyphOnDemand(uint32_t codepoint);
    
private:
    std::vector<CodepointRange> getCharacterSetRanges(CharacterSet charset) const;
    std::vector<uint32_t> collectCodepoints(const std::vector<CodepointRange>& ranges) const;
    bool buildAtlas(const uint8_t* ttfData, size_t ttfSize, float fontSize, const std::vector<CodepointRange>& ranges, int atlasWidth, int atlasHeight);
    uint32_t utf8ToCodepoint(const char*& str) const;
    
    Texture m_atlasTexture;
    std::unordered_map<uint32_t, GlyphInfo> m_glyphs;
    std::vector<uint8_t> m_ttfData;
    float m_fontSize, m_lineHeight, m_ascent, m_descent;
    int m_atlasWidth, m_atlasHeight, m_currentX, m_currentY, m_rowHeight;
};

#endif // USE_GLES

#ifdef USE_IMGUI

class IGFont
{
public:
    IGFont();
    ~IGFont();
    
    bool loadFromConfig(const FontConfig& config);
    void setNativeFont(void* imFont, float fontSize);
    
    float getFontSize() const;
    float getLineHeight() const;
    float getAscent() const;
    float getDescent() const;
    
    Vec2 measureText(const std::string& text, float fontSize) const;
    void* getNativeHandle();
    const void* getNativeHandle() const;
    
private:
    void* m_imFont;
    float m_fontSize;
    bool m_ownsFont;
};

#endif // USE_IMGUI

// ============================================================================
// FontManager - Singleton for font management
// ============================================================================

class FontManager
{
public:
    static FontManager& getInstance();
    
    bool initialize();
    void cleanup();
    bool loadFont(const FontConfig& config);
    void setDefaultFont(const std::string& id);
    Vec2 measureText(const std::string& text, const Paint& paint);
    
    #ifdef USE_GLES
        GLFont* getGLFont(const std::string& id);
        GLFont* getDefaultGLFont();
    #endif
    
    #ifdef USE_IMGUI
        IGFont* getIGFont(const std::string& id);
        IGFont* getDefaultIGFont();
        
        // Context-aware font loading for multi-context support
        bool loadFontForContext(const FontConfig& config, void* imguiContext);
        IGFont* getIGFontForContext(const std::string& id, void* imguiContext);
        IGFont* getDefaultIGFontForContext(void* imguiContext);
        void cleanupContextFonts(void* imguiContext);
        bool setDefaultFontForContext(const std::string& id, void* imguiContext);
    #endif
    
    const std::string& getDefaultFontId() const { return m_defaultFontId; }
    
private:
    FontManager();
    ~FontManager();
    FontManager(const FontManager&) = delete;
    FontManager& operator=(const FontManager&) = delete;
    
    #ifdef USE_GLES
        std::unordered_map<std::string, std::unique_ptr<GLFont>> m_glFonts;
    #endif
    #ifdef USE_IMGUI
        std::unordered_map<std::string, std::unique_ptr<IGFont>> m_igFonts;
        // Map: context -> (fontId -> IGFont)
        std::unordered_map<void*, std::unordered_map<std::string, std::unique_ptr<IGFont>>> m_contextFonts;
        std::unordered_map<void*, std::string> m_contextDefaultFonts;
    #endif
    std::string m_defaultFontId;
    bool m_initialized;
};

// ============================================================================
// ICanvas - Abstract rendering interface
// ============================================================================

class ICanvas
{
public:
    virtual ~ICanvas() = default;
    
    virtual bool initialize(int width, int height) = 0;
    virtual void shutdown() = 0;
    virtual bool isInitialized() const = 0;
    
    virtual void begin() = 0;
    virtual void end() = 0;
    virtual void setScreenSize(int width, int height) = 0;
    
    virtual void drawRect(const RectF& rect, const Paint& paint) = 0;
    virtual void drawCircle(const Vec2& center, float radius, const Paint& paint) = 0;
    virtual void drawLine(const Vec2& start, const Vec2& end, const Paint& paint) = 0;
    virtual void drawPolygon(const std::vector<Vec2>& points, const Paint& paint) = 0;
    virtual void drawTexture(Texture* texture, const RectF& srcRect, const RectF& dstRect, const Paint& paint) = 0;
    virtual void drawText(const std::string& text, const Vec2& position, const Paint& paint) = 0;
    virtual Vec2 measureText(const std::string& text, const Paint& paint) = 0;
    
    virtual void pushClip(const RectF& rect) = 0;
    virtual void popClip() = 0;
    
    virtual bool renderToFramebuffer(Framebuffer* fb, View* view) = 0;
    
    virtual void  resetForAppSwitch()                                        {}
    virtual void  reloadFonts(const std::vector<FontConfig>& /*configs*/)    {}
    virtual void* getCurrentContext()                                        { return nullptr; }
};

// ============================================================================
// IGCanvas - ImGui-based canvas implementation
// ============================================================================

#ifdef USE_IMGUI

class IGCanvas : public ICanvas
{
public:
    IGCanvas();
    ~IGCanvas() override;
    
    bool initialize(int width, int height) override;
    void shutdown() override;
    bool isInitialized() const override;
    
    void begin() override;
    void end() override;
    void setScreenSize(int width, int height) override;
    
    void drawRect(const RectF& rect, const Paint& paint) override;
    void drawCircle(const Vec2& center, float radius, const Paint& paint) override;
    void drawLine(const Vec2& start, const Vec2& end, const Paint& paint) override;
    void drawPolygon(const std::vector<Vec2>& points, const Paint& paint) override;
    void drawTexture(Texture* texture, const RectF& srcRect, const RectF& dstRect, const Paint& paint) override;
    void drawText(const std::string& text, const Vec2& position, const Paint& paint) override;
    
    void pushClip(const RectF& rect) override;
    void popClip() override;
    
    bool renderToFramebuffer(Framebuffer* fb, View* view) override;
    Vec2 measureText(const std::string& text, const Paint& paint) override;
    
    void resetForAppSwitch() override;
    void reloadFonts(const std::vector<FontConfig>& configs) override;

    // Context management
    void  activateContext();
    void  deactivateContext();
    void* getCurrentContext() override;
    
    // Font loading for this context
    void loadFontsForContext(const std::vector<FontConfig>& configs);
    bool setDefaultFont(const std::string& fontId);
    
private:
    void  rebuildFontAtlas();
    void* getImGuiFontFromPaint(const Paint& paint);
    
    void* m_drawList;
    void* m_imguiContext;
    int   m_screenWidth;
    int   m_screenHeight;
    bool  m_initialized;
    bool  m_frameActive;
};

#endif // USE_IMGUI

// ============================================================================
// GLCanvas - OpenGL-based canvas implementation
// ============================================================================

#ifdef USE_GLES

class GLCanvas : public ICanvas
{
public:
    GLCanvas();
    ~GLCanvas() override;
    
    bool initialize(int width, int height) override;
    void shutdown() override;
    bool isInitialized() const override;
    
    void begin() override;
    void end() override;
    void setScreenSize(int width, int height) override;
    
    void drawRect(const RectF& rect, const Paint& paint) override;
    void drawCircle(const Vec2& center, float radius, const Paint& paint) override;
    void drawLine(const Vec2& start, const Vec2& end, const Paint& paint) override;
    void drawPolygon(const std::vector<Vec2>& points, const Paint& paint) override;
    void drawTexture(Texture* texture, const RectF& srcRect, const RectF& dstRect, const Paint& paint) override;
    void drawText(const std::string& text, const Vec2& position, const Paint& paint) override;
    
    void pushClip(const RectF& rect) override;
    void popClip() override;
    
    bool renderToFramebuffer(Framebuffer* fb, View* view) override;
    Vec2 measureText(const std::string& text, const Paint& paint) override;
    void reloadFonts(const std::vector<FontConfig>& configs) override;
    void resetState();
    
private:
    struct Vertex { float x, y, u, v, r, g, b, a; };
    
    bool createShaders();
    bool createBuffers();
    void releaseShaders();
    void releaseBuffers();
    
    void flushBatch();
    void ensureBatchCapacity(size_t vertexCount, size_t indexCount);
    void addQuad(const RectF& rect, const Color4& color, const RectF& uv = RectF(0, 0, 1, 1));
    void addQuad(const RectF& rect, const Color4& color, float u0, float v0, float u1, float v1);
    void addPolygonFan(const std::vector<Vec2>& points, const Color4& color);
    
    void drawRectSDF(const RectF& rect, const Paint& paint);
    void drawCircleSDF(const Vec2& center, float radius, const Paint& paint);
    void drawTextWithGLFont(const std::string& text, const Vec2& position, const Paint& paint, GLFont* font);
    
    GLFont* getGLFontFromPaint(const Paint& paint);
    
    int m_screenWidth, m_screenHeight;
    bool m_initialized;
    
    uint32_t m_shapeShader, m_textureShader, m_textShader, m_flatShader;
    uint32_t m_vbo, m_ibo;
    
    int m_shapeProjLoc, m_shapeCenterLoc, m_shapeSizeLoc, m_shapeRadiusLoc;
    int m_shapeStrokeWidthLoc, m_shapeFillColorLoc, m_shapeStrokeColorLoc;
    int m_textureProjLoc, m_textureTexLoc, m_textureRectLoc, m_textureRadiusLoc;
    int m_textProjLoc, m_textTexLoc;
    int m_flatProjLoc;
    
    std::vector<Vertex> m_vertices;
    std::vector<uint16_t> m_indices;
    size_t m_batchVertexCount, m_batchIndexCount;
    uint32_t m_currentTexture, m_currentShader;
    std::vector<RectF> m_clipStack;
};

#endif // USE_GLES

// ============================================================================
// View - Base class for all UI elements
// ============================================================================

class View
{
public:
    using ClickCallback = std::function<void(View*)>;
    
    View();
    virtual ~View();
    
    // Lifecycle
    virtual void onAttach();
    virtual void onDetach();
    virtual void onMeasure(MeasureSpec widthSpec, MeasureSpec heightSpec);
    virtual void onLayout(const RectF& bounds);
    virtual void onDraw(ICanvas& canvas);
    
    // Input handling
    virtual bool onKeyEvent(const IO::KeyEvent& event) { return false; }
    virtual bool onMotionEvent(const IO::MotionEvent& event) { return false; }
    virtual void onFocusChanged(bool hasFocus) { m_hasFocus = hasFocus; }
    
    // Localization support - override in subclasses that display text
    virtual void setLocalizedText(const std::string& text) {}
    
    // Hierarchy
    void setParent(ViewGroup* parent);
    ViewGroup* getParent() const;
    
    // Layout
    void setLayoutParams(const LayoutParams& params);
    LayoutParams& getLayoutParams();
    const LayoutParams& getLayoutParams() const;
    void setBounds(const RectF& bounds);
    const RectF& getBounds() const;
    Vec2 getMeasuredSize() const;
    void setMeasuredSize(const Vec2& size);
    
    // Visibility
    void setVisibility(Visibility v);
    Visibility getVisibility() const;
    bool isVisible() const;
    
    // Paint & appearance
    void setNormalPaint(const Paint& paint);
    const Paint& getNormalPaint() const;
    void setHoverPaint(const Paint& paint);
    const Paint& getHoverPaint() const;
    void setFocusedPaint(const Paint& paint);
    const Paint& getFocusedPaint() const;
    void setPressedPaint(const Paint& paint);
    const Paint& getPressedPaint() const;
    void setDisabledPaint(const Paint& paint);
    const Paint& getDisabledPaint() const;
    
    void setBackgroundColor(const Color4& color);
    void setCornerRadius(float radius);
    void setOpacity(float opacity);
    void setNormalColor(const Color4& color);
    void setHoverColor(const Color4& color);
    void setFocusedColor(const Color4& color);
    void setPressedColor(const Color4& color);
    void setDisabledColor(const Color4& color);
    void setBorderColor(const Color4& color, float width = 2.0f);
    void setBorderWidth(float width);
    
    Color4 getBackgroundColor() const;
    float getCornerRadius() const;
    float getOpacity() const;
    
    // State
    bool isFocusable() const;
    void setFocusable(bool focusable);
    bool hasFocus() const;
    void setHovered(bool hovered);
    bool isHovered() const;
    void setPressed(bool pressed);
    bool isPressed() const;
    void setEnabled(bool enabled);
    bool isEnabled() const;
    
    // Focus system support
    void setAutoRegisterFocus(bool autoRegister) { m_autoRegisterFocus = autoRegister; }
    bool isAutoRegisterFocus() const { return m_autoRegisterFocus; }
    void setFocusManager(FocusManager* manager);
    FocusManager* getFocusManager() const;
    void setFocusContext(std::shared_ptr<FocusContext> context);
    std::shared_ptr<FocusContext> getFocusContext() const { return m_focusContext; }
    
    // Click handling
    void setOnClickListener(ClickCallback callback);
    bool hasClickListener() const;
    void performClick();
    
    // Framebuffer
    void setFramebufferEnabled(bool enabled);
    bool isFramebufferEnabled() const;
    Texture* getFramebufferTexture();
    uint32_t getFramebufferTextureId();
    
    // Drawing utilities (public for framebuffer rendering)
    void drawViewContent(ICanvas& canvas);
    
protected:
    const Paint& getCurrentPaint() const;
    void drawBackground(ICanvas& canvas);
    void drawFocusBorder(ICanvas& canvas);
    RectF getContentBounds() const;
    void updateFramebufferSize(int width, int height);
    
    static void applyGravity(const Vec2& childSize, const RectF& parentBounds, Gravity gravity, const LayoutParams& lp, RectF& outBounds);
    
    ViewGroup* m_parent;
    LayoutParams m_layoutParams;
    RectF m_bounds;
    Vec2 m_measuredSize;
    Visibility m_visibility;
    
    bool m_focusable, m_hasFocus;
    bool m_isHovered, m_isPressed, m_isEnabled;
    bool m_autoRegisterFocus;
    
    FocusManager* m_focusManager;
    std::shared_ptr<FocusContext> m_focusContext;
    
    Paint m_normalPaint, m_hoverPaint, m_focusPaint, m_pressedPaint, m_disabledPaint;
    ClickCallback m_onClickCallback;
    
    bool m_fbEnabled;
    std::unique_ptr<Framebuffer> m_framebuffer;
};

// ============================================================================
// ViewGroup - Container for child views
// ============================================================================
//
// OWNERSHIP MODEL:
// ----------------
// ViewGroup takes ownership of child Views added via addView().
// Child Views will be automatically deleted when:
// - removeView() is called on that child
// - removeAllViews() is called
// - The ViewGroup itself is destroyed
//
// USAGE RULES:
// ------------
// 1. Create Views with 'new' and add them to a ViewGroup
// 2. Do NOT delete Views manually after adding them to a ViewGroup
// 3. Do NOT add the same View instance to multiple ViewGroups
// 4. The ViewGroup will handle cleanup automatically
//
// Example:
//    ViewGroup* container = new ViewGroup();
//    TextView* label = new TextView();  // Create with new
//    container->addView(label);         // ViewGroup takes ownership
//    // DO NOT: delete label;            // ViewGroup will delete it
//
// ============================================================================

class ViewGroup : public View
{
public:
    ViewGroup();
    virtual ~ViewGroup();
    
    // Child management
    void addView(View* child);
    void addViewAt(View* child, size_t index);
    void removeView(View* child);
    void removeViewNoDelete(View* child);
    void removeAllViews();
    size_t getChildCount() const;
    View* getChildAt(size_t index) const;
    
    // Overrides
    void onMeasure(MeasureSpec widthSpec, MeasureSpec heightSpec) override;
    void onLayout(const RectF& bounds) override;
    void onDraw(ICanvas& canvas) override;
    bool onKeyEvent(const IO::KeyEvent& event) override;
    bool onMotionEvent(const IO::MotionEvent& event) override;
    
    // Focus context propagation
    void setFocusManager(FocusManager* manager);
    void setFocusContext(std::shared_ptr<FocusContext> context);
    
    // Z-order and event handling
    View* findTopChildAt(float x, float y) const;
    virtual bool shouldConsumeEvent(const IO::MotionEvent& event) const;
    
    // Drawing utilities (public for framebuffer rendering)
    void drawViewGroupContent(ICanvas& canvas);
    
protected:
    virtual void measureChildren(MeasureSpec widthSpec, MeasureSpec heightSpec);
    virtual void layoutChildren() = 0;
    
    void renderToFramebuffer(ICanvas& canvas);
    MeasureSpec getChildMeasureSpec(MeasureSpec parentSpec, const LayoutParams& lp, bool isWidth);
    
    float getAvailableWidth() const;
    float getAvailableHeight() const;
    RectF getChildLayoutBounds() const;
    
    std::vector<View*> m_children;
};

} // namespace UI

} // namespace APP

#endif // UI_CORE_H
