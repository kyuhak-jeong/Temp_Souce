#ifndef UI_ELEMENTS_H
#define UI_ELEMENTS_H

#include "ui_core.h"
#include "ui_focus.h"
#include "constants.h"
#include <string>
#include <vector>
#include <functional>
#include <memory>

namespace APP
{
namespace UI
{

// ── TextView ─────────────────────────────────────────────────────────────────

class TextView : public View
{
public:
    TextView();

    void setText(const std::string& text)          { m_text = text; m_marqueeOffset = 0.0f; m_marqueeDelay = MARQUEE_PAUSE_SEC; }
    const std::string& getText() const             { return m_text; }
    void setTextPaint(const Paint& paint)          { m_textPaint = paint; }
    const Paint& getTextPaint() const              { return m_textPaint; }
    void setTextColor(const Color4& color)         { m_textPaint.fgColor = color; }
    const Color4& getTextColor() const             { return m_textPaint.fgColor; }
    void setTextSize(float size)                   { m_textPaint.textProps.size = size; }
    float getTextSize() const                      { return m_textPaint.textProps.size; }
    void setTextProps(const TextProps& prop)       { m_textPaint.textProps = prop; }
    const TextProps& getTextProps() const          { return m_textPaint.textProps; }
    void setTextGravity(Gravity gravity)           { m_textGravity = gravity; }
    Gravity getTextGravity() const                 { return m_textGravity; }
    void setTextOpacity(float opacity)             { m_textPaint.opacity = opacity; }
    float getTextOpacity() const                   { return m_textPaint.opacity; }

    void setMarquee(bool enabled)                  { m_marquee = enabled; if (!enabled) { m_marqueeOffset = 0.0f; m_marqueeDelay = MARQUEE_PAUSE_SEC; } }
    bool isMarquee() const                         { return m_marquee; }
    void resetMarquee()                            { m_marqueeOffset = 0.0f; m_marqueeDelay = MARQUEE_PAUSE_SEC; }
    void update(float deltaTime);

    void onMeasure(MeasureSpec widthSpec, MeasureSpec heightSpec) override;
    void onDraw(ICanvas& canvas) override;
    void setLocalizedText(const std::string& text) override { setText(text); }

private:
    static constexpr float MARQUEE_SPEED     = 60.0f;
    static constexpr float MARQUEE_PAUSE_SEC = 1.5f;
    static constexpr float MARQUEE_GAP       = 40.0f;

    std::string m_text;
    Paint       m_textPaint;
    Gravity     m_textGravity;
    bool        m_marquee        = false;
    float       m_marqueeOffset  = 0.0f;
    float       m_marqueeDelay   = MARQUEE_PAUSE_SEC;
};

// ── ImageView ────────────────────────────────────────────────────────────────

class ImageView : public View
{
public:
    ImageView();
    ~ImageView() override;

    void setTexture(Texture* texture, bool yflip = false) { m_texture = texture; m_isVerticalFlip = yflip; }
    Texture* getTexture() const                  { return m_texture; }
    void setScaleType(ScaleType type)            { m_scaleType = type; }
    ScaleType getScaleType() const               { return m_scaleType; }
    void setImagePaint(const Paint& paint)       { m_imagePaint = paint; }
    const Paint& getImagePaint() const           { return m_imagePaint; }
    void setTint(const Color4& color)            { m_imagePaint.bgColor = color; }
    const Color4& getTint() const                { return m_imagePaint.bgColor; }
    void setImageOpacity(float opacity)          { m_imagePaint.opacity = opacity; }
    float getImageOpacity() const                { return m_imagePaint.opacity; }
    RectF getImageRect() const;

    void onMeasure(MeasureSpec widthSpec, MeasureSpec heightSpec) override;
    void onDraw(ICanvas& canvas) override;

private:
    RectF calculateDrawRect() const;

    Texture*  m_texture;
    ScaleType m_scaleType;
    Paint     m_imagePaint;
    bool      m_isVerticalFlip;
};

// ── Button ───────────────────────────────────────────────────────────────────

class Button : public View
{
public:
    Button();
    ~Button();

    void setText(const std::string& text)        { m_text = text; }
    const std::string& getText() const           { return m_text; }
    void setTextPaint(const Paint& paint)        { m_textPaint = paint; }
    const Paint& getTextPaint() const            { return m_textPaint; }
    void setTextColor(const Color4& color)       { m_textPaint.fgColor = color; }
    const Color4& getTextColor() const           { return m_textPaint.fgColor; }
    void setTextSize(float size)                 { m_textPaint.textProps.size = size; }
    float getTextSize() const                    { return m_textPaint.textProps.size; }
    void setTextProps(const TextProps& prop)     { m_textPaint.textProps = prop; }
    const TextProps& getTextProps() const        { return m_textPaint.textProps; }
    void setTextOpacity(float opacity)           { m_textPaint.opacity = opacity; }
    float getTextOpacity() const                 { return m_textPaint.opacity; }

    void setTexture(Texture* texture)            { m_texture = texture; }
    Texture* getTexture() const                  { return m_texture; }
    void setImageSize(float w, float h)          { m_imageWidth = w; m_imageHeight = h; }
    float getImageWidth() const                  { return m_imageWidth; }
    float getImageHeight() const                 { return m_imageHeight; }
    void setImagePaint(const Paint& paint)       { m_imagePaint = paint; }
    const Paint& getImagePaint() const           { return m_imagePaint; }
    void setImageTint(const Color4& color)       { m_imagePaint.bgColor = color; }
    const Color4& getImageTint() const           { return m_imagePaint.bgColor; }
    void setImageOpacity(float opacity)          { m_imagePaint.opacity = opacity; }
    float getImageOpacity() const                { return m_imagePaint.opacity; }

    void setContentOrientation(Orientation o)    { m_contentOrientation = o; }
    Orientation getContentOrientation() const    { return m_contentOrientation; }
    void setContentSpacing(float spacing)        { m_contentSpacing = spacing; }
    float getContentSpacing() const              { return m_contentSpacing; }
    void setContentGravity(Gravity gravity)      { m_contentGravity = gravity; }
    Gravity getContentGravity() const            { return m_contentGravity; }

    void onMeasure(MeasureSpec widthSpec, MeasureSpec heightSpec) override;
    void onDraw(ICanvas& canvas) override;
    bool onMotionEvent(const IO::MotionEvent& event) override;
    void onFocusChanged(bool hasFocus) override;
    void setLocalizedText(const std::string& text) override { setText(text); }

private:
    RectF calculateImageRect(const RectF& availableBounds, const Vec2& imageSize) const;

