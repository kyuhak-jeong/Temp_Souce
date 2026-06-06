#include "ui_elements.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <cmath>

namespace APP
{

namespace UI
{

// ============================================================================
// TextView Implementation
// ============================================================================

TextView::TextView()
    : m_textGravity(Gravity::CENTER)
{
    m_textPaint.fgColor = Color::White;
    m_textPaint.opacity = 1.0f;
    m_textPaint.textProps = TextProps("default", TextSize::Small);
}

void TextView::onMeasure(MeasureSpec widthSpec, MeasureSpec heightSpec)
{
    Vec2 textSize(0.0f, 0.0f);
    
    if (m_text.empty() == false)
    {
        textSize = FontManager::getInstance().measureText(m_text, m_textPaint);
    }
    
    float contentWidth = textSize.x + m_layoutParams.getPaddingHorizontal();
    float contentHeight = textSize.y + m_layoutParams.getPaddingVertical();
    
    float width = 0.0f;
    float height = 0.0f;
    
    if (m_layoutParams.isExactWidth() == true) width = m_layoutParams.width;
    else if (m_layoutParams.isMatchParentWidth() == true)
    {
        if (widthSpec.mode == MeasureSpecMode::EXACTLY || widthSpec.mode == MeasureSpecMode::AT_MOST)
            width = widthSpec.size;
        else
            width = contentWidth;
    }
    else
    {
        width = contentWidth;
        if (widthSpec.mode == MeasureSpecMode::AT_MOST)
            width = std::min(width, widthSpec.size);
    }
    
    if (m_layoutParams.isExactHeight() == true) height = m_layoutParams.height;
    else if (m_layoutParams.isMatchParentHeight() == true)
    {
        if (heightSpec.mode == MeasureSpecMode::EXACTLY || heightSpec.mode == MeasureSpecMode::AT_MOST)
            height = heightSpec.size;
        else
            height = contentHeight;
    }
    else
    {
        height = contentHeight;
        if (heightSpec.mode == MeasureSpecMode::AT_MOST)
            height = std::min(height, heightSpec.size);
    }
    
    m_measuredSize = Vec2(width, height);
}

void TextView::update(float deltaTime)
{
    if (m_marquee == false || m_text.empty() == true) return;

    const float textW   = FontManager::getInstance().measureText(m_text, m_textPaint).x;
    const float boundsW = getContentBounds().width();

    if (textW <= boundsW) { m_marqueeOffset = 0.0f; m_marqueeDelay = MARQUEE_PAUSE_SEC; return; }

    if (m_marqueeDelay > 0.0f) { m_marqueeDelay -= deltaTime; return; }

    m_marqueeOffset += MARQUEE_SPEED * deltaTime;
    if (m_marqueeOffset >= textW - boundsW + MARQUEE_GAP * 2.0f)
    {
        m_marqueeOffset = 0.0f;
        m_marqueeDelay  = MARQUEE_PAUSE_SEC;
    }
}

void TextView::onDraw(ICanvas& canvas)
{
    if (isVisible() == false) return;

    View::onDraw(canvas);

    if (m_text.empty() == true) return;

    RectF contentBounds = getContentBounds();
    Vec2  textSize      = FontManager::getInstance().measureText(m_text, m_textPaint);

    canvas.pushClip(m_bounds);

    if (m_marquee == true && m_marqueeOffset > 0.0f)
    {
        const float inset = getCornerRadius();
        if (inset > 0.0f)
        {
            canvas.pushClip(RectF::fromLTRB(m_bounds.left  + inset, m_bounds.top,
                                            m_bounds.right - inset, m_bounds.bottom));
        }
        float y = contentBounds.top + (contentBounds.height() - textSize.y) * 0.5f;
        canvas.drawText(m_text, Vec2(contentBounds.left + MARQUEE_GAP - m_marqueeOffset, y), m_textPaint);
        if (inset > 0.0f) canvas.popClip();
    }
    else
    {
        RectF        textBounds;
        LayoutParams tempLp;
        applyGravity(textSize, contentBounds, m_textGravity, tempLp, textBounds);
        canvas.drawText(m_text, Vec2(textBounds.left, textBounds.top), m_textPaint);
    }

    canvas.popClip();
}

// ============================================================================
// ImageView Implementation
// ============================================================================

ImageView::ImageView()
    : m_texture(nullptr)
    , m_scaleType(ScaleType::FIT_CENTER)
    , m_isVerticalFlip(false)
{
    m_imagePaint.bgColor = Color::White;
    m_imagePaint.opacity = 1.0f;
}

ImageView::~ImageView()
{
    m_texture = nullptr;
}

void ImageView::onMeasure(MeasureSpec widthSpec, MeasureSpec heightSpec)
{
    float width = 100.0f;
    float height = 100.0f;
    
    if (m_texture != nullptr && m_texture->isValid() == true)
    {
        width = m_texture->width;
        height = m_texture->height;
    }
    
    if (m_layoutParams.isExactWidth() == true) width = m_layoutParams.width;
    else if (m_layoutParams.isMatchParentWidth() == true)
    {
        if (widthSpec.mode == MeasureSpecMode::EXACTLY || widthSpec.mode == MeasureSpecMode::AT_MOST) width = widthSpec.size;
    }
    else
    {
        if (widthSpec.mode == MeasureSpecMode::AT_MOST) width = std::min(width, widthSpec.size);
    }
    
    if (m_layoutParams.isExactHeight() == true) height = m_layoutParams.height;
    else if (m_layoutParams.isMatchParentHeight() == true)
    {
        if (heightSpec.mode == MeasureSpecMode::EXACTLY || heightSpec.mode == MeasureSpecMode::AT_MOST) height = heightSpec.size;
    }
    else
    {
        if (heightSpec.mode == MeasureSpecMode::AT_MOST) height = std::min(height, heightSpec.size);
    }
    
    m_measuredSize = Vec2(width, height);
}

RectF ImageView::getImageRect() const
{
    // Returns the rect where the image is actually drawn (not the full view bounds).
    return calculateDrawRect();
}

RectF ImageView::calculateDrawRect() const
{
    if (m_texture == nullptr || m_texture->isValid() == false) return getContentBounds();
    
    RectF contentBounds = getContentBounds();
    float texWidth = m_texture->width;
    float texHeight = m_texture->height;
    float viewWidth = contentBounds.width();
    float viewHeight = contentBounds.height();
    LayoutParams emptyParams;
    
    switch (m_scaleType)
    {
        case ScaleType::FIT_XY:
            return contentBounds;
        
        case ScaleType::FIT_CENTER:
        {
            float scale = std::min(viewWidth / texWidth, viewHeight / texHeight);
            float scaledWidth = texWidth * scale;
            float scaledHeight = texHeight * scale;
            RectF drawRect;
            applyGravity(Vec2(scaledWidth, scaledHeight), contentBounds, Gravity::CENTER, emptyParams, drawRect);
            return drawRect;
        }
        
        case ScaleType::CENTER_CROP:
        {
            float scale = std::max(viewWidth / texWidth, viewHeight / texHeight);
            float scaledWidth = texWidth * scale;
            float scaledHeight = texHeight * scale;
            RectF drawRect;
            applyGravity(Vec2(scaledWidth, scaledHeight), contentBounds, Gravity::CENTER, emptyParams, drawRect);
            return drawRect;
        }
        
        case ScaleType::CENTER_INSIDE:
        {
            if (texWidth <= viewWidth && texHeight <= viewHeight)
            {
                RectF drawRect;
                applyGravity(Vec2(texWidth, texHeight), contentBounds, Gravity::CENTER, emptyParams, drawRect);
                return drawRect;
            }
            else
            {
                float scale = std::min(viewWidth / texWidth, viewHeight / texHeight);
                float scaledWidth = texWidth * scale;
                float scaledHeight = texHeight * scale;
                RectF drawRect;
                applyGravity(Vec2(scaledWidth, scaledHeight), contentBounds, Gravity::CENTER, emptyParams, drawRect);
                return drawRect;
            }
        }
    }
    
    return contentBounds;
}

void ImageView::onDraw(ICanvas& canvas)
{
    if (isVisible() == false) return;
    
    drawBackground(canvas);
    
    if (m_texture == nullptr || m_texture->isValid() == false)
    {
        drawFocusBorder(canvas);
        return;
    }
    
    RectF srcRect = RectF::fromLTRB(0, 0, m_texture->width, m_texture->height);

    if (m_isVerticalFlip == true)
    {
        float temp = srcRect.top;
        srcRect.top = srcRect.bottom;
        srcRect.bottom = temp;
    }

    RectF dstRect = calculateDrawRect();
    
    Paint texPaint = m_imagePaint;
    texPaint.cornerRadius = m_normalPaint.cornerRadius;
    
    canvas.pushClip(m_bounds);
    canvas.drawTexture(m_texture, srcRect, dstRect, texPaint);
    canvas.popClip();
    
    drawFocusBorder(canvas);
}

// ============================================================================
// Button Implementation
// ============================================================================

Button* Button::s_currentlyHoveredButton = nullptr;

Button::Button()
    : m_texture(nullptr)
    , m_imageWidth(32.0f)
    , m_imageHeight(32.0f)
    , m_contentOrientation(Orientation::HORIZONTAL)
    , m_contentSpacing(8.0f)
    , m_contentGravity(Gravity::CENTER)
{
    m_textPaint.fgColor = Color::White;
    m_textPaint.opacity = 1.0f;
    m_textPaint.textProps = TextProps("default", TextSize::Small);
    
    m_imagePaint.bgColor = Color::White;
    m_imagePaint.opacity = 1.0f;
    
    m_focusable = true;
    
    setNormalColor(Color::Transparent);
    setFocusedColor(Color::Transparent);
    setHoverColor(Color::AccentPrimary);
    setPressedColor(Color::AccentTertiary);
    setCornerRadius(8.0f);
}

Button::~Button()
{
    if (s_currentlyHoveredButton == this) s_currentlyHoveredButton = nullptr;
}

void Button::onMeasure(MeasureSpec widthSpec, MeasureSpec heightSpec)
{
    bool hasText = (m_text.empty() == false);
    bool hasImage = (m_texture != nullptr && m_texture->isValid() == true);
    
    Vec2 textSize(0.0f, 0.0f);
    Vec2 imageSize(0.0f, 0.0f);
    
    if (hasText == true)
    {
        textSize = FontManager::getInstance().measureText(m_text, m_textPaint);
    }
    
    if (hasImage == true)
    {
        imageSize = Vec2(m_imageWidth, m_imageHeight);
    }
    
    float contentWidth = 0.0f;
    float contentHeight = 0.0f;
    
    if (hasText == true && hasImage == true)
    {
        // Both text and image
        if (m_contentOrientation == Orientation::HORIZONTAL)
        {
            contentWidth = imageSize.x + m_contentSpacing + textSize.x;
            contentHeight = std::max(imageSize.y, textSize.y);
        }
        else // VERTICAL
        {
            contentWidth = std::max(imageSize.x, textSize.x);
            contentHeight = imageSize.y + m_contentSpacing + textSize.y;
        }
    }
    else if (hasText == true)
    {
        // Text only
        contentWidth = textSize.x;
        contentHeight = textSize.y;
    }
    else if (hasImage == true)
    {
        // Image only
        contentWidth = imageSize.x;
        contentHeight = imageSize.y;
    }
    
    contentWidth += 20.0f + m_layoutParams.getPaddingHorizontal();
    contentHeight += 10.0f + m_layoutParams.getPaddingVertical();
    
    float width = 0.0f;
    float height = 0.0f;
    
    if (m_layoutParams.isExactWidth() == true) width = m_layoutParams.width;
    else if (m_layoutParams.isMatchParentWidth() == true)
    {
        if (widthSpec.mode == MeasureSpecMode::EXACTLY || widthSpec.mode == MeasureSpecMode::AT_MOST)
            width = widthSpec.size;
        else
            width = contentWidth;
    }
    else
    {
        width = contentWidth;
        if (widthSpec.mode == MeasureSpecMode::AT_MOST)
            width = std::min(width, widthSpec.size);
    }
    
    if (m_layoutParams.isExactHeight() == true) height = m_layoutParams.height;
    else if (m_layoutParams.isMatchParentHeight() == true)
    {
        if (heightSpec.mode == MeasureSpecMode::EXACTLY || heightSpec.mode == MeasureSpecMode::AT_MOST)
            height = heightSpec.size;
        else
            height = contentHeight;
    }
    else
    {
        height = contentHeight;
        if (heightSpec.mode == MeasureSpecMode::AT_MOST)
            height = std::min(height, heightSpec.size);
    }
    
    m_measuredSize = Vec2(width, height);
}

RectF Button::calculateImageRect(const RectF& availableBounds, const Vec2& imageSize) const
{
    RectF imageRect;
    LayoutParams tempLp;
    applyGravity(imageSize, availableBounds, Gravity::CENTER, tempLp, imageRect);
    return imageRect;
}

void Button::onDraw(ICanvas& canvas)
{
    if (isVisible() == false) return;
    
    drawBackground(canvas);
    
    bool hasText = (m_text.empty() == false);
    bool hasImage = (m_texture != nullptr && m_texture->isValid() == true);
    
    if (hasText == false && hasImage == false)
    {
        const Paint& currentPaint = getCurrentPaint();
        Color4 fgColor = currentPaint.getEffectiveFgColor();
        
        if (currentPaint.strokeWidth > 0 && fgColor.a > 0)
        {
            Paint borderPaint;
            borderPaint.bgColor = Color::Transparent;
            borderPaint.fgColor = fgColor;
            borderPaint.filled = false;
            borderPaint.strokeWidth = currentPaint.strokeWidth;
            borderPaint.cornerRadius = currentPaint.cornerRadius;
            borderPaint.opacity = currentPaint.opacity;
            canvas.drawRect(m_bounds, borderPaint);
        }
        
        drawFocusBorder(canvas);
        return;
    }
    
    RectF contentBounds = getContentBounds();
    
    Vec2 textSize(0.0f, 0.0f);
    Vec2 imageSize(0.0f, 0.0f);
    
    if (hasText == true)
    {
        textSize = FontManager::getInstance().measureText(m_text, m_textPaint);
    }
    
    if (hasImage == true)
    {
        imageSize = Vec2(m_imageWidth, m_imageHeight);
    }
    
    if (hasText == true && hasImage == true)
    {
        // Both text and image
        Vec2 totalSize;
        if (m_contentOrientation == Orientation::HORIZONTAL)
        {
            totalSize = Vec2(imageSize.x + m_contentSpacing + textSize.x, 
                           std::max(imageSize.y, textSize.y));
        }
        else // VERTICAL
        {
            totalSize = Vec2(std::max(imageSize.x, textSize.x), 
                           imageSize.y + m_contentSpacing + textSize.y);
        }
        
        RectF contentRect;
        LayoutParams tempLp;
        applyGravity(totalSize, contentBounds, m_contentGravity, tempLp, contentRect);
        
        if (m_contentOrientation == Orientation::HORIZONTAL)
        {
            RectF imageBounds = RectF::fromXYWH(
                contentRect.left, 
                contentRect.top + (totalSize.y - imageSize.y) * 0.5f,
                imageSize.x, 
                imageSize.y
            );
            
            RectF textBounds = RectF::fromXYWH(
                contentRect.left + imageSize.x + m_contentSpacing,
                contentRect.top + (totalSize.y - textSize.y) * 0.5f,
                textSize.x, 
                textSize.y
            );

            // Draw image
            RectF srcRect = RectF::fromLTRB(0, 0, m_texture->width, m_texture->height);
            RectF dstRect = calculateImageRect(imageBounds, imageSize);
            Paint imgPaint = m_imagePaint;
            imgPaint.cornerRadius = 0.0f;
            canvas.drawTexture(m_texture, srcRect, dstRect, imgPaint);
            
            // Draw text - position is top-left of the text bounding box
            canvas.drawText(m_text, Vec2(textBounds.left, textBounds.top), m_textPaint);
        }
        else // VERTICAL
        {
            // Image on top, text on bottom
            RectF imageBounds = RectF::fromXYWH(contentRect.left + (totalSize.x - imageSize.x) * 0.5f,
                                               contentRect.top,
                                               imageSize.x, imageSize.y);
            RectF textBounds = RectF::fromXYWH(contentRect.left + (totalSize.x - textSize.x) * 0.5f,
                                              contentRect.top + imageSize.y + m_contentSpacing,
                                              textSize.x, textSize.y);
            
            // Draw image
            RectF srcRect = RectF::fromLTRB(0, 0, m_texture->width, m_texture->height);
            RectF dstRect = calculateImageRect(imageBounds, imageSize);
            Paint imgPaint = m_imagePaint;
            imgPaint.cornerRadius = 0.0f;
            canvas.drawTexture(m_texture, srcRect, dstRect, imgPaint);
            
            // Draw text
            canvas.drawText(m_text, Vec2(textBounds.left, textBounds.top), m_textPaint);
        }
    }
    else if (hasText == true)
    {
        // Text only
        RectF textBounds;
        LayoutParams tempLp;
        applyGravity(textSize, contentBounds, m_contentGravity, tempLp, textBounds);
        canvas.drawText(m_text, Vec2(textBounds.left, textBounds.top), m_textPaint);
    }
    else if (hasImage == true)
    {
        // Image only
        RectF imageBounds;
        LayoutParams tempLp;
        applyGravity(imageSize, contentBounds, m_contentGravity, tempLp, imageBounds);
        
        RectF srcRect = RectF::fromLTRB(0, 0, m_texture->width, m_texture->height);
        RectF dstRect = calculateImageRect(imageBounds, imageSize);
        Paint imgPaint = m_imagePaint;
        imgPaint.cornerRadius = 0.0f;
        canvas.drawTexture(m_texture, srcRect, dstRect, imgPaint);
    }
    
    const Paint& currentPaint = getCurrentPaint();
    Color4 fgColor = currentPaint.getEffectiveFgColor();
    
    if (currentPaint.strokeWidth > 0 && fgColor.a > 0)
    {
        Paint borderPaint;
        borderPaint.bgColor = Color::Transparent;
        borderPaint.fgColor = fgColor;
        borderPaint.filled = false;
        borderPaint.strokeWidth = currentPaint.strokeWidth;
        borderPaint.cornerRadius = currentPaint.cornerRadius;
        borderPaint.opacity = currentPaint.opacity;
        canvas.drawRect(m_bounds, borderPaint);
    }
    
    drawFocusBorder(canvas);
}

bool Button::onMotionEvent(const IO::MotionEvent& event)
{
    if (event.action == IO::MotionAction::DOWN && event.button == IO::MouseButton::LEFT)
    {
        if (m_bounds.contains(event.x, event.y) == true)
        {
            setPressed(true);
            return true;
        }
    }
    else if (event.action == IO::MotionAction::UP)
    {
        if (m_isPressed == true && m_bounds.contains(event.x, event.y) == true && m_onClickCallback != nullptr) m_onClickCallback(this);
        
        setPressed(false);
        setHovered(m_bounds.contains(event.x, event.y));
        return true;
    }
    else if (event.action == IO::MotionAction::HOVER_MOVE || event.action == IO::MotionAction::MOVE)
    {
        bool isInside = m_bounds.contains(event.x, event.y);
        
        if (isInside == true)
        {
            if (s_currentlyHoveredButton != nullptr && s_currentlyHoveredButton != this) s_currentlyHoveredButton->setHovered(false);
            
            if (m_isHovered == false)
            {
                setHovered(true);
                s_currentlyHoveredButton = this;
            }
        }
        else
        {
            if (m_isHovered == true && s_currentlyHoveredButton == this)
            {
                setHovered(false);
                s_currentlyHoveredButton = nullptr;
            }
        }
        
        return isInside;
    }
    else if (event.action == IO::MotionAction::HOVER_EXIT)
    {
        if (m_isHovered == true && s_currentlyHoveredButton == this)
        {
            setHovered(false);
            s_currentlyHoveredButton = nullptr;
        }
        return true;
    }
    
    return View::onMotionEvent(event);
}

void Button::onFocusChanged(bool hasFocus)
{
    View::onFocusChanged(hasFocus);
    setPressed(false);
    
    if (hasFocus == true && m_isHovered == true)
    {
        setHovered(false);
        if (s_currentlyHoveredButton == this) s_currentlyHoveredButton = nullptr;
    }
}

// ============================================================================
// CheckBox Implementation
// ============================================================================

CheckBox::CheckBox()
    : m_checked(false)
    , m_focusHighlight(true)
    , m_orientation(Orientation::HORIZONTAL)
    , m_textGravity(Gravity::LEFT | Gravity::CENTER_VERTICAL)
    , m_spacing(20.0f)
    , m_checkBoxSize(24.0f)
    , m_onCheckedChange(nullptr)
{
    m_textPaint.fgColor       = Color::TextPrimary;
    m_textPaint.opacity       = 1.0f;
    m_textPaint.textProps     = TextProps("default", TextSize::Small);
    m_checkPaint.bgColor      = Color::AccentPrimary;
    m_checkPaint.opacity      = 1.0f;
    m_checkPaint.filled       = true;
    m_checkPaint.cornerRadius = 4.0f;
    m_focusable               = true;
}

void CheckBox::onMeasure(MeasureSpec widthSpec, MeasureSpec heightSpec)
{
    const bool hasText = (m_text.empty() == false);
    Vec2 ts = hasText ? FontManager::getInstance().measureText(m_text, m_textPaint) : Vec2(0.0f, 0.0f);

    float w = 0.0f, h = 0.0f;
    if (m_orientation == Orientation::HORIZONTAL)
    {
        w = m_checkBoxSize + (hasText ? m_spacing + ts.x : 0.0f);
        h = std::max(m_checkBoxSize, ts.y);
    }
    else
    {
        w = std::max(m_checkBoxSize, ts.x);
        h = m_checkBoxSize + (hasText ? m_spacing + ts.y : 0.0f);
    }

    if      (m_layoutParams.isExactWidth())       w = m_layoutParams.width;
    else if (m_layoutParams.isMatchParentWidth())
    {
        if (widthSpec.mode == MeasureSpecMode::EXACTLY || widthSpec.mode == MeasureSpecMode::AT_MOST)
            w = widthSpec.size;
    }
    else if (widthSpec.mode == MeasureSpecMode::AT_MOST) w = std::min(w, widthSpec.size);

    if      (m_layoutParams.isExactHeight())      h = m_layoutParams.height;
    else if (m_layoutParams.isMatchParentHeight())
    {
        if (heightSpec.mode == MeasureSpecMode::EXACTLY || heightSpec.mode == MeasureSpecMode::AT_MOST)
            h = heightSpec.size;
    }
    else if (heightSpec.mode == MeasureSpecMode::AT_MOST) h = std::min(h, heightSpec.size);

    m_measuredSize = Vec2(w, h);
}

void CheckBox::onDraw(ICanvas& canvas)
{
    View::onDraw(canvas);

    const float bx   = m_bounds.left;
    const float by   = m_bounds.top;
    const float bw   = m_bounds.width();
    const float bh   = m_bounds.height();
    const float half = m_checkBoxSize * 0.5f;

    // Box position
    const float boxLeft = (m_orientation == Orientation::HORIZONTAL) ? bx : bx + bw * 0.5f - half;
    const float boxTop  = (m_orientation == Orientation::HORIZONTAL) ? by + bh * 0.5f - half : by;
    const RectF boxRect = RectF::fromXYWH(boxLeft, boxTop, m_checkBoxSize, m_checkBoxSize);

    // Box border
    Paint borderPaint;
    borderPaint.bgColor      = Color::Transparent;
    borderPaint.fgColor      = m_focusHighlight ? Color::BorderFocus : Color::Border;
    borderPaint.filled       = false;
    borderPaint.strokeWidth  = m_focusHighlight ? 6.0f : 6.0f;
    borderPaint.cornerRadius = 4.0f;
    borderPaint.opacity      = 1.0f;
    canvas.drawRect(boxRect, borderPaint);

    if (m_checked == true) canvas.drawRect(boxRect.inset(6.0f), m_checkPaint);

    // Label
    if (m_text.empty() == false)
    {
        const Vec2 ts = FontManager::getInstance().measureText(m_text, m_textPaint);
        float tx = 0.0f, ty = 0.0f;

        if (m_orientation == Orientation::HORIZONTAL)
        {
            // Text region: to the right of the box
            const float rLeft = boxLeft + m_checkBoxSize + m_spacing;
            const float rW    = bx + bw - rLeft;
            tx = hasGravity(m_textGravity, Gravity::RIGHT)             ? rLeft + rW - ts.x
               : hasGravity(m_textGravity, Gravity::CENTER_HORIZONTAL) ? rLeft + (rW - ts.x) * 0.5f
                                                                       : rLeft;
            ty = hasGravity(m_textGravity, Gravity::BOTTOM)            ? by + bh - ts.y
               : hasGravity(m_textGravity, Gravity::CENTER_VERTICAL)   ? by + (bh - ts.y) * 0.5f
                                                                       : by;
        }
        else
        {
            // Text region: below the box
            const float rTop = boxTop + m_checkBoxSize + m_spacing;
            const float rH   = by + bh - rTop;
            tx = hasGravity(m_textGravity, Gravity::RIGHT)             ? bx + bw - ts.x
               : hasGravity(m_textGravity, Gravity::CENTER_HORIZONTAL) ? bx + (bw - ts.x) * 0.5f
                                                                       : bx;
            ty = hasGravity(m_textGravity, Gravity::BOTTOM)            ? rTop + rH - ts.y
               : hasGravity(m_textGravity, Gravity::CENTER_VERTICAL)   ? rTop + (rH - ts.y) * 0.5f
                                                                       : rTop;
        }

        canvas.drawText(m_text, Vec2(tx, ty), m_textPaint);
    }
}

bool CheckBox::onMotionEvent(const IO::MotionEvent& event)
{
    if (event.action == IO::MotionAction::DOWN
        && event.button == IO::MouseButton::LEFT
        && m_bounds.contains(event.x, event.y) == true)
    {
        m_checked = !m_checked;
        if (m_onCheckedChange != nullptr) m_onCheckedChange(this, m_checked);
        return true;
    }
    return View::onMotionEvent(event);
}

// ============================================================================
// SliderView Implementation
// ============================================================================

SliderView::SliderView()
    : m_value(0.0f)
    , m_valueBeforeAdjust(0.0f)
    , m_minValue(0.0f)
    , m_maxValue(100.0f)
    , m_step(1.0f)
    , m_orientation(Orientation::HORIZONTAL)
    , m_textPosition(TextPosition::AFTER)
    , m_textGravity(Gravity::CENTER)
    , m_labelSpacing(8.0f)
    , m_valueFormat("%.0f")
    , m_trackThickness(8.0f)
    , m_thumbRadius(10.0f)
    , m_trackColor(Color4::fromRGBA(60, 70, 90, 200))
    , m_fillColor(Color::AccentPrimary)
    , m_thumbColor(Color::AccentPrimary)
    , m_adjustingColor(Color4::fromRGBA(255, 200, 80, 255))
    , m_adjusting(false)
    , m_dragging(false)
{
    m_labelPaint.fgColor        = Color::TextSecondary;
    m_labelPaint.opacity        = 1.0f;
    m_labelPaint.textProps      = TextProps("default", TextSize::Small);
    m_labelPaint.textProps.size = 28.0f;
    m_layoutParams.setPadding(10.0f, 6.0f, 10.0f, 6.0f);
}

void SliderView::setRange(float minV, float maxV, float step)
{
    m_minValue = minV;
    m_maxValue = maxV;
    m_step     = step;
    m_value    = clamp(m_value);
}

void SliderView::setValue(float value, bool notify)
{
    float clamped = clamp(value);
    if (clamped == m_value) return;
    m_value = clamped;
    if (notify == true && m_onValueChanged != nullptr) m_onValueChanged(m_value);
}

void SliderView::setAdjusting(bool a)
{
    if (a == true) m_valueBeforeAdjust = m_value;
    m_adjusting = a;
}

// ── Private helpers ───────────────────────────────────────────────────────────

std::string SliderView::formatValue() const
{
    char buf[32];
    snprintf(buf, sizeof(buf), m_valueFormat.c_str(), m_value);
    return buf;
}

RectF SliderView::calcTrackRect() const
{
    const RectF cb = getContentBounds();
    if (m_textPosition == TextPosition::NONE) return cb;

    char maxBuf[32];
    snprintf(maxBuf, sizeof(maxBuf), m_valueFormat.c_str(), m_maxValue);
    const Vec2  labelSz = FontManager::getInstance().measureText(std::string(maxBuf), m_labelPaint);
    const float spacing = m_labelSpacing;

    if (m_orientation == Orientation::HORIZONTAL)
    {
        // BEFORE = label left of track,  AFTER = label right of track
        const float labelW = labelSz.x + spacing;
        if (m_textPosition == TextPosition::BEFORE)
            return RectF::fromXYWH(cb.left + labelW, cb.top, cb.width() - labelW, cb.height());
        else
            return RectF::fromXYWH(cb.left, cb.top, cb.width() - labelW, cb.height());
    }
    else
    {
        // BEFORE = label above track,  AFTER = label below track
        const float labelH = labelSz.y + spacing;
        if (m_textPosition == TextPosition::BEFORE)
            return RectF::fromXYWH(cb.left, cb.top + labelH, cb.width(), cb.height() - labelH);
        else
            return RectF::fromXYWH(cb.left, cb.top, cb.width(), cb.height() - labelH);
    }
}

float SliderView::posToValue(float pos) const
{
    const RectF  track = calcTrackRect();
    const float  range = m_maxValue - m_minValue;
    if (range <= 0.0f) return m_minValue;

    float t = 0.0f;
    if (m_orientation == Orientation::HORIZONTAL)
    {
        const float trackW = track.width() - m_thumbRadius * 2.0f;
        if (trackW <= 0.0f) return m_minValue;
        t = (pos - (track.left + m_thumbRadius)) / trackW;
    }
    else
    {
        const float trackH = track.height() - m_thumbRadius * 2.0f;
        if (trackH <= 0.0f) return m_minValue;
        t = 1.0f - (pos - (track.top + m_thumbRadius)) / trackH;   // bottom = min
    }

    t = std::max(0.0f, std::min(1.0f, t));

    const float raw     = m_minValue + t * range;
    const float snapped = m_minValue + std::round((raw - m_minValue) / m_step) * m_step;
    return clamp(snapped);
}

float SliderView::clamp(float v) const { return std::max(m_minValue, std::min(m_maxValue, v)); }

// ── onMeasure ─────────────────────────────────────────────────────────────────

void SliderView::onMeasure(MeasureSpec widthSpec, MeasureSpec heightSpec)
{
    Vec2 labelSz(0.0f, 0.0f);
    if (m_textPosition != TextPosition::NONE)
    {
        char maxBuf[32];
        snprintf(maxBuf, sizeof(maxBuf), m_valueFormat.c_str(), m_maxValue);
        labelSz = FontManager::getInstance().measureText(std::string(maxBuf), m_labelPaint);
    }
    const float hPad = m_layoutParams.getPaddingHorizontal();
    const float vPad = m_layoutParams.getPaddingVertical();
 
    float naturalW, naturalH;
    if (m_orientation == Orientation::HORIZONTAL)
    {
        // label is left or right of bar — adds to width, not height
        naturalW = 200.0f + hPad + (m_textPosition != TextPosition::NONE ? labelSz.x + m_labelSpacing : 0.0f);
        naturalH = m_thumbRadius * 2.0f + vPad;
    }
    else
    {
        // label is above or below bar — adds to height, not width
        naturalW = m_thumbRadius * 2.0f + hPad;
        naturalH = 200.0f + vPad + (m_textPosition != TextPosition::NONE ? labelSz.y + m_labelSpacing : 0.0f);
    }

    auto resolve = [](MeasureSpec spec, float natural) -> float
    {
        if (spec.mode == MeasureSpecMode::EXACTLY) return spec.size;
        if (spec.mode == MeasureSpecMode::AT_MOST) return std::min(natural, spec.size);
        return natural;
    };

    float w = (m_layoutParams.isMatchParentWidth()  || m_layoutParams.isExactWidth())
              ? resolve(widthSpec,  naturalW) : naturalW;
    float h = (m_layoutParams.isMatchParentHeight() || m_layoutParams.isExactHeight())
              ? resolve(heightSpec, naturalH) : naturalH;

    m_measuredSize = Vec2(w, h);
}

// ── onDraw ────────────────────────────────────────────────────────────────────

void SliderView::onDraw(ICanvas& canvas)
{
    if (isVisible() == false) return;
    View::onDraw(canvas);

    const RectF  track  = calcTrackRect();
    const float  t      = (m_maxValue > m_minValue)
                        ? (m_value - m_minValue) / (m_maxValue - m_minValue) : 0.0f;
    const Color4 active = m_adjusting == true ? m_adjustingColor : m_fillColor;
    const Color4 thumb  = m_adjusting == true ? m_adjustingColor : m_thumbColor;

    canvas.pushClip(m_bounds);

    auto makeFilledRect = [](const Color4& col, float cornerR) -> Paint
    {
        Paint p; p.bgColor = col; p.cornerRadius = cornerR; p.filled = true; return p;
    };
    auto makeCircle = [](const Color4& col, bool filled, float strokeW = 0.0f) -> Paint
    {
        Paint p; p.bgColor = p.fgColor = col; p.filled = filled; p.strokeWidth = strokeW; return p;
    };

    if (m_orientation == Orientation::HORIZONTAL)
    {
        const float cy    = track.top + track.height() * 0.5f;
        const float left  = track.left  + m_thumbRadius;
        const float right = track.right - m_thumbRadius;
        const float len   = right - left;
        const float half  = m_trackThickness * 0.5f;

        canvas.drawRect(RectF::fromXYWH(left, cy - half, len,       m_trackThickness), makeFilledRect(m_trackColor, half));
        if (t > 0.0f)
            canvas.drawRect(RectF::fromXYWH(left, cy - half, len * t, m_trackThickness), makeFilledRect(active, half));

        const float tx = left + len * t;
        canvas.drawCircle(Vec2(tx, cy), m_thumbRadius,        makeCircle(thumb, true));
        if (hasFocus() == true)
            canvas.drawCircle(Vec2(tx, cy), m_thumbRadius + 3.0f, makeCircle(Color::BorderFocus, false, 2.0f));

        if (m_textPosition != TextPosition::NONE)
        {
            const std::string lbl    = formatValue();
            const Vec2        lblSz  = FontManager::getInstance().measureText(lbl, m_labelPaint);
            // vertically centre the label with the track band
            const float       ly     = track.top + (track.height() - lblSz.y) * 0.5f;
            // BEFORE = label left of track rect;  AFTER = label right of track rect
            const float       lx     = (m_textPosition == TextPosition::BEFORE)
                                       ? (track.left  - lblSz.x - m_labelSpacing)
                                       : (track.right + m_labelSpacing);
            canvas.drawText(lbl, Vec2(lx, ly), m_labelPaint);
        }
    }
    else  // VERTICAL
    {
        const float cx     = track.left + track.width() * 0.5f;
        const float bottom = track.bottom - m_thumbRadius;
        const float top    = track.top    + m_thumbRadius;
        const float len    = bottom - top;
        const float half   = m_trackThickness * 0.5f;

        canvas.drawRect(RectF::fromXYWH(cx - half, top, m_trackThickness, len),             makeFilledRect(m_trackColor, half));
        if (t > 0.0f)
        {
            const float fillLen = len * t;
            canvas.drawRect(RectF::fromXYWH(cx - half, bottom - fillLen, m_trackThickness, fillLen), makeFilledRect(active, half));
        }

        const float ty = bottom - len * t;
        canvas.drawCircle(Vec2(cx, ty), m_thumbRadius,        makeCircle(thumb, true));
        if (hasFocus() == true)
            canvas.drawCircle(Vec2(cx, ty), m_thumbRadius + 3.0f, makeCircle(Color::BorderFocus, false, 2.0f));

        if (m_textPosition != TextPosition::NONE)
        {
            const std::string lbl   = formatValue();
            const Vec2        lblSz = FontManager::getInstance().measureText(lbl, m_labelPaint);
            // BEFORE = label above track;  AFTER = label below track
            const float       ly    = (m_textPosition == TextPosition::BEFORE)
                                      ? (track.top - lblSz.y - m_labelSpacing)
                                      : (track.bottom + m_labelSpacing);
            // textGravity controls horizontal placement inside the track width
            float lx = track.left;
            if      ((m_textGravity & Gravity::RIGHT)              != Gravity::NO_GRAVITY) lx = track.right - lblSz.x;
            else if ((m_textGravity & Gravity::CENTER_HORIZONTAL)  != Gravity::NO_GRAVITY) lx = track.left + (track.width() - lblSz.x) * 0.5f;
            canvas.drawText(lbl, Vec2(lx, ly), m_labelPaint);
        }
    }

    canvas.popClip();
}

// ── onKeyEvent ────────────────────────────────────────────────────────────────

bool SliderView::onKeyEvent(const IO::KeyEvent& event)
{
    if (event.action != IO::KeyAction::DOWN && event.action != IO::KeyAction::MULTIPLE) return false;

    const bool isEnter = (event.keyCode == IO::KeyCode::ENTER   || event.keyCode == IO::KeyCode::DPAD_CENTER);
    const bool isEsc   = (event.keyCode == IO::KeyCode::ESCAPE  || event.keyCode == IO::KeyCode::BACK);

    if (isEnter == true)
    {
        if (m_adjusting == false)
        {
            setAdjusting(true);
        }
        else
        {
            m_adjusting = false;
            if (m_onValueChanged != nullptr) m_onValueChanged(m_value);
        }
        return true;
    }

    if (isEsc == true && m_adjusting == true)
    {
        m_value     = m_valueBeforeAdjust;
        m_adjusting = false;
        return true;
    }

    if (m_adjusting == false) return false;

    const bool isPos = (m_orientation == Orientation::HORIZONTAL)
                       ? (event.keyCode == IO::KeyCode::DPAD_RIGHT)
                       : (event.keyCode == IO::KeyCode::DPAD_UP);
    const bool isNeg = (m_orientation == Orientation::HORIZONTAL)
                       ? (event.keyCode == IO::KeyCode::DPAD_LEFT)
                       : (event.keyCode == IO::KeyCode::DPAD_DOWN);

    if (isPos == true) { m_value = clamp(m_value + m_step); if (m_onValueChanged != nullptr) m_onValueChanged(m_value); return true; }
    if (isNeg == true) { m_value = clamp(m_value - m_step); if (m_onValueChanged != nullptr) m_onValueChanged(m_value); return true; }

    return false;
}

// ── onMotionEvent ─────────────────────────────────────────────────────────────

bool SliderView::onMotionEvent(const IO::MotionEvent& event)
{
    const float pos = (m_orientation == Orientation::HORIZONTAL) ? event.x : event.y;

    if (event.action == IO::MotionAction::DOWN && m_bounds.contains(event.x, event.y))
    {
        m_dragging = true;
        if (m_adjusting == false) setAdjusting(true);
        m_value = posToValue(pos);
        return true;
    }

    if (event.action == IO::MotionAction::MOVE && m_dragging == true)
    {
        m_value = posToValue(pos);
        return true;
    }

    if (event.action == IO::MotionAction::UP)
    {
        if (m_dragging == true)
        {
            m_dragging  = false;
            m_adjusting = false;
            if (m_onValueChanged != nullptr) m_onValueChanged(m_value);
        }
        return m_bounds.contains(event.x, event.y);
    }

    return false;
}

// ── onFocusChanged ────────────────────────────────────────────────────────────

void SliderView::onFocusChanged(bool hasFocus)
{
    View::onFocusChanged(hasFocus);
    if (hasFocus == false && m_adjusting == true)
    {
        m_adjusting = false;
        if (m_onValueChanged != nullptr) m_onValueChanged(m_value);
    }
}

// ============================================================================
// CursorView Implementation
// ============================================================================

CursorView::CursorView()
    : View()
    , m_position(0.0f, 0.0f)
    , m_isPressed(false)
    , m_lastInputSource(IO::Source::MOUSE)
    , m_mouseButton(IO::MouseButton::NONE)
    , m_visibilityMode(VisibilityMode::SHOW_ON_MOVE)
    , m_cursorStyle(CursorStyle::DOT_WITH_RING)
    , m_cursorSize(24.0f)
    , m_mouseColor(Color4::fromRGBA(100, 180, 255, 200))
    , m_touchColor(Color4::fromRGBA(255, 100, 150, 220))
    , m_pressedColor(Color4::fromRGBA(255, 200, 80, 240))
    , m_fadeOutDuration(0.5f)
    , m_timeSinceLastMove(0.0f)
    , m_currentAlpha(1.0f)
    , m_rippleActive(false)
    , m_rippleProgress(0.0f)
    , m_rippleDuration(0.4f)
    , m_lastPosition(0.0f, 0.0f)
    , m_hasMoved(false)
    , m_touchPointerCount(0)
{
    setFocusable(false);
    
    m_layoutParams.width = MATCH_PARENT;
    m_layoutParams.height = MATCH_PARENT;
    
    setBackgroundColor(Color::Transparent);
}

CursorView::~CursorView()
{
}

void CursorView::updatePosition(float x, float y)
{
    Vec2 newPos(x, y);
    
    float dx = newPos.x - m_lastPosition.x;
    float dy = newPos.y - m_lastPosition.y;
    float distSq = dx * dx + dy * dy;
    
    if (distSq > 0.01f)
    {
        m_hasMoved = true;
        m_timeSinceLastMove = 0.0f;
        m_lastPosition = newPos;
    }
    
    m_position = newPos;
    updateVisibility();
}

void CursorView::updateFromMotionEvent(const IO::MotionEvent& event)
{
    m_lastInputSource = event.source;

    if (event.isTouch() == true)
    {
        m_touchPointerCount = 0;
        for (int i = 0; i < event.pointerCount && m_touchPointerCount < IO::MAX_TOUCH_POINTS; ++i)
            m_touchPointerPositions[m_touchPointerCount++] = Vec2(event.pointers[i].x, event.pointers[i].y);

        if (m_touchPointerCount > 0)
            updatePosition(m_touchPointerPositions[0].x, m_touchPointerPositions[0].y);

        if (event.action == IO::MotionAction::DOWN || event.action == IO::MotionAction::POINTER_DOWN)
        {
            setPressed(true);
            triggerRipple();
        }
        else if (event.action == IO::MotionAction::UP || event.action == IO::MotionAction::CANCEL)
        {
            setPressed(false);
        }
    }
    else
    {
        m_touchPointerCount = 0;
        updatePosition(event.x, event.y);

        if (event.isMouse() == true)
        {
            m_mouseButton = event.button;
            if      (event.action == IO::MotionAction::DOWN) { setPressed(true);  triggerRipple(); }
            else if (event.action == IO::MotionAction::UP)     setPressed(false);
        }
    }
}

void CursorView::setPressed(bool pressed)
{
    m_isPressed = pressed;
    updateVisibility();
}

void CursorView::update(float deltaTime)
{
    // Update fade out
    if (m_visibilityMode == VisibilityMode::SHOW_ON_MOVE)
    {
        m_timeSinceLastMove += deltaTime;
        
        if (m_timeSinceLastMove > m_fadeOutDuration)
        {
            float fadeTime = 0.3f; // 300ms fade
            float fadeProgress = (m_timeSinceLastMove - m_fadeOutDuration) / fadeTime;
            m_currentAlpha = std::max(0.0f, 1.0f - fadeProgress);
        }
        else
        {
            m_currentAlpha = 1.0f;
        }
    }
    else if (m_visibilityMode == VisibilityMode::SHOW_ON_TOUCH)
    {
        m_currentAlpha = (m_isPressed || m_rippleActive) ? 1.0f : 0.0f;
    }
    else if (m_visibilityMode == VisibilityMode::ALWAYS_VISIBLE)
    {
        m_currentAlpha = 1.0f;
    }
    else
    {
        m_currentAlpha = 0.0f;
    }
    
    // Update ripple animation
    if (m_rippleActive == true)
    {
        m_rippleProgress += deltaTime / m_rippleDuration;
        
        if (m_rippleProgress >= 1.0f)
        {
            m_rippleActive = false;
            m_rippleProgress = 0.0f;
        }
    }
}

void CursorView::triggerRipple()
{
    m_rippleActive = true;
    m_rippleProgress = 0.0f;
}

void CursorView::updateVisibility()
{
    bool visible = shouldBeVisible();
    setVisibility(visible ? Visibility::VISIBLE : Visibility::GONE);
}

bool CursorView::shouldBeVisible() const
{
    switch (m_visibilityMode)
    {
        case VisibilityMode::ALWAYS_VISIBLE:
            return true;
            
        case VisibilityMode::SHOW_ON_TOUCH:
            return m_isPressed || m_rippleActive;
            
        case VisibilityMode::SHOW_ON_MOVE:
            return m_currentAlpha > 0.01f;
            
        case VisibilityMode::NEVER_VISIBLE:
            return false;
            
        default:
            return false;
    }
}

void CursorView::onMeasure(MeasureSpec widthSpec, MeasureSpec heightSpec)
{
    float width = widthSpec.size;
    float height = heightSpec.size;
    
    if (widthSpec.mode == MeasureSpecMode::UNSPECIFIED)
        width = 0.0f;
    if (heightSpec.mode == MeasureSpecMode::UNSPECIFIED)
        height = 0.0f;
    
    setMeasuredSize(Vec2(width, height));
}

void CursorView::onDraw(ICanvas& canvas)
{
    if (m_currentAlpha < 0.01f) return;

    Color4 currentColor = getCurrentColor();
    currentColor = currentColor.withAlpha(currentColor.a * m_currentAlpha);

    auto drawCursorAt = [&](const Vec2& pos, float sizeScale, const Color4& color)
    {
        switch (m_cursorStyle)
        {
            case CursorStyle::CIRCLE:
            {
                float r = m_cursorSize * 0.5f * sizeScale;
                if (m_isPressed == true) r *= 1.2f;
                drawCircle(canvas, pos, r, color, true);
                break;
            }
            case CursorStyle::RING:
            {
                float r = m_cursorSize * 0.5f * sizeScale;
                float sw = std::max(2.0f, m_cursorSize * 0.15f);
                if (m_isPressed == true) r *= 1.2f;
                drawRing(canvas, pos, r, color, sw);
                break;
            }
            case CursorStyle::CROSSHAIR:
            {
                float sz = m_cursorSize * sizeScale;
                float sw = std::max(2.0f, m_cursorSize * 0.1f);
                drawCrosshair(canvas, pos, sz, color, sw);
                break;
            }
            case CursorStyle::DOT_WITH_RING:
            {
                float dotR  = m_cursorSize * 0.2f * sizeScale;
                float ringR = m_cursorSize * 0.5f * sizeScale;
                float sw    = std::max(2.0f, m_cursorSize * 0.12f);
                if (m_isPressed == true) { dotR *= 1.3f; ringR *= 1.15f; }
                drawCircle(canvas, pos, dotR, color, true);
                drawRing(canvas, pos, ringR, color.withAlpha(color.a * 0.6f), sw);
                break;
            }
            case CursorStyle::RIPPLE:
            {
                float dotR = m_cursorSize * 0.15f * sizeScale;
                drawCircle(canvas, pos, dotR, color, true);
                break;
            }
        }
    };

    if (m_touchPointerCount > 1)
    {
        for (int i = 0; i < m_touchPointerCount; ++i)
        {
            Color4 fingerColor = (i == 0) ? currentColor : currentColor.withAlpha(currentColor.a * 0.75f);
            drawCursorAt(m_touchPointerPositions[i], 1.0f, fingerColor);

            if (m_rippleActive == true)
                drawRipple(canvas, m_touchPointerPositions[i], m_rippleProgress,
                           fingerColor.withAlpha(fingerColor.a * 0.5f));
        }
        return;
    }

    if (m_rippleActive == true && m_cursorStyle == CursorStyle::RIPPLE)
        drawRipple(canvas, m_position, m_rippleProgress, currentColor);

    drawCursorAt(m_position, 1.0f, currentColor);

    if (m_rippleActive == true && m_cursorStyle != CursorStyle::RIPPLE)
        drawRipple(canvas, m_position, m_rippleProgress, currentColor.withAlpha(currentColor.a * 0.5f));
}

Color4 CursorView::getCurrentColor() const
{
    if (m_isPressed == true)
        return m_pressedColor;
    
    if (m_lastInputSource == IO::Source::TOUCHSCREEN)
        return m_touchColor;
    
    if (m_lastInputSource == IO::Source::MOUSE)
    {
        if (m_mouseButton == IO::MouseButton::RIGHT)
            return Color4::fromRGBA(255, 150, 100, 200);
        else if (m_mouseButton == IO::MouseButton::MIDDLE)
            return Color4::fromRGBA(150, 255, 150, 200);
        
        return m_mouseColor;
    }
    
    return m_mouseColor;
}

void CursorView::drawCircle(ICanvas& canvas, const Vec2& center, float radius, const Color4& color, bool filled)
{
    Paint paint;
    paint.bgColor = color;
    paint.filled = filled;
    paint.opacity = 1.0f;
    
    canvas.drawCircle(center, radius, paint);
}

void CursorView::drawRing(ICanvas& canvas, const Vec2& center, float radius, const Color4& color, float strokeWidth)
{
    Paint paint;
    paint.bgColor = Color::Transparent;
    paint.fgColor = color;
    paint.filled = false;
    paint.strokeWidth = strokeWidth;
    paint.opacity = 1.0f;
    
    canvas.drawCircle(center, radius, paint);
}

void CursorView::drawCrosshair(ICanvas& canvas, const Vec2& center, float size, const Color4& color, float strokeWidth)
{
    Paint paint;
    paint.fgColor = color;
    paint.strokeWidth = strokeWidth;
    paint.opacity = 1.0f;
    
    float halfSize = size * 0.5f;
    float gapSize = size * 0.2f;
    
    canvas.drawLine(Vec2(center.x - halfSize, center.y), 
                   Vec2(center.x - gapSize, center.y), paint);
    canvas.drawLine(Vec2(center.x + gapSize, center.y), 
                   Vec2(center.x + halfSize, center.y), paint);
    
    canvas.drawLine(Vec2(center.x, center.y - halfSize), 
                   Vec2(center.x, center.y - gapSize), paint);
    canvas.drawLine(Vec2(center.x, center.y + gapSize), 
                   Vec2(center.x, center.y + halfSize), paint);
}

void CursorView::drawRipple(ICanvas& canvas, const Vec2& center, float progress, const Color4& color)
{
    // Ease-out function for smooth animation
    float eased = 1.0f - (1.0f - progress) * (1.0f - progress);
    
    float startRadius = m_cursorSize * 0.5f;
    float endRadius = m_cursorSize * 2.0f;
    float radius = startRadius + (endRadius - startRadius) * eased;
    
    // Fade out as ripple expands
    float alpha = (1.0f - eased) * 0.6f;
    Color4 rippleColor = color.withAlpha(color.a * alpha);
    
    float strokeWidth = std::max(2.0f, m_cursorSize * 0.15f * (1.0f - eased));
    
    drawRing(canvas, center, radius, rippleColor, strokeWidth);
}

// ============================================================================
// FloatingObject Implementation
// ============================================================================

FloatingObject::FloatingObject()
    : m_shape(FloatingShape::CIRCLE)
    , m_position(0, 0)
    , m_velocity(50, 50)
    , m_size(20, 20)
{
    m_paint.filled = true;
    m_paint.bgColor = Color::White;
    m_paint.opacity = 1.0f;
}

void FloatingObject::update(float deltaTime, const RectF& bounds)
{
    m_position = m_position + m_velocity * deltaTime;
    
    if (m_shape == FloatingShape::CIRCLE)
    {
        float radius = m_size.x * 0.5f;
        
        if (m_position.x - radius < bounds.left)
        {
            m_position.x = bounds.left + radius;
            m_velocity.x = std::abs(m_velocity.x);
        }
        else if (m_position.x + radius > bounds.right)
        {
            m_position.x = bounds.right - radius;
            m_velocity.x = -std::abs(m_velocity.x);
        }
        
        if (m_position.y - radius < bounds.top)
        {
            m_position.y = bounds.top + radius;
            m_velocity.y = std::abs(m_velocity.y);
        }
        else if (m_position.y + radius > bounds.bottom)
        {
            m_position.y = bounds.bottom - radius;
            m_velocity.y = -std::abs(m_velocity.y);
        }
    }
    else
    {
        if (m_position.x < bounds.left)
        {
            m_position.x = bounds.left;
            m_velocity.x = std::abs(m_velocity.x);
        }
        else if (m_position.x + m_size.x > bounds.right)
        {
            m_position.x = bounds.right - m_size.x;
            m_velocity.x = -std::abs(m_velocity.x);
        }
        
        if (m_position.y < bounds.top)
        {
            m_position.y = bounds.top;
            m_velocity.y = std::abs(m_velocity.y);
        }
        else if (m_position.y + m_size.y > bounds.bottom)
        {
            m_position.y = bounds.bottom - m_size.y;
            m_velocity.y = -std::abs(m_velocity.y);
        }
    }
}

void FloatingObject::draw(ICanvas& canvas)
{
    if (m_shape == FloatingShape::CIRCLE)
    {
        float radius = m_size.x * 0.5f;
        canvas.drawCircle(m_position, radius, m_paint);
    }
    else
    {
        RectF rect = RectF::fromLTRB(
            m_position.x, 
            m_position.y, 
            m_position.x + m_size.x, 
            m_position.y + m_size.y
        );
        canvas.drawRect(rect, m_paint);
    }
}

// ============================================================================
// FrameLayout Implementation
// ============================================================================

FrameLayout::FrameLayout() {}

void FrameLayout::layoutChildren()
{
    RectF contentBounds = getChildLayoutBounds();
    
    for (View* child : m_children)
    {
        if (child->getVisibility() == Visibility::GONE) continue;
        
        const LayoutParams& lp = child->getLayoutParams();
        Vec2 childSize = child->getMeasuredSize();
        
        RectF childBounds;
        if (lp.gravity != Gravity::NO_GRAVITY)
        {
            applyGravity(childSize, contentBounds, lp.gravity, lp, childBounds);
        }
        else
        {
            childBounds = RectF::fromLTRB(
                contentBounds.left + lp.marginLeft,
                contentBounds.top + lp.marginTop,
                contentBounds.left + lp.marginLeft + childSize.x,
                contentBounds.top + lp.marginTop + childSize.y
            );
        }
        
        child->onLayout(childBounds);
    }
}

// ============================================================================
// LinearLayout Implementation
// ============================================================================

LinearLayout::LinearLayout()
    : m_orientation(Orientation::HORIZONTAL)
    , m_gravity(Gravity::NO_GRAVITY)
    , m_spacing(0.0f)
{
}

void LinearLayout::onMeasure(MeasureSpec widthSpec, MeasureSpec heightSpec)
{
    float availableWidth = widthSpec.size - m_layoutParams.getPaddingHorizontal();
    float availableHeight = heightSpec.size - m_layoutParams.getPaddingVertical();
    
    availableWidth = std::max(0.0f, availableWidth);
    availableHeight = std::max(0.0f, availableHeight);
    
    int visibleChildren = 0;
    int matchParentCount = 0;
    float totalWeight = 0.0f;
    
    for (View* child : m_children)
    {
        if (child->getVisibility() == Visibility::GONE) continue;
        visibleChildren++;
        
        const LayoutParams& lp = child->getLayoutParams();
        
        if (lp.hasWeight() == true) totalWeight += lp.weight;
        else
        {
            if (m_orientation == Orientation::HORIZONTAL && lp.isMatchParentWidth()) matchParentCount++;
            if (m_orientation == Orientation::VERTICAL && lp.isMatchParentHeight()) matchParentCount++;
        }
    }
    
    float totalSpacing = (visibleChildren > 1) ? (visibleChildren - 1) * m_spacing : 0.0f;
    
    float usedWidth = 0;
    float usedHeight = 0;
    float maxChildWidth = 0;
    float maxChildHeight = 0;
    
    // Pass 1: fixed-size children
    for (View* child : m_children)
    {
        if (child->getVisibility() == Visibility::GONE) continue;
        
        const LayoutParams& lp = child->getLayoutParams();
        
        if (lp.hasWeight() == true) continue;
        if (m_orientation == Orientation::HORIZONTAL && lp.isMatchParentWidth()) continue;
        if (m_orientation == Orientation::VERTICAL && lp.isMatchParentHeight()) continue;
        
        MeasureSpec childWidthSpec = getChildMeasureSpec(MeasureSpec::makeAtMost(availableWidth), lp, true);
        MeasureSpec childHeightSpec = getChildMeasureSpec(MeasureSpec::makeAtMost(availableHeight), lp, false);
        
        child->onMeasure(childWidthSpec, childHeightSpec);
        Vec2 childSize = child->getMeasuredSize();
        
        if (m_orientation == Orientation::HORIZONTAL)
        {
            usedWidth += childSize.x + lp.getMarginHorizontal();
            maxChildHeight = std::max(maxChildHeight, childSize.y + lp.getMarginVertical());
        }
        else
        {
            maxChildWidth = std::max(maxChildWidth, childSize.x + lp.getMarginHorizontal());
            usedHeight += childSize.y + lp.getMarginVertical();
        }
    }
    
    if (m_orientation == Orientation::HORIZONTAL) usedWidth += totalSpacing;
    else usedHeight += totalSpacing;
    
    // Pass 2: weighted children
    if (totalWeight > 0.0f)
    {
        if (m_orientation == Orientation::HORIZONTAL)
        {
            float remainingWidth = std::max(0.0f, availableWidth - usedWidth);
            
            for (View* child : m_children)
            {
                if (child->getVisibility() == Visibility::GONE) continue;
                
                const LayoutParams& lp = child->getLayoutParams();
                if (lp.hasWeight() == true)
                {
                    float weightedWidth = (remainingWidth * lp.weight / totalWeight);
                    float childWidthAfterMargin = std::max(0.0f, weightedWidth - lp.getMarginHorizontal());
                    
                    MeasureSpec childWidthSpec = MeasureSpec::makeExactly(childWidthAfterMargin);
                    MeasureSpec childHeightSpec = getChildMeasureSpec(MeasureSpec::makeAtMost(availableHeight), lp, false);
                    
                    child->onMeasure(childWidthSpec, childHeightSpec);
                    
                    Vec2 childSize = child->getMeasuredSize();
                    usedWidth += childSize.x + lp.getMarginHorizontal();
                    maxChildHeight = std::max(maxChildHeight, childSize.y + lp.getMarginVertical());
                }
            }
        }
        else
        {
            float remainingHeight = std::max(0.0f, availableHeight - usedHeight);
            
            for (View* child : m_children)
            {
                if (child->getVisibility() == Visibility::GONE) continue;
                
                const LayoutParams& lp = child->getLayoutParams();
                if (lp.hasWeight() == true)
                {
                    float weightedHeight = (remainingHeight * lp.weight / totalWeight);
                    float childHeightAfterMargin = std::max(0.0f, weightedHeight - lp.getMarginVertical());
                    
                    MeasureSpec childWidthSpec = getChildMeasureSpec(MeasureSpec::makeAtMost(availableWidth), lp, true);
                    MeasureSpec childHeightSpec = MeasureSpec::makeExactly(childHeightAfterMargin);
                    
                    child->onMeasure(childWidthSpec, childHeightSpec);
                    
                    Vec2 childSize = child->getMeasuredSize();
                    maxChildWidth = std::max(maxChildWidth, childSize.x + lp.getMarginHorizontal());
                    usedHeight += childSize.y + lp.getMarginVertical();
                }
            }
        }
    }
    
    // Pass 3: MATCH_PARENT children
    if (matchParentCount > 0)
    {
        if (m_orientation == Orientation::HORIZONTAL)
        {
            float remainingWidth = std::max(0.0f, availableWidth - usedWidth);
            float matchParentWidth = remainingWidth / matchParentCount;
            
            for (View* child : m_children)
            {
                if (child->getVisibility() == Visibility::GONE) continue;
                
                const LayoutParams& lp = child->getLayoutParams();
                if (lp.hasWeight() == false && lp.isMatchParentWidth() == true)
                {
                    float childWidthAfterMargin = std::max(0.0f, matchParentWidth - lp.getMarginHorizontal());
                    
                    MeasureSpec childWidthSpec = MeasureSpec::makeExactly(childWidthAfterMargin);
                    MeasureSpec childHeightSpec = getChildMeasureSpec(MeasureSpec::makeAtMost(availableHeight), lp, false);
                    
                    child->onMeasure(childWidthSpec, childHeightSpec);
                    
                    Vec2 childSize = child->getMeasuredSize();
                    usedWidth += childSize.x + lp.getMarginHorizontal();
                    maxChildHeight = std::max(maxChildHeight, childSize.y + lp.getMarginVertical());
                }
            }
        }
        else
        {
            float remainingHeight = std::max(0.0f, availableHeight - usedHeight);
            float matchParentHeight = remainingHeight / matchParentCount;
            
            for (View* child : m_children)
            {
                if (child->getVisibility() == Visibility::GONE) continue;
                
                const LayoutParams& lp = child->getLayoutParams();
                if (lp.hasWeight() == false && lp.isMatchParentHeight() == true)
                {
                    float childHeightAfterMargin = std::max(0.0f, matchParentHeight - lp.getMarginVertical());
                    
                    MeasureSpec childWidthSpec = getChildMeasureSpec(MeasureSpec::makeAtMost(availableWidth), lp, true);
                    MeasureSpec childHeightSpec = MeasureSpec::makeExactly(childHeightAfterMargin);
                    
                    child->onMeasure(childWidthSpec, childHeightSpec);
                    
                    Vec2 childSize = child->getMeasuredSize();
                    maxChildWidth = std::max(maxChildWidth, childSize.x + lp.getMarginHorizontal());
                    usedHeight += childSize.y + lp.getMarginVertical();
                }
            }
        }
    }
    
    float finalWidth = (m_orientation == Orientation::HORIZONTAL) ? usedWidth : maxChildWidth;
    float finalHeight = (m_orientation == Orientation::VERTICAL) ? usedHeight : maxChildHeight;
    
    finalWidth += m_layoutParams.getPaddingHorizontal();
    finalHeight += m_layoutParams.getPaddingVertical();
    
    if (m_layoutParams.isExactWidth() == true) finalWidth = m_layoutParams.width;
    else if (m_layoutParams.isMatchParentWidth() == true)
    {
        if (widthSpec.mode == MeasureSpecMode::EXACTLY || widthSpec.mode == MeasureSpecMode::AT_MOST) finalWidth = widthSpec.size;
    }
    else
    {
        if (widthSpec.mode == MeasureSpecMode::AT_MOST) finalWidth = std::min(finalWidth, widthSpec.size);
    }
    
    if (m_layoutParams.isExactHeight() == true) finalHeight = m_layoutParams.height;
    else if (m_layoutParams.isMatchParentHeight() == true)
    {
        if (heightSpec.mode == MeasureSpecMode::EXACTLY || heightSpec.mode == MeasureSpecMode::AT_MOST) finalHeight = heightSpec.size;
    }
    else
    {
        if (heightSpec.mode == MeasureSpecMode::AT_MOST) finalHeight = std::min(finalHeight, heightSpec.size);
    }
    
    m_measuredSize = Vec2(finalWidth, finalHeight);
}

void LinearLayout::layoutChildren()
{
    RectF contentBounds = getChildLayoutBounds();
    float x = contentBounds.left;
    float y = contentBounds.top;
    
    int visibleIndex = 0;
    int totalVisible = 0;
    
    for (View* child : m_children)
    {
        if (child->getVisibility() != Visibility::GONE) totalVisible++;
    }
    
    for (View* child : m_children)
    {
        if (child->getVisibility() == Visibility::GONE) continue;
        
        const LayoutParams& lp = child->getLayoutParams();
        Vec2 childSize = child->getMeasuredSize();
        
        float childLeft = x + lp.marginLeft;
        float childTop = y + lp.marginTop;
        float childRight = childLeft + childSize.x;
        float childBottom = childTop + childSize.y;
        
        if (m_orientation == Orientation::HORIZONTAL)
        {
            if (hasGravity(m_gravity, Gravity::CENTER_VERTICAL))
            {
                float offset = (contentBounds.height() - childSize.y - lp.getMarginVertical()) * 0.5f;
                childTop = contentBounds.top + offset + lp.marginTop;
                childBottom = childTop + childSize.y;
            }
            else if (hasGravity(m_gravity, Gravity::BOTTOM))
            {
                childTop = contentBounds.bottom - childSize.y - lp.marginBottom;
                childBottom = childTop + childSize.y;
            }
            
            x = childRight + lp.marginRight;
            if (visibleIndex < totalVisible - 1) x += m_spacing;
        }
        else
        {
            if (hasGravity(m_gravity, Gravity::CENTER_HORIZONTAL))
            {
                float offset = (contentBounds.width() - childSize.x - lp.getMarginHorizontal()) * 0.5f;
                childLeft = contentBounds.left + offset + lp.marginLeft;
                childRight = childLeft + childSize.x;
            }
            else if (hasGravity(m_gravity, Gravity::RIGHT))
            {
                childLeft = contentBounds.right - childSize.x - lp.marginRight;
                childRight = childLeft + childSize.x;
            }
            
            y = childBottom + lp.marginBottom;
            if (visibleIndex < totalVisible - 1) y += m_spacing;
        }
        
        child->onLayout(RectF::fromLTRB(childLeft, childTop, childRight, childBottom));
        visibleIndex++;
    }
}

// ============================================================================
// RelativeLayout Implementation
// ============================================================================

RelativeLayout::RelativeLayout() {}

void RelativeLayout::onMeasure(MeasureSpec widthSpec, MeasureSpec heightSpec)
{
    ViewGroup::onMeasure(widthSpec, heightSpec);
}

void RelativeLayout::layoutChildren()
{
    RectF contentBounds = getChildLayoutBounds();
    
    for (View* child : m_children)
    {
        if (child->getVisibility() == Visibility::GONE) continue;
        
        const LayoutParams& lp = child->getLayoutParams();
        const RelativeLayoutParams* relLp = dynamic_cast<const RelativeLayoutParams*>(&lp);
        
        if (relLp != nullptr)
        {
            applyConstraints(child, *relLp, contentBounds);
        }
        else
        {
            Vec2 childSize = child->getMeasuredSize();
            child->onLayout(RectF::fromXYWH(contentBounds.left, contentBounds.top, childSize.x, childSize.y));
        }
    }
}

void RelativeLayout::applyConstraints(View* child, const RelativeLayoutParams& params, const RectF& bounds)
{
    Vec2 childSize = child->getMeasuredSize();
    float left = bounds.left;
    float top = bounds.top;
    
    switch (params.rule)
    {
        case RelativeRule::ALIGN_PARENT_LEFT:
            left = bounds.left + params.marginLeft;
            break;
        case RelativeRule::ALIGN_PARENT_TOP:
            top = bounds.top + params.marginTop;
            break;
        case RelativeRule::ALIGN_PARENT_RIGHT:
            left = bounds.right - childSize.x - params.marginRight;
            break;
        case RelativeRule::ALIGN_PARENT_BOTTOM:
            top = bounds.bottom - childSize.y - params.marginBottom;
            break;
        case RelativeRule::CENTER_IN_PARENT:
            left = bounds.left + (bounds.width() - childSize.x) * 0.5f;
            top = bounds.top + (bounds.height() - childSize.y) * 0.5f;
            break;
        case RelativeRule::CENTER_HORIZONTAL:
            left = bounds.left + (bounds.width() - childSize.x) * 0.5f;
            break;
        case RelativeRule::CENTER_VERTICAL:
            top = bounds.top + (bounds.height() - childSize.y) * 0.5f;
            break;
        default:
            break;
    }
    
    child->onLayout(RectF::fromXYWH(left, top, childSize.x, childSize.y));
}

// ============================================================================
// ConstraintLayout Implementation
// ============================================================================

ConstraintLayout::ConstraintLayout() {}

void ConstraintLayout::onMeasure(MeasureSpec widthSpec, MeasureSpec heightSpec)
{
    ViewGroup::onMeasure(widthSpec, heightSpec);
}

void ConstraintLayout::layoutChildren()
{
    RectF contentBounds = getChildLayoutBounds();
    
    for (View* child : m_children)
    {
        if (child->getVisibility() == Visibility::GONE) continue;
        
        const LayoutParams& lp = child->getLayoutParams();
        const ConstraintLayoutParams* clp = dynamic_cast<const ConstraintLayoutParams*>(&lp);
        
        if (clp != nullptr)
        {
            applyConstraints(child, *clp, contentBounds);
        }
        else
        {
            Vec2 childSize = child->getMeasuredSize();
            child->onLayout(RectF::fromXYWH(contentBounds.left, contentBounds.top, childSize.x, childSize.y));
        }
    }
}

void ConstraintLayout::applyConstraints(View* child, const ConstraintLayoutParams& params, const RectF& bounds)
{
    Vec2 childSize = child->getMeasuredSize();
    float left = bounds.left;
    float top = bounds.top;
    
    for (const Constraint& constraint : params.constraints)
    {
        switch (constraint.type)
        {
            case ConstraintType::CENTER_HORIZONTAL:
                left = bounds.left + (bounds.width() - childSize.x) * 0.5f;
                break;
            case ConstraintType::CENTER_VERTICAL:
                top = bounds.top + (bounds.height() - childSize.y) * 0.5f;
                break;
            default:
                break;
        }
    }
    
    child->onLayout(RectF::fromXYWH(left, top, childSize.x, childSize.y));
}

// ============================================================================
// ScrollView Implementation
// ============================================================================

ScrollView::ScrollView()
    : m_scrollDirection(Orientation::VERTICAL)
    , m_scrollOffset(0.0f)
    , m_maxScrollOffset(0.0f)
    , m_isDragging(false)
{
}

void ScrollView::scrollTo(float offset)
{
    m_scrollOffset = std::max(0.0f, std::min(offset, m_maxScrollOffset));
}

void ScrollView::scrollBy(float delta)
{
    scrollTo(m_scrollOffset + delta);
}

void ScrollView::onMeasure(MeasureSpec widthSpec, MeasureSpec heightSpec)
{
    float availableWidth = widthSpec.size - m_layoutParams.getPaddingHorizontal();
    float availableHeight = heightSpec.size - m_layoutParams.getPaddingVertical();
    
    availableWidth = std::max(0.0f, availableWidth);
    availableHeight = std::max(0.0f, availableHeight);
    
    for (View* child : m_children)
    {
        if (child->getVisibility() == Visibility::GONE) continue;
        
        const LayoutParams& lp = child->getLayoutParams();
        
        MeasureSpec childWidthSpec = getChildMeasureSpec(MeasureSpec::makeAtMost(availableWidth), lp, true);
        MeasureSpec childHeightSpec = getChildMeasureSpec(MeasureSpec::makeAtMost(availableHeight), lp, false);
        
        child->onMeasure(childWidthSpec, childHeightSpec);
    }
    
    float totalWidth = 0.0f;
    float totalHeight = 0.0f;
    
    for (View* child : m_children)
    {
        if (child->getVisibility() == Visibility::GONE) continue;
        
        const LayoutParams& lp = child->getLayoutParams();
        Vec2 size = child->getMeasuredSize();
        
        if (m_scrollDirection == Orientation::VERTICAL)
        {
            totalWidth = std::max(totalWidth, size.x + lp.getMarginHorizontal());
            totalHeight += size.y + lp.getMarginVertical();
        }
        else
        {
            totalWidth += size.x + lp.getMarginHorizontal();
            totalHeight = std::max(totalHeight, size.y + lp.getMarginVertical());
        }
    }
    
    totalWidth += m_layoutParams.getPaddingHorizontal();
    totalHeight += m_layoutParams.getPaddingVertical();
    
    float width = totalWidth;
    float height = totalHeight;
    
    if (m_layoutParams.isExactWidth() == true) width = m_layoutParams.width;
    else if (m_layoutParams.isMatchParentWidth() == true)
    {
        if (widthSpec.mode == MeasureSpecMode::EXACTLY || widthSpec.mode == MeasureSpecMode::AT_MOST)
            width = widthSpec.size;
    }
    
    if (m_layoutParams.isExactHeight() == true) height = m_layoutParams.height;
    else if (m_layoutParams.isMatchParentHeight() == true)
    {
        if (heightSpec.mode == MeasureSpecMode::EXACTLY || heightSpec.mode == MeasureSpecMode::AT_MOST)
            height = heightSpec.size;
    }
    
    m_measuredSize = Vec2(width, height);
    
    if (m_scrollDirection == Orientation::VERTICAL)
        m_maxScrollOffset = std::max(0.0f, totalHeight - height);
    else
        m_maxScrollOffset = std::max(0.0f, totalWidth - width);
}

void ScrollView::layoutChildren()
{
    RectF contentBounds = getChildLayoutBounds();
    float x = contentBounds.left;
    float y = contentBounds.top;
    
    if (m_scrollDirection == Orientation::VERTICAL) y -= m_scrollOffset;
    else x -= m_scrollOffset;
    
    for (View* child : m_children)
    {
        if (child->getVisibility() == Visibility::GONE) continue;
        
        const LayoutParams& lp = child->getLayoutParams();
        Vec2 size = child->getMeasuredSize();
        
        float childLeft = x + lp.marginLeft;
        float childTop = y + lp.marginTop;
        
        child->onLayout(RectF::fromLTRB(childLeft, childTop, childLeft + size.x, childTop + size.y));
        
        if (m_scrollDirection == Orientation::VERTICAL) y += size.y + lp.getMarginVertical();
        else x += size.x + lp.getMarginHorizontal();
    }
}

void ScrollView::onDraw(ICanvas& canvas)
{
    if (isVisible() == false) return;
    
    View::onDraw(canvas);
    
    canvas.pushClip(m_bounds);
    
    for (View* child : m_children)
    {
        if (child->isVisible() == true) child->onDraw(canvas);
    }
    
    canvas.popClip();
}

bool ScrollView::onMotionEvent(const IO::MotionEvent& event)
{
    if (event.action == IO::MotionAction::DOWN)
    {
        if (m_bounds.contains(event.x, event.y) == true)
        {
            m_lastTouchPos = Vec2(event.x, event.y);
            m_isDragging = true;
            return true;
        }
    }
    else if (event.action == IO::MotionAction::MOVE && m_isDragging == true)
    {
        Vec2 currentPos(event.x, event.y);
        Vec2 delta = currentPos - m_lastTouchPos;
        
        if (m_scrollDirection == Orientation::VERTICAL) scrollBy(-delta.y);
        else scrollBy(-delta.x);
        
        m_lastTouchPos = currentPos;
        return true;
    }
    else if (event.action == IO::MotionAction::UP)
    {
        m_isDragging = false;
    }
    
    return ViewGroup::onMotionEvent(event);
}

// ============================================================================
// ListViewItem Implementation
// ============================================================================

ListViewItem* ListViewItem::s_currentlyHoveredItem = nullptr;

ListViewItem::ListViewItem()
    : m_isItemFocused(false)
    , m_actionFocusIndex(-1)
    , m_iconText(nullptr)
    , m_nameText(nullptr)
    , m_infoText(nullptr)
    , m_actionsLayout(nullptr)
{
    setOrientation(Orientation::HORIZONTAL);
    setGravity(Gravity::LEFT | Gravity::CENTER_VERTICAL);
    setSpacing(10.0f);
    setNormalColor(Color::CardBackground);
    setHoverColor(Color::CardBackgroundHover);
    setFocusedColor(Color::AccentPrimary);
    setCornerRadius(6.0f);
    getLayoutParams().width = MATCH_PARENT;
    getLayoutParams().height = 50;
    getLayoutParams().setPadding(12.0f, 8.0f, 12.0f, 8.0f);
    
    setupContent();
}

ListViewItem::~ListViewItem()
{
    if (s_currentlyHoveredItem == this) s_currentlyHoveredItem = nullptr;
}

void ListViewItem::setupContent()
{
    m_iconText = new TextView();
    m_iconText->setTextColor(Color::AccentPrimary);
    m_iconText->setTextSize(TextSize::Small);
    m_iconText->setTextGravity(Gravity::CENTER);
    m_iconText->getLayoutParams().width = 30;
    m_iconText->getLayoutParams().height = WRAP_CONTENT;
    addView(m_iconText);
    
    LinearLayout* textLayout = new LinearLayout();
    textLayout->setOrientation(Orientation::VERTICAL);
    textLayout->setGravity(Gravity::LEFT);
    textLayout->setSpacing(2.0f);
    textLayout->getLayoutParams().width = MATCH_PARENT;
    textLayout->getLayoutParams().height = WRAP_CONTENT;
    textLayout->getLayoutParams().weight = 1.0f;
    
    m_nameText = new TextView();
    m_nameText->setTextColor(Color::TextPrimary);
    m_nameText->setTextSize(TextSize::Small);
    m_nameText->setTextGravity(Gravity::LEFT);
    m_nameText->getLayoutParams().width = MATCH_PARENT;
    m_nameText->getLayoutParams().height = WRAP_CONTENT;
    textLayout->addView(m_nameText);
    
    m_infoText = new TextView();
    m_infoText->setTextColor(Color::TextSecondary);
    m_infoText->setTextSize(TextSize::Small);
    m_infoText->setTextGravity(Gravity::LEFT);
    m_infoText->getLayoutParams().width = MATCH_PARENT;
    m_infoText->getLayoutParams().height = WRAP_CONTENT;
    textLayout->addView(m_infoText);
    
    addView(textLayout);
}

void ListViewItem::updateVisualState()
{
    if (m_isItemFocused == true)
    {
        if (m_actionsLayout != nullptr && m_actions.empty() == false)
        {
            m_actionsLayout->setVisibility(Visibility::VISIBLE);
        }
    }
    else if (m_isHovered == true)
    {
        if (m_actionsLayout != nullptr && m_actions.empty() == false)
        {
            m_actionsLayout->setVisibility(Visibility::VISIBLE);
        }
    }
    else
    {
        if (m_actionsLayout != nullptr)
        {
            m_actionsLayout->setVisibility(Visibility::GONE);
        }
    }
}

void ListViewItem::setIcon(const std::string& text, const Color4& color)
{
    if (m_iconText != nullptr)
    {
        m_iconText->setText(text);
        m_iconText->setTextColor(color);
    }
}

void ListViewItem::setName(const std::string& name)
{
    if (m_nameText != nullptr) m_nameText->setText(name);
}

void ListViewItem::setInfo(const std::string& info)
{
    if (m_infoText != nullptr) m_infoText->setText(info);
}

void ListViewItem::setItemFocused(bool focused)
{
    m_isItemFocused = focused;
    m_hasFocus = focused;
    if (focused == false) m_actionFocusIndex = -1;
    updateVisualState();
}

void ListViewItem::addAction(const ActionButton& action)
{
    if (m_actionsLayout == nullptr)
    {
        m_actionsLayout = new LinearLayout();
        m_actionsLayout->setOrientation(Orientation::HORIZONTAL);
        m_actionsLayout->setGravity(Gravity::CENTER);
        m_actionsLayout->setSpacing(4.0f);
        m_actionsLayout->setVisibility(Visibility::GONE);
        m_actionsLayout->getLayoutParams().width = WRAP_CONTENT;
        m_actionsLayout->getLayoutParams().height = WRAP_CONTENT;
        addView(m_actionsLayout);
    }
    
    ActionButton newAction = action;
    newAction.button = new Button();
    newAction.button->setTextColor(Color::TextPrimary);
    newAction.button->setTextSize(TextSize::ExtraLarge);
    
    newAction.button->setNormalColor(action.normalColor);
    newAction.button->setFocusedColor(action.focusedColor);
    newAction.button->setPressedColor(action.pressedColor);
    newAction.button->setHoverColor(action.hoverColor);
    
    newAction.button->setCornerRadius(6.0f);
    newAction.button->getLayoutParams().width = WRAP_CONTENT;
    newAction.button->getLayoutParams().height = 30;
    newAction.button->getLayoutParams().setPadding(8.0f, 4.0f, 8.0f, 4.0f);
    
    if (action.callback != nullptr)
    {
        newAction.button->setOnClickListener([action](View* v) { action.callback(); });
    }
    
    m_actionsLayout->addView(newAction.button);
    m_actions.push_back(newAction);
}

void ListViewItem::clearActions()
{
    if (m_actionsLayout != nullptr)
    {
        m_actionsLayout->removeAllViews();
    }
    
    for (auto& action : m_actions)
    {
        action.button = nullptr;
    }
    
    m_actions.clear();
    m_actionFocusIndex = -1;
}

void ListViewItem::triggerAction(int32_t index)
{
    if (index >= 0 && index < static_cast<int32_t>(m_actions.size()))
    {
        if (m_actions[index].callback != nullptr)
        {
            m_actions[index].callback();
        }
    }
}

void ListViewItem::setActionFocusIndex(int32_t index)
{
    if (index >= -1 && index < static_cast<int32_t>(m_actions.size()))
    {
        m_actionFocusIndex = index;
        
        for (size_t i = 0; i < m_actions.size(); ++i)
        {
            if (m_actions[i].button != nullptr)
            {
                m_actions[i].button->onFocusChanged(static_cast<int32_t>(i) == m_actionFocusIndex);
            }
        }
    }
}

Button* ListViewItem::getActionButton(int32_t index)
{
    if (index >= 0 && index < static_cast<int32_t>(m_actions.size()))
    {
        return m_actions[index].button;
    }
    return nullptr;
}

const Button* ListViewItem::getActionButton(int32_t index) const
{
    if (index >= 0 && index < static_cast<int32_t>(m_actions.size()))
    {
        return m_actions[index].button;
    }
    return nullptr;
}

bool ListViewItem::onMotionEvent(const IO::MotionEvent& event)
{
    if (m_actionsLayout != nullptr && m_actionsLayout->getVisibility() == Visibility::VISIBLE)
    {
        for (ActionButton& action : m_actions)
        {
            if (action.button != nullptr && action.button->getBounds().contains(event.x, event.y) == true)
            {
                if (action.button->onMotionEvent(event) == true) return true;
            }
        }
    }
    
    if (event.action == IO::MotionAction::HOVER_MOVE || event.action == IO::MotionAction::MOVE)
    {
        bool wasHovered = m_isHovered;
        m_isHovered = m_bounds.contains(event.x, event.y);
        
        if (m_isHovered != wasHovered) updateVisualState();
        
        if (m_isHovered == true && s_currentlyHoveredItem != this)
        {
            if (s_currentlyHoveredItem != nullptr)
            {
                s_currentlyHoveredItem->m_isHovered = false;
                s_currentlyHoveredItem->updateVisualState();
            }
            s_currentlyHoveredItem = this;
        }
    }
    else if (event.action == IO::MotionAction::HOVER_EXIT)
    {
        if (m_isHovered == true && s_currentlyHoveredItem == this)
        {
            m_isHovered = false;
            s_currentlyHoveredItem = nullptr;
            updateVisualState();
        }
    }
    
    return LinearLayout::onMotionEvent(event);
}

void ListViewItem::onDraw(ICanvas& canvas)
{
    LinearLayout::onDraw(canvas);
}

// ============================================================================
// ListView Implementation
// ============================================================================

ListView::ListView()
    : m_itemSpacing(0.0f)
    , m_scrollOffset(0.0f)
    , m_maxScrollOffset(0.0f)
    , m_maxHeight(-1.0f)
    , m_isDragging(false)
    , m_dragStartY(0.0f)
    , m_dragThreshold(5.0f)
    , m_focusedItemIndex(-1)
{
}

void ListView::setAdapter(std::shared_ptr<ListAdapter> adapter)
{
    m_adapter = adapter;
    
    if (m_adapter != nullptr)
    {
        m_adapter->setOnDataSetChangedCallback([this]() { rebuildViews(); });
        rebuildViews();
    }
}

void ListView::setFocusedItemIndex(int32_t index)
{
    if (m_adapter == nullptr) return;
    
    int32_t itemCount = m_adapter->getCount();
    if (index < -1 || index >= itemCount) return;
    
    m_focusedItemIndex = index;
    updateItemFocus();
    
    if (m_focusedItemIndex >= 0)
    {
        scrollToPosition(m_focusedItemIndex);
    }
}

void ListView::clearItemFocus()
{
    m_focusedItemIndex = -1;
    updateItemFocus();
}

bool ListView::moveFocusUp()
{
    if (m_adapter == nullptr || m_adapter->getCount() == 0) return false;
    
    ListViewItem* currentItem = getItemAt(m_focusedItemIndex);
    
    if (currentItem != nullptr && currentItem->getActionFocusIndex() >= 0)
    {
        currentItem->setActionFocusIndex(-1);
        return true;
    }
    
    if (m_focusedItemIndex > 0)
    {
        setFocusedItemIndex(m_focusedItemIndex - 1);
        return true;
    }
    
    return false;
}

bool ListView::moveFocusDown()
{
    if (m_adapter == nullptr) return false;
    
    int32_t itemCount = m_adapter->getCount();
    if (itemCount == 0) return false;
    
    ListViewItem* currentItem = getItemAt(m_focusedItemIndex);
    
    if (currentItem != nullptr && currentItem->getActionFocusIndex() >= 0)
    {
        currentItem->setActionFocusIndex(-1);
        return true;
    }
    
    if (m_focusedItemIndex == -1)
    {
        setFocusedItemIndex(0);
        return true;
    }
    
    if (m_focusedItemIndex < itemCount - 1)
    {
        setFocusedItemIndex(m_focusedItemIndex + 1);
        return true;
    }
    
    return false;
}

bool ListView::moveFocusToActionLeft()
{
    ListViewItem* currentItem = getItemAt(m_focusedItemIndex);
    if (currentItem == nullptr || currentItem->hasActions() == false) return false;
    
    int32_t currentActionIndex = currentItem->getActionFocusIndex();
    
    if (currentActionIndex > 0)
    {
        currentItem->setActionFocusIndex(currentActionIndex - 1);
        return true;
    }
    else if (currentActionIndex == 0)
    {
        currentItem->setActionFocusIndex(-1);
        return true;
    }
    
    return false;
}

bool ListView::moveFocusToActionRight()
{
    ListViewItem* currentItem = getItemAt(m_focusedItemIndex);
    if (currentItem == nullptr || currentItem->hasActions() == false) return false;
    
    int32_t currentActionIndex = currentItem->getActionFocusIndex();
    int32_t actionCount = currentItem->getActionCount();
    
    if (currentActionIndex == -1 && actionCount > 0)
    {
        currentItem->setActionFocusIndex(0);
        return true;
    }
    else if (currentActionIndex >= 0 && currentActionIndex < actionCount - 1)
    {
        currentItem->setActionFocusIndex(currentActionIndex + 1);
        return true;
    }
    
    return false;
}

bool ListView::canMoveFocusUp() const
{
    ListViewItem* currentItem = getItemAt(m_focusedItemIndex);
    if (currentItem != nullptr && currentItem->getActionFocusIndex() >= 0) return true;
    return m_focusedItemIndex > 0;
}

bool ListView::canMoveFocusDown() const
{
    if (m_adapter == nullptr) return false;
    
    ListViewItem* currentItem = getItemAt(m_focusedItemIndex);
    if (currentItem != nullptr && currentItem->getActionFocusIndex() >= 0) return true;
    
    if (m_focusedItemIndex == -1) return m_adapter->getCount() > 0;
    return m_focusedItemIndex < m_adapter->getCount() - 1;
}

void ListView::scrollToPosition(int32_t position)
{
    if (m_adapter == nullptr || position < 0 || position >= m_adapter->getCount()) return;
    
    float offset = 0.0f;
    for (int32_t i = 0; i < position; ++i)
    {
        if (i < static_cast<int32_t>(m_children.size()))
        {
            offset += m_children[i]->getMeasuredSize().y + m_itemSpacing;
        }
    }
    
    float itemHeight = (position < static_cast<int32_t>(m_children.size())) ? 
                       m_children[position]->getMeasuredSize().y : 50.0f;
    
    float viewHeight = m_bounds.height();
    
    if (offset < m_scrollOffset)
    {
        scrollTo(offset);
    }
    else if (offset + itemHeight > m_scrollOffset + viewHeight)
    {
        scrollTo(offset + itemHeight - viewHeight);
    }
}

void ListView::scrollTo(float offset)
{
    m_scrollOffset = std::max(0.0f, std::min(offset, m_maxScrollOffset));
}

void ListView::onMeasure(MeasureSpec widthSpec, MeasureSpec heightSpec)
{
    float availableWidth = widthSpec.size - m_layoutParams.getPaddingHorizontal();
    float availableHeight = heightSpec.size - m_layoutParams.getPaddingVertical();
    
    availableWidth = std::max(0.0f, availableWidth);
    availableHeight = std::max(0.0f, availableHeight);
    
    for (size_t i = 0; i < m_children.size(); ++i)
    {
        View* child = m_children[i];
        if (child->getVisibility() == Visibility::GONE) continue;
        
        const LayoutParams& lp = child->getLayoutParams();
        
        MeasureSpec childWidthSpec = getChildMeasureSpec(MeasureSpec::makeAtMost(availableWidth), lp, true);
        MeasureSpec childHeightSpec = getChildMeasureSpec(MeasureSpec::makeAtMost(availableHeight), lp, false);
        
        child->onMeasure(childWidthSpec, childHeightSpec);
    }
    
    float totalHeight = 0.0f;
    float maxWidth = 0.0f;
    
    for (size_t i = 0; i < m_children.size(); ++i)
    {
        View* child = m_children[i];
        if (child->getVisibility() == Visibility::GONE) continue;
        
        const LayoutParams& lp = child->getLayoutParams();
        Vec2 size = child->getMeasuredSize();
        
        maxWidth = std::max(maxWidth, size.x + lp.getMarginHorizontal());
        totalHeight += size.y + lp.getMarginVertical();
        
        if (i < m_children.size() - 1) totalHeight += m_itemSpacing;
    }
    
    totalHeight += m_layoutParams.getPaddingVertical();
    maxWidth += m_layoutParams.getPaddingHorizontal();
    
    float width = maxWidth;
    float height = totalHeight;

    if (m_layoutParams.isExactWidth() == true) width = m_layoutParams.width;
    else if (m_layoutParams.isMatchParentWidth() == true)
    {
        if (widthSpec.mode == MeasureSpecMode::EXACTLY || widthSpec.mode == MeasureSpecMode::AT_MOST)
            width = widthSpec.size;
    }
    
    if (m_layoutParams.isExactHeight() == true)
    {
        height = m_layoutParams.height;
    }
    else if (m_layoutParams.isMatchParentHeight() == true)
    {
        if (heightSpec.mode == MeasureSpecMode::EXACTLY || heightSpec.mode == MeasureSpecMode::AT_MOST)
            height = heightSpec.size;
    }
    else
    {
        if (m_maxHeight > 0.0f && totalHeight > m_maxHeight)
            height = m_maxHeight;
        
        if (heightSpec.mode == MeasureSpecMode::AT_MOST)
            height = std::min(height, heightSpec.size);
    }
    
    m_measuredSize = Vec2(width, height);
    m_maxScrollOffset = std::max(0.0f, totalHeight - height);
}

void ListView::layoutChildren()
{
    RectF contentBounds = getChildLayoutBounds();
    float y = contentBounds.top - m_scrollOffset;
    
    for (View* child : m_children)
    {
        if (child->getVisibility() == Visibility::GONE) continue;
        
        Vec2 size = child->getMeasuredSize();
        child->onLayout(RectF::fromLTRB(contentBounds.left, y, contentBounds.right, y + size.y));
        y += size.y + m_itemSpacing;
    }
}

void ListView::onDraw(ICanvas& canvas)
{
    if (isVisible() == false) return;
    
    View::onDraw(canvas);
    
    canvas.pushClip(m_bounds);
    
    for (View* child : m_children)
    {
        if (child->isVisible() == true) child->onDraw(canvas);
    }
    
    canvas.popClip();
}

bool ListView::onKeyEvent(const IO::KeyEvent& event)
{
    if (hasFocus() == false) return false;
    if (event.action != IO::KeyAction::DOWN && event.action != IO::KeyAction::MULTIPLE) return false;
    
    bool handled = false;
    
    switch (event.keyCode)
    {
        case IO::KeyCode::DPAD_UP:
            // If no item is focused yet, or movement fails, let focus manager handle it
            if (m_focusedItemIndex == -1)
                handled = false;
            else
                handled = moveFocusUp();

            break;
            
        case IO::KeyCode::DPAD_DOWN:
            // If no item is focused, start at first item
            if (m_focusedItemIndex == -1 && m_adapter != nullptr && m_adapter->getCount() > 0)
            {
                setFocusedItemIndex(0);
                handled = true;
            }
            else
            {
                handled = moveFocusDown();
            }
            break;
            
        case IO::KeyCode::DPAD_LEFT:
            handled = moveFocusToActionLeft();
            break;
            
        case IO::KeyCode::DPAD_RIGHT:
            handled = moveFocusToActionRight();
            break;
            
        case IO::KeyCode::DPAD_CENTER:
        case IO::KeyCode::ENTER:
        case IO::KeyCode::SPACE:
        {
            if (m_focusedItemIndex >= 0)
            {
                ListViewItem* item = getItemAt(m_focusedItemIndex);
                if (item != nullptr)
                {
                    int32_t actionIndex = item->getActionFocusIndex();
                    if (actionIndex >= 0)
                        item->triggerAction(actionIndex);
                    else if (m_itemClickListener != nullptr)
                        m_itemClickListener(m_focusedItemIndex);

                    handled = true;
                }
            }
            break;
        }
        
        default:
            break;
    }
    
    return handled;
}

bool ListView::onMotionEvent(const IO::MotionEvent& event)
{
    bool canScroll = (m_maxScrollOffset > 0.0f);
    
    if (event.action == IO::MotionAction::SCROLL)
    {
        if (m_bounds.contains(event.x, event.y) == true && canScroll == true)
        {
            scrollBy(-event.scrollDelta * 50.0f);
            return true;
        }
        return false;
    }
    
    if (event.action == IO::MotionAction::DOWN)
    {
        if (m_bounds.contains(event.x, event.y) == true)
        {
            m_lastTouchPos = Vec2(event.x, event.y);
            m_dragStartY = event.y;
            m_isDragging = false;
            
            for (size_t i = 0; i < m_children.size(); ++i)
            {
                View* child = m_children[i];
                if (child->getBounds().contains(event.x, event.y) == true)
                {
                    if (child->onMotionEvent(event) == true) return true;
                }
            }
            
            return true;
        }
    }
    else if (event.action == IO::MotionAction::MOVE)
    {
        if (m_bounds.contains(event.x, event.y) == true || m_isDragging == true)
        {
            Vec2 currentPos(event.x, event.y);
            Vec2 delta = currentPos - m_lastTouchPos;
            
            if (m_isDragging == false && std::abs(currentPos.y - m_dragStartY) > m_dragThreshold)
            {
                m_isDragging = true;
            }
            
            if (m_isDragging == true && canScroll == true && std::abs(delta.y) > 0.5f)
            {
                scrollBy(-delta.y);
                m_lastTouchPos = currentPos;
            }
            
            for (View* child : m_children)
            {
                if (child->isVisible() == true) child->onMotionEvent(event);
            }
            
            return true;
        }
    }
    else if (event.action == IO::MotionAction::UP)
    {
        if (m_isDragging == false && m_bounds.contains(event.x, event.y) == true)
        {
            bool childHandled = false;
            
            for (size_t i = 0; i < m_children.size(); ++i)
            {
                View* child = m_children[i];
                if (child->getBounds().contains(event.x, event.y) == true)
                {
                    if (child->onMotionEvent(event) == true)
                    {
                        childHandled = true;
                        break;
                    }
                }
            }
            
            if (childHandled == false)
            {
                for (size_t i = 0; i < m_children.size(); ++i)
                {
                    View* child = m_children[i];
                    if (child->getBounds().contains(event.x, event.y) == true)
                    {
                        if (m_itemClickListener != nullptr)
                        {
                            m_itemClickListener(static_cast<int32_t>(i));
                        }
                        return true;
                    }
                }
            }
            
            return childHandled;
        }
        
        m_isDragging = false;
    }
    else if (event.action == IO::MotionAction::HOVER_MOVE)
    {
        for (size_t i = 0; i < m_children.size(); ++i)
        {
            View* child = m_children[i];
            if (child->isVisible() == true && child->getBounds().contains(event.x, event.y) == true)
            {
                setFocusedItemIndex(static_cast<int32_t>(i));
                break;
            }
        }
        
        for (View* child : m_children)
        {
            if (child->isVisible() == true) child->onMotionEvent(event);
        }
    }
    
    return ViewGroup::onMotionEvent(event);
}

void ListView::onFocusChanged(bool hasFocus)
{
    View::onFocusChanged(hasFocus);
    
    if (hasFocus == true && m_focusedItemIndex == -1 && 
        m_adapter != nullptr && m_adapter->getCount() > 0)
    {
        setFocusedItemIndex(0);
    }
    else if (hasFocus == false)
    {
        clearItemFocus();
    }
}

void ListView::rebuildViews()
{
    removeAllViews();
    
    if (m_adapter == nullptr) return;
    
    int32_t count = m_adapter->getCount();
    for (int32_t i = 0; i < count; ++i)
    {
        View* view = m_adapter->getView(i, nullptr);
        if (view != nullptr) addView(view);
    }
    
    updateItemFocus();
}

void ListView::updateItemFocus()
{
    for (size_t i = 0; i < m_children.size(); ++i)
    {
        ListViewItem* item = getItemAt(static_cast<int32_t>(i));
        if (item != nullptr)
        {
            item->setItemFocused(static_cast<int32_t>(i) == m_focusedItemIndex);
        }
    }
}

ListViewItem* ListView::getItemAt(int32_t index) const
{
    if (index < 0 || index >= static_cast<int32_t>(m_children.size())) return nullptr;
    return dynamic_cast<ListViewItem*>(m_children[index]);
}

// ============================================================================
// AIOverlay Implementation
// ============================================================================

AIOverlay::AIOverlay()
    : m_imageView(nullptr)
    , m_nativeW(WND_WIDTH)
    , m_nativeH(WND_HEIGHT)
    , m_showBoundingBoxes(true)
    , m_showBBoxLabels(true)
    , m_bboxStrokeWidth(3.0f)
    , m_activeZoneIndex(-1)
    , m_activeDragPointIndex(-1)
    , m_dpadSelectedPointIndex(-1)
{
    getLayoutParams().width  = MATCH_PARENT;
    getLayoutParams().height = MATCH_PARENT;
}

Vec2 AIOverlay::nativeToScreen(const Vec2& nativePt) const
{
    if (m_imageView == nullptr || m_nativeW <= 0 || m_nativeH <= 0) return Vec2(0.0f, 0.0f);
    RectF imgRect = m_imageView->getImageRect();
    return Vec2(
        imgRect.left + (nativePt.x / static_cast<float>(m_nativeW)) * imgRect.width(),
        imgRect.top  + (nativePt.y / static_cast<float>(m_nativeH)) * imgRect.height());
}

Vec2 AIOverlay::screenToNative(float screenX, float screenY) const
{
    if (m_imageView == nullptr || m_nativeW <= 0 || m_nativeH <= 0) return Vec2(0.0f, 0.0f);
    RectF  imgRect = m_imageView->getImageRect();
    float  iw      = imgRect.width();
    float  ih      = imgRect.height();
    if (iw <= 0.0f || ih <= 0.0f) return Vec2(0.0f, 0.0f);

    float nativeX = std::max(0.0f, std::min(((screenX - imgRect.left) / iw) * static_cast<float>(m_nativeW), static_cast<float>(m_nativeW)));
    float nativeY = std::max(0.0f, std::min(((screenY - imgRect.top)  / ih) * static_cast<float>(m_nativeH), static_cast<float>(m_nativeH)));
    return Vec2(nativeX, nativeY);
}

RectF AIOverlay::bboxNativeToScreen(const AI::BoundingBox& bbox) const
{
    if (m_imageView == nullptr || m_nativeW <= 0 || m_nativeH <= 0) return RectF(0.0f, 0.0f, 0.0f, 0.0f);
    RectF  imgRect = m_imageView->getImageRect();
    float  scaleX  = imgRect.width()  / static_cast<float>(m_nativeW);
    float  scaleY  = imgRect.height() / static_cast<float>(m_nativeH);
    return RectF::fromXYWH(
        imgRect.left + bbox.x * static_cast<float>(m_nativeW) * scaleX,
        imgRect.top  + bbox.y * static_cast<float>(m_nativeH) * scaleY,
        bbox.width   * static_cast<float>(m_nativeW) * scaleX,
        bbox.height  * static_cast<float>(m_nativeH) * scaleY);
}

int AIOverlay::hitTestROIPoint(float screenX, float screenY, float hitRadiusPx) const
{
    const float r2 = hitRadiusPx * hitRadiusPx;
    for (const AI::ROIZone& zone : m_roiZones)
    {
        if (zone.visible == false) continue;
        for (int i = 0; i < static_cast<int>(zone.points.size()); i++)
        {
            Vec2  sc = nativeToScreen(zone.points[i]);
            float dx = screenX - sc.x;
            float dy = screenY - sc.y;
            if ((dx * dx + dy * dy) <= r2) return i;
        }
    }
    return -1;
}

void AIOverlay::onDraw(ICanvas& canvas)
{
    if (isVisible() == false) return;
    drawROIZones(canvas);
    if (m_showBoundingBoxes == true) drawBoundingBoxes(canvas);
}

bool AIOverlay::onMotionEvent(const IO::MotionEvent& event)
{
    if (m_onROIPointMoved == nullptr) return false;

    if (event.action == IO::MotionAction::DOWN)
    {
        m_activeDragPointIndex = hitTestROIPoint(event.x, event.y, 30.0f);
        return m_activeDragPointIndex >= 0;
    }

    if (event.action == IO::MotionAction::MOVE || event.action == IO::MotionAction::UP)
    {
        if (m_activeDragPointIndex < 0) return false;
        Vec2 native = screenToNative(event.x, event.y);
        m_onROIPointMoved(m_activeDragPointIndex, native.x, native.y);
        if (event.action == IO::MotionAction::UP) m_activeDragPointIndex = -1;
        return true;
    }

    return false;
}

void AIOverlay::drawROIZones(ICanvas& canvas)
{
    if (m_roiZones.empty() == true || m_imageView == nullptr) return;

    canvas.pushClip(m_imageView->getImageRect());

    for (int zoneIdx = 0; zoneIdx < static_cast<int>(m_roiZones.size()); zoneIdx++)
    {
        const AI::ROIZone& zone = m_roiZones[zoneIdx];
        if (zone.visible == false || zone.points.size() < 3) continue;

        std::vector<Vec2> screenPoints;
        screenPoints.reserve(zone.points.size());
        for (const Vec2& pt : zone.points)
        {
            screenPoints.push_back(nativeToScreen(pt));
        }

        if (zone.fillColor.a > 0.0f)
        {
            Paint fillPaint;
            fillPaint.filled     = true;
            fillPaint.bgColor    = zone.fillColor;
            fillPaint.strokeWidth = 0.0f;
            canvas.drawPolygon(screenPoints, fillPaint);
        }

        if (zone.strokeWidth > 0.0f && zone.strokeColor.a > 0.0f)
        {
            Paint strokePaint;
            strokePaint.filled      = false;
            strokePaint.fgColor     = zone.strokeColor;
            strokePaint.strokeWidth = zone.strokeWidth;
            canvas.drawPolygon(screenPoints, strokePaint);
        }

        // Draw drag handles only on the active zone.
        if (m_onROIPointMoved == nullptr) continue;
        if (zoneIdx != m_activeZoneIndex) continue;

        for (int i = 0; i < static_cast<int>(screenPoints.size()); i++)
        {
            const Vec2& sc = screenPoints[i];

            // Colour priority: touch-dragging > dpad-selected > idle.
            Color4 fillColor;
            Color4 ringColor;
            float  innerRadius;
            float  outerRadius;

            if (i == m_activeDragPointIndex)
            {
                // Currently touch-dragged: bright yellow, fully opaque.
                fillColor    = Color4::fromRGBA(255, 220,   0, 230);
                ringColor    = Color4::fromRGBA(255, 255, 255, 200);
                innerRadius  = 24.0f;
                outerRadius  = 30.0f;
            }
            else if (i == m_dpadSelectedPointIndex)
            {
                // Selected by dpad: cyan, semi-transparent.
                fillColor    = Color4::fromRGBA(  0, 200, 255, 180);
                ringColor    = Color4::fromRGBA(255, 255, 255, 160);
                innerRadius  = 22.0f;
                outerRadius  = 28.0f;
            }
            else
            {
                // Idle: white, more transparent.
                fillColor    = Color4::fromRGBA(255, 255, 255,  90);
                ringColor    = Color4::fromRGBA(200, 200, 200,  80);
                innerRadius  = 20.0f;
                outerRadius  = 26.0f;
            }

            // Outer ring.
            Paint ringPaint;
            ringPaint.filled      = false;
            ringPaint.fgColor     = ringColor;
            ringPaint.strokeWidth = 3.0f;
            canvas.drawCircle(sc, outerRadius, ringPaint);

            // Filled inner circle.
            Paint fillPaint;
            fillPaint.filled      = true;
            fillPaint.bgColor     = fillColor;
            fillPaint.strokeWidth = 0.0f;
            canvas.drawCircle(sc, innerRadius, fillPaint);
        }
    }

    canvas.popClip();
}

void AIOverlay::drawBoundingBoxes(ICanvas& canvas)
{
    if (m_boundingBoxes.empty() == true || m_imageView == nullptr) return;
    
    for (const AI::BoundingBox& bbox : m_boundingBoxes)
    {
        RectF screenRect = bboxNativeToScreen(bbox);
        
        Paint boxPaint;
        boxPaint.filled = true;
        boxPaint.bgColor = bbox.color.withAlpha(0.2f);
        boxPaint.strokeWidth = m_bboxStrokeWidth;
        canvas.drawRect(screenRect, boxPaint);
        
        if (m_showBBoxLabels == true && bbox.label.empty() == false)
        {
            Paint labelPaint;
            labelPaint.fgColor = Color::White;
            labelPaint.textProps = TextProps("default", TextSize::Title);
            
            float labelX = screenRect.left + 5.0f;
            float labelY = screenRect.top - 5.0f;
            
            if (labelY < 20.0f) labelY = screenRect.top + 20.0f;
            
            std::string labelText = bbox.label;
            if (bbox.confidence > 0.0f)
            {
                char conf[32];
                snprintf(conf, sizeof(conf), " %.0f%%", bbox.confidence * 100.0f);
                labelText += conf;
            }
            
            canvas.drawText(labelText, Vec2(labelX, labelY), labelPaint);
        }
    }
}

// ============================================================================
// AIImageView Implementation
// ============================================================================

AIImageView::AIImageView()
    : m_imageView(nullptr)
    , m_aiOverlay(nullptr)
    , m_nativeW(WND_WIDTH)
    , m_nativeH(WND_HEIGHT)
{
    m_imageView = new ImageView();
    m_imageView->getLayoutParams().width  = MATCH_PARENT;
    m_imageView->getLayoutParams().height = MATCH_PARENT;
    m_imageView->setScaleType(ScaleType::FIT_XY);
    addView(m_imageView);

    m_aiOverlay = new AIOverlay();
    m_aiOverlay->setImageView(m_imageView);
    m_aiOverlay->setNativeSize(m_nativeW, m_nativeH);
    addView(m_aiOverlay);

    setFramebufferEnabled(true);
}

void AIImageView::setNativeSize(int nativeW, int nativeH)
{
    if (nativeW <= 0 || nativeH <= 0) return;
    m_nativeW = nativeW;
    m_nativeH = nativeH;
    if (m_aiOverlay != nullptr) m_aiOverlay->setNativeSize(nativeW, nativeH);
    updateFramebufferSize(nativeW, nativeH);
}

void AIImageView::onLayout(const RectF& bounds)
{
    m_bounds = bounds;
    RectF nativeBounds = RectF::fromXYWH(0.0f, 0.0f,
        static_cast<float>(m_nativeW), static_cast<float>(m_nativeH));
    for (View* child : m_children)
        if (child != nullptr) child->onLayout(nativeBounds);
    updateFramebufferSize(m_nativeW, m_nativeH);
}

void AIImageView::onDraw(ICanvas& canvas)
{
    if (isVisible() == false) return;

    // 1. Render children (ImageView + AIOverlay) into the fixed-size FB.
    if (m_framebuffer != nullptr && m_framebuffer->isValid() == true)
    {
        canvas.renderToFramebuffer(m_framebuffer.get(), this);
    }

    // 2. Draw the FB texture scaled to fit the actual cell bounds on screen.
    Texture* fbTex = getFramebufferTexture();
    if (fbTex != nullptr && fbTex->isValid() == true)
    {
        RectF srcRect = RectF::fromLTRB(0.0f,
                                         static_cast<float>(fbTex->height),
                                         static_cast<float>(fbTex->width),
                                         0.0f);  // flipY: FB is upside-down in OpenGL
        Paint texPaint;
        texPaint.opacity = 1.0f;
        texPaint.cornerRadius = m_normalPaint.cornerRadius;

        // Use FIT_CENTER so aspect ratio is preserved inside the cell.
        float cellW = m_bounds.width();
        float cellH = m_bounds.height();
        float texW  = static_cast<float>(fbTex->width);
        float texH  = static_cast<float>(fbTex->height);
        float scale = std::min(cellW / texW, cellH / texH);
        float dstW  = texW * scale;
        float dstH  = texH * scale;
        float dstX  = m_bounds.left + (cellW - dstW) * 0.5f;
        float dstY  = m_bounds.top  + (cellH - dstH) * 0.5f;
        RectF dstRect = RectF::fromXYWH(dstX, dstY, dstW, dstH);

        canvas.drawTexture(fbTex, srcRect, dstRect, texPaint);
    }
}

bool AIImageView::onMotionEvent(const IO::MotionEvent& event)
{
    if (m_aiOverlay != nullptr) return m_aiOverlay->onMotionEvent(event);
    return false;
}

void AIImageView::setImage(Texture* texture, bool yflip)
{
    if (m_imageView != nullptr) m_imageView->setTexture(texture, yflip);
}

// Delegate ROI/bbox/overlay methods to AIOverlay
void AIImageView::addROIZone(const AI::ROIZone& zone)
{
    if (m_aiOverlay != nullptr) m_aiOverlay->addROIZone(zone);
}

void AIImageView::clearROIZones()
{
    if (m_aiOverlay != nullptr) m_aiOverlay->clearROIZones();
}

const std::vector<AI::ROIZone>& AIImageView::getROIZones() const
{
    static std::vector<AI::ROIZone> empty;
    return (m_aiOverlay != nullptr) ? m_aiOverlay->getROIZones() : empty;
}

void AIImageView::setBoundingBoxes(const std::vector<AI::BoundingBox>& boxes)
{
    if (m_aiOverlay != nullptr) m_aiOverlay->setBoundingBoxes(boxes);
}

void AIImageView::clearBoundingBoxes()
{
    if (m_aiOverlay != nullptr) m_aiOverlay->clearBoundingBoxes();
}

const std::vector<AI::BoundingBox>& AIImageView::getBoundingBoxes() const
{
    static std::vector<AI::BoundingBox> empty;
    return (m_aiOverlay != nullptr) ? m_aiOverlay->getBoundingBoxes() : empty;
}

void AIImageView::setShowBoundingBoxes(bool show)
{
    if (m_aiOverlay != nullptr) m_aiOverlay->setShowBoundingBoxes(show);
}

bool AIImageView::isShowingBoundingBoxes() const
{
    return (m_aiOverlay != nullptr) ? m_aiOverlay->isShowingBoundingBoxes() : false;
}

void AIImageView::setBBoxStrokeWidth(float width)
{
    if (m_aiOverlay != nullptr) m_aiOverlay->setBBoxStrokeWidth(width);
}

void AIImageView::setShowBBoxLabels(bool show)
{
    if (m_aiOverlay != nullptr) m_aiOverlay->setShowBBoxLabels(show);
}

void AIImageView::setOnROIPointMoved(ROIPointMovedCallback cb)
{
    if (m_aiOverlay != nullptr) m_aiOverlay->setOnROIPointMoved(cb);
}

void AIImageView::setActiveZoneIndex(int zoneIdx)
{
    if (m_aiOverlay != nullptr) m_aiOverlay->setActiveZoneIndex(zoneIdx);
}

void AIImageView::setDpadSelectedPoint(int ptIndex)
{
    if (m_aiOverlay != nullptr) m_aiOverlay->setDpadSelectedPoint(ptIndex);
}

int AIImageView::hitTestROIPoint(float screenX, float screenY, float hitRadiusPx) const
{
    return (m_aiOverlay != nullptr) ? m_aiOverlay->hitTestROIPoint(screenX, screenY, hitRadiusPx) : -1;
}

// ============================================================================
// GridCellView Implementation
// ============================================================================

GridCellView* GridCellView::s_currentlyHoveredCell = nullptr;

GridCellView::GridCellView()
    : FrameLayout()
    , m_isCellFocused(false)
    , m_isCellHovered(false)
    , m_isCellPressed(false)
    , m_warningVisible(false)
    , m_cellData(nullptr)
    , m_aiImageView(nullptr)
    , m_nameText(nullptr)
    , m_infoText(nullptr)
    , m_actionsLayout(nullptr)
    , m_warningOverlay(nullptr)
{
    setNormalColor(Color::Transparent);
    setHoverColor(Color::Transparent);
    setFocusedColor(Color::Transparent);
    setPressedColor(Color::CardBackgroundHover);
    
    setupContent();
}

GridCellView::~GridCellView()
{
    if (s_currentlyHoveredCell == this)
    {
        s_currentlyHoveredCell = nullptr;
    }
}

void GridCellView::setupContent()
{
    // Create image container (replaces plain ImageView)
    m_aiImageView = new AIImageView();
    m_aiImageView->getLayoutParams().width  = MATCH_PARENT;
    m_aiImageView->getLayoutParams().height = MATCH_PARENT;
    m_aiImageView->getLayoutParams().weight = 1.0f;
    addView(m_aiImageView);

    // Create warning overlay
    m_warningOverlay = new ImageView();
    m_warningOverlay->setBackgroundColor(Color4::fromRGBA(255, 0, 0, 50));
    m_warningOverlay->getLayoutParams().width  = MATCH_PARENT;
    m_warningOverlay->getLayoutParams().height = MATCH_PARENT;
    m_warningOverlay->setVisibility(Visibility::GONE);
    addView(m_warningOverlay);
    
    // Create text layout for name/info
    LinearLayout* textLayout = new LinearLayout();
    textLayout->setBackgroundColor(Color::CardBackground);
    textLayout->setCornerRadius(10.0f);
    textLayout->setOrientation(Orientation::VERTICAL);
    textLayout->setGravity(Gravity::LEFT);
    textLayout->setSpacing(2.0f);
    textLayout->getLayoutParams().gravity = Gravity::TOP_LEFT;
    textLayout->getLayoutParams().width = WRAP_CONTENT;
    textLayout->getLayoutParams().height = WRAP_CONTENT;
    textLayout->getLayoutParams().setPadding(6.0f);
    textLayout->getLayoutParams().setMargin(6.0f);
    
    m_nameText = new TextView();
    m_nameText->setTextColor(Color::TextPrimary);
    m_nameText->setTextSize(TextSize::ExtraLarge);
    m_nameText->setTextGravity(Gravity::LEFT);
    m_nameText->getLayoutParams().width = WRAP_CONTENT;
    m_nameText->getLayoutParams().height = WRAP_CONTENT;
    textLayout->addView(m_nameText);
    
    m_infoText = new TextView();
    m_infoText->setTextColor(Color::TextSecondary);
    m_infoText->setTextSize(TextSize::Large);
    m_infoText->setTextGravity(Gravity::LEFT);
    m_infoText->getLayoutParams().width = WRAP_CONTENT;
    m_infoText->getLayoutParams().height = WRAP_CONTENT;
    textLayout->addView(m_infoText);
    
    addView(textLayout);
}

void GridCellView::updateVisualState()
{
    if (m_isCellFocused == true)
    {
        if (m_actionsLayout != nullptr && m_actions.empty() == false)
        {
            m_actionsLayout->setVisibility(Visibility::VISIBLE);
        }
    }
    else if (m_isCellHovered == true)
    {
        if (m_actionsLayout != nullptr && m_actions.empty() == false)
        {
            m_actionsLayout->setVisibility(Visibility::VISIBLE);
        }
    }
    else
    {
        if (m_actionsLayout != nullptr)
        {
            m_actionsLayout->setVisibility(Visibility::GONE);
        }
    }
}

void GridCellView::setCornerRadius(float radius)
{
    View::setCornerRadius(radius);
    if (m_warningOverlay != nullptr) m_warningOverlay->setCornerRadius(radius);
    if (m_aiImageView    != nullptr)
    {
        m_aiImageView->setCornerRadius(radius);
        if(m_aiImageView->getImageView() != nullptr) m_aiImageView->getImageView()->setCornerRadius(radius);
    }
}

void GridCellView::setName(const std::string& name)
{
    if (m_nameText != nullptr)
    {
        m_nameText->setText(name);
    }
}

void GridCellView::setInfo(const std::string& info)
{
    if (m_infoText != nullptr)
    {
        m_infoText->setText(info);
    }
}

void GridCellView::setImage(Texture* texture, bool yflip)
{
    if (m_aiImageView != nullptr)
    {
        m_aiImageView->setImage(texture, yflip);
    }
}

void GridCellView::setWarningVisible(bool visible)
{
    m_warningVisible = visible;
    if (m_warningOverlay != nullptr)
    {
        m_warningOverlay->setVisibility(visible ? Visibility::VISIBLE : Visibility::GONE);
    }
}

void GridCellView::setWarningColor(const Color4& color)
{
    if (m_warningOverlay != nullptr)
        m_warningOverlay->setBackgroundColor(color);
}

void GridCellView::setWarningTexture(Texture* texture)
{
    if (m_warningOverlay != nullptr)
    {
        m_warningOverlay->setTexture(texture);
        m_warningOverlay->setScaleType(ScaleType::FIT_XY);
    }
}

void GridCellView::setWarningSize(float w, float h)
{
    if (m_warningOverlay == nullptr) return;
    LayoutParams& lp = m_warningOverlay->getLayoutParams();
    lp.width  = static_cast<int>(w);
    lp.height = static_cast<int>(h);
    m_warningOverlay->setCornerRadius(std::min(w, h) * 0.15f);
}

void GridCellView::setWarningGravity(Gravity gravity, float margin)
{
    if (m_warningOverlay == nullptr) return;
    m_warningOverlay->getLayoutParams().gravity = gravity;
    m_warningOverlay->getLayoutParams().setMargin(margin);
}

void GridCellView::setCellFocused(bool focused)
{
    if (m_isCellFocused != focused)
    {
        m_isCellFocused = focused;
        updateVisualState();
    }
}

void GridCellView::setCellHovered(bool hovered)
{
    if (m_isCellHovered != hovered)
    {
        m_isCellHovered = hovered;
        updateVisualState();
    }
}

void GridCellView::setCellPressed(bool pressed)
{
    if (m_isCellPressed != pressed)
    {
        m_isCellPressed = pressed;
    }
}

void GridCellView::onFocusChanged(bool hasFocus)
{
    View::onFocusChanged(hasFocus);
    
    // Sync with cell-specific focus state
    if (m_isCellFocused != hasFocus)
    {
        m_isCellFocused = hasFocus;
        
        // When gaining focus via keyboard, clear any previous hover state
        if (hasFocus && s_currentlyHoveredCell != nullptr && s_currentlyHoveredCell != this)
        {
            s_currentlyHoveredCell->m_isCellHovered = false;
            s_currentlyHoveredCell->updateVisualState();
            s_currentlyHoveredCell = nullptr;
        }
        
        updateVisualState();
    }
}

void GridCellView::addAction(const ActionButton& action)
{
    if (m_actionsLayout == nullptr)
    {
        m_actionsLayout = new LinearLayout();
        m_actionsLayout->setOrientation(Orientation::HORIZONTAL);
        m_actionsLayout->setGravity(Gravity::CENTER);
        m_actionsLayout->setSpacing(8.0f);
        m_actionsLayout->getLayoutParams().width = WRAP_CONTENT;
        m_actionsLayout->getLayoutParams().height = WRAP_CONTENT;
        m_actionsLayout->getLayoutParams().gravity = Gravity::TOP | Gravity::RIGHT;
        m_actionsLayout->getLayoutParams().setMargin(8.0f);
        m_actionsLayout->setVisibility(Visibility::GONE);
        addView(m_actionsLayout);
    }
    
    Button* btn = new Button();
    btn->setBackgroundColor(action.normalColor);
    btn->setHoverColor(action.hoverColor);
    btn->setFocusedColor(action.focusedColor);
    btn->setPressedColor(action.pressedColor);
    btn->setCornerRadius(8.0f);
    btn->getLayoutParams().width = WRAP_CONTENT;
    btn->getLayoutParams().height = WRAP_CONTENT;
    btn->getLayoutParams().setPadding(8.0f, 8.0f, 8.0f, 8.0f);
    btn->setFocusable(true);
    
    if (action.callback != nullptr)
    {
        btn->setOnClickListener([action](View* v) { action.callback(); });
    }
    
    m_actionsLayout->addView(btn);
    m_actions.push_back(action);
}

void GridCellView::clearActions()
{
    if (m_actionsLayout != nullptr)
    {
        m_actionsLayout->removeAllViews();
        m_actions.clear();
    }
}

void GridCellView::triggerAction(int32_t index)
{
    if (index >= 0 && index < static_cast<int32_t>(m_actions.size()))
    {
        if (m_actions[index].callback != nullptr)
        {
            m_actions[index].callback();
        }
    }
}

Button* GridCellView::getActionButton(int32_t index)
{
    if (m_actionsLayout == nullptr || index < 0 || index >= (int32_t)m_actionsLayout->getChildCount())
    {
        return nullptr;
    }
    return dynamic_cast<Button*>(m_actionsLayout->getChildAt(index));
}

const Button* GridCellView::getActionButton(int32_t index) const
{
    if (m_actionsLayout == nullptr || index < 0 || index >= (int32_t)m_actionsLayout->getChildCount())
    {
        return nullptr;
    }
    return dynamic_cast<const Button*>(m_actionsLayout->getChildAt(index));
}

bool GridCellView::onMotionEvent(const IO::MotionEvent& event)
{
    if (event.action == IO::MotionAction::HOVER_ENTER)
    {
        if (m_isCellHovered == false)
        {
            m_isCellHovered = true;
            updateVisualState();
            s_currentlyHoveredCell = this;
        }
    }
    else if (event.action == IO::MotionAction::HOVER_EXIT)
    {
        if (m_isCellHovered == true && s_currentlyHoveredCell == this)
        {
            m_isCellHovered = false;
            s_currentlyHoveredCell = nullptr;
            updateVisualState();
        }
    }
    else if (event.action == IO::MotionAction::DOWN)
    {
        if (m_bounds.contains(event.x, event.y) == true)
        {
            setPressed(true);
            if (m_isCellHovered == false)
            {
                m_isCellHovered = true;
                if (s_currentlyHoveredCell != nullptr && s_currentlyHoveredCell != this)
                {
                    s_currentlyHoveredCell->m_isCellHovered = false;
                    s_currentlyHoveredCell->updateVisualState();
                }
                s_currentlyHoveredCell = this;
            }
            updateVisualState();
            return true;
        }
    }
    else if (event.action == IO::MotionAction::UP)
    {
        if (m_isPressed == true && m_bounds.contains(event.x, event.y) == true && m_onClickCallback != nullptr)
            m_onClickCallback(this);
        setPressed(false);
        updateVisualState();
        return true;
    }

    return FrameLayout::onMotionEvent(event);
}

void GridCellView::onDraw(ICanvas& canvas)
{
    FrameLayout::onDraw(canvas);
}

// ============================================================================
// AutoGridView Implementation
// ============================================================================

AutoGridView::AutoGridView()
    : ViewGroup()
    , m_numColumns(3)
    , m_numRows(2)
    , m_horizontalSpacing(8.0f)
    , m_verticalSpacing(8.0f)
    , m_cellAspectRatio(16.0f / 9.0f)
    , m_scrollOffset(0.0f)
    , m_maxScrollOffset(0.0f)
    , m_isDragging(false)
    , m_dragStartY(0.0f)
    , m_dragThreshold(10.0f)
    , m_focusedCellIndex(0)
    , m_fullscreenCellIndex(-1)
{
}

void AutoGridView::setAdapter(std::shared_ptr<AutoGridAdapter> adapter)
{
    if (m_adapter != nullptr)
    {
        m_adapter->setOnDataSetChangedCallback(nullptr);
    }
    
    m_adapter = adapter;
    
    if (m_adapter != nullptr)
    {
        m_adapter->setOnDataSetChangedCallback([this]() { rebuildCells();
        });
    }
    
    rebuildCells();
}

void AutoGridView::setNumColumns(int columns)
{
    if (m_numColumns != columns && columns > 0)
    {
        m_numColumns = columns;
        recalculateGrid();
    }
}

void AutoGridView::setNumRows(int rows)
{
    if (m_numRows != rows && rows > 0)
    {
        m_numRows = rows;
        recalculateGrid();
    }
}

void AutoGridView::setFullscreenCell(int32_t cellIndex)
{
    if (m_adapter == nullptr || cellIndex < 0 || cellIndex >= m_adapter->getCount())
    {
        return;
    }
    
    if (m_fullscreenCellIndex == cellIndex)
    {
        return;
    }
    
    if (m_fullscreenCellIndex >= 0)
    {
        exitFullscreen();
    }
    
    m_fullscreenCellIndex = cellIndex;
    
    if (m_onFullscreenChange != nullptr)
    {
        m_onFullscreenChange(cellIndex, true);
    }
}

void AutoGridView::exitFullscreen()
{
    if (m_fullscreenCellIndex < 0)
    {
        return;
    }
    
    const int32_t previousIndex = m_fullscreenCellIndex;
    m_fullscreenCellIndex = -1;
    
    if (m_onFullscreenChange != nullptr)
    {
        m_onFullscreenChange(previousIndex, false);
    }
}

void AutoGridView::toggleFullscreen(int32_t cellIndex)
{
    if (m_fullscreenCellIndex == cellIndex)
    {
        exitFullscreen();
    }
    else
    {
        setFullscreenCell(cellIndex);
    }
}

void AutoGridView::setFocusedCellIndex(int32_t index)
{
    if (m_focusedCellIndex != index)
    {
        m_focusedCellIndex = index;
        updateCellFocus();
    }
}

void AutoGridView::clearCellFocus()
{
    setFocusedCellIndex(-1);
}

bool AutoGridView::moveFocusUp()
{
    if (m_fullscreenCellIndex >= 0 || canMoveFocusUp() == false)
    {
        return false;
    }
    
    const int32_t newIndex = m_focusedCellIndex - m_numColumns;
    setFocusedCellIndex(newIndex);
    scrollToCell(newIndex);
    return true;
}

bool AutoGridView::moveFocusDown()
{
    if (m_fullscreenCellIndex >= 0 || canMoveFocusDown() == false)
    {
        return false;
    }
    
    const int32_t newIndex = m_focusedCellIndex + m_numColumns;
    setFocusedCellIndex(newIndex);
    scrollToCell(newIndex);
    return true;
}

bool AutoGridView::moveFocusLeft()
{
    if (m_fullscreenCellIndex >= 0 || canMoveFocusLeft() == false)
    {
        return false;
    }
    
    const int32_t newIndex = m_focusedCellIndex - 1;
    setFocusedCellIndex(newIndex);
    scrollToCell(newIndex);
    return true;
}

bool AutoGridView::moveFocusRight()
{
    if (m_fullscreenCellIndex >= 0 || canMoveFocusRight() == false)
    {
        return false;
    }
    
    const int32_t newIndex = m_focusedCellIndex + 1;
    setFocusedCellIndex(newIndex);
    scrollToCell(newIndex);
    return true;
}

bool AutoGridView::canMoveFocusUp() const
{
    if (m_focusedCellIndex < 0 || m_adapter == nullptr)
    {
        return false;
    }
    return m_focusedCellIndex >= m_numColumns;
}

bool AutoGridView::canMoveFocusDown() const
{
    if (m_focusedCellIndex < 0 || m_adapter == nullptr)
    {
        return false;
    }
    const int32_t newIndex = m_focusedCellIndex + m_numColumns;
    return newIndex < m_adapter->getCount();
}

bool AutoGridView::canMoveFocusLeft() const
{
    if (m_focusedCellIndex < 0 || m_adapter == nullptr)
    {
        return false;
    }
    return (m_focusedCellIndex % m_numColumns) > 0;
}

bool AutoGridView::canMoveFocusRight() const
{
    if (m_focusedCellIndex < 0 || m_adapter == nullptr)
    {
        return false;
    }
    const int32_t newIndex = m_focusedCellIndex + 1;
    return (newIndex < m_adapter->getCount()) && ((newIndex % m_numColumns) != 0);
}

void AutoGridView::updateCellFocus()
{
    for (size_t i = 0; i < m_cells.size(); ++i)
    {
        GridCellView* cell = m_cells[i];
        if (cell != nullptr)
        {
            cell->setCellFocused(static_cast<int32_t>(i) == m_focusedCellIndex);
        }
    }
}

void AutoGridView::scrollTo(float offset)
{
    m_scrollOffset = std::max(0.0f, std::min(offset, m_maxScrollOffset));
}

void AutoGridView::scrollToCell(int32_t cellIndex)
{
    GridCellView* cell = getCellAt(cellIndex);
    if (cell == nullptr)
    {
        return;
    }
    
    const RectF& cellBounds = cell->getBounds();
    const RectF& viewBounds = getBounds();
    
    if (cellBounds.top < viewBounds.top)
    {
        scrollBy(cellBounds.top - viewBounds.top);
    }
    else if (cellBounds.bottom > viewBounds.bottom)
    {
        scrollBy(cellBounds.bottom - viewBounds.bottom);
    }
}

void AutoGridView::calculateCellSize(float availableWidth, float availableHeight,
                                 float& outCellWidth, float& outCellHeight)
{
    const float totalHSpacing = m_horizontalSpacing * static_cast<float>(m_numColumns - 1);
    const float totalVSpacing = m_verticalSpacing * static_cast<float>(m_numRows - 1);
    
    outCellWidth = (availableWidth - totalHSpacing) / static_cast<float>(m_numColumns);
    
    if (m_cellAspectRatio > 0.0f)
    {
        outCellHeight = outCellWidth / m_cellAspectRatio;
    }
    else
    {
        outCellHeight = (availableHeight - totalVSpacing) / static_cast<float>(m_numRows);
    }
}

void AutoGridView::onMeasure(MeasureSpec widthSpec, MeasureSpec heightSpec)
{
    const float width = widthSpec.size;
    const float height = heightSpec.size;
    
    setMeasuredSize(Vec2(width, height));
    
    if (m_adapter != nullptr && m_cells.empty() == false)
    {
        float cellWidth = 0.0f;
        float cellHeight = 0.0f;
        
        if (m_fullscreenCellIndex >= 0)
        {
            cellWidth = width;
            cellHeight = height;
        }
        else
        {
            calculateCellSize(width, height, cellWidth, cellHeight);
        }
        
        MeasureSpec cellWidthSpec = MeasureSpec::makeExactly(cellWidth);
        MeasureSpec cellHeightSpec = MeasureSpec::makeExactly(cellHeight);
        
        for (GridCellView* cell : m_cells)
        {
            if (cell != nullptr)
            {
                cell->onMeasure(cellWidthSpec, cellHeightSpec);
            }
        }
    }
}

void AutoGridView::layoutChildren()
{
    if (m_fullscreenCellIndex >= 0)
    {
        layoutFullscreenCell();
    }
    else
    {
        layoutNormalGrid();
    }
}

void AutoGridView::layoutFullscreenCell()
{
    if (m_adapter == nullptr || m_fullscreenCellIndex < 0)
    {
        return;
    }
    
    GridCellView* fullscreenCell = getCellAt(m_fullscreenCellIndex);
    if (fullscreenCell == nullptr)
    {
        return;
    }
    
    const RectF& bounds = getBounds();
    fullscreenCell->onLayout(bounds);
    fullscreenCell->setVisibility(Visibility::VISIBLE);
    
    // Hide all other cells
    for (size_t i = 0; i < m_cells.size(); ++i)
    {
        if (static_cast<int32_t>(i) != m_fullscreenCellIndex && m_cells[i] != nullptr)
        {
            m_cells[i]->setVisibility(Visibility::GONE);
        }
    }
}

void AutoGridView::layoutNormalGrid()
{
    if (m_adapter == nullptr || m_cells.empty() == true)
    {
        return;
    }
    
    const RectF& bounds = getBounds();
    float cellWidth = 0.0f;
    float cellHeight = 0.0f;
    calculateCellSize(bounds.width(), bounds.height(), cellWidth, cellHeight);
    
    float y = bounds.top - m_scrollOffset;
    int32_t position = 0;
    
    for (int row = 0; row < m_numRows && position < static_cast<int32_t>(m_cells.size()); ++row)
    {
        float x = bounds.left;
        
        for (int col = 0; col < m_numColumns && position < static_cast<int32_t>(m_cells.size()); ++col)
        {
            GridCellView* cell = m_cells[position];
            if (cell != nullptr)
            {
                RectF cellBounds = RectF::fromXYWH(x, y, cellWidth, cellHeight);
                cell->onLayout(cellBounds);
                cell->setVisibility(Visibility::VISIBLE);
            }
            
            x += cellWidth + m_horizontalSpacing;
            ++position;
        }
        
        y += cellHeight + m_verticalSpacing;
    }
    
    // Calculate max scroll
    const float totalRows = std::ceil(static_cast<float>(m_adapter->getCount()) / static_cast<float>(m_numColumns));
    const float totalHeight = (totalRows * cellHeight) + ((totalRows - 1) * m_verticalSpacing);
    m_maxScrollOffset = std::max(0.0f, totalHeight - bounds.height());
}

void AutoGridView::onDraw(ICanvas& canvas)
{
    ViewGroup::onDraw(canvas);
}

bool AutoGridView::onKeyEvent(const IO::KeyEvent& event)
{
    if (event.action == IO::KeyAction::DOWN)
    {
        // Fullscreen toggle with F key
        if ((event.keyCode == IO::KeyCode::F || event.keyCode == IO::KeyCode::ENTER || event.keyCode == IO::KeyCode::DPAD_CENTER) 
            && m_focusedCellIndex >= 0)
        {
            toggleFullscreen(m_focusedCellIndex);
            return true;
        }
        
        // Exit fullscreen with ESC
        if (event.keyCode == IO::KeyCode::ESCAPE && m_fullscreenCellIndex >= 0)
        {
            exitFullscreen();
            return true;
        }
        
        // Navigation (disabled in fullscreen)
        if (m_fullscreenCellIndex < 0)
        {
            if (event.keyCode == IO::KeyCode::DPAD_UP)
            {
                return moveFocusUp();
            }
            else if (event.keyCode == IO::KeyCode::DPAD_DOWN)
            {
                return moveFocusDown();
            }
            else if (event.keyCode == IO::KeyCode::DPAD_LEFT)
            {
                return moveFocusLeft();
            }
            else if (event.keyCode == IO::KeyCode::DPAD_RIGHT)
            {
                return moveFocusRight();
            }
        }
    }
    
    return ViewGroup::onKeyEvent(event);
}

bool AutoGridView::onMotionEvent(const IO::MotionEvent& event)
{
    // Let children handle first
    if (ViewGroup::onMotionEvent(event) == true)
    {
        return true;
    }
    
    const Vec2 pos(event.x, event.y);
    
    if (event.action == IO::MotionAction::DOWN)
    {
        m_lastTouchPos = pos;
        m_dragStartY = pos.y;
        m_isDragging = false;
        
        // Check if clicking on a cell
        for (size_t i = 0; i < m_cells.size(); ++i)
        {
            GridCellView* cell = m_cells[i];
            if (cell != nullptr && cell->getBounds().contains(pos.x, pos.y) == true)
            {
                setFocusedCellIndex(static_cast<int32_t>(i));
                if (m_onItemClick != nullptr)
                {
                    m_onItemClick(static_cast<int32_t>(i));
                }
                return true;
            }
        }
    }
    else if (event.action == IO::MotionAction::MOVE)
    {
        if (m_fullscreenCellIndex < 0)  // Only scroll in normal mode
        {
            const float deltaY = pos.y - m_lastTouchPos.y;
            
            if (m_isDragging == false && std::abs(pos.y - m_dragStartY) > m_dragThreshold)
            {
                m_isDragging = true;
            }
            
            if (m_isDragging == true)
            {
                scrollBy(-deltaY);
            }
            
            m_lastTouchPos = pos;
        }
    }
    else if (event.action == IO::MotionAction::UP)
    {
        m_isDragging = false;
    }
    
    return false;
}

void AutoGridView::rebuildCells()
{
    // Clean up existing cells
    for (GridCellView* cell : m_cells)
    {
        if (m_adapter != nullptr)
        {
            m_adapter->onCellRecycled(cell);
        }
        removeView(cell);
        delete cell;
    }
    m_cells.clear();
    
    // Create new cells
    if (m_adapter != nullptr)
    {
        const int32_t count = m_adapter->getCount();
        for (int32_t i = 0; i < count; ++i)
        {
            GridCellView* cell = m_adapter->createCell();
            if (cell != nullptr)
            {
                m_adapter->bindCell(cell, i);
                m_cells.push_back(cell);
                addView(cell);
            }
        }
    }
    
    updateCellFocus();
}

void AutoGridView::recalculateGrid()
{
}

GridCellView* AutoGridView::getCellAt(int32_t index) const
{
    if (index >= 0 && index < static_cast<int32_t>(m_cells.size()))
    {
        return m_cells[index];
    }
    return nullptr;
}

// ============================================================================
// CustomGridView Implementation
// ============================================================================

// Static member
const UI::GridCellRect CustomGridView::s_emptyRect = {};

CustomGridView::CustomGridView()
    : FrameLayout()
    , m_gap(8.0f)
    , m_aspectRatio(16.0f / 9.0f)
    , m_gridWidth(0.0f)
    , m_gridHeight(0.0f)
    , m_fullscreenSlot(-1)
{
}

void CustomGridView::setSlot(int index, const GridCellSlot& slot)
{
    if (index >= 0 && index < static_cast<int>(m_slots.size()))
        m_slots[index] = slot;
}

bool CustomGridView::addCell(int slotIndex, GridCellView* cell)
{
    if (slotIndex < 0 || slotIndex >= static_cast<int>(m_slots.size())) return false;
    if (m_slots[slotIndex].enabled == false) return false;

    if (static_cast<int>(m_cells.size()) <= slotIndex)
        m_cells.resize(m_slots.size(), nullptr);

    m_cells[slotIndex] = cell;
    addView(cell);
    return true;
}

GridCellView* CustomGridView::getCellAt(int slotIndex) const
{
    if (slotIndex < 0 || slotIndex >= static_cast<int>(m_cells.size())) return nullptr;
    return m_cells[slotIndex];
}

const GridCellRect& CustomGridView::getCellRect(int slotIndex) const
{
    if (slotIndex < 0 || slotIndex >= static_cast<int>(m_cellRects.size())) return s_emptyRect;
    return m_cellRects[slotIndex];
}

void CustomGridView::rebuildLayout(int screenW, int screenH)
{
    computeGeometry(screenW, screenH);
    if (m_fullscreenSlot >= 0) applyFullscreenLayout();
    else                       applyNormalLayout();
}

void CustomGridView::onLayout(const RectF& bounds)
{
    FrameLayout::onLayout(bounds);
    if (m_fullscreenSlot >= 0) applyFullscreenLayout();
    else                       applyNormalLayout();
}

void CustomGridView::computeGeometry(int screenW, int screenH)
{
    m_cellRects.assign(m_slots.size(), GridCellRect{});
    m_gridWidth  = 0.0f;
    m_gridHeight = 0.0f;

    int numCols = 0, numRows = 0;
    for (const auto& s : m_slots)
    {
        if (s.enabled == false) continue;
        if (s.col + s.colSpan > numCols) numCols = s.col + s.colSpan;
        if (s.row + 1         > numRows) numRows = s.row + 1;
    }
    if (numCols == 0 || numRows == 0) return;

    // Gather row weights
    std::vector<int> rowWeight(numRows, 1);
    for (const auto& s : m_slots)
    {
        if (s.enabled == false) continue;
        rowWeight[s.row] = s.rowWeight > 0 ? s.rowWeight : 1;
    }

    int totalWeightUnits = 0;
    for (int w : rowWeight) totalWeightUnits += w;

    const float H         = static_cast<float>(screenH);
    const float W         = static_cast<float>(screenW);
    const float totalGapH = static_cast<float>(numRows + 1) * m_gap;
    const float totalGapW = static_cast<float>(numCols + 1) * m_gap;

    float unitH = (H - totalGapH) / static_cast<float>(totalWeightUnits);
    float unitW = unitH * m_aspectRatio;
    float gridW = numCols * unitW + totalGapW;

    if (gridW > W)
    {
        unitW = (W - totalGapW) / static_cast<float>(numCols);
        unitH = unitW / m_aspectRatio;
        gridW = W;
    }

    // Compute row Y offsets
    std::vector<float> rowY(numRows, 0.0f);
    rowY[0] = m_gap;
    for (int r = 1; r < numRows; r++)
        rowY[r] = rowY[r-1] + static_cast<float>(rowWeight[r-1]) * unitH + m_gap;

    m_gridWidth  = gridW;
    m_gridHeight = static_cast<float>(totalWeightUnits) * unitH + totalGapH;

    for (int i = 0; i < static_cast<int>(m_slots.size()); i++)
    {
        const auto& s = m_slots[i];
        if (s.enabled == false) { m_cellRects[i] = {}; continue; }
        int wt = s.rowWeight > 0 ? s.rowWeight : 1;
        m_cellRects[i] = {
            m_gap + static_cast<float>(s.col) * (unitW + m_gap),
            rowY[s.row],
            static_cast<float>(s.colSpan) * unitW + static_cast<float>(s.colSpan - 1) * m_gap,
            static_cast<float>(wt) * unitH
        };
    }
}

void CustomGridView::applyNormalLayout()
{
    int n = static_cast<int>(m_slots.size());
    for (int i = 0; i < n; i++)
    {
        GridCellView* cell = (i < static_cast<int>(m_cells.size())) ? m_cells[i] : nullptr;
        if (cell == nullptr) continue;

        if (m_slots[i].enabled == false)
        {
            cell->setVisibility(Visibility::GONE);
            continue;
        }

        const GridCellRect& r = m_cellRects[i];
        cell->getLayoutParams().width   = static_cast<int>(r.w);
        cell->getLayoutParams().height  = static_cast<int>(r.h);
        cell->getLayoutParams().gravity = Gravity::TOP_LEFT;
        cell->getLayoutParams().setMargin(r.x, r.y, 0.0f, 0.0f);
        cell->setVisibility(Visibility::VISIBLE);
    }

    // Grid container itself sized to computed geometry
    getLayoutParams().width   = static_cast<int>(m_gridWidth);
    getLayoutParams().height  = static_cast<int>(m_gridHeight);
    getLayoutParams().gravity = Gravity::TOP | Gravity::CENTER_HORIZONTAL;
}

void CustomGridView::applyFullscreenLayout()
{
    int n = static_cast<int>(m_slots.size());
    for (int i = 0; i < n; i++)
    {
        GridCellView* cell = (i < static_cast<int>(m_cells.size())) ? m_cells[i] : nullptr;
        if (cell == nullptr) continue;

        if (i == m_fullscreenSlot)
        {
            cell->getLayoutParams().width   = MATCH_PARENT;
            cell->getLayoutParams().height  = MATCH_PARENT;
            cell->getLayoutParams().gravity = Gravity::TOP_LEFT;
            cell->getLayoutParams().setMargin(0.0f, 0.0f, 0.0f, 0.0f);
            cell->setVisibility(Visibility::VISIBLE);
        }
        else
            cell->setVisibility(Visibility::GONE);
    }

    getLayoutParams().width   = MATCH_PARENT;
    getLayoutParams().height  = MATCH_PARENT;
    getLayoutParams().gravity = Gravity::TOP_LEFT;
}

void CustomGridView::enterFullscreen(int slotIndex)
{
    if (slotIndex < 0 || slotIndex >= static_cast<int>(m_slots.size())) return;
    if (m_slots[slotIndex].enabled == false) return;
    m_fullscreenSlot = slotIndex;
    applyFullscreenLayout();
}

void CustomGridView::exitFullscreen()
{
    if (m_fullscreenSlot < 0) return;
    m_fullscreenSlot = -1;
    applyNormalLayout();
}

void CustomGridView::toggleFullscreen(int slotIndex)
{
    if (m_fullscreenSlot == slotIndex) exitFullscreen();
    else                               enterFullscreen(slotIndex);
}

// ============================================================================
// PiPView
// ============================================================================

PiPView::PiPView()
    : FrameLayout()
    , m_primaryIndex(0)
    , m_dock(PiPDock::BOTTOM_LEFT)
    , m_orientation(Orientation::HORIZONTAL)
    , m_thumbW(480.0f)
    , m_thumbH(270.0f)
    , m_spacing(8.0f)
    , m_margin(8.0f)
    , m_viewW(0.0f)
    , m_viewH(0.0f)
{}

void PiPView::addCell(GridCellView* cell)
{
    if (cell == nullptr) return;
    m_cells.push_back(cell);
    addView(cell);
}

void PiPView::clearCells()
{
    for (GridCellView* cell : m_cells)
    {
        if (cell != nullptr) removeViewNoDelete(cell);
    }
    m_cells.clear();
    m_primaryIndex = 0;
}

GridCellView* PiPView::getCellAt(int index) const
{
    return (index >= 0 && index < static_cast<int>(m_cells.size())) ? m_cells[index] : nullptr;
}

void PiPView::setPrimaryIndex(int index)
{
    if (index >= 0 && index < static_cast<int>(m_cells.size()))
        m_primaryIndex = index;
}

void PiPView::rebuildLayout(int screenW, int screenH)
{
    m_viewW = static_cast<float>(screenW);
    m_viewH = static_cast<float>(screenH);
    applyLayout();
}

void PiPView::onLayout(const RectF& bounds)
{
    FrameLayout::onLayout(bounds);
    m_viewW = bounds.width();
    m_viewH = bounds.height();
    applyLayout();
}

void PiPView::applyLayout()
{
    const int n = static_cast<int>(m_cells.size());
    if (n == 0) return;

    getLayoutParams().width   = MATCH_PARENT;
    getLayoutParams().height  = MATCH_PARENT;
    getLayoutParams().gravity = Gravity::TOP_LEFT;

    // Reorder children: primary drawn first (background), thumbnails on top
    for (int i = 0; i < n; i++)
    {
        if (m_cells[i] == nullptr) continue;
        removeViewNoDelete(m_cells[i]);
    }
    addView(m_cells[m_primaryIndex]);
    for (int i = 0; i < n; i++)
    {
        if (i == m_primaryIndex || m_cells[i] == nullptr) continue;
        addView(m_cells[i]);
    }

    // Primary cell — fullscreen
    GridCellView* primary = m_cells[m_primaryIndex];
    primary->getLayoutParams().width   = MATCH_PARENT;
    primary->getLayoutParams().height  = MATCH_PARENT;
    primary->getLayoutParams().gravity = Gravity::TOP_LEFT;
    primary->getLayoutParams().setMargin(0.0f, 0.0f, 0.0f, 0.0f);
    primary->setNormalColor(Color::Transparent);
    primary->setVisibility(Visibility::VISIBLE);

    // Thumbnail strip
    const bool isHorizontal = (m_orientation == Orientation::HORIZONTAL);
    int        numThumbs    = 0;
    for (int i = 0; i < n; i++) { if (i != m_primaryIndex) numThumbs++; }

    if (numThumbs == 0) return;

    const float stripW = isHorizontal ? numThumbs * m_thumbW + (numThumbs - 1) * m_spacing : m_thumbW;
    const float stripH = isHorizontal ? m_thumbH : numThumbs * m_thumbH + (numThumbs - 1) * m_spacing;

    float originX = 0.0f, originY = 0.0f;
    switch (m_dock)
    {
        case PiPDock::TOP_LEFT:     originX = m_margin;                    originY = m_margin;                    break;
        case PiPDock::TOP_RIGHT:    originX = m_viewW - stripW - m_margin; originY = m_margin;                    break;
        case PiPDock::BOTTOM_LEFT:  originX = m_margin;                    originY = m_viewH - stripH - m_margin; break;
        case PiPDock::BOTTOM_RIGHT: originX = m_viewW - stripW - m_margin; originY = m_viewH - stripH - m_margin; break;
    }

    int t = 0;
    for (int i = 0; i < n; i++)
    {
        if (i == m_primaryIndex) continue;
        GridCellView* cell = m_cells[i];
        const float cx = isHorizontal ? originX + t * (m_thumbW + m_spacing) : originX;
        const float cy = isHorizontal ? originY : originY + t * (m_thumbH + m_spacing);
        cell->getLayoutParams().width   = static_cast<int>(m_thumbW);
        cell->getLayoutParams().height  = static_cast<int>(m_thumbH);
        cell->getLayoutParams().gravity = Gravity::TOP_LEFT;
        cell->getLayoutParams().setMargin(cx, cy, 0.0f, 0.0f);
        cell->setNormalColor(Color::Black);  // opaque — blocks primary bleed-through under semi-transparent elements
        cell->setVisibility(Visibility::VISIBLE);
        t++;
    }
}

// ============================================================================
// NumpadView Implementation
// ============================================================================

NumpadView::NumpadView()
    : m_dotButton(nullptr)
    , m_deleteButton(nullptr)
    , m_onNumberClick(nullptr)
    , m_onDotClick(nullptr)
    , m_onDeleteClick(nullptr)
{
    for (int i = 0; i < 10; ++i) m_numberButtons[i] = nullptr;
    for (int i = 0; i < 3; ++i) m_rows[i] = nullptr;
    
    setupUI();
}

NumpadView::~NumpadView()
{
}

void NumpadView::setupUI()
{
    setOrientation(Orientation::VERTICAL);
    setGravity(Gravity::CENTER);
    setSpacing(8.0f);
    setBackgroundColor(Color::Transparent);
    getLayoutParams().width = WRAP_CONTENT;
    getLayoutParams().height = WRAP_CONTENT;
    
    for (int row = 0; row < 3; ++row)
    {
        m_rows[row] = new LinearLayout();
        m_rows[row]->setOrientation(Orientation::HORIZONTAL);
        m_rows[row]->setGravity(Gravity::CENTER);
        m_rows[row]->setSpacing(8.0f);
        m_rows[row]->setBackgroundColor(Color::Transparent);
        m_rows[row]->getLayoutParams().width = WRAP_CONTENT;
        m_rows[row]->getLayoutParams().height = WRAP_CONTENT;
        addView(m_rows[row]);
    }
    
    createButton("1", 0, 0, [this]() { if (m_onNumberClick != nullptr) m_onNumberClick(1); });
    createButton("2", 0, 1, [this]() { if (m_onNumberClick != nullptr) m_onNumberClick(2); });
    createButton("3", 0, 2, [this]() { if (m_onNumberClick != nullptr) m_onNumberClick(3); });
    createButton(".", 0, 3, [this]() { if (m_onDotClick != nullptr) m_onDotClick(); }, true);
    
    createButton("4", 1, 0, [this]() { if (m_onNumberClick != nullptr) m_onNumberClick(4); });
    createButton("5", 1, 1, [this]() { if (m_onNumberClick != nullptr) m_onNumberClick(5); });
    createButton("6", 1, 2, [this]() { if (m_onNumberClick != nullptr) m_onNumberClick(6); });
    createButton("0", 1, 3, [this]() { if (m_onNumberClick != nullptr) m_onNumberClick(0); });
    
    createButton("7", 2, 0, [this]() { if (m_onNumberClick != nullptr) m_onNumberClick(7); });
    createButton("8", 2, 1, [this]() { if (m_onNumberClick != nullptr) m_onNumberClick(8); });
    createButton("9", 2, 2, [this]() { if (m_onNumberClick != nullptr) m_onNumberClick(9); });
    createButton("DEL", 2, 3, [this]() { if (m_onDeleteClick != nullptr) m_onDeleteClick(); }, true);
    
    m_numberButtons[1] = static_cast<Button*>(m_rows[0]->getChildAt(0));
    m_numberButtons[2] = static_cast<Button*>(m_rows[0]->getChildAt(1));
    m_numberButtons[3] = static_cast<Button*>(m_rows[0]->getChildAt(2));
    m_dotButton = static_cast<Button*>(m_rows[0]->getChildAt(3));
    
    m_numberButtons[4] = static_cast<Button*>(m_rows[1]->getChildAt(0));
    m_numberButtons[5] = static_cast<Button*>(m_rows[1]->getChildAt(1));
    m_numberButtons[6] = static_cast<Button*>(m_rows[1]->getChildAt(2));
    m_numberButtons[0] = static_cast<Button*>(m_rows[1]->getChildAt(3));

    m_numberButtons[7] = static_cast<Button*>(m_rows[2]->getChildAt(0));
    m_numberButtons[8] = static_cast<Button*>(m_rows[2]->getChildAt(1));
    m_numberButtons[9] = static_cast<Button*>(m_rows[2]->getChildAt(2));
    m_deleteButton = static_cast<Button*>(m_rows[2]->getChildAt(3));
}

void NumpadView::createButton(const std::string& text, int row, int col, 
                               std::function<void()> callback, bool isSpecial)
{
    const int buttonSize = 80;
    const float buttonTextSize = isSpecial ? 24.0f : 28.0f;
    
    Button* button = new Button();
    button->setText(text);
    button->setTextColor(Color::TextPrimary);
    button->setTextSize(buttonTextSize);
    button->setNormalColor(isSpecial ? Color::AccentPrimary.withAlpha(0.3f) : Color::CardBackground);
    button->setHoverColor(isSpecial ? Color::AccentPrimary.withAlpha(0.5f) : Color::CardBackgroundHover);
    button->setFocusedColor(isSpecial ? Color::AccentPrimary.withAlpha(0.3f) : Color::CardBackground);
    button->setPressedColor(isSpecial ? Color::AccentSecondary : Color::CardBackgroundHover);
    button->setCornerRadius(12.0f);
    button->setBorderWidth(5.0f);
    button->setFocusable(true);
    button->getLayoutParams().width = buttonSize;
    button->getLayoutParams().height = buttonSize;
    button->setOnClickListener([callback](View* v) { if (callback != nullptr) callback(); });
    
    m_rows[row]->addView(button);
}

void NumpadView::setDotEnabled(bool enabled)
{
    if (m_dotButton != nullptr)
    {
        m_dotButton->setEnabled(enabled);
        m_dotButton->setFocusable(enabled);
    }
}

void NumpadView::setDeleteEnabled(bool enabled)
{
    if (m_deleteButton != nullptr)
    {
        m_deleteButton->setEnabled(enabled);
        m_deleteButton->setFocusable(enabled);
    }
}

bool NumpadView::handleKeyEvent(const IO::KeyEvent& event)
{
    if (event.action != IO::KeyAction::DOWN) return false;
    
    if (event.keyCode >= IO::KeyCode::NUM_0 && event.keyCode <= IO::KeyCode::NUM_9)
    {
        int num = static_cast<int>(event.keyCode) - static_cast<int>(IO::KeyCode::NUM_0);
        if (m_onNumberClick != nullptr) m_onNumberClick(num);
        return true;
    }
    
    if (event.keyCode == IO::KeyCode::PERIOD || event.keyCode == IO::KeyCode::COMMA)
    {
        if (m_onDotClick != nullptr) m_onDotClick();
        return true;
    }
    
    if (event.keyCode == IO::KeyCode::BACKSPACE || event.keyCode == IO::KeyCode::DELETE)
    {
        if (m_onDeleteClick != nullptr) m_onDeleteClick();
        return true;
    }
    
    return false;
}

// ============================================================================
// NumpadPopup Implementation
// ============================================================================

NumpadPopup::NumpadPopup()
    : m_isShowing(false)
    , m_currentInput("")
    , m_title("Enter Number")
    , m_allowDecimal(true)
    , m_allowNegative(false)
    , m_maxLength(10)
    , m_maxValue(999999.0f)
    , m_minValue(0.0f)
    , m_overlay(nullptr)
    , m_dialogContainer(nullptr)
    , m_titleText(nullptr)
    , m_displayContainer(nullptr)
    , m_displayText(nullptr)
    , m_numpadView(nullptr)
    , m_buttonRow(nullptr)
    , m_cancelButton(nullptr)
    , m_confirmButton(nullptr)
    , m_focusManager(nullptr)
    , m_popupContext(nullptr)
    , m_onConfirm(nullptr)
    , m_onCancel(nullptr)
{
    setupUI();
}

NumpadPopup::~NumpadPopup()
{
    if (m_focusManager != nullptr && m_popupContext != nullptr)
    {
        unregisterPopupFocusables();
        m_focusManager->removeContext(m_popupContext);
        m_popupContext = nullptr;
    }
}

void NumpadPopup::setupUI()
{
    getLayoutParams().width = MATCH_PARENT;
    getLayoutParams().height = MATCH_PARENT;
    setVisibility(Visibility::GONE);
    setBackgroundColor(Color::Transparent);
    
    // Overlay to block background interaction
    m_overlay = new FrameLayout();
    m_overlay->setBackgroundColor(Color::OverlayDark);
    m_overlay->getLayoutParams().width = MATCH_PARENT;
    m_overlay->getLayoutParams().height = MATCH_PARENT;
    m_overlay->setFocusable(false); // Overlay itself is not focusable
    
    m_dialogContainer = new LinearLayout();
    m_dialogContainer->setOrientation(Orientation::VERTICAL);
    m_dialogContainer->setGravity(Gravity::CENTER_HORIZONTAL);
    m_dialogContainer->setSpacing(16.0f);
    m_dialogContainer->setBackgroundColor(Color::DarkSurface);
    m_dialogContainer->setCornerRadius(20.0f);
    m_dialogContainer->getLayoutParams().width = WRAP_CONTENT;
    m_dialogContainer->getLayoutParams().height = WRAP_CONTENT;
    m_dialogContainer->getLayoutParams().gravity = Gravity::CENTER;
    m_dialogContainer->getLayoutParams().setPadding(24.0f);
    
    m_titleText = new TextView();
    m_titleText->setText("Enter Number");
    m_titleText->setTextColor(Color::TextPrimary);
    m_titleText->setTextSize(TextSize::Display);
    m_titleText->setTextGravity(Gravity::CENTER);
    m_titleText->getLayoutParams().width = WRAP_CONTENT;
    m_titleText->getLayoutParams().height = WRAP_CONTENT;
    m_titleText->getLayoutParams().setMargin(0.0f, 0.0f, 0.0f, 8.0f);
    m_dialogContainer->addView(m_titleText);
    
    m_displayContainer = new FrameLayout();
    m_displayContainer->setBackgroundColor(Color::CardBackground);
    m_displayContainer->setCornerRadius(12.0f);
    m_displayContainer->getLayoutParams().width = 360;
    m_displayContainer->getLayoutParams().height = 70;
    m_displayContainer->getLayoutParams().setPadding(16.0f);
    m_dialogContainer->addView(m_displayContainer);
    
    m_displayText = new TextView();
    m_displayText->setText("0");
    m_displayText->setTextColor(Color::TextPrimary);
    m_displayText->setTextSize(TextSize::Display);
    m_displayText->setTextGravity(Gravity::CENTER);
    m_displayText->getLayoutParams().width = MATCH_PARENT;
    m_displayText->getLayoutParams().height = MATCH_PARENT;
    m_displayContainer->addView(m_displayText);
    
    m_numpadView = new NumpadView();
    m_numpadView->setOnNumberClick([this](int num) { handleNumberInput(num); });
    m_numpadView->setOnDotClick([this]() { handleDotInput(); });
    m_numpadView->setOnDeleteClick([this]() { handleDeleteInput(); });
    m_dialogContainer->addView(m_numpadView);
    
    m_buttonRow = new LinearLayout();
    m_buttonRow->setOrientation(Orientation::HORIZONTAL);
    m_buttonRow->setGravity(Gravity::CENTER);
    m_buttonRow->setSpacing(16.0f);
    m_buttonRow->setBackgroundColor(Color::Transparent);
    m_buttonRow->getLayoutParams().width = WRAP_CONTENT;
    m_buttonRow->getLayoutParams().height = WRAP_CONTENT;
    m_buttonRow->getLayoutParams().setMargin(0.0f, 8.0f, 0.0f, 0.0f);
    m_dialogContainer->addView(m_buttonRow);
    
    m_cancelButton = new Button();
    m_cancelButton->setText("Cancel");
    m_cancelButton->setTextColor(Color::TextPrimary);
    m_cancelButton->setTextSize(TextSize::Title);
    m_cancelButton->setNormalColor(Color::CardBackground);
    m_cancelButton->setHoverColor(Color::CardBackgroundHover);
    m_cancelButton->setFocusedColor(Color::CardBackground);
    m_cancelButton->setPressedColor(Color::CardBackgroundHover);
    m_cancelButton->setCornerRadius(12.0f);
    m_cancelButton->setBorderWidth(5.0f);
    m_cancelButton->setFocusable(true);
    m_cancelButton->getLayoutParams().width = 150;
    m_cancelButton->getLayoutParams().height = 60;
    m_cancelButton->setOnClickListener([this](View* v) { handleCancel(); });
    m_buttonRow->addView(m_cancelButton);
    
    m_confirmButton = new Button();
    m_confirmButton->setText("Confirm");
    m_confirmButton->setTextColor(Color::TextPrimary);
    m_confirmButton->setTextSize(TextSize::Title);
    m_confirmButton->setNormalColor(Color::CardBackground);
    m_confirmButton->setHoverColor(Color::CardBackgroundHover);
    m_confirmButton->setFocusedColor(Color::CardBackground);
    m_confirmButton->setPressedColor(Color::CardBackgroundHover);
    m_confirmButton->setCornerRadius(12.0f);
    m_confirmButton->setBorderWidth(5.0f);
    m_confirmButton->setFocusable(true);
    m_confirmButton->getLayoutParams().width = 150;
    m_confirmButton->getLayoutParams().height = 60;
    m_confirmButton->setOnClickListener([this](View* v) { handleConfirm(); });
    m_buttonRow->addView(m_confirmButton);
    
    addView(m_overlay);
    addView(m_dialogContainer);
}

void NumpadPopup::setupFocusNavigation()
{
    if (m_focusManager == nullptr || m_numpadView == nullptr) return;

    if (m_popupContext == nullptr)
    {
        m_popupContext = m_focusManager->createContext(FocusContextType::POPUP);
        m_popupContext->setNavigationStrategy(NavigationStrategy::CUSTOM);
        m_popupContext->setWrapAround(false);
    }

    LinearLayout* row0 = static_cast<LinearLayout*>(m_numpadView->getChildAt(0));
    LinearLayout* row1 = static_cast<LinearLayout*>(m_numpadView->getChildAt(1));
    LinearLayout* row2 = static_cast<LinearLayout*>(m_numpadView->getChildAt(2));

    if (row0 == nullptr || row1 == nullptr || row2 == nullptr) return;

    // Retrieve all buttons by their fixed layout positions.
    Button* btn1   = static_cast<Button*>(row0->getChildAt(0));
    Button* btn2   = static_cast<Button*>(row0->getChildAt(1));
    Button* btn3   = static_cast<Button*>(row0->getChildAt(2));
    Button* btnDot = static_cast<Button*>(row0->getChildAt(3));
    Button* btn4   = static_cast<Button*>(row1->getChildAt(0));
    Button* btn5   = static_cast<Button*>(row1->getChildAt(1));
    Button* btn6   = static_cast<Button*>(row1->getChildAt(2));
    Button* btn0   = static_cast<Button*>(row1->getChildAt(3));
    Button* btn7   = static_cast<Button*>(row2->getChildAt(0));
    Button* btn8   = static_cast<Button*>(row2->getChildAt(1));
    Button* btn9   = static_cast<Button*>(row2->getChildAt(2));
    Button* btnDel = static_cast<Button*>(row2->getChildAt(3));

    // Register all focusable buttons in the popup context.
    auto reg = [&](Button* btn)
    {
        if (btn != nullptr && btn->isFocusable())
        {
            btn->setFocusContext(m_popupContext);
            btn->setFocusManager(m_focusManager);
        }
    };
    reg(btn1); reg(btn2); reg(btn3); reg(btnDot);
    reg(btn4); reg(btn5); reg(btn6); reg(btn0);
    reg(btn7); reg(btn8); reg(btn9); reg(btnDel);
    reg(m_cancelButton); reg(m_confirmButton);

    // Col 3: dot / +/- / absent depending on mode
    Button* col3row0 = (btnDot != nullptr && btnDot->isFocusable()) ? btnDot : nullptr;

    auto link = [&](Button* src, Button* dst, IO::KeyCode dir)
    {
        if (src != nullptr && src->isFocusable() && dst != nullptr && dst->isFocusable())
            m_focusManager->setFocusLink(src, dst, dir);
    };

    // ── Horizontal ───────────────────────────────────────────────────────────
    // Row 0
    link(btn1, btn2,     IO::KeyCode::DPAD_RIGHT);
    link(btn2, btn3,     IO::KeyCode::DPAD_RIGHT);
    link(btn3, col3row0, IO::KeyCode::DPAD_RIGHT);  // no-op when dot disabled
    link(col3row0, btn3, IO::KeyCode::DPAD_LEFT);
    link(btn3, btn2,     IO::KeyCode::DPAD_LEFT);
    link(btn2, btn1,     IO::KeyCode::DPAD_LEFT);
    // Row 1
    link(btn4, btn5,  IO::KeyCode::DPAD_RIGHT);
    link(btn5, btn6,  IO::KeyCode::DPAD_RIGHT);
    link(btn6, btn0,  IO::KeyCode::DPAD_RIGHT);
    link(btn0, btn6,  IO::KeyCode::DPAD_LEFT);
    link(btn6, btn5,  IO::KeyCode::DPAD_LEFT);
    link(btn5, btn4,  IO::KeyCode::DPAD_LEFT);
    // Row 2
    link(btn7,   btn8,   IO::KeyCode::DPAD_RIGHT);
    link(btn8,   btn9,   IO::KeyCode::DPAD_RIGHT);
    link(btn9,   btnDel, IO::KeyCode::DPAD_RIGHT);
    link(btnDel, btn9,   IO::KeyCode::DPAD_LEFT);
    link(btn9,   btn8,   IO::KeyCode::DPAD_LEFT);
    link(btn8,   btn7,   IO::KeyCode::DPAD_LEFT);

    // ── Vertical ─────────────────────────────────────────────────────────────
    // Col 0: 1 ↔ 4 ↔ 7
    link(btn1, btn4, IO::KeyCode::DPAD_DOWN);
    link(btn4, btn7, IO::KeyCode::DPAD_DOWN);
    link(btn7, btn4, IO::KeyCode::DPAD_UP);
    link(btn4, btn1, IO::KeyCode::DPAD_UP);
    // Col 1: 2 ↔ 5 ↔ 8
    link(btn2, btn5, IO::KeyCode::DPAD_DOWN);
    link(btn5, btn8, IO::KeyCode::DPAD_DOWN);
    link(btn8, btn5, IO::KeyCode::DPAD_UP);
    link(btn5, btn2, IO::KeyCode::DPAD_UP);
    // Col 2: 3 ↔ 6 ↔ 9
    link(btn3, btn6, IO::KeyCode::DPAD_DOWN);
    link(btn6, btn9, IO::KeyCode::DPAD_DOWN);
    link(btn9, btn6, IO::KeyCode::DPAD_UP);
    link(btn6, btn3, IO::KeyCode::DPAD_UP);
    // Col 3: dot ↔ 0 ↔ DEL  (dot row only wired when enabled)
    link(col3row0, btn0,   IO::KeyCode::DPAD_DOWN);
    link(btn0,     btnDel, IO::KeyCode::DPAD_DOWN);
    link(btnDel,   btn0,   IO::KeyCode::DPAD_UP);
    link(btn0,     col3row0, IO::KeyCode::DPAD_UP);  // no-op when dot disabled → stays at btn0

    // ── Row 2 → bottom bar ────────────────────────────────────────────────────
    link(btn7,   m_cancelButton,  IO::KeyCode::DPAD_DOWN);
    link(btn8,   m_cancelButton,  IO::KeyCode::DPAD_DOWN);
    link(btn9,   m_confirmButton, IO::KeyCode::DPAD_DOWN);
    link(btnDel, m_confirmButton, IO::KeyCode::DPAD_DOWN);
    link(m_cancelButton,  btn7, IO::KeyCode::DPAD_UP);
    link(m_confirmButton, btn9, IO::KeyCode::DPAD_UP);

    // ── Bottom bar horizontal ─────────────────────────────────────────────────
    link(m_cancelButton,  m_confirmButton, IO::KeyCode::DPAD_RIGHT);
    link(m_confirmButton, m_cancelButton,  IO::KeyCode::DPAD_LEFT);
}

void NumpadPopup::registerPopupFocusables()
{
}

void NumpadPopup::unregisterPopupFocusables()
{
}

void NumpadPopup::show(const std::string& title, const std::string& initialValue, bool allowDecimal)
{
    m_isShowing = true;
    m_title = title;
    m_currentInput = initialValue.empty() ? "" : initialValue;
    m_allowDecimal = allowDecimal;
    
    if (m_titleText != nullptr)
    {
        m_titleText->setText(title);
    }
    
    if (m_numpadView != nullptr)
    {
        m_numpadView->setDotEnabled(allowDecimal);

        // Repurpose dot slot as +/- when decimal not needed
        LinearLayout* row0 = static_cast<LinearLayout*>(m_numpadView->getChildAt(0));
        if (row0 != nullptr)
        {
            Button* dotBtn = static_cast<Button*>(row0->getChildAt(3));
            if (dotBtn != nullptr)
            {
                if (allowDecimal == false && m_allowNegative == true)
                {
                    dotBtn->setText("+/-");
                    dotBtn->setEnabled(true);
                    dotBtn->setFocusable(true);
                    dotBtn->setOnClickListener([this](View*)
                    {
                        if (m_currentInput.empty() == true || m_currentInput == "0") return;
                        if (m_currentInput[0] == '-')
                            m_currentInput.erase(m_currentInput.begin());
                        else
                            m_currentInput.insert(m_currentInput.begin(), '-');
                        updateDisplay();
                    });
                }
                else if (allowDecimal == false)
                {
                    // Not decimal, not negative — hide the button entirely
                    dotBtn->setText("");
                    dotBtn->setEnabled(false);
                    dotBtn->setFocusable(false);
                    dotBtn->setOnClickListener(nullptr);
                }
                else
                {
                    // Restore normal dot behaviour (in case popup is reused)
                    dotBtn->setText(".");
                    dotBtn->setOnClickListener([this](View*) { handleDotInput(); });
                }
            }
        }
    }
    
    updateDisplay();
    setVisibility(Visibility::VISIBLE);
    
    if (m_focusManager != nullptr)
    {
        setupFocusNavigation();
        registerPopupFocusables();
        
        LinearLayout* row0 = static_cast<LinearLayout*>(m_numpadView->getChildAt(0));
        if (row0 != nullptr)
        {
            Button* btn1 = static_cast<Button*>(row0->getChildAt(0));
            if (btn1 != nullptr)
            {
                m_focusManager->setFocus(btn1);
            }
        }
    }
}

void NumpadPopup::hide()
{
    m_isShowing = false;
    setVisibility(Visibility::GONE);
    m_currentInput = "";
    
    if (m_focusManager != nullptr)
    {
        m_focusManager->clearFocus();
        unregisterPopupFocusables();
        
        if (m_popupContext != nullptr)
        {
            m_focusManager->removeContext(m_popupContext);
            m_popupContext = nullptr;
        }
    }
}

void NumpadPopup::setCancelText(const std::string& text)
{
    if (m_cancelButton != nullptr) m_cancelButton->setText(text);
}

void NumpadPopup::setConfirmText(const std::string& text)
{
    if (m_confirmButton != nullptr) m_confirmButton->setText(text);
}

void NumpadPopup::updateDisplay()
{
    if (m_displayText == nullptr) return;
    
    if (m_currentInput.empty() == true)
    {
        m_displayText->setText("0");
    }
    else
    {
        m_displayText->setText(m_currentInput);
    }
}

void NumpadPopup::handleNumberInput(int number)
{
    if (m_currentInput.length() >= static_cast<size_t>(m_maxLength)) return;
    
    // Don't allow leading zeros unless it's a decimal
    if (m_currentInput.empty() == true && number == 0)
    {
        m_currentInput = "0";
    }
    else if (m_currentInput == "0")
    {
        m_currentInput = std::to_string(number);
    }
    else
    {
        m_currentInput += std::to_string(number);
    }
    
    updateDisplay();
}

void NumpadPopup::handleDotInput()
{
    if (m_allowDecimal == false) return;
    
    if (m_currentInput.find('.') != std::string::npos) return;
    
    if (m_currentInput.empty() == true)
    {
        m_currentInput = "0.";
    }
    else
    {
        m_currentInput += ".";
    }
    
    updateDisplay();
}

void NumpadPopup::handleDeleteInput()
{
    if (m_currentInput.empty() == true) return;
    
    m_currentInput.pop_back();
    updateDisplay();
}

void NumpadPopup::handleConfirm()
{
    if (isValidInput() == false)
    {
        return;
    }
    
    std::string result = m_currentInput.empty() ? "0" : m_currentInput;
    
    if (result.empty() == false && result.back() == '.')
    {
        result.pop_back();
    }
    
    if (m_onConfirm != nullptr)
    {
        m_onConfirm(result);
    }
    
    hide();
}

void NumpadPopup::handleCancel()
{
    if (m_onCancel != nullptr)
    {
        m_onCancel();
    }
    
    hide();
}

bool NumpadPopup::isValidInput() const
{
    if (m_currentInput.empty() == true) return true; // Empty is valid (will use 0)
    
    try
    {
        float value = getCurrentValue();
        
        if (value < m_minValue || value > m_maxValue) return false;
        
        return true;
    }
    catch (...)
    {
        return false;
    }
}

float NumpadPopup::getCurrentValue() const
{
    if (m_currentInput.empty() == true) return 0.0f;
    
    try
    {
        return std::stof(m_currentInput);
    }
    catch (...)
    {
        return 0.0f;
    }
}

bool NumpadPopup::handleKeyEvent(const IO::KeyEvent& event)
{
    if (m_isShowing == false) return false;
    
    if (event.action == IO::KeyAction::DOWN)
    {
        // ESC or BACK to cancel
        if (event.keyCode == IO::KeyCode::ESCAPE || event.keyCode == IO::KeyCode::BACK)
        {
            handleCancel();
            return true;
        }

        // Hardware minus key toggles sign when negative input is allowed
        if (m_allowNegative == true && event.keyCode == IO::KeyCode::MINUS)
        {
            if (m_currentInput.empty() == false && m_currentInput != "0")
            {
                if (m_currentInput[0] == '-')
                    m_currentInput.erase(m_currentInput.begin());
                else
                    m_currentInput.insert(m_currentInput.begin(), '-');
                updateDisplay();
            }
            return true;
        }
        
        // Handle numpad number keys directly (before ENTER handling)
        if (m_numpadView != nullptr && m_numpadView->handleKeyEvent(event) == true)
        {
            return true;
        }
        
        // Let focus manager handle navigation keys first
        if (m_focusManager != nullptr)
        {
            IO::KeyCode keyCode = event.keyCode;
            if (keyCode == IO::KeyCode::DPAD_UP || keyCode == IO::KeyCode::DPAD_DOWN ||
                keyCode == IO::KeyCode::DPAD_LEFT || keyCode == IO::KeyCode::DPAD_RIGHT)
            {
                if (m_focusManager->handleKeyEvent(event) == true)
                {
                    return true;
                }
            }
        }
        
        // ENTER or DPAD_CENTER activates focused button
        if (event.keyCode == IO::KeyCode::ENTER || event.keyCode == IO::KeyCode::DPAD_CENTER)
        {
            if (m_focusManager != nullptr)
            {
                View* focused = m_focusManager->getFocusedView();
                if (focused != nullptr && focused->hasClickListener() == true)
                {
                    // Trigger the focused view's click
                    focused->performClick();
                    return true;
                }
            }
            
            // If no button focused, default to confirm
            handleConfirm();
            return true;
        }
    }
    
    // Pass through to child views for other events
    return FrameLayout::onKeyEvent(event);
}

bool NumpadPopup::handleMotionEvent(const IO::MotionEvent& event)
{
    if (m_isShowing == false) return false;
    
    // Block all motion events from reaching background
    if (m_dialogContainer != nullptr)
    {
        RectF dialogBounds = m_dialogContainer->getBounds();
        
        // If event is outside dialog, consume it (block it)
        if (dialogBounds.contains(event.x, event.y) == false)
        {
            if (event.action == IO::MotionAction::DOWN)
            {
                // Optionally: handleCancel();
            }
            return true; // Block background interaction
        }
    }
    
    // Handle focus changes on DOWN before button processes event
    if (event.action == IO::MotionAction::DOWN && m_focusManager != nullptr)
    {
        View* viewAtPointer = m_focusManager->findFocusableAt(event.x, event.y);
        if (viewAtPointer != nullptr && m_focusManager->canInteractWithView(viewAtPointer) == true)
        {
            m_focusManager->setFocus(viewAtPointer);
        }
    }
    
    // Let child views (buttons) handle the event
    bool handled = FrameLayout::onMotionEvent(event);
    if (handled == true)
    {
        return true;
    }
    
    // Let focus manager handle hover events for visual feedback
    if (event.action == IO::MotionAction::HOVER_MOVE || 
        event.action == IO::MotionAction::HOVER_ENTER)
    {
        if (m_focusManager != nullptr)
        {
            m_focusManager->handleMotionEvent(event);
        }
    }
    
    return false;
}

void NumpadPopup::onDraw(ICanvas& canvas)
{
    if (m_isShowing == false || getVisibility() != Visibility::VISIBLE) return;
    
    FrameLayout::onDraw(canvas);
}

// ============================================================================
// PopupView
// ============================================================================

PopupView::PopupView()
    : m_isShowing(false)
    , m_buttonMode(PopupButtonMode::OK_CANCEL)
    , m_focusManager(nullptr)
    , m_popupContext(nullptr)
    , m_overlay(nullptr)
    , m_card(nullptr)
    , m_iconView(nullptr)
    , m_titleView(nullptr)
    , m_detailView(nullptr)
    , m_buttonRow(nullptr)
    , m_okButton(nullptr)
    , m_cancelButton(nullptr)
    , m_focusedButton(nullptr)
    , m_onConfirm(nullptr)
    , m_onCancel(nullptr)
{
    buildUI();
}

PopupView::~PopupView()
{
    if (m_focusManager != nullptr && m_popupContext != nullptr)
    {
        unregisterFocusables();
        m_focusManager->removeContext(m_popupContext);
        m_popupContext = nullptr;
    }
}

void PopupView::buildUI()
{
    getLayoutParams().width  = MATCH_PARENT;
    getLayoutParams().height = MATCH_PARENT;
    setVisibility(Visibility::GONE);
    setBackgroundColor(Color::Transparent);

    // Full-screen dimming overlay
    m_overlay = new FrameLayout();
    m_overlay->setBackgroundColor(Color4::fromRGBA(0, 0, 0, 180));
    m_overlay->getLayoutParams().width  = MATCH_PARENT;
    m_overlay->getLayoutParams().height = MATCH_PARENT;
    m_overlay->setFocusable(false);
    addView(m_overlay);

    // Centred dialog card
    m_card = new LinearLayout();
    m_card->setOrientation(Orientation::VERTICAL);
    m_card->setGravity(Gravity::CENTER_HORIZONTAL);
    m_card->setSpacing(0.0f);
    m_card->setBackgroundColor(Color::DarkSurface);
    m_card->setCornerRadius(28.0f);
    m_card->getLayoutParams().width   = 700;
    m_card->getLayoutParams().height  = WRAP_CONTENT;
    m_card->getLayoutParams().gravity = Gravity::CENTER;
    m_card->getLayoutParams().setPadding(60.0f);
    addView(m_card);

    // Icon (120 x 120)
    m_iconView = new ImageView();
    m_iconView->setScaleType(ScaleType::FIT_CENTER);
    m_iconView->getLayoutParams().width   = 120;
    m_iconView->getLayoutParams().height  = 120;
    m_iconView->getLayoutParams().gravity = Gravity::CENTER_HORIZONTAL;
    m_iconView->getLayoutParams().setMargin(0.0f, 0.0f, 0.0f, 16.0f);
    m_card->addView(m_iconView);

    // Title
    m_titleView = new TextView();
    m_titleView->setText("");
    m_titleView->setTextColor(Color::TextPrimary);
    m_titleView->setTextSize(48.0f);
    m_titleView->setTextGravity(Gravity::CENTER);
    m_titleView->getLayoutParams().width  = MATCH_PARENT;
    m_titleView->getLayoutParams().height = WRAP_CONTENT;
    m_titleView->getLayoutParams().setMargin(0.0f, 0.0f, 0.0f, 10.0f);
    m_card->addView(m_titleView);

    // Detail
    m_detailView = new TextView();
    m_detailView->setText("");
    m_detailView->setTextColor(Color::TextSecondary);
    m_detailView->setTextSize(36.0f);
    m_detailView->setTextGravity(Gravity::CENTER);
    m_detailView->getLayoutParams().width  = MATCH_PARENT;
    m_detailView->getLayoutParams().height = WRAP_CONTENT;
    m_detailView->getLayoutParams().setMargin(0.0f, 0.0f, 0.0f, 0.0f);
    m_detailView->setVisibility(Visibility::GONE);
    m_card->addView(m_detailView);

    // Button row wrapper
    FrameLayout* btnWrapper = new FrameLayout();
    btnWrapper->setBackgroundColor(Color::Transparent);
    btnWrapper->getLayoutParams().width  = MATCH_PARENT;
    btnWrapper->getLayoutParams().height = WRAP_CONTENT;
    btnWrapper->getLayoutParams().setMargin(0.0f, 60.0f, 0.0f, 0.0f);
    m_card->addView(btnWrapper);

    m_buttonRow = new LinearLayout();
    m_buttonRow->setOrientation(Orientation::HORIZONTAL);
    m_buttonRow->setSpacing(16.0f);
    m_buttonRow->setBackgroundColor(Color::Transparent);
    m_buttonRow->getLayoutParams().width   = WRAP_CONTENT;
    m_buttonRow->getLayoutParams().height  = WRAP_CONTENT;
    m_buttonRow->getLayoutParams().gravity = Gravity::CENTER;
    btnWrapper->addView(m_buttonRow);

    // Buttons use custom focus ring, not system focus.
    auto makeBtn = [](const std::string& label) -> Button*
    {
        Button* btn = new Button();
        btn->setText(label);
        btn->setTextSize(TextSize::Title);
        btn->setTextColor(Color::TextPrimary);
        btn->setNormalColor(Color::CardBackground);
        btn->setHoverColor(Color::CardBackgroundHover);
        btn->setFocusedColor(Color::CardBackground);
        btn->setPressedColor(Color::CardBackgroundHover);
        btn->setBorderWidth(5.0f);
        btn->setCornerRadius(12.0f);
        btn->setFocusable(false);
        btn->getLayoutParams().width  = 150;
        btn->getLayoutParams().height = 60;
        return btn;
    };

    m_cancelButton = makeBtn("Cancel");
    m_cancelButton->setOnClickListener([this](View*) { fireCancel(); });
    m_buttonRow->addView(m_cancelButton);

    m_okButton = makeBtn("OK");
    m_okButton->setOnClickListener([this](View*) { fireConfirm(); });
    m_buttonRow->addView(m_okButton);
}

void PopupView::setFocusManager(FocusManager* mgr)
{
    if (m_focusManager == mgr) return;
    if (m_focusManager != nullptr && m_popupContext != nullptr)
    {
        unregisterFocusables();
        m_focusManager->removeContext(m_popupContext);
        m_popupContext = nullptr;
    }
    m_focusManager = mgr;
    if (m_focusManager != nullptr)
    {
        m_popupContext = m_focusManager->createContext(FocusContextType::POPUP);
        m_popupContext->setNavigationStrategy(NavigationStrategy::LINEAR);
        m_popupContext->setBlocking(true);
        m_popupContext->setActive(false);
    }
}

void PopupView::registerFocusables()
{
    if (m_focusManager == nullptr || m_popupContext == nullptr) return;
    m_popupContext->setActive(true);
    applyButtonFocus(m_okButton, true);
    if (m_cancelButton->getVisibility() == Visibility::VISIBLE)
        applyButtonFocus(m_cancelButton, false);
    m_focusedButton = m_okButton;
}

void PopupView::applyButtonFocus(Button* btn, bool focused)
{
    if (btn == nullptr) return;
    btn->setBorderColor(focused ? Color::BorderFocus : Color::Transparent, 5.0f);
}

void PopupView::unregisterFocusables()
{
    if (m_popupContext != nullptr) m_popupContext->setActive(false);
    m_focusedButton = nullptr;
}

void PopupView::applyButtonMode()
{
    m_cancelButton->setVisibility(
        (m_buttonMode == PopupButtonMode::OK_ONLY) ? Visibility::GONE : Visibility::VISIBLE);
}

void PopupView::setOkText(const std::string& text)     { if (m_okButton != nullptr)      m_okButton->setText(text); }
void PopupView::setCancelText(const std::string& text) { if (m_cancelButton != nullptr)  m_cancelButton->setText(text); }
void PopupView::setIcon(Texture* texture)              { if (m_iconView != nullptr)      m_iconView->setTexture(texture); }
void PopupView::setTitle(const std::string& title)
{
    if (m_titleView == nullptr) return;
    m_titleView->setText(title);
    m_titleView->setMarquee(!title.empty());
}

void PopupView::setDetail(const std::string& detail)
{
    if (m_detailView == nullptr) return;
    m_detailView->setText(detail);
    m_detailView->setMarquee(!detail.empty());
    m_detailView->setVisibility(detail.empty() ? Visibility::GONE : Visibility::VISIBLE);
}

void PopupView::setButtonMode(PopupButtonMode mode)
{
    m_buttonMode = mode;
    applyButtonMode();
}

void PopupView::show(const std::string& title, const std::string& detail,
                     Texture* icon, PopupButtonMode mode,
                     const std::string& okText, const std::string& cancelText)
{
    setTitle(title);
    setDetail(detail);
    setIcon(icon);
    setButtonMode(mode);
    setOkText(okText);
    setCancelText(cancelText);
    m_isShowing = true;
    setVisibility(Visibility::VISIBLE);
    registerFocusables();
}

void PopupView::hide()
{
    if (m_isShowing == false) return;
    m_isShowing = false;
    if (m_titleView  != nullptr) m_titleView->resetMarquee();
    if (m_detailView != nullptr) m_detailView->resetMarquee();
    setVisibility(Visibility::GONE);
    unregisterFocusables();
}

void PopupView::fireConfirm() { hide(); if (m_onConfirm) m_onConfirm(); }
void PopupView::fireCancel()  { hide(); if (m_onCancel)  m_onCancel();  }

bool PopupView::handleKeyEvent(const IO::KeyEvent& event)
{
    if (m_isShowing == false) return false;
    if (event.action != IO::KeyAction::DOWN) return true;

    if (event.keyCode == IO::KeyCode::ENTER      ||
        event.keyCode == IO::KeyCode::DPAD_CENTER ||
        event.keyCode == IO::KeyCode::SPACE)
    {
        if (m_focusedButton == m_cancelButton) fireCancel();
        else                                   fireConfirm();
        return true;
    }

    if (event.keyCode == IO::KeyCode::BACK || event.keyCode == IO::KeyCode::ESCAPE)
    {
        fireCancel();
        return true;
    }

    if (event.keyCode == IO::KeyCode::DPAD_LEFT || event.keyCode == IO::KeyCode::DPAD_RIGHT)
    {
        bool cancelVisible = m_cancelButton != nullptr &&
                             m_cancelButton->getVisibility() == Visibility::VISIBLE;
        if (cancelVisible == true)
        {
            m_focusedButton = (m_focusedButton == m_okButton) ? m_cancelButton : m_okButton;
            applyButtonFocus(m_okButton,     m_focusedButton == m_okButton);
            applyButtonFocus(m_cancelButton, m_focusedButton == m_cancelButton);
        }
        return true;
    }

    return true;
}

bool PopupView::handleMotionEvent(const IO::MotionEvent& event)
{
    if (m_isShowing == false) return false;

    const bool isDown = (event.action == IO::MotionAction::DOWN);
    const bool isUp   = (event.action == IO::MotionAction::UP);

    if (isDown == true || isUp == true)
    {
        auto hitButton = [&](Button* btn) -> bool
        {
            if (btn == nullptr || btn->getVisibility() != Visibility::VISIBLE) return false;
            return btn->getBounds().contains(event.x, event.y);
        };

        if (isDown == true)
        {
            if (hitButton(m_okButton))
            {
                m_okButton->setPressed(true);
                applyButtonFocus(m_okButton,     true);
                applyButtonFocus(m_cancelButton, false);
                m_focusedButton = m_okButton;
                return true;
            }
            if (hitButton(m_cancelButton))
            {
                m_cancelButton->setPressed(true);
                applyButtonFocus(m_cancelButton, true);
                applyButtonFocus(m_okButton,     false);
                m_focusedButton = m_cancelButton;
                return true;
            }
        }
        else // UP
        {
            bool okWasPressed     = m_okButton     != nullptr && m_okButton->isPressed();
            bool cancelWasPressed = m_cancelButton != nullptr && m_cancelButton->isPressed();
            if (m_okButton != nullptr)     m_okButton->setPressed(false);
            if (m_cancelButton != nullptr) m_cancelButton->setPressed(false);

            if (okWasPressed     == true && hitButton(m_okButton))     { fireConfirm(); return true; }
            if (cancelWasPressed == true && hitButton(m_cancelButton)) { fireCancel();  return true; }
            return true;
        }
    }

    FrameLayout::onMotionEvent(event);
    return true;
}

void PopupView::onDraw(ICanvas& canvas)
{
    if (m_isShowing == false || getVisibility() != Visibility::VISIBLE) return;
    FrameLayout::onDraw(canvas);
}

void PopupView::update(float deltaTime)
{
    if (m_isShowing == false) return;
    if (m_titleView  != nullptr) m_titleView->update(deltaTime);
    if (m_detailView != nullptr) m_detailView->update(deltaTime);
}

} // namespace UI

} // namespace APP