    std::string m_text;
    Paint       m_textPaint;
    Texture*    m_texture;
    float       m_imageWidth;
    float       m_imageHeight;
    Paint       m_imagePaint;
    Orientation m_contentOrientation;
    float       m_contentSpacing;
    Gravity     m_contentGravity;

    static Button* s_currentlyHoveredButton;
};

// ── CheckBox ─────────────────────────────────────────────────────────────────

class CheckBox : public View
{
public:
    using OnCheckedChangeListener = std::function<void(CheckBox*, bool)>;

    CheckBox();

    void               setText(const std::string& text) { m_text    = text; }
    const std::string& getText()                  const { return m_text;    }
    void               setChecked(bool checked)         { m_checked = checked; }
    bool               isChecked()                const { return m_checked; }

    void        setOrientation(Orientation o)           { m_orientation = o; }
    Orientation getOrientation()              const     { return m_orientation; }
    void        setTextGravity(Gravity g)               { m_textGravity = g; }
    Gravity     getTextGravity()              const     { return m_textGravity; }
    void        setSpacing(float spacing)               { m_spacing = spacing; }
    float       getSpacing()                  const     { return m_spacing; }
    void        setCheckBoxSize(float size)             { m_checkBoxSize = size; }
    float       getCheckBoxSize()             const     { return m_checkBoxSize; }

    void          setTextPaint(const Paint& paint)      { m_textPaint  = paint; }
    const Paint&  getTextPaint()              const     { return m_textPaint; }
    void          setTextColor(const Color4& color)     { m_textPaint.fgColor = color; }
    void          setTextSize(float size)               { m_textPaint.textProps.size = size; }
    float         getTextSize()               const     { return m_textPaint.textProps.size; }
    void          setTextProps(const TextProps& prop)   { m_textPaint.textProps = prop; }
    const TextProps& getTextProps()           const     { return m_textPaint.textProps; }
    void          setCheckPaint(const Paint& paint)     { m_checkPaint = paint; }
    const Paint&  getCheckPaint()             const     { return m_checkPaint; }
    void          setCheckColor(const Color4& color)    { m_checkPaint.bgColor = color; }
    void          setFocusHighlight(bool highlight)     { m_focusHighlight = highlight; }
    bool          getFocusHighlight()         const     { return m_focusHighlight; }

    void setOnCheckedChangeListener(OnCheckedChangeListener l) { m_onCheckedChange = l; }

    void onMeasure(MeasureSpec widthSpec, MeasureSpec heightSpec) override;
    void onDraw(ICanvas& canvas)                                  override;
    bool onMotionEvent(const IO::MotionEvent& event)              override;

private:
    bool          m_checked;
    bool          m_focusHighlight;
    Orientation   m_orientation;
    Gravity       m_textGravity;
    float         m_spacing;
    float         m_checkBoxSize;
    Paint         m_textPaint;
    Paint         m_checkPaint;
    OnCheckedChangeListener m_onCheckedChange;
    std::string   m_text;
};

// ── SliderView ────────────────────────────────────────────────────────────────

class SliderView : public View
{
public:
    enum class TextPosition { NONE, BEFORE, AFTER };

    using ValueChangedCallback = std::function<void(float value)>;

    SliderView();

    // ── Value ──────────────────────────────────────────────────────────────
    void  setValue(float value, bool notify = false);
    float getValue()    const { return m_value; }
    void  setRange(float minV, float maxV, float step = 1.0f);
    float getMinValue() const { return m_minValue; }
    float getMaxValue() const { return m_maxValue; }
    float getStep()     const { return m_step; }

    // ── Orientation ────────────────────────────────────────────────────────
    void        setOrientation(Orientation o) { m_orientation = o; }
    Orientation getOrientation()        const { return m_orientation; }

    // ── Label ──────────────────────────────────────────────────────────────
    void          setTextPosition(TextPosition p) { m_textPosition = p; }
    TextPosition  getTextPosition()         const { return m_textPosition; }
    void          setTextGravity(Gravity g)       { m_textGravity = g; }
    Gravity       getTextGravity()          const { return m_textGravity; }
    void          setTextSize(float s)            { m_labelPaint.textProps.size = s; }
    float         getTextSize()             const { return m_labelPaint.textProps.size; }
    void          setTextColor(const Color4& c)   { m_labelPaint.fgColor = c; }
    const Color4& getTextColor()            const { return m_labelPaint.fgColor; }
    void          setTextPaint(const Paint& p)    { m_labelPaint = p; }
    const Paint&  getTextPaint()            const { return m_labelPaint; }
    void          setLabelSpacing(float s)        { m_labelSpacing = s; }
    float         getLabelSpacing()         const { return m_labelSpacing; }
    // Printf-style format string for the displayed value; default "%.0f"
    void               setValueFormat(const std::string& fmt) { m_valueFormat = fmt; }
    const std::string& getValueFormat()               const   { return m_valueFormat; }

    // ── Track / thumb visuals ──────────────────────────────────────────────
    void          setTrackThickness(float t)       { m_trackThickness = t; }
    float         getTrackThickness()        const { return m_trackThickness; }
    void          setTrackColor(const Color4& c)   { m_trackColor = c; }
    const Color4& getTrackColor()            const { return m_trackColor; }
    void          setFillColor(const Color4& c)    { m_fillColor = c; }
    const Color4& getFillColor()             const { return m_fillColor; }
    void          setThumbRadius(float r)          { m_thumbRadius = r; }
    float         getThumbRadius()           const { return m_thumbRadius; }
    void          setThumbColor(const Color4& c)   { m_thumbColor = c; }
    const Color4& getThumbColor()            const { return m_thumbColor; }
    void          setAdjustingColor(const Color4& c) { m_adjustingColor = c; }
    const Color4& getAdjustingColor()          const { return m_adjustingColor; }

    // ── Adjust mode ────────────────────────────────────────────────────────
    bool isAdjusting() const { return m_adjusting; }
    void setAdjusting(bool a);

    // ── Callback ───────────────────────────────────────────────────────────
    void setOnValueChanged(ValueChangedCallback cb) { m_onValueChanged = cb; }

    // ── Overrides ──────────────────────────────────────────────────────────
    void onMeasure(MeasureSpec widthSpec, MeasureSpec heightSpec) override;
    void onDraw(ICanvas& canvas)                                  override;
    bool onKeyEvent(const IO::KeyEvent& event)                    override;
    bool onMotionEvent(const IO::MotionEvent& event)              override;
    void onFocusChanged(bool hasFocus)                            override;

private:
    std::string formatValue()             const;
    RectF       calcTrackRect()           const;
    float       posToValue(float pos)     const;
    float       clamp(float v)            const;

    float        m_value;
    float        m_valueBeforeAdjust;
    float        m_minValue;
    float        m_maxValue;
    float        m_step;

    Orientation  m_orientation;
    TextPosition m_textPosition;
    Gravity      m_textGravity;
    float        m_labelSpacing;
    std::string  m_valueFormat;
    Paint        m_labelPaint;

    float        m_trackThickness;
    float        m_thumbRadius;
    Color4       m_trackColor;
    Color4       m_fillColor;
    Color4       m_thumbColor;
    Color4       m_adjustingColor;

    bool         m_adjusting;
    bool         m_dragging;

    ValueChangedCallback m_onValueChanged;
};

// ── CursorView ───────────────────────────────────────────────────────────────

class CursorView : public View
{
public:
    enum class VisibilityMode { ALWAYS_VISIBLE, SHOW_ON_TOUCH, SHOW_ON_MOVE, NEVER_VISIBLE };
    enum class CursorStyle    { CIRCLE, RING, CROSSHAIR, DOT_WITH_RING, RIPPLE };

    CursorView();
    virtual ~CursorView();

    void updatePosition(float x, float y);
    void updateFromMotionEvent(const IO::MotionEvent& event);
    Vec2 getPosition() const { return m_position; }

    void setPressed(bool pressed);
    bool isPressed() const                         { return m_isPressed; }
    void setLastInputSource(IO::Source source)     { m_lastInputSource = source; }
    IO::Source getLastInputSource() const          { return m_lastInputSource; }
    void setMouseButton(IO::MouseButton button)    { m_mouseButton = button; }
    IO::MouseButton getMouseButton() const         { return m_mouseButton; }

    void setVisibilityMode(VisibilityMode mode)    { m_visibilityMode = mode; }
    VisibilityMode getVisibilityMode() const       { return m_visibilityMode; }
    void setCursorStyle(CursorStyle style)         { m_cursorStyle = style; }
    CursorStyle getCursorStyle() const             { return m_cursorStyle; }
    void setCursorSize(float size)                 { m_cursorSize = size; }
    float getCursorSize() const                    { return m_cursorSize; }
    void setMouseColor(const Color4& color)        { m_mouseColor = color; }
    const Color4& getMouseColor() const            { return m_mouseColor; }
    void setTouchColor(const Color4& color)        { m_touchColor = color; }
    const Color4& getTouchColor() const            { return m_touchColor; }
    void setPressedColor(const Color4& color)      { m_pressedColor = color; }
    const Color4& getPressedColor() const          { return m_pressedColor; }
    void setFadeOutDuration(float seconds)         { m_fadeOutDuration = seconds; }
    float getFadeOutDuration() const               { return m_fadeOutDuration; }

    void update(float deltaTime);
    void triggerRipple();

    void onMeasure(MeasureSpec widthSpec, MeasureSpec heightSpec) override;
    void onDraw(ICanvas& canvas) override;

private:
    void updateVisibility();
    void drawCircle(ICanvas& canvas, const Vec2& center, float radius, const Color4& color, bool filled);
    void drawRing(ICanvas& canvas, const Vec2& center, float radius, const Color4& color, float strokeWidth);
    void drawCrosshair(ICanvas& canvas, const Vec2& center, float size, const Color4& color, float strokeWidth);
    void drawRipple(ICanvas& canvas, const Vec2& center, float progress, const Color4& color);
    Color4 getCurrentColor() const;
    bool shouldBeVisible() const;

    Vec2            m_position;
    bool            m_isPressed;
    IO::Source      m_lastInputSource;
    IO::MouseButton m_mouseButton;
    VisibilityMode  m_visibilityMode;
    CursorStyle     m_cursorStyle;
    float           m_cursorSize;
    Color4          m_mouseColor;
    Color4          m_touchColor;
    Color4          m_pressedColor;
    float           m_fadeOutDuration;
    float           m_timeSinceLastMove;
    float           m_currentAlpha;
    bool            m_rippleActive;
    float           m_rippleProgress;
    float           m_rippleDuration;
    Vec2            m_lastPosition;
    bool            m_hasMoved;

    // Multi-touch: positions of all active fingers
    int             m_touchPointerCount;
    Vec2            m_touchPointerPositions[IO::MAX_TOUCH_POINTS];
};

// ── FloatingObject ───────────────────────────────────────────────────────────

enum class FloatingShape { CIRCLE, RECTANGLE };

class FloatingObject
{
public:
    FloatingObject();

    void setShape(FloatingShape shape)           { m_shape = shape; }
    FloatingShape getShape() const               { return m_shape; }
    void setPosition(const Vec2& pos)            { m_position = pos; }
    const Vec2& getPosition() const              { return m_position; }
    void setVelocity(const Vec2& vel)            { m_velocity = vel; }
    const Vec2& getVelocity() const              { return m_velocity; }
    void setSize(const Vec2& size)               { m_size = size; }
    const Vec2& getSize() const                  { return m_size; }
    void setPaint(const Paint& paint)            { m_paint = paint; }
    const Paint& getPaint() const                { return m_paint; }
    void setBgColor(const Color4& color)         { m_paint.bgColor = color; }
    const Color4& getBgColor() const             { return m_paint.bgColor; }
    void setFgColor(const Color4& color)         { m_paint.fgColor = color; }
    const Color4& getFgColor() const             { return m_paint.fgColor; }
    void setFilled(bool filled)                  { m_paint.filled = filled; }
    bool isFilled() const                        { return m_paint.filled; }
    void setStrokeWidth(float width)             { m_paint.strokeWidth = width; }
    float getStrokeWidth() const                 { return m_paint.strokeWidth; }
    void setCornerRadius(float radius)           { m_paint.cornerRadius = radius; }
    float getCornerRadius() const                { return m_paint.cornerRadius; }
    void setOpacity(float opacity)               { m_paint.opacity = opacity; }
    float getOpacity() const                     { return m_paint.opacity; }

    void update(float deltaTime, const RectF& bounds);
    void draw(ICanvas& canvas);

private:
    FloatingShape m_shape;
    Vec2          m_position;
    Vec2          m_velocity;
    Vec2          m_size;
    Paint         m_paint;
};

// ── FrameLayout ──────────────────────────────────────────────────────────────

class FrameLayout : public ViewGroup
{
public:
    FrameLayout();
protected:
    void layoutChildren() override;
};

// ── LinearLayout ─────────────────────────────────────────────────────────────

class LinearLayout : public ViewGroup
{
public:
    LinearLayout();

    void setOrientation(Orientation o)  { m_orientation = o; }
    Orientation getOrientation() const  { return m_orientation; }
    void setGravity(Gravity gravity)    { m_gravity = gravity; }
    Gravity getGravity() const          { return m_gravity; }
    void setSpacing(float spacing)      { m_spacing = spacing; }
    float getSpacing() const            { return m_spacing; }

    void onMeasure(MeasureSpec widthSpec, MeasureSpec heightSpec) override;

protected:
    void layoutChildren() override;

private:
    Orientation m_orientation;
    Gravity     m_gravity;
    float       m_spacing;
};

// ── RelativeLayout ───────────────────────────────────────────────────────────

enum class RelativeRule
{
    NONE = 0,
    ALIGN_PARENT_LEFT, ALIGN_PARENT_TOP, ALIGN_PARENT_RIGHT, ALIGN_PARENT_BOTTOM,
    CENTER_IN_PARENT, CENTER_HORIZONTAL, CENTER_VERTICAL
};

struct RelativeLayoutParams : public LayoutParams
{
    RelativeRule rule;
    RelativeLayoutParams() : LayoutParams(), rule(RelativeRule::NONE) {}
};

class RelativeLayout : public ViewGroup
{
public:
    RelativeLayout();
    void onMeasure(MeasureSpec widthSpec, MeasureSpec heightSpec) override;
protected:
    void layoutChildren() override;
    void applyConstraints(View* child, const RelativeLayoutParams& params, const RectF& bounds);
};

// ── ConstraintLayout ─────────────────────────────────────────────────────────

enum class ConstraintType
{
    NONE = 0,
    LEFT_TO_LEFT, LEFT_TO_RIGHT, RIGHT_TO_LEFT, RIGHT_TO_RIGHT,
    TOP_TO_TOP, TOP_TO_BOTTOM, BOTTOM_TO_TOP, BOTTOM_TO_BOTTOM,
    CENTER_HORIZONTAL, CENTER_VERTICAL
};

struct Constraint
{
    View*          targetView;
    ConstraintType type;
    float          margin;

    Constraint() : targetView(nullptr), type(ConstraintType::NONE), margin(0.0f) {}
    Constraint(View* target, ConstraintType t, float m = 0.0f) : targetView(target), type(t), margin(m) {}
};

struct ConstraintLayoutParams : public LayoutParams
{
    std::vector<Constraint> constraints;
    void addConstraint(View* target, ConstraintType type, float margin = 0.0f)
        { constraints.push_back(Constraint(target, type, margin)); }
};

class ConstraintLayout : public ViewGroup
{
public:
    ConstraintLayout();
    void onMeasure(MeasureSpec widthSpec, MeasureSpec heightSpec) override;
protected:
    void layoutChildren() override;
    void applyConstraints(View* child, const ConstraintLayoutParams& params, const RectF& bounds);
};

// ── ScrollView ───────────────────────────────────────────────────────────────

class ScrollView : public ViewGroup
{
public:
    ScrollView();

    void setScrollDirection(Orientation direction) { m_scrollDirection = direction; }
    Orientation getScrollDirection() const         { return m_scrollDirection; }
    void scrollTo(float offset);
    void scrollBy(float delta);
    float getScrollOffset() const                  { return m_scrollOffset; }
    float getMaxScrollOffset() const               { return m_maxScrollOffset; }

    void onMeasure(MeasureSpec widthSpec, MeasureSpec heightSpec) override;
    void onDraw(ICanvas& canvas) override;
    bool onMotionEvent(const IO::MotionEvent& event) override;

protected:
    void layoutChildren() override;

private:
    Orientation m_scrollDirection;
    float       m_scrollOffset;
    float       m_maxScrollOffset;
    Vec2        m_lastTouchPos;
    bool        m_isDragging;
};

// ── Forward declarations ─────────────────────────────────────────────────────

class ListAdapter;
class AutoGridAdapter;

struct ActionButton
{
    std::function<void()> callback;
    Color4 normalColor;
    Color4 focusedColor;
    Color4 pressedColor;
    Color4 hoverColor;
    bool   useDefaultColors;
    Button* button;

    ActionButton()
        : callback(nullptr)
        , normalColor(Color::AccentPrimary.withAlpha(0.8f))
        , focusedColor(Color::AccentSecondary.withAlpha(0.9f))
        , pressedColor(Color::AccentTertiary)
        , hoverColor(Color::AccentSecondary.withAlpha(0.9f))
        , useDefaultColors(true)
        , button(nullptr)
    {}

    ActionButton(const Color4& normal, const Color4& focused, const Color4& pressed, const Color4& hover)
        : callback(nullptr)
        , normalColor(normal), focusedColor(focused)
        , pressedColor(pressed), hoverColor(hover)
        , useDefaultColors(false)
        , button(nullptr)
    {}

    void setColors(const Color4& normal, const Color4& focused, const Color4& pressed, const Color4& hover)
    {
        normalColor      = normal;
        focusedColor     = focused;
        pressedColor     = pressed;
        hoverColor       = hover;
        useDefaultColors = false;
    }
};

// ── ListViewItem ─────────────────────────────────────────────────────────────

class ListViewItem : public LinearLayout
{
public:
    ListViewItem();
    ~ListViewItem();

    void setIcon(const std::string& text, const Color4& color = Color::AccentPrimary);
    void setName(const std::string& name);
    void setInfo(const std::string& info);
    void setItemFocused(bool focused);
    bool isItemFocused() const                   { return m_isItemFocused; }

    void addAction(const ActionButton& action);
    void clearActions();
    void triggerAction(int32_t index);
    void setActionFocusIndex(int32_t index);
    int32_t getActionFocusIndex() const          { return m_actionFocusIndex; }
    int32_t getActionCount() const               { return static_cast<int32_t>(m_actions.size()); }
    bool hasActions() const                      { return m_actions.empty() == false; }
    Button* getActionButton(int32_t index);
    const Button* getActionButton(int32_t index) const;

    bool onMotionEvent(const IO::MotionEvent& event) override;
    void onDraw(ICanvas& canvas) override;

private:
    void setupContent();
    void updateVisualState();

    bool            m_isItemFocused;
    int32_t         m_actionFocusIndex;
    TextView*       m_iconText;
    TextView*       m_nameText;
    TextView*       m_infoText;
    LinearLayout*   m_actionsLayout;
    std::vector<ActionButton> m_actions;

    static ListViewItem* s_currentlyHoveredItem;
};

// ── ListAdapter ──────────────────────────────────────────────────────────────

class ListAdapter
{
public:
    virtual ~ListAdapter() = default;

    virtual int32_t getCount() const = 0;
    virtual View* getView(int32_t position, View* convertView) = 0;
    virtual void onViewRecycled(View*) {}

    void notifyDataSetChanged() { if (m_changeCallback != nullptr) m_changeCallback(); }
    void setOnDataSetChangedCallback(std::function<void()> cb) { m_changeCallback = cb; }

private:
    std::function<void()> m_changeCallback;
};

// ── ListView ─────────────────────────────────────────────────────────────────

class ListView : public ViewGroup
{
public:
    ListView();

    void setAdapter(std::shared_ptr<ListAdapter> adapter);
    std::shared_ptr<ListAdapter> getAdapter() const { return m_adapter; }
    void setItemSpacing(float spacing)              { m_itemSpacing = spacing; }
    float getItemSpacing() const                    { return m_itemSpacing; }
    void setMaxHeight(float maxHeight)              { m_maxHeight = maxHeight; }
    float getMaxHeight() const                      { return m_maxHeight; }
    void setOnItemClickListener(std::function<void(int32_t)> l) { m_itemClickListener = l; }
    void setFocusedItemIndex(int32_t index);
    int32_t getFocusedItemIndex() const             { return m_focusedItemIndex; }
    bool hasFocusedItem() const                     { return m_focusedItemIndex >= 0; }
    void clearItemFocus();
    void scrollToPosition(int32_t position);
    void scrollTo(float offset);
    void scrollBy(float delta)                      { scrollTo(m_scrollOffset + delta); }
    bool moveFocusUp();
    bool moveFocusDown();
    bool moveFocusToActionLeft();
    bool moveFocusToActionRight();
    bool canMoveFocusUp() const;
    bool canMoveFocusDown() const;

    void onMeasure(MeasureSpec widthSpec, MeasureSpec heightSpec) override;
    void onDraw(ICanvas& canvas) override;
    bool onKeyEvent(const IO::KeyEvent& event) override;
    bool onMotionEvent(const IO::MotionEvent& event) override;
    void onFocusChanged(bool hasFocus) override;

protected:
    void layoutChildren() override;

private:
    void rebuildViews();
    void updateItemFocus();
    ListViewItem* getItemAt(int32_t index) const;

    std::shared_ptr<ListAdapter> m_adapter;
    float   m_itemSpacing;
    float   m_scrollOffset;
    float   m_maxScrollOffset;
    float   m_maxHeight;
    Vec2    m_lastTouchPos;
    bool    m_isDragging;
    float   m_dragStartY;
    float   m_dragThreshold;
    int32_t m_focusedItemIndex;
    std::function<void(int32_t)> m_itemClickListener;
};

// ── AIOverlay ────────────────────────────────────────────────────────────────
// Draws ROI zones and bounding boxes over an ImageView.
// All coordinates are in native pixel space [0..nativeW] x [0..nativeH];
// the overlay maps them to the imageView bounds at draw time.

class AIOverlay : public View
{
public:
    using ROIPointMovedCallback = std::function<void(int ptIndex, float nativeX, float nativeY)>;

    AIOverlay();

    void setImageView(ImageView* imageView)             { m_imageView = imageView; }

    void addROIZone(const AI::ROIZone& zone)            { m_roiZones.push_back(zone); }
    void clearROIZones()                                { m_roiZones.clear(); }
    const std::vector<AI::ROIZone>& getROIZones() const { return m_roiZones; }

    void setBoundingBoxes(const std::vector<AI::BoundingBox>& boxes) { m_boundingBoxes = boxes; }
    void clearBoundingBoxes()                                        { m_boundingBoxes.clear(); }
    const std::vector<AI::BoundingBox>& getBoundingBoxes() const     { return m_boundingBoxes; }

    void setShowBoundingBoxes(bool show)             { m_showBoundingBoxes = show; }
    bool isShowingBoundingBoxes() const              { return m_showBoundingBoxes; }
    void setBBoxStrokeWidth(float width)             { m_bboxStrokeWidth = width; }
    void setShowBBoxLabels(bool show)                { m_showBBoxLabels = show; }

    void setNativeSize(int nativeW, int nativeH)     { m_nativeW = nativeW; m_nativeH = nativeH; }
    int getNativeW() const                           { return m_nativeW; }
    int getNativeH() const                           { return m_nativeH; }

    void setActiveDragPoint(int ptIndex)             { m_activeDragPointIndex = ptIndex; }
    int getActiveDragPoint() const                   { return m_activeDragPointIndex; }
    void setOnROIPointMoved(ROIPointMovedCallback cb){ m_onROIPointMoved = cb; }

    void setActiveZoneIndex(int zoneIdx)             { m_activeZoneIndex = zoneIdx; }
    int getActiveZoneIndex() const                   { return m_activeZoneIndex; }

    void setDpadSelectedPoint(int ptIndex)           { m_dpadSelectedPointIndex = ptIndex; }
    int getDpadSelectedPoint() const                 { return m_dpadSelectedPointIndex; }

    int hitTestROIPoint(float screenX, float screenY, float hitRadiusPx) const;
    Vec2 screenToNative(float screenX, float screenY) const;

    void onDraw(ICanvas& canvas) override;
    bool onMotionEvent(const IO::MotionEvent& event) override;

private:
    void drawROIZones(ICanvas& canvas);
    void drawBoundingBoxes(ICanvas& canvas);
    Vec2  nativeToScreen(const Vec2& nativePt) const;
    RectF bboxNativeToScreen(const AI::BoundingBox& bbox) const;

    ImageView*                    m_imageView;
    int                           m_nativeW;
    int                           m_nativeH;
    std::vector<AI::ROIZone>      m_roiZones;
    std::vector<AI::BoundingBox>  m_boundingBoxes;
    bool                          m_showBoundingBoxes;
    bool                          m_showBBoxLabels;
    float                         m_bboxStrokeWidth;
    int                           m_activeZoneIndex;
    int                           m_activeDragPointIndex;
    int                           m_dpadSelectedPointIndex;
    ROIPointMovedCallback         m_onROIPointMoved;
};

// ── AIImageView ───────────────────────────────────────────────────────────────
// Hosts an ImageView and an AIOverlay at a fixed native resolution.

class AIImageView : public FrameLayout
{
public:
    using ROIPointMovedCallback = std::function<void(int ptIndex, float nativeX, float nativeY)>;

    AIImageView();
    ~AIImageView() = default;

    void setNativeSize(int nativeW, int nativeH);
    int getNativeW() const { return m_nativeW; }
    int getNativeH() const { return m_nativeH; }

    void setImage(Texture* texture, bool yflip = false);
    ImageView* getImageView() const { return m_imageView; }

    void addROIZone(const AI::ROIZone& zone);
    void clearROIZones();
    const std::vector<AI::ROIZone>& getROIZones() const;

    void setBoundingBoxes(const std::vector<AI::BoundingBox>& boxes);
    void clearBoundingBoxes();
    const std::vector<AI::BoundingBox>& getBoundingBoxes() const;

    void setShowBoundingBoxes(bool show);
    bool isShowingBoundingBoxes() const;
    void setBBoxStrokeWidth(float width);
    void setShowBBoxLabels(bool show);

    void setOnROIPointMoved(ROIPointMovedCallback cb);
    void setActiveZoneIndex(int zoneIdx);
    void setDpadSelectedPoint(int ptIndex);
    int  hitTestROIPoint(float screenX, float screenY, float hitRadiusPx) const;

    void onLayout(const RectF& bounds) override;
    void onDraw(ICanvas& canvas) override;
    bool onMotionEvent(const IO::MotionEvent& event) override;

private:
    ImageView*   m_imageView;
    AIOverlay*   m_aiOverlay;
    int          m_nativeW;
    int          m_nativeH;
};

// ── GridCellView ─────────────────────────────────────────────────────────────

class GridCellView : public FrameLayout
{
public:
    GridCellView();
    virtual ~GridCellView();

    void setName(const std::string& name);
    void setInfo(const std::string& info);
    void setImage(Texture* texture, bool yflip = false);
    void setCornerRadius(float radius);

    AIImageView*    getAIImageView()    const { return m_aiImageView; }
    TextView*       getNameText()       const { return m_nameText; }
    TextView*       getInfoText()       const { return m_infoText; }
    ImageView*      getWarningOverlay() const { return m_warningOverlay; }

    void setCellFocused(bool focused);
    bool isCellFocused() const              { return m_isCellFocused; }
    void setCellHovered(bool hovered);
    bool isCellHovered() const              { return m_isCellHovered; }
    void setCellPressed(bool pressed);
    bool isCellPressed() const              { return m_isCellPressed; }
    void onFocusChanged(bool hasFocus) override;

    void setWarningVisible(bool visible);
    bool isWarningVisible() const           { return m_warningVisible; }
    void setWarningColor(const Color4& color);
    void setWarningTexture(Texture* texture);
    void setWarningSize(float w, float h);
    void setWarningGravity(Gravity gravity, float margin = 8.0f);

    void addAction(const ActionButton& action);
    void clearActions();
    void triggerAction(int32_t index);
    int32_t getActionCount() const          { return static_cast<int32_t>(m_actions.size()); }
    bool hasActions() const                 { return m_actions.empty() == false; }
    Button* getActionButton(int32_t index);
    const Button* getActionButton(int32_t index) const;

    virtual void setData(void* data) { m_cellData = data; }
    void* getData() const            { return m_cellData; }
    virtual void updateContent()     {}

    bool onMotionEvent(const IO::MotionEvent& event) override;
    void onDraw(ICanvas& canvas) override;

protected:
    void setupContent();
    virtual void updateVisualState();

    bool   m_isCellFocused;
    bool   m_isCellHovered;
    bool   m_isCellPressed;
    bool   m_warningVisible;
    void*  m_cellData;

    AIImageView*    m_aiImageView;
    TextView*       m_nameText;
    TextView*       m_infoText;
    LinearLayout*   m_actionsLayout;
    ImageView*      m_warningOverlay;

    std::vector<ActionButton> m_actions;

    static GridCellView* s_currentlyHoveredCell;
};

// ── AutoGridAdapter ───────────────────────────────────────────────────────────

class AutoGridAdapter
{
public:
    virtual ~AutoGridAdapter() = default;

    virtual int32_t getCount() const = 0;
    virtual GridCellView* createCell() = 0;
    virtual void bindCell(GridCellView* cell, int32_t position) = 0;
    virtual void onCellRecycled(GridCellView*) {}

    void notifyDataSetChanged() { if (m_changeCallback != nullptr) m_changeCallback(); }
    void setOnDataSetChangedCallback(std::function<void()> cb) { m_changeCallback = cb; }

private:
    std::function<void()> m_changeCallback;
};

// ── AutoGridView ──────────────────────────────────────────────────────────────
// Regular adapter-based grid with uniform cells, scrolling, and fullscreen support.

class AutoGridView : public ViewGroup
{
public:
    AutoGridView();

    void setAdapter(std::shared_ptr<AutoGridAdapter> adapter);
    std::shared_ptr<AutoGridAdapter> getAdapter() const { return m_adapter; }

    void setNumColumns(int columns);
    int getNumColumns() const                       { return m_numColumns; }
    void setNumRows(int rows);
    int getNumRows() const                          { return m_numRows; }
    void setHorizontalSpacing(float spacing)        { m_horizontalSpacing = spacing; }
    float getHorizontalSpacing() const              { return m_horizontalSpacing; }
    void setVerticalSpacing(float spacing)          { m_verticalSpacing = spacing; }
    float getVerticalSpacing() const                { return m_verticalSpacing; }
    void setCellAspectRatio(float ratio)            { m_cellAspectRatio = ratio; }
    float getCellAspectRatio() const                { return m_cellAspectRatio; }

    void setFocusedCellIndex(int32_t index);
    int32_t getFocusedCellIndex() const             { return m_focusedCellIndex; }
    bool hasFocusedCell() const                     { return m_focusedCellIndex >= 0; }
    void clearCellFocus();
    bool moveFocusUp();
    bool moveFocusDown();
    bool moveFocusLeft();
    bool moveFocusRight();
    bool canMoveFocusUp() const;
    bool canMoveFocusDown() const;
    bool canMoveFocusLeft() const;
    bool canMoveFocusRight() const;

    void setFullscreenCell(int32_t cellIndex);
    void exitFullscreen();
    bool isInFullscreenMode() const                 { return m_fullscreenCellIndex >= 0; }
    int32_t getFullscreenCellIndex() const          { return m_fullscreenCellIndex; }
    void toggleFullscreen(int32_t cellIndex);

    void scrollTo(float offset);
    void scrollBy(float delta)                      { scrollTo(m_scrollOffset + delta); }
    void scrollToCell(int32_t cellIndex);
    float getScrollOffset() const                   { return m_scrollOffset; }
    float getMaxScrollOffset() const                { return m_maxScrollOffset; }

    void setOnItemClickListener(std::function<void(int32_t)> l)             { m_onItemClick = l; }
    void setOnItemLongClickListener(std::function<void(int32_t)> l)         { m_onItemLongClick = l; }
    void setOnFullscreenChangeListener(std::function<void(int32_t,bool)> l) { m_onFullscreenChange = l; }

    void onMeasure(MeasureSpec widthSpec, MeasureSpec heightSpec) override;
    void onDraw(ICanvas& canvas) override;
    bool onKeyEvent(const IO::KeyEvent& event) override;
    bool onMotionEvent(const IO::MotionEvent& event) override;

protected:
    void layoutChildren() override;

private:
    void rebuildCells();
    void recalculateGrid();
    void updateCellFocus();
    GridCellView* getCellAt(int32_t index) const;
    void calculateCellSize(float availableWidth, float availableHeight,
                           float& outCellWidth, float& outCellHeight);
    void layoutNormalGrid();
    void layoutFullscreenCell();

    std::shared_ptr<AutoGridAdapter> m_adapter;
    std::vector<GridCellView*>       m_cells;

    int   m_numColumns;
    int   m_numRows;
    float m_horizontalSpacing;
    float m_verticalSpacing;
    float m_cellAspectRatio;
    float m_scrollOffset;
    float m_maxScrollOffset;
    Vec2  m_lastTouchPos;
    bool  m_isDragging;
    float m_dragStartY;
    float m_dragThreshold;

    int32_t m_focusedCellIndex;
    int32_t m_fullscreenCellIndex;

    std::function<void(int32_t)>       m_onItemClick;
    std::function<void(int32_t)>       m_onItemLongClick;
    std::function<void(int32_t, bool)> m_onFullscreenChange;
};

// ── CustomGridView ────────────────────────────────────────────────────────────
// A topology-based grid where each cell is explicitly positioned by row/column/span.
// Computes geometry internally from screen dimensions and cell slot descriptors.
// Cells are GridCellView subclasses created externally and registered via addCell().

struct GridCellSlot
{
    bool enabled    = false; // false → cell is not created/shown
    int  row        = 0;     // logical row index (0-based, top to bottom)
    int  col        = 0;     // logical column index (0-based, left to right)
    int  colSpan    = 1;     // number of columns this cell spans
    int  rowWeight  = 1;     // relative height weight (proportional to total weight)
};

struct GridCellRect
{
    float x = 0.0f, y = 0.0f, w = 0.0f, h = 0.0f;
};

class CustomGridView : public FrameLayout
{
public:
    CustomGridView();

    // Slot topology — must be set before addCell() calls.
    void setSlots(const std::vector<GridCellSlot>& slots) { m_slots = slots; }
    void setSlot(int index, const GridCellSlot& slot);
    const std::vector<GridCellSlot>& getSlots() const     { return m_slots; }
    int getSlotCount() const                              { return static_cast<int>(m_slots.size()); }

    // Gap between cells (default: 8 px).
    void setGap(float gap)        { m_gap = gap; }
    float getGap() const          { return m_gap; }

    // Aspect ratio of a single unit cell (width / height, default: 16.0f/9.0f).
    void setAspectRatio(float ratio) { m_aspectRatio = (ratio > 0.0f) ? ratio : 16.0f / 9.0f; }
    float getAspectRatio() const     { return m_aspectRatio; }

    // Register a GridCellView for a slot.
    bool addCell(int slotIndex, GridCellView* cell);
    GridCellView* getCellAt(int slotIndex) const;
    int getCellCount() const { return static_cast<int>(m_cells.size()); }

    // Geometry queries — valid after onLayout() has been called once.
    const GridCellRect& getCellRect(int slotIndex) const;
    float getGridWidth()  const { return m_gridWidth; }
    float getGridHeight() const { return m_gridHeight; }

    // Fullscreen support: expand one cell to fill the entire view.
    void enterFullscreen(int slotIndex);
    void exitFullscreen();
    bool isFullscreen() const    { return m_fullscreenSlot >= 0; }
    int getFullscreenSlot() const { return m_fullscreenSlot; }
    void toggleFullscreen(int slotIndex);

    // Recompute geometry and re-apply layout params to all registered cells.
    void rebuildLayout(int screenW, int screenH);

    void onLayout(const RectF& bounds) override;

private:
    void computeGeometry(int screenW, int screenH);
    void applyNormalLayout();
    void applyFullscreenLayout();

    std::vector<GridCellSlot>    m_slots;
    std::vector<GridCellView*>   m_cells;
    std::vector<GridCellRect>    m_cellRects;
    float                        m_gap;
    float                        m_aspectRatio;
    float                        m_gridWidth;
    float                        m_gridHeight;
    int                          m_fullscreenSlot;

    static const GridCellRect    s_emptyRect;
};

// ── PiPView ───────────────────────────────────────────────────────────────────
// Picture-in-Picture layout

enum class PiPDock { TOP_LEFT, TOP_RIGHT, BOTTOM_LEFT, BOTTOM_RIGHT };

class PiPView : public FrameLayout
{
public:
    PiPView();

    // ── Cell registration ─────────────────────────────────────────────────
    void          addCell(GridCellView* cell);
    void          clearCells();
    GridCellView* getCellAt(int index) const;
    int           getCellCount() const { return static_cast<int>(m_cells.size()); }

    // ── Primary cell ──────────────────────────────────────────────────────
    void setPrimaryIndex(int index);
    int  getPrimaryIndex() const { return m_primaryIndex; }

    // ── Thumbnail strip settings ──────────────────────────────────────────
    void          setDock(PiPDock dock)              { m_dock = dock; }
    PiPDock       getDock() const                    { return m_dock; }
    void          setOrientation(Orientation o)      { m_orientation = o; }
    Orientation   getOrientation() const             { return m_orientation; }
    void          setThumbnailSize(float w, float h) { m_thumbW = w; m_thumbH = h; }
    float         getThumbnailWidth()  const         { return m_thumbW; }
    float         getThumbnailHeight() const         { return m_thumbH; }
    void          setSpacing(float spacing)          { m_spacing = spacing; }
    float         getSpacing() const                 { return m_spacing; }
    void          setMargin(float margin)            { m_margin = margin; }
    float         getMargin() const                  { return m_margin; }

    // ── Layout ────────────────────────────────────────────────────────────
    void rebuildLayout(int screenW, int screenH);
    void onLayout(const RectF& bounds) override;

private:
    void applyLayout();

    std::vector<GridCellView*> m_cells;

    int          m_primaryIndex;
    PiPDock      m_dock;
    Orientation  m_orientation;
    float        m_thumbW;
    float        m_thumbH;
    float        m_spacing;
    float        m_margin;
    float        m_viewW;
    float        m_viewH;
};

// ── NumpadView ───────────────────────────────────────────────────────────────
// 3x4 numpad keyboard for numeric input.

class NumpadView : public LinearLayout
{
public:
    NumpadView();
    virtual ~NumpadView();

    void setOnNumberClick(std::function<void(int)> cb) { m_onNumberClick = cb; }
    void setOnDotClick(std::function<void()> cb)       { m_onDotClick = cb; }
    void setOnDeleteClick(std::function<void()> cb)    { m_onDeleteClick = cb; }

    void setDotEnabled(bool enabled);
    void setDeleteEnabled(bool enabled);
    bool handleKeyEvent(const IO::KeyEvent& event);

private:
    void setupUI();
    void createButton(const std::string& text, int row, int col,
                      std::function<void()> callback, bool isSpecial = false);

    LinearLayout* m_rows[3];
    Button*       m_numberButtons[10];
    Button*       m_dotButton;
    Button*       m_deleteButton;

    std::function<void(int)> m_onNumberClick;
    std::function<void()>    m_onDotClick;
    std::function<void()>    m_onDeleteClick;
};

// ── NumpadPopup ──────────────────────────────────────────────────────────────
// Modal popup with numpad for numeric input.

class NumpadPopup : public FrameLayout
{
public:
    NumpadPopup();
    virtual ~NumpadPopup();

    void show(const std::string& title, const std::string& initialValue = "", bool allowDecimal = true);
    void setCancelText(const std::string& text);
    void setConfirmText(const std::string& text);
    void hide();
    bool isShowing() const { return m_isShowing; }

    void setFocusManager(FocusManager* fm)         { m_focusManager = fm; }
    FocusManager* getFocusManager() const          { return m_focusManager; }

    void setOnConfirm(std::function<void(const std::string&)> cb) { m_onConfirm = cb; }
    void setOnCancel(std::function<void()> cb)     { m_onCancel = cb; }

    void setMaxLength(int maxLength)               { m_maxLength = maxLength; }
    void setMaxValue(float maxValue)               { m_maxValue = maxValue; }
    void setMinValue(float minValue)               { m_minValue = minValue; }
    void setAllowDecimal(bool allow)               { m_allowDecimal = allow; }
    void setAllowNegative(bool allow)              { m_allowNegative = allow; }

    bool handleKeyEvent(const IO::KeyEvent& event);
    bool handleMotionEvent(const IO::MotionEvent& event);
    void onDraw(ICanvas& canvas) override;

private:
    void setupUI();
    void setupFocusNavigation();
    void registerPopupFocusables();
    void unregisterPopupFocusables();
    void updateDisplay();
    void handleNumberInput(int number);
    void handleDotInput();
    void handleDeleteInput();
    void handleConfirm();
    void handleCancel();
    bool isValidInput() const;
    float getCurrentValue() const;

    bool        m_isShowing;
    std::string m_currentInput;
    std::string m_title;
    bool        m_allowDecimal;
    bool        m_allowNegative;
    int         m_maxLength;
    float       m_maxValue;
    float       m_minValue;

    FrameLayout*  m_overlay;
    LinearLayout* m_dialogContainer;
    TextView*     m_titleText;
    FrameLayout*  m_displayContainer;
    TextView*     m_displayText;
    NumpadView*   m_numpadView;
    LinearLayout* m_buttonRow;
    Button*       m_cancelButton;
    Button*       m_confirmButton;

    FocusManager*                 m_focusManager;
    std::shared_ptr<FocusContext> m_popupContext;

    std::function<void(const std::string&)> m_onConfirm;
    std::function<void()>                   m_onCancel;
};

// ── PopupView ─────────────────────────────────────────────────────────────────
// Modal popup: dark full-screen overlay + centred card (icon, title, detail, buttons).
// Add once to root FrameLayout (MATCH_PARENT × MATCH_PARENT). GONE by default.

enum class PopupButtonMode { OK_ONLY, OK_CANCEL };

class PopupView : public FrameLayout
{
public:
    PopupView();
    virtual ~PopupView();

    void setIcon(Texture* texture);
    void setTitle(const std::string& title);
    void setDetail(const std::string& detail);
    void setButtonMode(PopupButtonMode mode);
    void setOkText(const std::string& text);
    void setCancelText(const std::string& text);

    void show(const std::string& title, const std::string& detail,
              Texture* icon = nullptr,
              PopupButtonMode mode = PopupButtonMode::OK_CANCEL,
              const std::string& okText     = "OK",
              const std::string& cancelText = "Cancel");
    void hide();
    bool isShowing() const { return m_isShowing; }

    void setFocusManager(FocusManager* mgr);
    FocusManager* getFocusManager() const { return m_focusManager; }

    void setOnConfirm(std::function<void()> cb) { m_onConfirm = std::move(cb); }
    void setOnCancel(std::function<void()> cb)  { m_onCancel  = std::move(cb); }

    bool handleKeyEvent(const IO::KeyEvent& event);
    bool handleMotionEvent(const IO::MotionEvent& event);
    void onDraw(ICanvas& canvas) override;
    void update(float deltaTime);

private:
    void buildUI();
    void registerFocusables();
    void unregisterFocusables();
    void applyButtonMode();
    void applyButtonFocus(Button* btn, bool focused);
    void fireConfirm();
    void fireCancel();

    bool            m_isShowing;
    PopupButtonMode m_buttonMode;
    FocusManager*   m_focusManager;
    std::shared_ptr<FocusContext> m_popupContext;

    FrameLayout*  m_overlay;
    LinearLayout* m_card;
    ImageView*    m_iconView;
    TextView*     m_titleView;
    TextView*     m_detailView;
    LinearLayout* m_buttonRow;
    Button*       m_okButton;
    Button*       m_cancelButton;

    Button* m_focusedButton = nullptr;

    std::function<void()> m_onConfirm;
    std::function<void()> m_onCancel;
};

} // namespace UI
} // namespace APP

#endif // UI_ELEMENTS_H
