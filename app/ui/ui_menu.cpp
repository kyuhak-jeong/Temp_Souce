#include "ui_menu.h"
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <cmath>

namespace APP
{
namespace UI
{

// ============================================================================
// MenuItem Implementation
// ============================================================================

void MenuItem::addChild(std::shared_ptr<MenuItem> child)
{
    if (child == nullptr) return;
    child->setParent(this);
    m_children.push_back(child);
}

void MenuItem::removeChild(const std::string& id)
{
    for (auto it = m_children.begin(); it != m_children.end(); ++it)
    {
        if ((*it)->getId() == id)
        {
            (*it)->setParent(nullptr);
            m_children.erase(it);
            return;
        }
    }
}

std::shared_ptr<MenuItem> MenuItem::findChild(const std::string& id) const
{
    for (const auto& child : m_children)
        if (child->getId() == id) return child;
    return nullptr;
}

MenuItem* MenuItem::find(const std::string& id)
{
    if (m_id == id) return this;
    for (const auto& child : m_children)
        if (MenuItem* found = child->find(id)) return found;
    return nullptr;
}

void MenuItem::setCurrentOptionIndex(int index)
{
    if (index < 0 || index >= static_cast<int>(m_options.size())) return;
    m_currentOptionIndex = index;
    m_valueInt = m_options[index].value;
    if (m_onValueChanged != nullptr) m_onValueChanged(this);
}

const MenuOption* MenuItem::getCurrentOption() const
{
    if (m_currentOptionIndex >= 0 && m_currentOptionIndex < static_cast<int>(m_options.size()))
        return &m_options[m_currentOptionIndex];
    return nullptr;
}

// ============================================================================
// MenuItemView Implementation
// ============================================================================

MenuItemView::MenuItemView()
    : FrameLayout()
    , m_item(nullptr)
    , m_selected(false)
    , m_contentLayout(nullptr)
    , m_iconView(nullptr)
    , m_nameText(nullptr)
{
    setNormalColor(Color::CardBackground);
    setHoverColor(Color::CardBackgroundHover);
    setFocusedColor(Color::CardBackground);
    setPressedColor(Color::CardBackgroundHover);
    setBorderWidth(BORDER_W);
    setCornerRadius(24.0f);
    setFocusable(true);

    m_contentLayout = new LinearLayout();
    m_contentLayout->setOrientation(Orientation::VERTICAL);
    m_contentLayout->setGravity(Gravity::CENTER_HORIZONTAL);
    m_contentLayout->setSpacing(48.0f);
    m_contentLayout->getLayoutParams().width  = MATCH_PARENT;
    m_contentLayout->getLayoutParams().height = MATCH_PARENT;
    m_contentLayout->getLayoutParams().setPadding(0.0f, 48.0f);
    addView(m_contentLayout);
}

void MenuItemView::setMenuItem(MenuItem* item)
{
    m_item = item;
    m_contentLayout->removeAllViews();
    m_iconView = nullptr;
    m_nameText = nullptr;

    if (m_item == nullptr) return;

    if (m_item->getIconTexture() != nullptr)
    {
        m_iconView = new ImageView();
        m_iconView->setTexture(m_item->getIconTexture());
        m_iconView->setScaleType(ScaleType::FIT_CENTER);
        m_iconView->setCornerRadius(24.0f);
        m_iconView->setBackgroundColor(Color::Transparent);
        m_iconView->getLayoutParams().width  = WRAP_CONTENT;
        m_iconView->getLayoutParams().height = WRAP_CONTENT;
        m_iconView->getLayoutParams().setMargin(48.0f, 0.0f);
        m_contentLayout->addView(m_iconView);
    }
    else
    {
        FrameLayout* placeholder = new FrameLayout();
        placeholder->setBackgroundColor(Color::AccentPrimary.withAlpha(0.3f));
        placeholder->setCornerRadius(24.0f);
        placeholder->getLayoutParams().width  = MATCH_PARENT;
        placeholder->getLayoutParams().height = 180.0f;
        placeholder->getLayoutParams().setMargin(48.0f, 0.0f);

        TextView* placeholderText = new TextView();
        placeholderText->setText("No icon");
        placeholderText->setTextSize(32.0f);
        placeholderText->setTextGravity(Gravity::CENTER);
        placeholderText->getLayoutParams().width  = MATCH_PARENT;
        placeholderText->getLayoutParams().height = MATCH_PARENT;

        placeholder->addView(placeholderText);
        m_contentLayout->addView(placeholder);
    }

    m_nameText = new TextView();
    m_nameText->setText(m_item->getName());
    m_nameText->setTextColor(Color::TextPrimary);
    m_nameText->setTextSize(60.0f);
    m_nameText->setTextGravity(Gravity::CENTER);
    m_nameText->setMarquee(true);
    m_nameText->getLayoutParams().width  = MATCH_PARENT;
    m_nameText->getLayoutParams().height = MATCH_PARENT;
    m_nameText->getLayoutParams().setMargin(10.0f, 0.0f);
    m_contentLayout->addView(m_nameText);

    updateVisualState();
}

void MenuItemView::setSelected(bool selected)
{
    if (m_selected == selected) return;
    m_selected = selected;
    updateVisualState();
}

void MenuItemView::updateVisualState()
{
    const bool active = (m_selected == true) || (hasFocus() == true) || (isHovered() == true) || (isPressed() == true);
    if (isEnabled() == false)        { setBorderColor(Color::Transparent, BORDER_W); setBackgroundColor(Color::CardBackground.withOpacity(0.15f)); }
    else if (active == true)         setBorderColor(Color::BorderFocus, BORDER_W);
    else                             setBorderColor(Color::Transparent, BORDER_W);

    if (active == false && m_nameText != nullptr) m_nameText->resetMarquee();
}

void MenuItemView::update(float deltaTime)
{
    const bool active = m_selected == true || hasFocus() == true;
    if (active == true && m_nameText != nullptr) m_nameText->update(deltaTime);
}

void MenuItemView::onDraw(ICanvas& canvas)
{
    updateVisualState();
    FrameLayout::onDraw(canvas);
}

bool MenuItemView::onMotionEvent(const IO::MotionEvent& event)
{
    bool inside = m_bounds.contains(event.x, event.y);

    if (event.canHover() == true)
    {
        bool wasHovered = isHovered();
        setHovered(inside);
        if (isHovered() != wasHovered) updateVisualState();
    }

    if (inside == true && event.action == IO::MotionAction::DOWN)
    {
        setPressed(true);
        if (event.isTouch() == true) setFocusable(true);
        updateVisualState();
        return true;
    }

    if (event.action == IO::MotionAction::UP && isPressed() == true)
    {
        setPressed(false);
        updateVisualState();
        if (inside == true) performClick();
        return true;
    }

    return FrameLayout::onMotionEvent(event);
}

void MenuItemView::onFocusChanged(bool hasFocus)
{
    FrameLayout::onFocusChanged(hasFocus);
    updateVisualState();
}

// ============================================================================
// MainMenuView Implementation
// ============================================================================

MainMenuView::MainMenuView()
    : m_rootItem(nullptr)
    , m_selectedIndex(0)
    , m_keyboardActive(false)
    , m_hoveredIndex(-1)
    , m_onItemActivated(nullptr)
{
    setBackgroundColor(Color::DarkSurface);
}

void MainMenuView::setMenuItem(MenuItem* rootItem)
{
    m_rootItem = rootItem;
    m_selectedIndex = 0;
    rebuildItems();
}

void MainMenuView::setSelectedIndex(int index)
{
    if (m_rootItem == nullptr) return;
    const auto& children = m_rootItem->getChildren();
    if (index < -1 || index >= static_cast<int>(children.size())) return;
    m_selectedIndex = index;
    updateSelection();
}

MenuItem* MainMenuView::getSelectedItem() const
{
    if (m_rootItem == nullptr) return nullptr;
    const auto& children = m_rootItem->getChildren();
    if (m_selectedIndex >= 0 && m_selectedIndex < static_cast<int>(children.size()))
        return children[m_selectedIndex].get();
    return nullptr;
}

void MainMenuView::rebuildItems()
{
    for (MenuItemView* view : m_itemViews) removeView(view);
    m_itemViews.clear();

    if (m_rootItem == nullptr) return;

    for (const auto& child : m_rootItem->getChildren())
    {
        MenuItem* item = child.get();
        if (item->isVisible() == false) continue;

        MenuItemView* itemView = new MenuItemView();
        itemView->setMenuItem(item);
        itemView->getLayoutParams().width  = 320;
        itemView->getLayoutParams().height = 400;
        itemView->getLayoutParams().setMargin(24.0f);
        itemView->setOnClickListener([this, item](View*)
        {
            if (m_onItemActivated != nullptr) m_onItemActivated(item);
        });
        addView(itemView);
        m_itemViews.push_back(itemView);
    }
    updateSelection();
}

void MainMenuView::updateSelection()
{
    for (size_t i = 0; i < m_itemViews.size(); ++i)
    {
        bool selected = m_keyboardActive && (m_selectedIndex >= 0 && static_cast<int>(i) == m_selectedIndex);
        m_itemViews[i]->setSelected(selected);
    }
}

void MainMenuView::update(float deltaTime)
{
    if (m_keyboardActive == false || m_selectedIndex < 0) return;
    if (m_selectedIndex < static_cast<int>(m_itemViews.size()))
        m_itemViews[m_selectedIndex]->update(deltaTime);
}
void MainMenuView::setItemBackground(size_t index, const Color4& color)
{
    if (index < m_itemViews.size()) m_itemViews[index]->setBackgroundColor(color);
}

void MainMenuView::onMeasure(MeasureSpec widthSpec, MeasureSpec heightSpec)
{
    float availW = std::max(0.0f, widthSpec.size  - m_layoutParams.getPaddingHorizontal());
    float availH = std::max(0.0f, heightSpec.size - m_layoutParams.getPaddingVertical());

    for (View* child : m_children)
    {
        if (child->getVisibility() == Visibility::GONE) continue;
        const LayoutParams& lp = child->getLayoutParams();
        child->onMeasure(
            getChildMeasureSpec(MeasureSpec::makeAtMost(availW), lp, true),
            getChildMeasureSpec(MeasureSpec::makeAtMost(availH), lp, false));
    }
    m_measuredSize = Vec2(widthSpec.size, heightSpec.size);
}

void MainMenuView::layoutChildren()
{
    RectF contentBounds = getChildLayoutBounds();

    float totalWidth = 0.0f;
    for (View* child : m_children)
    {
        if (child->getVisibility() == Visibility::GONE) continue;
        totalWidth += child->getMeasuredSize().x + child->getLayoutParams().getMarginHorizontal();
    }

    float currentX = contentBounds.left + (contentBounds.width() - totalWidth) * 0.5f;
    float centerY  = contentBounds.top  + contentBounds.height() * 0.5f;

    for (View* child : m_children)
    {
        if (child->getVisibility() == Visibility::GONE) continue;
        Vec2 size = child->getMeasuredSize();
        const LayoutParams& lp = child->getLayoutParams();
        float x = currentX + lp.marginLeft;
        float y = centerY  - size.y * 0.5f;
        child->onLayout(RectF::fromLTRB(x, y, x + size.x, y + size.y));
        currentX = x + size.x + lp.marginRight;
    }
}

void MainMenuView::onDraw(ICanvas& canvas)
{
    if (isVisible() == false) return;
    ViewGroup::onDraw(canvas);
}

bool MainMenuView::onKeyEvent(const IO::KeyEvent& event)
{
    if (event.action != IO::KeyAction::DOWN && event.action != IO::KeyAction::MULTIPLE)
        return ViewGroup::onKeyEvent(event);

    const auto& children = (m_rootItem != nullptr)
        ? m_rootItem->getChildren()
        : std::vector<std::shared_ptr<MenuItem>>();

    int visibleCount = 0;
    for (const auto& child : children)
        if (child->isVisible() == true) ++visibleCount;

    if (visibleCount == 0) return false;

    if (event.keyCode == IO::KeyCode::DPAD_LEFT || event.keyCode == IO::KeyCode::DPAD_RIGHT)
    {
        m_keyboardActive = true;
        int dir  = (event.keyCode == IO::KeyCode::DPAD_LEFT) ? -1 : 1;
        int next = m_selectedIndex + dir;
        if (next < 0)            next = visibleCount - 1;
        if (next >= visibleCount) next = 0;
        setSelectedIndex(next);
        return true;
    }

    if ((event.keyCode == IO::KeyCode::ENTER || event.keyCode == IO::KeyCode::DPAD_CENTER)
        && event.action == IO::KeyAction::DOWN)
    {
        MenuItem* item = getSelectedItem();
        if (item != nullptr && m_onItemActivated != nullptr)
        {
            m_onItemActivated(item);
            return true;
        }
    }

    return ViewGroup::onKeyEvent(event);
}

bool MainMenuView::onMotionEvent(const IO::MotionEvent& event)
{
    bool handled       = false;
    int  newHoveredIdx = -1;

    for (size_t i = 0; i < m_itemViews.size(); ++i)
    {
        MenuItemView* itemView = m_itemViews[i];
        bool inside = itemView->getBounds().contains(event.x, event.y);

        if (inside == true && event.canHover() == true)
        {
            newHoveredIdx = static_cast<int>(i);
            if (m_keyboardActive == true)
            {
                m_keyboardActive = false;
                updateSelection();
            }
        }

        if (itemView->onMotionEvent(event) == true)
        {
            handled = true;
            if (event.action == IO::MotionAction::DOWN && inside == true)
                setSelectedIndex(static_cast<int>(i));
        }
    }

    if (event.canHover() == true) m_hoveredIndex = newHoveredIdx;
    if (handled == false)         handled = ViewGroup::onMotionEvent(event);
    return handled;
}

// ============================================================================
// SubMenuView – static style helpers
// ============================================================================

static void applyRowStyle(View* row, bool selected, bool adjusting, Color4 tint = Color::Transparent)
{
    if (row == nullptr) return;
    const bool hasTint = (tint.a > 0.0f);

    if (adjusting == true)
    {
        row->setNormalColor(Color::AccentSuccess.withAlpha(0.15f));
        row->setHoverColor(Color::AccentSuccess.withAlpha(0.15f));
        row->setFocusedColor(Color::AccentSuccess.withAlpha(0.15f));
        row->setPressedColor(Color::AccentSuccess.withAlpha(0.15f));
        row->setBorderColor(Color::AccentSuccess, BORDER_W);
    }
    else if (selected == true)
    {
        row->setNormalColor(hasTint ? tint : Color::CardBackground);
        row->setHoverColor(hasTint ? tint : Color::CardBackgroundHover);
        row->setFocusedColor(hasTint ? tint : Color::CardBackground);
        row->setPressedColor(hasTint ? tint : Color::CardBackgroundHover);
        row->setBorderColor(Color::BorderFocus, BORDER_W);
    }
    else
    {
        row->setNormalColor(hasTint ? tint : Color::CardBackground);
        row->setHoverColor(hasTint ? tint : Color::CardBackgroundHover);
        row->setFocusedColor(hasTint ? tint : Color::CardBackground);
        row->setPressedColor(hasTint ? tint : Color::CardBackgroundHover);
        row->setBorderColor(Color::Transparent, BORDER_W);
    }
}

static void applyPillStyle(Button* pill, bool isSelected, bool fieldDisabled, bool rowIsEditing, Color4 tint = Color::Transparent)
{
    if (pill == nullptr) return;
    const bool hasTint = (tint.a > 0.0f);
    if (isSelected == true && fieldDisabled == false)
    {
        Color4 c = rowIsEditing == true ? Color::AccentSuccess.withAlpha(0.85f) : Color::AccentPrimary;
        pill->setNormalColor(c);
        pill->setHoverColor(c);
        pill->setPressedColor(c);
        pill->setFocusedColor(c);
    }
    else
    {
        pill->setNormalColor(hasTint ? tint : Color::CardBackgroundHover);
        pill->setHoverColor(hasTint ? tint : Color::CardBackgroundActive);
        pill->setPressedColor(hasTint ? tint : Color::CardBackgroundActive);
        pill->setFocusedColor(Color::AccentPrimary);
    }
}

// ============================================================================
// SubMenuView Implementation
// ============================================================================

SubMenuView::SubMenuView()
    : m_currentItem(nullptr)
    , m_keyboardActive(false)
    , m_isAdjusting(false)
    , m_gridMode(false)
    , m_gridColumns(3)
    , m_selCol(0)
    , m_selRow(0)
    , m_digitIndex(0)
    , m_previewWeight(DEFAULT_PREVIEW_WEIGHT)
    , m_subItemsWeight(DEFAULT_SUBITEMS_WEIGHT)
    , m_previewContainer(nullptr)
    , m_previewImage(nullptr)
    , m_subItemsContainer(nullptr)
    , m_onValueChanged(nullptr)
    , m_onAdjustmentConfirm(nullptr)
    , m_onSubItemSelected(nullptr)
    , m_onNumpadRequest(nullptr)
{
    setupUI();
}

void SubMenuView::setupUI()
{
    setOrientation(Orientation::HORIZONTAL);
    setGravity(Gravity::NO_GRAVITY);
    setSpacing(20.0f);
    setBackgroundColor(Color::DarkSurface);
    getLayoutParams().width  = MATCH_PARENT;
    getLayoutParams().height = MATCH_PARENT;
    getLayoutParams().setPadding(20.0f);

    m_previewContainer = new FrameLayout();
    m_previewContainer->setBackgroundColor(Color::DarkBackground);
    m_previewContainer->setCornerRadius(24.0f);
    m_previewContainer->getLayoutParams().width  = MATCH_PARENT;
    m_previewContainer->getLayoutParams().height = MATCH_PARENT;
    m_previewContainer->getLayoutParams().weight = m_previewWeight;

    m_previewImage = new ImageView();
    m_previewImage->setScaleType(ScaleType::CENTER_INSIDE);
    m_previewImage->setBackgroundColor(Color::CardBackground);
    m_previewImage->setCornerRadius(24.0f);
    m_previewImage->getLayoutParams().width  = MATCH_PARENT;
    m_previewImage->getLayoutParams().height = MATCH_PARENT;
    m_previewContainer->addView(m_previewImage);
    addView(m_previewContainer);

    m_subItemsContainer = new LinearLayout();
    m_subItemsContainer->setOrientation(Orientation::HORIZONTAL);
    m_subItemsContainer->setGravity(Gravity::NO_GRAVITY);
    m_subItemsContainer->setSpacing(0.0f);
    m_subItemsContainer->setBackgroundColor(Color::DarkBackground);
    m_subItemsContainer->setCornerRadius(24.0f);
    m_subItemsContainer->getLayoutParams().width  = MATCH_PARENT;
    m_subItemsContainer->getLayoutParams().height = MATCH_PARENT;
    m_subItemsContainer->getLayoutParams().weight = m_subItemsWeight;
    m_subItemsContainer->getLayoutParams().setPadding(20.0f);
    addView(m_subItemsContainer);
}

// ── Configuration ─────────────────────────────────────────────────────────────

void SubMenuView::setLayoutWeights(float previewWeight, float subItemsWeight)
{
    m_previewWeight  = previewWeight;
    m_subItemsWeight = subItemsWeight;
    if (m_previewContainer  != nullptr) m_previewContainer->getLayoutParams().weight  = m_previewWeight;
    if (m_subItemsContainer != nullptr) m_subItemsContainer->getLayoutParams().weight = m_subItemsWeight;
}

void SubMenuView::getLayoutWeights(float& outPreview, float& outSubItems) const
{
    outPreview  = m_previewWeight;
    outSubItems = m_subItemsWeight;
}

void SubMenuView::setGridMode(bool enabled, int numColumns)
{
    m_gridMode    = enabled;
    m_gridColumns = numColumns;
    m_gridColDefs.clear();
}

void SubMenuView::setGridColumns(const std::vector<GridColumnDef>& colDefs) { m_gridColDefs = colDefs; }
void SubMenuView::setNumpadRequestCallback(NumpadRequestCallback cb)        { m_onNumpadRequest = cb; }

// ── MenuItem binding ──────────────────────────────────────────────────────────

void SubMenuView::setMenuItem(MenuItem* item)
{
    m_currentItem = item;
    m_isAdjusting = false;
    m_selCol      = 0;
    m_selRow      = (item != nullptr) ? item->getSelectedChildIndex() : 0;

    const bool showPreview = (m_currentItem == nullptr || m_currentItem->shouldShowPreview() == true);
    m_previewContainer->setVisibility(showPreview ? Visibility::VISIBLE : Visibility::GONE);
    m_previewContainer->getLayoutParams().weight  = showPreview ? m_previewWeight  : 0.0f;
    m_subItemsContainer->getLayoutParams().weight = showPreview ? m_subItemsWeight : 0.0f;
    m_subItemsContainer->getLayoutParams().width  = MATCH_PARENT;

    setupContent();
}

void SubMenuView::setAdjusting(bool adjusting)
{
    m_isAdjusting = adjusting;
    applyAllRowStyles();
    refreshAllSliderDisplays();
}

void SubMenuView::setKeyboardActive(bool active) { m_keyboardActive = active; applyAllRowStyles(); }

void SubMenuView::setSelectedSubItemIndex(int index)
{
    if (m_currentItem == nullptr) return;
    int flat = 0;
    for (const auto& child : m_currentItem->getChildren())
    {
        if (child == nullptr || child->isVisible() == false) continue;
        if (static_cast<int>(flat) == index || child.get()->getSelectedChildIndex() == index)
            { m_selRow = flat; break; }
        ++flat;
    }
    if (m_onSubItemSelected != nullptr) m_onSubItemSelected(index);
}

// ── Row orientation helper ────────────────────────────────────────────────────

bool SubMenuView::isVerticalRow() const
{
    // Vertical only in standard (1-col) mode when the preview panel is showing
    return (m_gridMode == false
         && m_currentItem != nullptr
         && m_currentItem->shouldShowPreview() == true);
}

// ── Content build ─────────────────────────────────────────────────────────────

void SubMenuView::setupContent()
{
    // Clear view-data on all old items before wiping the view tree
    for (MenuItem* item : m_rows) if (item != nullptr) item->clearViewData();

    m_subItemsContainer->removeAllViews();
    m_colLayouts.clear();
    m_rows.clear();
    m_selCol      = 0;
    m_digitIndex  = 0;
    m_isAdjusting = false;

    if (m_currentItem == nullptr) return;

    // Collect visible children
    std::vector<MenuItem*> items;
    for (const auto& child : m_currentItem->getChildren())
        if (child != nullptr && child->isVisible() == true) items.push_back(child.get());
    if (items.empty() == true) return;

    // Build effective column definitions
    std::vector<GridColumnDef> defs;
    if (m_gridColDefs.empty() == false)
    {
        defs = m_gridColDefs;
        // Expand last column if there are more items than defined slots
        int slots = 0;
        for (const auto& cd : defs) slots += static_cast<int>(cd.rows.size());
        for (int e = static_cast<int>(items.size()) - slots; e > 0; --e)
        {
            if (defs.empty() == true) defs.push_back(GridColumnDef());
            defs.back().rows.push_back(GridRowDef::distributed());
        }
    }
    else
    {
        const int numCols  = m_gridMode == true
                             ? std::min(std::max(1, std::min(m_gridColumns, MAX_GRID_COLS)),
                                        static_cast<int>(items.size()))
                             : 1;
        const int baseRows = static_cast<int>(items.size()) / numCols;
        const int rem      = static_cast<int>(items.size()) % numCols;
        const float fixedH = m_gridMode == true ? SUBMENU_FIXED_ROW_H_GRID : SUBMENU_FIXED_ROW_H;
        for (int c = 0; c < numCols; ++c)
        {
            GridColumnDef cd;
            cd.weight          = 1.0f;
            const int rowsInCol = baseRows + (c < rem ? 1 : 0);
            const bool useFixed = (rowsInCol < SUBMENU_MIN_ROWS);
            for (int r = 0; r < rowsInCol; ++r)
                cd.rows.push_back(useFixed ? GridRowDef::fixed(fixedH) : GridRowDef::distributed());
            defs.push_back(cd);
        }
    }

    // Create column LinearLayouts
    const int   numCols  = static_cast<int>(defs.size());
    const float HALF_GAP = 6.0f;
    for (int c = 0; c < numCols; ++c)
    {
        LinearLayout* col = new LinearLayout();
        col->setOrientation(Orientation::VERTICAL);
        col->setGravity(Gravity::TOP | Gravity::LEFT);
        col->setSpacing(m_gridMode == true ? 10.0f : 20.0f);
        col->getLayoutParams().width  = MATCH_PARENT;
        col->getLayoutParams().height = MATCH_PARENT;
        col->getLayoutParams().weight = defs[c].weight;
        col->getLayoutParams().setMargin(
            c == 0         ? 0.0f : HALF_GAP,
            0.0f,
            c == numCols-1 ? 0.0f : HALF_GAP,
            0.0f);
        m_subItemsContainer->addView(col);
        m_colLayouts.push_back(col);
    }

    // Build rows, mapping items col-major
    int itemIdx = 0;
    for (int c = 0; c < numCols; ++c)
        for (int r = 0; r < static_cast<int>(defs[c].rows.size()); ++r)
        {
            MenuItem* rowItem = (itemIdx < static_cast<int>(items.size()))
                                ? items[itemIdx++] : nullptr;
            buildRow(m_colLayouts[c], rowItem, c, r, defs[c].rows[r]);
        }

    applyAllRowStyles();
}

void SubMenuView::buildRow(LinearLayout* colLayout, MenuItem* item, int col, int rowIdx, const GridRowDef& def)
{
    // Empty slot — insert a transparent spacer that occupies the same height
    if (item == nullptr)
    {
        LinearLayout* spacer = new LinearLayout();
        spacer->getLayoutParams().width  = MATCH_PARENT;
        spacer->getLayoutParams().height = (def.fixedH > 0.0f)
                                           ? static_cast<int>(def.fixedH) : MATCH_PARENT;
        spacer->getLayoutParams().weight = (def.fixedH > 0.0f) ? 0.0f : def.weight;
        colLayout->addView(spacer);
        return;
    }

    const bool isSlider  = (item->getType() == MenuItemType::SLIDER);
    const bool enabled   = item->isEnabled();
    const bool vertical  = isVerticalRow();
    Gravity    cellGrav  = (def.gravity != Gravity::NO_GRAVITY)
                           ? def.gravity
                           : (vertical
                              ? static_cast<Gravity>(Gravity::CENTER_HORIZONTAL | Gravity::TOP)
                              : static_cast<Gravity>(Gravity::CENTER_VERTICAL   | Gravity::LEFT));

    // Scaled sizes for vertical rows (tighter — name + options must share the cell height)
    const float textSzName = m_gridMode ? 40.0f : 48.0f;
    const float textSzOpt  = m_gridMode ? 40.0f : 48.0f;
    const float pillPadH   = 24.0f;
    const float pillPadV   = 8.0f;

    // ── Outer row container ───────────────────────────────────────────────
    LinearLayout* rowView = new LinearLayout();
    rowView->setOrientation(vertical ? Orientation::VERTICAL : Orientation::HORIZONTAL);
    rowView->setGravity(cellGrav);
    rowView->setSpacing(vertical ? 5.0f : 10.0f);
    rowView->setBackgroundColor(Color::CardBackground);
    rowView->setBorderColor(Color::Transparent, BORDER_W);
    rowView->setCornerRadius(m_gridMode ? 16.0f : 24.0f);
    rowView->getLayoutParams().width = MATCH_PARENT;
    // Always weight-distributed — let the column divide its height among rows
    rowView->getLayoutParams().height = MATCH_PARENT;
    rowView->getLayoutParams().weight = (def.fixedH > 0.0f) ? 0.0f : def.weight;
    if (def.fixedH > 0.0f) rowView->getLayoutParams().height = def.fixedH;
    rowView->getLayoutParams().setPadding(
        m_gridMode ? 12.0f : 16.0f,
        m_gridMode ?  6.0f : 10.0f);

    // ── Name label ────────────────────────────────────────────────────────
    TextView* nameLabel = new TextView();
    nameLabel->setText(item->getName());
    nameLabel->setTextColor(enabled ? Color::TextPrimary : Color::TextDisabled);
    nameLabel->setTextSize(textSzName);
    if (vertical == true)
    {
        // Top section: name centered, weight=1 of the vertical space
        nameLabel->setTextGravity(Gravity::CENTER_HORIZONTAL | Gravity::CENTER_VERTICAL);
        nameLabel->setMarquee(true);
        nameLabel->getLayoutParams().width  = MATCH_PARENT;
        nameLabel->getLayoutParams().height = MATCH_PARENT;
        nameLabel->getLayoutParams().weight = 1.0f;
    }
    else
    {
        nameLabel->setTextGravity(Gravity::LEFT | Gravity::CENTER_VERTICAL);
        nameLabel->setMarquee(true);
        nameLabel->getLayoutParams().width  = MATCH_PARENT;
        nameLabel->getLayoutParams().height = MATCH_PARENT;
        nameLabel->getLayoutParams().weight = m_gridMode ? 0.65f : 1.0f;
        if (m_gridMode == false) nameLabel->getLayoutParams().setPadding(24.0f, 0.0f);
    }
    rowView->addView(nameLabel);

    // ── Value widget ──────────────────────────────────────────────────────
    LinearLayout* valueWidget = new LinearLayout();
    valueWidget->setOrientation(Orientation::HORIZONTAL);
    valueWidget->setSpacing(m_gridMode ? 8.0f : (vertical ? 10.0f : 16.0f));
    valueWidget->setGravity(Gravity::CENTER_VERTICAL | Gravity::RIGHT);
    if (vertical == true)
    {
        // Bottom section: options centered horizontally, weight=1 of vertical space
        valueWidget->getLayoutParams().width   = WRAP_CONTENT;
        valueWidget->getLayoutParams().height  = MATCH_PARENT;
        valueWidget->getLayoutParams().weight  = 1.0f;
    }
    else
    {
        // Right section: controls right-aligned; in grid mode 0.35 to complement name's 0.65
        valueWidget->getLayoutParams().width   = WRAP_CONTENT;
        valueWidget->getLayoutParams().height  = MATCH_PARENT;
        valueWidget->getLayoutParams().weight  = m_gridMode ? 0.35f : 1.0f;
        if (m_gridMode == false) valueWidget->getLayoutParams().setPadding(8.0f, 0.0f);
    }

    TextView* valLabel  = nullptr;
    Button*   numpadBtn = nullptr;

    if (isSlider == true)
    {
        valLabel = new TextView();
        valLabel->setText(formatValue(item->getValueFloat(), item->getMinValue(), item->getMaxValue(), item->isZeroPad()));
        valLabel->setTextColor(enabled ? Color::AccentInfo : Color::TextDisabled);
        valLabel->setTextSize(textSzOpt);
        valLabel->setTextGravity(Gravity::CENTER);
        valLabel->getLayoutParams().width  = MATCH_PARENT;
        valLabel->getLayoutParams().height = MATCH_PARENT;
        valLabel->getLayoutParams().weight = 1.0f;
        valueWidget->addView(valLabel);

        // Always create the numpad button — even for initially-disabled items.
        // If built only when enabled==true, items that become enabled later have
        // no button in their ViewData and can never show one.
        numpadBtn = new Button();
        numpadBtn->setText("...");
        numpadBtn->setTextSize(32.0f);
        numpadBtn->setTextColor(Color::TextSecondary);
        numpadBtn->setNormalColor(Color::CardBackgroundHover);
        numpadBtn->setHoverColor(Color::CardBackgroundActive);
        numpadBtn->setPressedColor(Color::AccentPrimary);
        numpadBtn->setCornerRadius(12.0f);
        numpadBtn->setVisibility(Visibility::INVISIBLE);
        numpadBtn->getLayoutParams().width   = 40;
        numpadBtn->getLayoutParams().height  = 40;
        numpadBtn->getLayoutParams().weight  = 0.0f;
        numpadBtn->getLayoutParams().gravity = Gravity::CENTER_VERTICAL | Gravity::RIGHT;
        {
            const int flatIdx = static_cast<int>(m_rows.size());
            numpadBtn->setOnClickListener([this, flatIdx](View*)
            {
                if (flatIdx < static_cast<int>(m_rows.size()))
                {
                    MenuItem::ViewData& vd = m_rows[flatIdx]->viewData();
                    m_selCol = vd.col;
                    m_selRow = vd.rowIdx;
                }
                showNumpad();
            });
        }
        valueWidget->addView(numpadBtn);
    }
    else if (item->getType() != MenuItemType::ACTION)
    {
        const bool rowSelected  = m_keyboardActive && (col == m_selCol && rowIdx == m_selRow);
        const bool rowAdjusting = m_isAdjusting && rowSelected;

        switch (item->getType())
        {
            case MenuItemType::CATEGORY:
            {
                const auto& subChildren = item->getChildren();
                const bool  compact     = (subChildren.size() > 4);
                const int   selIdx      = item->getSelectedChildIndex();
                valueWidget->setSpacing(compact ? 4.0f : 16.0f);
                for (size_t ci = 0; ci < subChildren.size(); ++ci)
                {
                    MenuItem*  child    = subChildren[ci].get();
                    const bool isSel    = (static_cast<int>(ci) == selIdx);
                    const bool disabled = (child->isEnabled() == false);
                    Button* btn = new Button();
                    btn->setText(child->getName());
                    btn->setTextSize(textSzOpt);
                    btn->setEnabled(disabled == false);
                    btn->setTextColor(disabled ? Color::TextDisabled : Color::TextPrimary);
                    btn->setCornerRadius(20.0f);
                    btn->getLayoutParams().width  = WRAP_CONTENT;
                    btn->getLayoutParams().height = WRAP_CONTENT;
                    btn->getLayoutParams().weight = 0.0f;
                    btn->getLayoutParams().setPadding(compact ? 6.0f : pillPadH, compact ? 2.0f : pillPadV);
                    applyPillStyle(btn, isSel, disabled, rowAdjusting, child->viewData().tintColor);
                    valueWidget->addView(btn);
                }
                break;
            }

            case MenuItemType::TOGGLE:
            {
                const bool val = item->getValueBool();
                auto makePill = [&](const char* label, bool active, bool isOn) -> TextView*
                {
                    TextView* pill = new TextView();
                    pill->setText(label);
                    pill->setTextSize(textSzOpt);
                    pill->setTextGravity(Gravity::CENTER);
                    pill->setCornerRadius(20.0f);
                    pill->setBackgroundColor(active
                        ? (isOn ? Color::AccentSuccess.withAlpha(0.8f) : Color::AccentPrimary)
                        : Color::CardBackground);
                    pill->setTextColor(active ? Color::TextPrimary : Color::TextSecondary);
                    pill->getLayoutParams().width  = vertical ? WRAP_CONTENT : 120;
                    pill->getLayoutParams().height = WRAP_CONTENT;
                    pill->getLayoutParams().setPadding(pillPadH, pillPadV);
                    return pill;
                };
                valueWidget->addView(makePill("OFF", val == false, false));
                valueWidget->addView(makePill("ON",  val == true,  true));
                break;
            }

            case MenuItemType::CHOICE:
            {
                const auto& options    = item->getOptions();
                const int   currentIdx = item->getCurrentOptionIndex();
                const bool  compact    = (options.size() > 4 || vertical);
                valueWidget->setSpacing(compact ? 4.0f : 16.0f);
                for (size_t optIdx = 0; optIdx < options.size(); ++optIdx)
                {
                    const bool isSel = (static_cast<int>(optIdx) == currentIdx);
                    Button* btn = new Button();
                    btn->setCornerRadius(20.0f);
                    btn->setText(options[optIdx].label);
                    btn->setTextSize(textSzOpt);
                    btn->setTextColor(Color::TextPrimary);
                    btn->setNormalColor(isSel ? Color::AccentPrimary : Color::Transparent);
                    btn->setFocusedColor(Color::AccentPrimary);
                    btn->getLayoutParams().width  = WRAP_CONTENT;
                    btn->getLayoutParams().height = WRAP_CONTENT;
                    btn->getLayoutParams().weight = 0.0f;
                    btn->getLayoutParams().setPadding(compact ? 6.0f : pillPadH, compact ? 2.0f : pillPadV);
                    btn->onFocusChanged(isSel);
                    valueWidget->addView(btn);
                }
                break;
            }

            case MenuItemType::CHECKBOX:
            {
                const auto& subChildren = item->getChildren();
                const bool  compact     = (subChildren.size() > 5);
                const float boxSize     = compact ? 40.0f : 48.0f;
                const int   focusedBox  = rowSelected ? item->getSelectedChildIndex() : -1;
                valueWidget->setSpacing(compact ? 12.0f : 24.0f);
                for (size_t ci = 0; ci < subChildren.size(); ++ci)
                {
                    MenuItem*  child      = subChildren[ci].get();
                    const bool isChecked  = item->isChildChecked(static_cast<int>(ci));
                    const bool isFocused  = (static_cast<int>(ci) == focusedBox);
                    const bool isDisabled = (child->isEnabled() == false);
                    CheckBox* cb = new CheckBox();
                    cb->setText(child->getName());
                    cb->setChecked(isChecked);
                    cb->setOrientation(Orientation::HORIZONTAL);
                    cb->setTextGravity(Gravity::CENTER_VERTICAL | Gravity::LEFT);
                    cb->setCheckBoxSize(boxSize);
                    cb->setSpacing(12.0f);
                    cb->setTextSize(textSzOpt);
                    cb->setTextColor(isDisabled ? Color::TextDisabled : Color::TextSecondary);
                    cb->setCheckColor(isDisabled ? Color::TextDisabled : Color::AccentPrimary);
                    cb->setFocusHighlight(isFocused);
                    cb->setEnabled(isDisabled == false);
                    cb->getLayoutParams().width  = WRAP_CONTENT;
                    cb->getLayoutParams().height = WRAP_CONTENT;
                    valueWidget->addView(cb);
                }
                break;
            }

            default: break;
        }
    }

    if (item->getType() != MenuItemType::ACTION) rowView->addView(valueWidget);

    colLayout->addView(rowView);

    // Write all widget pointers into the item's ViewData
    MenuItem::ViewData& vd = item->viewData();
    vd.row         = rowView;
    vd.nameLabel   = nameLabel;
    vd.valueWidget = (item->getType() != MenuItemType::ACTION) ? valueWidget : nullptr;
    vd.valLabel    = valLabel;
    vd.numpadBtn   = numpadBtn;
    vd.col         = col;
    vd.rowIdx      = rowIdx;

    m_rows.push_back(item);
}

// ── Content update ────────────────────────────────────────────────────────────

void SubMenuView::updateContent()
{
    for (MenuItem* item : m_rows) updateContent(item);
    refreshAllSliderDisplays();
    applyAllRowStyles();
}

void SubMenuView::updateContent(MenuItem* item)
{
    if (item == nullptr) return;
    const MenuItem::ViewData& vd = item->viewData();
    if (vd.valueWidget == nullptr) return;

    const bool rowSelected  = m_keyboardActive && (vd.col == m_selCol && vd.rowIdx == m_selRow);
    const bool rowAdjusting = m_isAdjusting && rowSelected;

    switch (item->getType())
    {
        case MenuItemType::SLIDER:
            // valLabel updated by refreshAllSliderDisplays
            break;

        case MenuItemType::CATEGORY:
        {
            const auto& subChildren = item->getChildren();
            const int   selIdx      = item->getSelectedChildIndex();
            if (vd.valueWidget->getChildCount() != subChildren.size()) break;
            for (size_t ci = 0; ci < subChildren.size(); ++ci)
            {
                Button* pill = dynamic_cast<Button*>(vd.valueWidget->getChildAt(ci));
                if (pill == nullptr) continue;
                MenuItem*  child    = subChildren[ci].get();
                const bool isSel    = (static_cast<int>(ci) == selIdx);
                const bool disabled = (child->isEnabled() == false);
                pill->setText(child->getName());
                pill->setTextColor(disabled ? Color::TextDisabled : Color::TextPrimary);
                pill->setEnabled(disabled == false);
                applyPillStyle(pill, isSel, disabled, rowAdjusting, child->viewData().tintColor);
            }
            break;
        }

        case MenuItemType::TOGGLE:
        {
            if (vd.valueWidget->getChildCount() < 2) break;
            const bool val = item->getValueBool();
            auto updatePill = [](View* v, bool active, bool isOn)
            {
                if (v == nullptr) return;
                v->setBackgroundColor(active
                    ? (isOn ? Color::AccentSuccess.withAlpha(0.8f) : Color::AccentPrimary)
                    : Color::CardBackground);
            };
            updatePill(vd.valueWidget->getChildAt(0), val == false, false);
            updatePill(vd.valueWidget->getChildAt(1), val == true,  true);
            break;
        }

        case MenuItemType::CHOICE:
        {
            const auto& options = item->getOptions();
            const int   curIdx  = item->getCurrentOptionIndex();
            if (vd.valueWidget->getChildCount() != options.size()) break;
            for (size_t optIdx = 0; optIdx < options.size(); ++optIdx)
            {
                Button* pill = dynamic_cast<Button*>(vd.valueWidget->getChildAt(optIdx));
                if (pill != nullptr)
                    applyPillStyle(pill, static_cast<int>(optIdx) == curIdx, false, rowAdjusting);
            }
            break;
        }

        case MenuItemType::CHECKBOX:
        {
            const auto& subChildren = item->getChildren();
            if (vd.valueWidget->getChildCount() != subChildren.size()) break;
            const int focusedBox = rowSelected ? item->getSelectedChildIndex() : -1;
            for (size_t ci = 0; ci < subChildren.size(); ++ci)
            {
                CheckBox* cb = dynamic_cast<CheckBox*>(vd.valueWidget->getChildAt(ci));
                if (cb == nullptr) continue;
                const bool isChecked  = item->isChildChecked(static_cast<int>(ci));
                const bool isFocused  = (static_cast<int>(ci) == focusedBox);
                const bool isDisabled = (subChildren[ci] == nullptr
                                      || subChildren[ci]->isEnabled() == false);
                cb->setChecked(isChecked);
                cb->setFocusHighlight(isFocused);
                cb->setEnabled(isDisabled == false);
                cb->setTextColor(isDisabled ? Color::TextDisabled : Color::TextSecondary);
                cb->setCheckColor(isDisabled ? Color::TextDisabled : Color::AccentPrimary);
            }
            break;
        }

        default: break;
    }
}

void SubMenuView::clearSelection()
{
    const bool saved = m_keyboardActive;
    m_keyboardActive = false;
    applyAllRowStyles();
    m_keyboardActive = saved;
}

void SubMenuView::restoreSelection() { updateContent(); }

// ── Row styles ────────────────────────────────────────────────────────────────

void SubMenuView::applyAllRowStyles()
{
    for (MenuItem* item : m_rows) applyRowStyle(item);
}

void SubMenuView::applyRowStyle(MenuItem* item)
{
    if (item == nullptr) return;
    const MenuItem::ViewData& vd  = item->viewData();
    const bool en      = item->isEnabled();
    const bool sel     = m_keyboardActive && (vd.col == m_selCol && vd.rowIdx == m_selRow);
    const bool adj     = m_isAdjusting && sel;
    const bool hovered = (vd.row != nullptr && vd.row->isHovered() == true);
    const bool hasTint = (vd.tintColor.a > 0.0f);

    APP::UI::applyRowStyle(vd.row, (sel && en) || hovered, adj && en, hasTint ? vd.tintColor : Color::Transparent);

    if (vd.nameLabel != nullptr)
    {
        vd.nameLabel->setTextColor(
            !en      ? Color::TextDisabled :
            adj      ? Color::AccentSuccess :
            hasTint  ? Color::TextPrimary   :
            sel      ? Color::TextPrimary   :
                       Color::TextSecondary);
        if (sel == false) vd.nameLabel->resetMarquee();
    }

    if (vd.valLabel != nullptr)
        vd.valLabel->setTextColor(
            !en  ? Color::TextDisabled :
            adj  ? Color::AccentSuccess :
                   Color::AccentInfo);

    if (vd.numpadBtn != nullptr)
        vd.numpadBtn->setVisibility(
            (en && (sel || adj || hovered)) ? Visibility::VISIBLE : Visibility::INVISIBLE);

    updateContent(item);
}

// ── Slider display ────────────────────────────────────────────────────────────

std::string SubMenuView::formatValue(float v, float minV, float maxV, bool zeroPad)
{
    const bool isInt = (minV == static_cast<float>(static_cast<int>(minV))
                     && maxV == static_cast<float>(static_cast<int>(maxV)));
    if (isInt == true)
    {
        char buf[32];
        if (zeroPad == true)
        {
            // Determine width from maxV digit count
            const int digits = static_cast<int>(std::log10(std::max(1.0f, maxV))) + 1;
            snprintf(buf, sizeof(buf), "%0*d", digits, static_cast<int>(v));
        }
        else
        {
            snprintf(buf, sizeof(buf), "%d", static_cast<int>(v));
        }
        return std::string(buf);
    }
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1) << v;
    return oss.str();
}

float SubMenuView::clampValue(float v, float minV, float maxV) { return std::max(minV, std::min(v, maxV)); }

void SubMenuView::refreshSliderDisplay(MenuItem* item)
{
    if (item == nullptr) return;
    const MenuItem::ViewData& vd = item->viewData();
    if (vd.valLabel == nullptr) return;

    const bool adj = m_isAdjusting && m_keyboardActive
                  && (vd.col == m_selCol && vd.rowIdx == m_selRow);

    if (adj == false)
    {
        vd.valLabel->setText(formatValue(item->getValueFloat(), item->getMinValue(), item->getMaxValue(), item->isZeroPad()));
        return;
    }

    std::string raw = formatValue(item->getValueFloat(), item->getMinValue(), item->getMaxValue(), item->isZeroPad());
    m_digitIndex    = std::max(0, std::min(m_digitIndex, static_cast<int>(raw.size()) - 1));
    std::string display = raw;
    if (m_digitIndex < static_cast<int>(raw.size())
        && raw[m_digitIndex] != '.' && raw[m_digitIndex] != '-')
    {
        display = raw.substr(0, m_digitIndex) + "[" + raw[m_digitIndex] + "]"
                + raw.substr(m_digitIndex + 1);
    }
    vd.valLabel->setText(display);
}

void SubMenuView::refreshAllSliderDisplays()
{
    for (MenuItem* item : m_rows)
        if (item != nullptr && item->viewData().valLabel != nullptr) refreshSliderDisplay(item);
}

// ── Selection helpers ─────────────────────────────────────────────────────────

MenuItem* SubMenuView::selectedItem() const
{
    for (MenuItem* item : m_rows)
        if (item != nullptr && item->viewData().col == m_selCol
                            && item->viewData().rowIdx == m_selRow) return item;
    return nullptr;
}

int SubMenuView::flatIndexOf(int col, int rowIdx) const
{
    for (size_t i = 0; i < m_rows.size(); ++i)
    {
        if (m_rows[i] == nullptr) continue;
        const MenuItem::ViewData& vd = m_rows[i]->viewData();
        if (vd.col == col && vd.rowIdx == rowIdx) return static_cast<int>(i);
    }
    return -1;
}

int SubMenuView::maxRowInCol(int col) const
{
    int maxR = -1;
    for (const MenuItem* item : m_rows)
    {
        if (item == nullptr) continue;
        const MenuItem::ViewData& vd = item->viewData();
        if (vd.col == col && vd.rowIdx > maxR) maxR = vd.rowIdx;
    }
    return maxR;
}

int SubMenuView::maxCol() const
{
    int maxC = 0;
    for (const MenuItem* item : m_rows)
        if (item != nullptr && item->viewData().col > maxC) maxC = item->viewData().col;
    return maxC;
}

// ── Navigation ────────────────────────────────────────────────────────────────

void SubMenuView::moveRow(int delta)
{
    const int maxR = maxRowInCol(m_selCol);
    if (maxR < 0) return;
    int next = m_selRow + delta;
    if (next < 0)    next = maxR;
    if (next > maxR) next = 0;
    for (int attempts = static_cast<int>(m_rows.size()); attempts > 0; --attempts)
    {
        const int fi = flatIndexOf(m_selCol, next);
        if (fi >= 0 && m_rows[fi]->isEnabled() == true)
        {
            m_selRow = next;
            if (m_onSubItemSelected != nullptr) m_onSubItemSelected(m_selRow);
            return;
        }
        next += delta;
        if (next < 0)    next = maxR;
        if (next > maxR) next = 0;
    }
}

void SubMenuView::moveCol(int delta)
{
    const int maxC = maxCol();
    int next = m_selCol + delta;
    if (next < 0)    next = maxC;
    if (next > maxC) next = 0;

    const int bestRow = maxRowInCol(next);
    if (bestRow < 0) return;
    const int targetRow = std::min(m_selRow, bestRow);
    for (int r = targetRow; r >= 0; --r)
    {
        const int fi = flatIndexOf(next, r);
        if (fi >= 0 && m_rows[fi]->isEnabled() == true)
        {
            m_selCol = next; m_selRow = r;
            if (m_onSubItemSelected != nullptr) m_onSubItemSelected(m_selRow);
            return;
        }
    }
    for (int r = targetRow + 1; r <= bestRow; ++r)
    {
        const int fi = flatIndexOf(next, r);
        if (fi >= 0 && m_rows[fi]->isEnabled() == true)
        {
            m_selCol = next; m_selRow = r;
            if (m_onSubItemSelected != nullptr) m_onSubItemSelected(m_selRow);
            return;
        }
    }
}

void SubMenuView::moveOption(int delta)
{
    MenuItem* item = selectedItem();
    if (item == nullptr) return;

    if (item->getType() == MenuItemType::CATEGORY)
    {
        const auto& sub = item->getChildren();
        if (sub.empty() == true) return;
        const int total = static_cast<int>(sub.size());
        auto wrap = [total](int i) { return (i < 0) ? total-1 : (i >= total) ? 0 : i; };
        int next = wrap(item->getSelectedChildIndex() + delta);
        for (int t = total; t > 0; --t, next = wrap(next + delta))
            if (sub[next]->isEnabled() == true) break;
        item->setSelectedChildIndex(next);
        item->setValueInt(next);
        if (m_onValueChanged != nullptr) m_onValueChanged(item);
    }
    else if (item->getType() == MenuItemType::CHOICE)
    {
        const int total = static_cast<int>(item->getOptions().size());
        int newIdx = item->getCurrentOptionIndex() + delta;
        if (newIdx < 0)      newIdx = total - 1;
        if (newIdx >= total) newIdx = 0;
        item->setCurrentOptionIndex(newIdx);
        if (m_onValueChanged != nullptr) m_onValueChanged(item);
    }
    else if (item->getType() == MenuItemType::TOGGLE)
    {
        item->setValueBool(!item->getValueBool());
        if (m_onValueChanged != nullptr) m_onValueChanged(item);
    }
    else if (item->getType() == MenuItemType::CHECKBOX)
    {
        const auto& sub = item->getChildren();
        if (sub.empty() == true) return;
        const int total = static_cast<int>(sub.size());
        int next = item->getSelectedChildIndex() + delta;
        if (next < 0)      next = total - 1;
        if (next >= total) next = 0;
        while (sub[next] != nullptr && sub[next]->isEnabled() == false)
        {
            next += delta;
            if (next < 0)      next = total - 1;
            if (next >= total) next = 0;
        }
        item->setSelectedChildIndex(next);
    }
    else if (item->getType() == MenuItemType::SLIDER)
        adjustValue(static_cast<float>(delta));

    updateContent(item);
    if (m_onSubItemSelected != nullptr) m_onSubItemSelected(m_selRow);
}

void SubMenuView::adjustValue(float delta)
{
    MenuItem* item = selectedItem();
    if (item == nullptr || item->getType() != MenuItemType::SLIDER) return;
    item->setValueFloat(clampValue(item->getValueFloat() + delta * item->getStep(),
                                   item->getMinValue(), item->getMaxValue()));
    if (m_onValueChanged != nullptr) m_onValueChanged(item);
    refreshSliderDisplay(item);
}

void SubMenuView::moveDigit(int delta)
{
    MenuItem* item = selectedItem();
    if (item == nullptr) return;
    const std::string raw  = formatValue(item->getValueFloat(), item->getMinValue(), item->getMaxValue(), item->isZeroPad());
    const int         last = static_cast<int>(raw.size()) - 1;
    m_digitIndex += delta;
    while (m_digitIndex >= 0 && m_digitIndex <= last
           && (raw[m_digitIndex] == '.' || raw[m_digitIndex] == '-'))
        m_digitIndex += delta;
    m_digitIndex = std::max(0, std::min(m_digitIndex, last));
    refreshSliderDisplay(item);
}

void SubMenuView::adjustDigit(int delta)
{
    MenuItem* item = selectedItem();
    if (item == nullptr || item->isEnabled() == false) return;

    const std::string raw = formatValue(item->getValueFloat(), item->getMinValue(), item->getMaxValue(), item->isZeroPad());
    if (m_digitIndex < 0 || m_digitIndex >= static_cast<int>(raw.size())) return;
    if (raw[m_digitIndex] == '.' || raw[m_digitIndex] == '-') return;

    const int dotPos = static_cast<int>(raw.find('.'));
    float place;
    if (dotPos == static_cast<int>(std::string::npos))
    {
        const int digitsFromRight = static_cast<int>(raw.size()) - 1 - m_digitIndex;
        place = 1.0f;
        for (int k = 0; k < digitsFromRight; ++k) place *= 10.0f;
    }
    else
    {
        const int digitsAfterDot = static_cast<int>(raw.size()) - dotPos - 1;
        const int thisOffset     = m_digitIndex - dotPos - 1;
        place = 1.0f;
        for (int k = 0; k < digitsAfterDot - thisOffset - 1; ++k) place /= 10.0f;
        if (m_digitIndex < dotPos) place *= 10.0f;
    }

    item->setValueFloat(clampValue(item->getValueFloat() + static_cast<float>(delta) * place,
                                   item->getMinValue(), item->getMaxValue()));
    if (m_onValueChanged != nullptr) m_onValueChanged(item);
    refreshSliderDisplay(item);
}

void SubMenuView::showNumpad()
{
    if (m_onNumpadRequest == nullptr) return;
    MenuItem* item = selectedItem();
    if (item == nullptr || item->isEnabled() == false) return;
    m_onNumpadRequest(item, [this](float) { applyAllRowStyles(); refreshAllSliderDisplays(); });
}

// ── Input handling ────────────────────────────────────────────────────────────

bool SubMenuView::onKeyEvent(const IO::KeyEvent& event)
{
    if (event.action != IO::KeyAction::DOWN && event.action != IO::KeyAction::MULTIPLE) return false;
    m_keyboardActive = true;

    MenuItem*  sel      = selectedItem();
    const bool isSlider = (sel != nullptr && sel->getType() == MenuItemType::SLIDER);

    // Digit-edit mode (SLIDER only)
    if (m_isAdjusting == true && isSlider == true)
    {
        switch (event.keyCode)
        {
            case IO::KeyCode::DPAD_UP:    adjustDigit(1);  applyRowStyle(sel); return true;
            case IO::KeyCode::DPAD_DOWN:  adjustDigit(-1); applyRowStyle(sel); return true;
            case IO::KeyCode::DPAD_LEFT:  moveDigit(-1); return true;
            case IO::KeyCode::DPAD_RIGHT: moveDigit(1);  return true;
            case IO::KeyCode::DPAD_CENTER:
            case IO::KeyCode::ENTER:
            case IO::KeyCode::SPACE:
                if (event.action == IO::KeyAction::DOWN)
                    { m_isAdjusting = false; m_digitIndex = 0; applyAllRowStyles(); refreshAllSliderDisplays(); }
                return true;
            default: break;
        }
        return false;
    }

    // Normal navigation
    switch (event.keyCode)
    {
        case IO::KeyCode::DPAD_UP:
            if (m_selRow == 0) return false;  // let caller move to Back button
            moveRow(-1); applyAllRowStyles(); return true;
        case IO::KeyCode::DPAD_DOWN:
            moveRow(1); applyAllRowStyles(); return true;
        case IO::KeyCode::DPAD_LEFT:
            if (m_gridMode == true) { moveCol(-1); applyAllRowStyles(); return true; }
            moveOption(-1); return true;
        case IO::KeyCode::DPAD_RIGHT:
            if (m_gridMode == true) { moveCol(1); applyAllRowStyles(); return true; }
            moveOption(1); return true;
        case IO::KeyCode::DPAD_CENTER:
        case IO::KeyCode::ENTER:
        case IO::KeyCode::SPACE:
            if (event.action != IO::KeyAction::DOWN) return true;
            if (sel == nullptr || sel->isEnabled() == false) return false;
            if      (sel->getType() == MenuItemType::ACTION) sel->activate();
            else if (sel->getType() == MenuItemType::CATEGORY)
            {
                const auto& sub = sel->getChildren();
                const int   idx = sel->getSelectedChildIndex();
                if (idx >= 0 && idx < static_cast<int>(sub.size())) sub[idx]->activate();
            }
            else if (sel->getType() == MenuItemType::CHECKBOX)
            {
                sel->toggleChildChecked(sel->getSelectedChildIndex());
                updateContent(sel);
            }
            else if (isSlider == true)
            {
                m_isAdjusting = true;
                m_digitIndex     = 0;
                applyAllRowStyles();
                refreshAllSliderDisplays();
            }
            else moveOption(1);
            return true;
        default: break;
    }
    return false;
}

bool SubMenuView::onMotionEvent(const IO::MotionEvent& event)
{
    // In adjustment mode: tap on a pill = confirm; tap on SLIDER row = toggle adj off
    if (m_isAdjusting == true)
    {
        if (event.action == IO::MotionAction::UP)
        {
            MenuItem* sel = selectedItem();
            if (sel != nullptr)
            {
                const MenuItem::ViewData& vd = sel->viewData();
                if (vd.valueWidget != nullptr && m_onAdjustmentConfirm != nullptr)
                {
                    const int selPillIdx = sel->getSelectedChildIndex();
                    View* activePill = vd.valueWidget->getChildAt(static_cast<size_t>(selPillIdx));
                    if (activePill != nullptr && activePill->getBounds().contains(event.x, event.y))
                        { m_onAdjustmentConfirm(); return true; }
                }
                if (sel->getType() == MenuItemType::SLIDER
                    && vd.row != nullptr && vd.row->getBounds().contains(event.x, event.y) == true
                    && (vd.numpadBtn == nullptr
                        || vd.numpadBtn->getBounds().contains(event.x, event.y) == false))
                {
                    m_isAdjusting = false;
                    m_digitIndex  = 0;
                    applyAllRowStyles();
                    refreshAllSliderDisplays();
                    return true;
                }
            }
        }
        return false;
    }

    bool handled = false;

    for (MenuItem* item : m_rows)
    {
        if (item == nullptr) continue;
        MenuItem::ViewData& vd = item->viewData();
        if (vd.row == nullptr) continue;
        const bool inside = vd.row->getBounds().contains(event.x, event.y);

        if (event.canHover() == true)
        {
            vd.row->setHovered(inside);
            if (inside == true)
            {
                const bool isSelectedRow = (vd.col == m_selCol && vd.rowIdx == m_selRow);
                if (m_keyboardActive == true && isSelectedRow == false)
                    { m_keyboardActive = false; applyAllRowStyles(); }
            }
            applyRowStyle(item);
        }

        if (inside == true && event.action == IO::MotionAction::DOWN)
        {
            if (item->isEnabled() == false) { handled = true; continue; }
            const bool changed = (vd.col != m_selCol || vd.rowIdx != m_selRow);
            m_selCol         = vd.col;
            m_selRow         = vd.rowIdx;
            m_keyboardActive = true;
            vd.row->setPressed(true);
            applyAllRowStyles();
            if (changed && m_onSubItemSelected != nullptr) m_onSubItemSelected(m_selRow);
            handled = true;
        }
        else if (event.action == IO::MotionAction::UP && vd.row->isPressed() == true)
        {
            vd.row->setPressed(false);
            if (inside == true)
            {
                if (item->getType() == MenuItemType::SLIDER)
                {
                    if (item->isEnabled() == true)
                    {
                        if (vd.numpadBtn != nullptr
                            && vd.numpadBtn->getBounds().contains(event.x, event.y) == true)
                        {
                            m_isAdjusting = true;
                            m_digitIndex     = 0;
                            applyAllRowStyles();
                            showNumpad();
                        }
                    }
                }
                else if (item->isEnabled() == false) { /* disabled row — absorb, no action */ }
                else if (item->getType() == MenuItemType::ACTION) item->activate();
                else if (item->getType() == MenuItemType::TOGGLE)
                    { moveOption(1); updateContent(item); }
                else if (vd.valueWidget != nullptr)
                {
                    bool tapped = false;
                    for (size_t pi = 0; pi < vd.valueWidget->getChildCount(); ++pi)
                    {
                        View* child = vd.valueWidget->getChildAt(pi);
                        if (child == nullptr
                            || child->getBounds().contains(event.x, event.y) == false) continue;

                        if (item->getType() == MenuItemType::CATEGORY)
                        {
                            const auto& sub = item->getChildren();
                            if (pi < sub.size() && sub[pi] != nullptr && sub[pi]->isEnabled() == true)
                            {
                                item->setSelectedChildIndex(static_cast<int>(pi));
                                item->setValueInt(static_cast<int>(pi));
                                if (m_onValueChanged != nullptr) m_onValueChanged(item);
                                sub[pi]->activate();
                                updateContent(item);
                            }
                            tapped = true;
                        }
                        else if (item->getType() == MenuItemType::CHOICE)
                        {
                            item->setCurrentOptionIndex(static_cast<int>(pi));
                            if (m_onValueChanged != nullptr) m_onValueChanged(item);
                            updateContent(item);
                            tapped = true;
                        }
                        else if (item->getType() == MenuItemType::CHECKBOX)
                        {
                            CheckBox* cb = dynamic_cast<CheckBox*>(child);
                            const auto& sub = item->getChildren();
                            if (cb != nullptr && pi < sub.size()
                                && sub[pi] != nullptr && sub[pi]->isEnabled() == true)
                            {
                                item->setSelectedChildIndex(static_cast<int>(pi));
                                item->toggleChildChecked(static_cast<int>(pi));
                                updateContent(item);
                            }
                            tapped = true;
                        }
                        break;
                    }
                    if (tapped == false) moveOption(1);
                }
            }
            applyAllRowStyles();
            handled = true;
        }
    }

    if (handled == false) handled = LinearLayout::onMotionEvent(event);
    return handled;
}

void SubMenuView::onMeasure(MeasureSpec widthSpec, MeasureSpec heightSpec) { LinearLayout::onMeasure(widthSpec, heightSpec); }
void SubMenuView::layoutChildren()                                         { LinearLayout::layoutChildren(); }

void SubMenuView::onDraw(ICanvas& canvas)
{
    if (isVisible() == false) return;
    LinearLayout::onDraw(canvas);
}

void SubMenuView::update(float deltaTime)
{
    if (m_keyboardActive == false) return;
    const int idx = m_selRow;
    if (idx < 0 || idx >= static_cast<int>(m_rows.size()) || m_rows[idx] == nullptr) return;
    TextView* label = m_rows[idx]->viewData().nameLabel;
    if (label != nullptr) label->update(deltaTime);
}
// ============================================================================
// FileMenuView Implementation
// ============================================================================

FileMenuView::FileMenuView()
    : m_currentItem(nullptr)
    , m_mgr(nullptr)
    , m_focusZone(FocusZone::FILE_LIST)
    , m_selectedFileIndex(0)
    , m_selectedGroupIndex(0)
    , m_selectedChannelIndex(0)
    , m_selectedControlIndex(1)
    , m_fileScrollOffset(0)
    , m_groupFileCount(0)
    , m_totalFileCount(0)
    , m_selectedActionIndex(-1)
    , m_playBtnSlot(1)
    , m_fileDragging(false)
    , m_fileListMouseDown(false)
    , m_fileLoaded(false)
    , m_actionModeActive(false)
    , m_previewFullscreen(false)
    , m_fsHudVisible(false)
    , m_fileDragStartY(0.0f)
    , m_fileDragTotalY(0.0f)
    , m_fsHudTimer(0.0f)
    , m_fileDragStartOffset(0)
    , m_rootRow(nullptr)
    , m_leftBlock(nullptr)
    , m_rightBlock(nullptr)
    , m_controlBar(nullptr)
    , m_channelColumn(nullptr)
    , m_fileListColumn(nullptr)
    , m_groupRangeRow(nullptr)
    , m_groupCopyRow(nullptr)
    , m_logoImage(nullptr)
    , m_previewContainer(nullptr)
    , m_previewImage(nullptr)
    , m_previewFsButton(nullptr)
    , m_fsRootLayout(nullptr)
    , m_fsHud(nullptr)
    , m_timeLabel(nullptr)
    , m_subtitleContainer(nullptr)
    , m_subtitleLine1(nullptr)
    , m_subtitleLine2(nullptr)
    , m_channelLabel(nullptr)
    , m_groupListLabel(nullptr)
    , m_fileListLabel(nullptr)
    , m_groupRangeLabel(nullptr)
    , m_fsHudFileLabel(nullptr)
    , m_fsHudTimeLabel(nullptr)
    , m_fsHudVolumeSlider(nullptr)
    , m_groupRangePrev(nullptr)
    , m_groupRangeNext(nullptr)
    , m_groupCopyBtn(nullptr)
    , m_hudFocusContext(nullptr)
{
    buildLayout();
}

void FileMenuView::buildLayout()
{
    setBackgroundColor(Color::DarkSurface);
    getLayoutParams().width  = MATCH_PARENT;
    getLayoutParams().height = MATCH_PARENT;

    m_rootRow = new LinearLayout();
    m_rootRow->setOrientation(Orientation::HORIZONTAL);
    m_rootRow->setSpacing(10.0f);
    m_rootRow->getLayoutParams().width  = MATCH_PARENT;
    m_rootRow->getLayoutParams().height = MATCH_PARENT;
    m_rootRow->getLayoutParams().setPadding(8.0f);
    addView(m_rootRow);

    buildLeftBlock();
    buildRightBlock();
}

void FileMenuView::buildLeftBlock()
{
    m_leftBlock = new LinearLayout();
    m_leftBlock->setOrientation(Orientation::VERTICAL);
    m_leftBlock->setSpacing(4.0f);
    m_leftBlock->setBackgroundColor(Color::Transparent);
    m_leftBlock->getLayoutParams().width  = MATCH_PARENT;
    m_leftBlock->getLayoutParams().height = MATCH_PARENT;
    m_leftBlock->getLayoutParams().weight = 51.0f;
    m_rootRow->addView(m_leftBlock);

    // Logo panel
    FrameLayout* logoContainer = new FrameLayout();
    logoContainer->setBackgroundColor(Color::DarkBackground);
    logoContainer->setCornerRadius(20.0f);
    logoContainer->getLayoutParams().width  = MATCH_PARENT;
    logoContainer->getLayoutParams().height = 100;

    m_logoImage = new ImageView();
    m_logoImage->setScaleType(ScaleType::FIT_CENTER);
    m_logoImage->getLayoutParams().width  = MATCH_PARENT;
    m_logoImage->getLayoutParams().height = MATCH_PARENT;
    logoContainer->addView(m_logoImage);
    m_leftBlock->addView(logoContainer);

    // Preview container
    m_previewContainer = new FrameLayout();
    m_previewContainer->setBackgroundColor(Color::DarkBackground);
    m_previewContainer->setCornerRadius(20.0f);
    m_previewContainer->getLayoutParams().width  = MATCH_PARENT;
    m_previewContainer->getLayoutParams().height = MATCH_PARENT;
    m_previewContainer->getLayoutParams().weight = 1.0f;

    m_previewImage = new ImageView();
    m_previewImage->setCornerRadius(20.0f);
    m_previewImage->setScaleType(ScaleType::FIT_CENTER);
    m_previewImage->getLayoutParams().width  = MATCH_PARENT;
    m_previewImage->getLayoutParams().height = MATCH_PARENT;
    m_previewContainer->addView(m_previewImage);

    // Subtitle overlay — vertical container with two individually centered lines
    m_subtitleContainer = new LinearLayout();
    m_subtitleContainer->setOrientation(Orientation::VERTICAL);
    m_subtitleContainer->setGravity(Gravity::CENTER_HORIZONTAL);
    m_subtitleContainer->setSpacing(2.0f);
    m_subtitleContainer->setBackgroundColor(Color4::fromRGBA(0, 0, 0, 80));
    m_subtitleContainer->setCornerRadius(8.0f);
    m_subtitleContainer->getLayoutParams().width   = WRAP_CONTENT;
    m_subtitleContainer->getLayoutParams().height  = WRAP_CONTENT;
    m_subtitleContainer->getLayoutParams().gravity = Gravity::BOTTOM | Gravity::CENTER_HORIZONTAL;
    m_subtitleContainer->getLayoutParams().setPadding(12.0f, 4.0f, 12.0f, 4.0f);
    m_subtitleContainer->getLayoutParams().setMargin(0.0f, 0.0f, 0.0f, 12.0f);
    m_subtitleContainer->setVisibility(Visibility::GONE);

    auto makeSubtitleLine = [&]() -> TextView*
    {
        TextView* tv = new TextView();
        tv->setTextColor(Color::TextPrimary);
        tv->setTextSize(NORMAL_SUBTITLE_SIZE);
        tv->setTextGravity(Gravity::CENTER);
        tv->getLayoutParams().width  = WRAP_CONTENT;
        tv->getLayoutParams().height = WRAP_CONTENT;
        return tv;
    };
    m_subtitleLine1 = makeSubtitleLine();
    m_subtitleLine2 = makeSubtitleLine();
    m_subtitleContainer->addView(m_subtitleLine1);
    m_subtitleContainer->addView(m_subtitleLine2);
    m_previewContainer->addView(m_subtitleContainer);

    // Fullscreen toggle button
    m_previewFsButton = new Button();
    m_previewFsButton->setNormalColor(Color::CardBackground);
    m_previewFsButton->setHoverColor(Color::CardBackgroundHover);
    m_previewFsButton->setFocusedColor(Color::CardBackground);
    m_previewFsButton->setPressedColor(Color::CardBackgroundActive);
    m_previewFsButton->setCornerRadius(8.0f);
    m_previewFsButton->setBorderColor(Color::Transparent, 4.0f);
    m_previewFsButton->getLayoutParams().width   = 56;
    m_previewFsButton->getLayoutParams().height  = 56;
    m_previewFsButton->getLayoutParams().gravity = Gravity::TOP | Gravity::RIGHT;
    m_previewFsButton->getLayoutParams().setMargin(0.0f, 8.0f, 8.0f, 0.0f);
    m_previewFsButton->setFocusable(false);

    Texture *fsIcon = TextureManager::getInstance().getTextureByName("08_icn_fullscreen");
    if (fsIcon != nullptr)
    {
        m_previewFsButton->setTexture(fsIcon);
        m_previewFsButton->setImageSize(24.0f, 24.0f);
    }

    m_previewFsButton->setOnClickListener([this](View*) { togglePreviewFullscreen(); });
    m_previewContainer->addView(m_previewFsButton);

    buildFsHud();
    m_leftBlock->addView(m_previewContainer);

    buildControlBar();
    m_leftBlock->addView(m_controlBar);
}

void FileMenuView::buildControlBar()
{
    m_controlBar = new LinearLayout();
    m_controlBar->setOrientation(Orientation::HORIZONTAL);
    m_controlBar->setBackgroundColor(Color::DarkBackground);
    m_controlBar->setCornerRadius(20.0f);
    m_controlBar->setSpacing(24.0f);
    m_controlBar->getLayoutParams().width  = MATCH_PARENT;
    m_controlBar->getLayoutParams().height = 120;
    m_controlBar->getLayoutParams().setPadding(12.0f);

    m_timeLabel = new TextView();
    m_timeLabel->setText("00:00.00 / 00:00.00");
    m_timeLabel->setTextSize(60.0f);
    m_timeLabel->setTextColor(Color::TextPrimary);
    m_timeLabel->setTextGravity(Gravity::CENTER);
    m_timeLabel->getLayoutParams().width  = MATCH_PARENT;
    m_timeLabel->getLayoutParams().height = MATCH_PARENT;
    m_timeLabel->getLayoutParams().weight = 45.0f;
    m_controlBar->addView(m_timeLabel);

    struct CtrlDef { const char* texName; const char* id; };
    static constexpr CtrlDef kControls[] = {
        { "08_icn_ctrl_backward", "ctrl_bwd"  },
        { "08_icn_ctrl_play",     "ctrl_play" },
        { "08_icn_ctrl_stop",     "ctrl_stop" },
        { "08_icn_ctrl_forward",  "ctrl_fwd"  },
    };
    static constexpr int kControlCount = static_cast<int>(sizeof(kControls) / sizeof(kControls[0]));

    LinearLayout* btnGroup = new LinearLayout();
    btnGroup->setOrientation(Orientation::HORIZONTAL);
    btnGroup->setGravity(Gravity::CENTER_VERTICAL | Gravity::CENTER_HORIZONTAL);
    btnGroup->setSpacing(24.0f);
    btnGroup->getLayoutParams().width  = MATCH_PARENT;
    btnGroup->getLayoutParams().height = MATCH_PARENT;
    btnGroup->getLayoutParams().weight = 55.0f;
    m_controlBar->addView(btnGroup);

    TextureManager& texMgr = TextureManager::getInstance();
    for (int ci = 0; ci < kControlCount; ++ci)
    {
        Button* btn = new Button();
        Texture* tex = texMgr.getTextureByName(kControls[ci].texName);
        if (tex != nullptr) { btn->setTexture(tex); btn->setImageSize(100.0f, 100.0f); }
        btn->setContentGravity(Gravity::CENTER);
        btn->setNormalColor(Color::Transparent);
        btn->setHoverColor(Color::Transparent);
        btn->setPressedColor(Color::CardBackgroundActive);
        btn->setBorderColor(Color::Transparent, 8.0f);
        btn->setCornerRadius(45.0f);
        btn->getLayoutParams().width   = 90;
        btn->getLayoutParams().height  = 90;
        btn->getLayoutParams().gravity = Gravity::CENTER_VERTICAL;

        if (std::string(kControls[ci].id) == "ctrl_play") m_playBtnSlot = ci;

        const std::string ctrlId  = kControls[ci].id;
        const int         ctrlIdx = ci;
        btn->setOnClickListener([this, ctrlId, ctrlIdx](View*)
        {
            if (m_focusZone != FocusZone::CONTROL) setFocusZone(FocusZone::CONTROL);
            activateControl(ctrlIdx);
            if (m_currentItem != nullptr)
            {
                MenuItem* ctrlItem = m_currentItem->findChild("control").get();
                if (ctrlItem != nullptr)
                {
                    MenuItem* ctrl = ctrlItem->findChild(ctrlId).get();
                    if (ctrl != nullptr) ctrl->activate();
                }
            }
        });

        m_controlButtons.push_back(btn);
        btnGroup->addView(btn);
    }
}

void FileMenuView::buildFsHud()
{
    static constexpr float CTRL_BTN_SIZE  = 90.0f;
    static constexpr float CTRL_ICON_SIZE = 100.0f;
    static constexpr float CTRL_SPACING   = 24.0f;
    static constexpr float CH_BTN_W       = 60.0f;
    static constexpr float CH_BTN_H       = 60.0f;
    static constexpr float CH_SPACING_H   = 8.0f;
    static constexpr float HUD_PADDING    = 16.0f;
    static constexpr float VOL_SLIDER_W   = 250.0f;

    m_fsHud = new FrameLayout();
    m_fsHud->setBackgroundColor(Color4::fromRGBA(0, 0, 0, 180));
    m_fsHud->setCornerRadius(16.0f);
    m_fsHud->getLayoutParams().width   = MATCH_PARENT;
    m_fsHud->getLayoutParams().height  = static_cast<int>(FS_HUD_HEIGHT);
    m_fsHud->getLayoutParams().gravity = Gravity::BOTTOM | Gravity::CENTER_HORIZONTAL;
    m_fsHud->getLayoutParams().setMargin(16.0f, 0.0f, 16.0f, FS_HUD_MARGIN_BOT);
    m_fsHud->setVisibility(Visibility::GONE);

    // ── LEFT: vertical block — [file name] / [time  |  vol-bar  val-label] ──
    LinearLayout* leftGroup = new LinearLayout();
    leftGroup->setOrientation(Orientation::VERTICAL);
    leftGroup->setGravity(Gravity::CENTER_VERTICAL | Gravity::LEFT);
    leftGroup->setSpacing(8.0f);
    leftGroup->getLayoutParams().width   = WRAP_CONTENT;
    leftGroup->getLayoutParams().height  = WRAP_CONTENT;
    leftGroup->getLayoutParams().gravity = Gravity::LEFT | Gravity::CENTER_VERTICAL;
    leftGroup->getLayoutParams().setMargin(HUD_PADDING, 0.0f, 0.0f, 0.0f);

    // row 1 — file name
    m_fsHudFileLabel = new TextView();
    m_fsHudFileLabel->setText("");
    m_fsHudFileLabel->setTextSize(40.0f);
    m_fsHudFileLabel->setTextColor(Color::TextSecondary);
    m_fsHudFileLabel->setTextGravity(Gravity::CENTER_VERTICAL | Gravity::LEFT);
    m_fsHudFileLabel->getLayoutParams().width  = WRAP_CONTENT;
    m_fsHudFileLabel->getLayoutParams().height = WRAP_CONTENT;
    leftGroup->addView(m_fsHudFileLabel);

    // row 2 — [time label]  [vol bar]  [vol value]
    LinearLayout* timeVolRow = new LinearLayout();
    timeVolRow->setOrientation(Orientation::HORIZONTAL);
    timeVolRow->setGravity(Gravity::CENTER_VERTICAL | Gravity::LEFT);
    timeVolRow->setSpacing(40.0f);
    timeVolRow->getLayoutParams().width  = WRAP_CONTENT;
    timeVolRow->getLayoutParams().height = WRAP_CONTENT;

    m_fsHudTimeLabel = new TextView();
    m_fsHudTimeLabel->setText("00:00.00 / 00:00.00");
    m_fsHudTimeLabel->setTextSize(60.0f);
    m_fsHudTimeLabel->setTextColor(Color::TextPrimary);
    m_fsHudTimeLabel->setTextGravity(Gravity::CENTER_VERTICAL | Gravity::LEFT);
    m_fsHudTimeLabel->getLayoutParams().width  = WRAP_CONTENT;
    m_fsHudTimeLabel->getLayoutParams().height = WRAP_CONTENT;
    timeVolRow->addView(m_fsHudTimeLabel);

    // volume slider — bar + value label built-in, bar width is stable because
    // calcTrackRect reserves space for the widest label (max value), not current value
    m_fsHudVolumeSlider = new SliderView();
    m_fsHudVolumeSlider->setOrientation(Orientation::HORIZONTAL);
    m_fsHudVolumeSlider->setTextPosition(SliderView::TextPosition::AFTER);
    m_fsHudVolumeSlider->setTextGravity(Gravity::CENTER_VERTICAL);
    m_fsHudVolumeSlider->setRange(0.0f, 100.0f, 5.0f);
    m_fsHudVolumeSlider->setValue(100.0f);
    m_fsHudVolumeSlider->setValueFormat("%.0f%%");
    m_fsHudVolumeSlider->setTextSize(36.0f);
    m_fsHudVolumeSlider->setTextColor(Color::TextSecondary);
    m_fsHudVolumeSlider->setLabelSpacing(8.0f);
    m_fsHudVolumeSlider->setTrackThickness(12.0f);
    m_fsHudVolumeSlider->setThumbRadius(18.0f);
    m_fsHudVolumeSlider->setFocusable(true);
    m_fsHudVolumeSlider->setFocusManager(&m_hudFocusManager);
    m_fsHudVolumeSlider->getLayoutParams().width   = static_cast<int>(VOL_SLIDER_W);
    m_fsHudVolumeSlider->getLayoutParams().height  = WRAP_CONTENT;
    m_fsHudVolumeSlider->getLayoutParams().gravity = Gravity::CENTER_VERTICAL;
    m_fsHudVolumeSlider->setOnValueChanged([this](float value)
    {
        resetFsHudTimer();
        if (m_onVolumeChanged != nullptr) m_onVolumeChanged(value / 100.0f);
    });
    timeVolRow->addView(m_fsHudVolumeSlider);

    leftGroup->addView(timeVolRow);
    m_fsHud->addView(leftGroup);

    // ── CENTER: playback control buttons ─────────────────────────────────
    LinearLayout* ctrlGroup = new LinearLayout();
    ctrlGroup->setOrientation(Orientation::HORIZONTAL);
    ctrlGroup->setGravity(Gravity::CENTER);
    ctrlGroup->setSpacing(CTRL_SPACING);
    ctrlGroup->getLayoutParams().width   = WRAP_CONTENT;
    ctrlGroup->getLayoutParams().height  = MATCH_PARENT;
    ctrlGroup->getLayoutParams().gravity = Gravity::CENTER;

    struct CtrlDef { const char* texName; const char* id; };
    static constexpr CtrlDef kControls[] = {
        { "08_icn_ctrl_backward", "ctrl_bwd"  },
        { "08_icn_ctrl_play",     "ctrl_play" },
        { "08_icn_ctrl_stop",     "ctrl_stop" },
        { "08_icn_ctrl_forward",  "ctrl_fwd"  },
    };
    static constexpr int kControlCount = static_cast<int>(sizeof(kControls) / sizeof(kControls[0]));

    TextureManager& texMgr = TextureManager::getInstance();
    for (int ci = 0; ci < kControlCount; ++ci)
    {
        Button* btn = new Button();
        Texture* tex = texMgr.getTextureByName(kControls[ci].texName);
        if (tex != nullptr) { btn->setTexture(tex); btn->setImageSize(CTRL_ICON_SIZE, CTRL_ICON_SIZE); }
        btn->setContentGravity(Gravity::CENTER);
        btn->setNormalColor(Color::Transparent);
        btn->setHoverColor(Color::Transparent);
        btn->setPressedColor(Color::CardBackgroundActive);
        btn->setBorderColor(Color::Transparent, 8.0f);
        btn->setCornerRadius(CTRL_BTN_SIZE / 2.0f);
        btn->getLayoutParams().width   = static_cast<int>(CTRL_BTN_SIZE);
        btn->getLayoutParams().height  = static_cast<int>(CTRL_BTN_SIZE);
        btn->getLayoutParams().gravity = Gravity::CENTER_VERTICAL;

        const std::string ctrlId  = kControls[ci].id;
        const int         ctrlIdx = ci;
        btn->setOnClickListener([this, ctrlId, ctrlIdx](View*)
        {
            resetFsHudTimer();
            Button* target = (ctrlIdx < static_cast<int>(m_fsHudControlButtons.size()))
                             ? m_fsHudControlButtons[ctrlIdx] : nullptr;
            if (target != nullptr) m_hudFocusManager.setFocus(target);
            applyHudFocusVisuals(target);
            if (m_currentItem != nullptr)
            {
                MenuItem* ctrlItem = m_currentItem->findChild("control").get();
                if (ctrlItem != nullptr)
                {
                    MenuItem* ctrl = ctrlItem->findChild(ctrlId).get();
                    if (ctrl != nullptr) ctrl->activate();
                }
            }
            activateControl(ctrlIdx);
        });

        m_fsHudControlButtons.push_back(btn);
        ctrlGroup->addView(btn);
    }
    m_fsHud->addView(ctrlGroup);

    // ── RIGHT: channel buttons ────────────────────────────────────────────
    LinearLayout* chRow = new LinearLayout();
    chRow->setOrientation(Orientation::HORIZONTAL);
    chRow->setGravity(Gravity::CENTER_VERTICAL | Gravity::RIGHT);
    chRow->setSpacing(CH_SPACING_H);
    chRow->getLayoutParams().width   = WRAP_CONTENT;
    chRow->getLayoutParams().height  = WRAP_CONTENT;
    chRow->getLayoutParams().gravity = Gravity::RIGHT | Gravity::CENTER_VERTICAL;
    chRow->getLayoutParams().setMargin(0.0f, 0.0f, HUD_PADDING, 0.0f);

    struct ChDef { const char* label; int btnIdx; };
    static constexpr ChDef kChannels[] = {
        {"1",0},{"2",1},{"3",2},{"4",3},{"1-4",4},
        {"5",5},{"6",6},{"7",7},{"8",8},{"5-8",9},
    };
    static constexpr int kChCount = static_cast<int>(sizeof(kChannels) / sizeof(kChannels[0]));

    for (int ci = 0; ci < kChCount; ++ci)
    {
        Button* btn = new Button();
        btn->setText(kChannels[ci].label);
        btn->setTextSize(32.0f);
        btn->setTextColor(Color::TextDisabled);
        btn->setNormalColor(Color::CardBackground);
        btn->setHoverColor(Color::AccentPrimary);
        btn->setPressedColor(Color::AccentPrimary);
        btn->setFocusedColor(Color::CardBackground);
        btn->setEnabled(false);
        btn->setCornerRadius(12.0f);
        btn->setBorderColor(Color::Transparent, 4.0f);
        btn->getLayoutParams().width   = static_cast<int>(CH_BTN_W);
        btn->getLayoutParams().height  = static_cast<int>(CH_BTN_H);
        btn->getLayoutParams().gravity = Gravity::CENTER_VERTICAL;

        const int btnIdx = kChannels[ci].btnIdx;
        btn->setOnClickListener([this, btnIdx](View* v)
        {
            if (v->isEnabled() == false) return;
            resetFsHudTimer();
            if (m_currentItem != nullptr)
            {
                MenuItem* ch = m_currentItem->findChild("channel").get();
                if (ch != nullptr && btnIdx < static_cast<int>(ch->getChildren().size()))
                    ch->getChildren()[btnIdx]->activate();
            }
            refreshChannelHighlight(btnIdx);
        });

        m_fsHudChannelButtons.push_back(btn);
        chRow->addView(btn);
    }
    m_fsHud->addView(chRow);
    m_previewContainer->addView(m_fsHud);

    // ── Register all HUD buttons with the dedicated focus manager ─────────
    m_hudFocusContext = m_hudFocusManager.createContext(FocusContextType::NORMAL);
    m_hudFocusContext->setActive(true);
    m_hudFocusContext->setBlocking(false);
    m_hudFocusContext->setNavigationStrategy(NavigationStrategy::LINEAR);
    m_hudFocusContext->setWrapAround(true);

    for (Button* btn : m_fsHudControlButtons)
    {
        if (btn == nullptr) continue;
        btn->setFocusable(true);
        btn->setFocusManager(&m_hudFocusManager);
        m_hudFocusManager.registerView(btn, m_hudFocusContext);
    }
    for (Button* btn : m_fsHudChannelButtons)
    {
        if (btn == nullptr) continue;
        btn->setFocusable(true);
        btn->setFocusManager(&m_hudFocusManager);
        m_hudFocusManager.registerView(btn, m_hudFocusContext);
    }
    if (m_previewFsButton != nullptr)
    {
        m_previewFsButton->setFocusable(true);
        m_previewFsButton->setFocusManager(&m_hudFocusManager);
        m_hudFocusManager.registerView(m_previewFsButton, m_hudFocusContext);
    }
    if (m_fsHudVolumeSlider != nullptr)
        m_hudFocusManager.registerView(m_fsHudVolumeSlider, m_hudFocusContext);
}

void FileMenuView::buildRightBlock()
{
    m_rightBlock = new LinearLayout();
    m_rightBlock->setOrientation(Orientation::HORIZONTAL);
    m_rightBlock->setSpacing(8.0f);
    m_rightBlock->getLayoutParams().width  = MATCH_PARENT;
    m_rightBlock->getLayoutParams().height = MATCH_PARENT;
    m_rightBlock->getLayoutParams().weight = 45.0f;
    m_rootRow->addView(m_rightBlock);

    buildChannelColumn();
    buildFileListColumn();
}

void FileMenuView::buildChannelColumn()
{
    m_channelColumn = new LinearLayout();
    m_channelColumn->setOrientation(Orientation::VERTICAL);
    m_channelColumn->setSpacing(15.0f);
    m_channelColumn->setBackgroundColor(Color::DarkBackground);
    m_channelColumn->setCornerRadius(20.0f);
    m_channelColumn->setBorderColor(Color::Transparent, BORDER_W);
    m_channelColumn->getLayoutParams().width  = MATCH_PARENT;
    m_channelColumn->getLayoutParams().height = MATCH_PARENT;
    m_channelColumn->getLayoutParams().weight = 25.0f;
    m_channelColumn->getLayoutParams().setPadding(15.0f);
    m_rightBlock->addView(m_channelColumn);

    m_channelLabel = new TextView();
    m_channelLabel->setTextSize(48.0f);
    m_channelLabel->setTextColor(Color::TextPrimary);
    m_channelLabel->setTextGravity(Gravity::CENTER);
    m_channelLabel->getLayoutParams().width  = MATCH_PARENT;
    m_channelLabel->getLayoutParams().height = MATCH_PARENT;
    m_channelLabel->getLayoutParams().weight = 1.0f;
    m_channelColumn->addView(m_channelLabel);

    struct ChDef { const char* label; int btnIdx; };
    static constexpr ChDef kChannels[] = {
        {"1",0},{"2",1},{"3",2},{"4",3},{"1-4",4},
        {"5",5},{"6",6},{"7",7},{"8",8},{"5-8",9},
    };
    static constexpr int kChCount = static_cast<int>(sizeof(kChannels) / sizeof(kChannels[0]));

    for (int ci = 0; ci < kChCount; ++ci)
    {
        Button* btn = new Button();
        btn->setText(kChannels[ci].label);
        btn->setTextSize(48.0f);
        btn->setTextColor(Color::TextDisabled);
        btn->setNormalColor(Color::CardBackground);
        btn->setHoverColor(Color::AccentPrimary);
        btn->setPressedColor(Color::AccentPrimary);
        btn->setFocusedColor(Color::AccentPrimary);
        btn->setEnabled(false);
        btn->setCornerRadius(20.0f);
        btn->setBorderWidth(0.0f);
        btn->getLayoutParams().width  = MATCH_PARENT;
        btn->getLayoutParams().height = MATCH_PARENT;
        btn->getLayoutParams().weight = 1.0f;

        const int btnIdx = ci;
        btn->setOnClickListener([this, btnIdx](View* v)
        {
            if (v->isEnabled() == false) return;
            if (m_currentItem != nullptr)
            {
                MenuItem* ch = m_currentItem->findChild("channel").get();
                if (ch != nullptr && btnIdx < static_cast<int>(ch->getChildren().size()))
                    ch->getChildren()[btnIdx]->activate();
            }
            refreshChannelHighlight(btnIdx);
        });

        m_channelButtons.push_back(btn);
        m_channelColumn->addView(btn);
    }
    m_selectedChannelIndex = 0;
}

void FileMenuView::buildFileListColumn()
{
    m_fileListColumn = new LinearLayout();
    m_fileListColumn->setOrientation(Orientation::VERTICAL);
    m_fileListColumn->setSpacing(15.0f);
    m_fileListColumn->setBackgroundColor(Color::DarkBackground);
    m_fileListColumn->setCornerRadius(20.0f);
    m_fileListColumn->getLayoutParams().width  = MATCH_PARENT;
    m_fileListColumn->getLayoutParams().height = MATCH_PARENT;
    m_fileListColumn->getLayoutParams().weight = 75.0f;
    m_fileListColumn->getLayoutParams().setPadding(15.0f);
    m_rightBlock->addView(m_fileListColumn);

    // Group list label
    m_groupListLabel = new TextView();
    m_groupListLabel->setTextSize(48.0f);
    m_groupListLabel->setTextColor(Color::TextPrimary);
    m_groupListLabel->setTextGravity(Gravity::CENTER);
    m_groupListLabel->getLayoutParams().width  = MATCH_PARENT;
    m_groupListLabel->getLayoutParams().height = MATCH_PARENT;
    m_groupListLabel->getLayoutParams().weight = 1.0f;
    m_fileListColumn->addView(m_groupListLabel);

    // Group range row
    m_groupRangeRow = new LinearLayout();
    m_groupRangeRow->setOrientation(Orientation::HORIZONTAL);
    m_groupRangeRow->setBackgroundColor(Color::CardBackground);
    m_groupRangeRow->setCornerRadius(20.0f);
    m_groupRangeRow->setGravity(Gravity::CENTER_VERTICAL);
    m_groupRangeRow->getLayoutParams().width  = MATCH_PARENT;
    m_groupRangeRow->getLayoutParams().height = MATCH_PARENT;
    m_groupRangeRow->getLayoutParams().weight = 1.0f;
    m_groupRangeRow->getLayoutParams().setPadding(0.0f);

    auto makeNavBtn = [&](const char* texName) -> Button*
    {
        Button* btn = new Button();
        Texture* tex = TextureManager::getInstance().getTextureByName(texName);
        if (tex != nullptr) { btn->setTexture(tex); btn->setImageSize(48.0f, 48.0f); }
        btn->setNormalColor(Color::Transparent);
        btn->setHoverColor(Color::Transparent);
        btn->setPressedColor(Color::AccentPrimary);
        btn->setCornerRadius(20.0f);
        btn->getLayoutParams().width  = 70;
        btn->getLayoutParams().height = MATCH_PARENT;
        return btn;
    };

    m_groupRangePrev = makeNavBtn("08_icn_ctrl_left");
    m_groupRangePrev->setOnClickListener([this](View*)
    {
        if (m_currentItem == nullptr) return;
        MenuItem* gr = m_currentItem->findChild("group_range").get();
        if (gr != nullptr) { MenuItem* l = gr->findChild("grp_left").get(); if (l != nullptr) l->activate(); }
    });
    m_groupRangeRow->addView(m_groupRangePrev);

    m_groupRangeLabel = new TextView();
    m_groupRangeLabel->setText("-- ~ --  (- / -)");
    m_groupRangeLabel->setTextSize(48.0f);
    m_groupRangeLabel->setTextColor(Color::TextPrimary);
    m_groupRangeLabel->setTextGravity(Gravity::CENTER);
    m_groupRangeLabel->getLayoutParams().width  = MATCH_PARENT;
    m_groupRangeLabel->getLayoutParams().height = MATCH_PARENT;
    m_groupRangeLabel->getLayoutParams().weight = 1.0f;
    m_groupRangeRow->addView(m_groupRangeLabel);

    m_groupRangeNext = makeNavBtn("08_icn_ctrl_right");
    m_groupRangeNext->setOnClickListener([this](View*)
    {
        if (m_currentItem == nullptr) return;
        MenuItem* gr = m_currentItem->findChild("group_range").get();
        if (gr != nullptr) { MenuItem* r = gr->findChild("grp_right").get(); if (r != nullptr) r->activate(); }
    });
    m_groupRangeRow->addView(m_groupRangeNext);
    m_fileListColumn->addView(m_groupRangeRow);

    // Group copy row
    m_groupCopyRow = new LinearLayout();
    m_groupCopyRow->setOrientation(Orientation::HORIZONTAL);
    m_groupCopyRow->setBackgroundColor(Color::CardBackground);
    m_groupCopyRow->setCornerRadius(20.0f);
    m_groupCopyRow->getLayoutParams().width  = MATCH_PARENT;
    m_groupCopyRow->getLayoutParams().height = MATCH_PARENT;
    m_groupCopyRow->getLayoutParams().weight = 1.0f;
    m_groupCopyRow->getLayoutParams().setPadding(0.0f);

    m_groupCopyBtn = new Button();
    m_groupCopyBtn->setTextSize(48.0f);
    m_groupCopyBtn->setTextColor(Color::TextPrimary);
    m_groupCopyBtn->setNormalColor(Color::Transparent);
    m_groupCopyBtn->setHoverColor(Color::Transparent);
    m_groupCopyBtn->setPressedColor(Color::AccentPrimary);
    m_groupCopyBtn->setCornerRadius(20.0f);
    m_groupCopyBtn->getLayoutParams().width  = MATCH_PARENT;
    m_groupCopyBtn->getLayoutParams().height = MATCH_PARENT;
    m_groupCopyBtn->setOnClickListener([this](View*)
    {
        if (m_currentItem == nullptr) return;
        MenuItem* gc = m_currentItem->findChild("group_copy").get();
        if (gc != nullptr) { MenuItem* b = gc->findChild("group_copy_btn").get(); if (b != nullptr) b->activate(); }
    });
    m_groupCopyRow->addView(m_groupCopyBtn);
    m_fileListColumn->addView(m_groupCopyRow);

    // File list label
    m_fileListLabel = new TextView();
    m_fileListLabel->setTextSize(48.0f);
    m_fileListLabel->setTextColor(Color::TextPrimary);
    m_fileListLabel->setTextGravity(Gravity::CENTER);
    m_fileListLabel->getLayoutParams().width  = MATCH_PARENT;
    m_fileListLabel->getLayoutParams().height = MATCH_PARENT;
    m_fileListLabel->getLayoutParams().weight = 1.0f;
    m_fileListColumn->addView(m_fileListLabel);

    // File rows
    TextureManager& rowTexMgr = TextureManager::getInstance();
    Texture* texOpen   = rowTexMgr.getTextureByName("08_icn_ctrl_right");
    Texture* texCopy   = rowTexMgr.getTextureByName("08_icn_ctrl_copy");
    Texture* texDelete = rowTexMgr.getTextureByName("08_icn_ctrl_delete");

    auto makeIconBtn = [](Texture* tex, Color4 focusColor) -> Button*
    {
        Button* btn = new Button();
        if (tex != nullptr) { btn->setTexture(tex); btn->setImageSize(40.0f, 40.0f); }
        btn->setContentGravity(Gravity::CENTER);
        btn->setNormalColor(Color::Transparent);
        btn->setHoverColor(Color::CardBackgroundHover);
        btn->setPressedColor(Color::CardBackgroundActive);
        btn->setFocusedColor(focusColor);
        btn->setCornerRadius(10.0f);
        btn->getLayoutParams().width   = 48;
        btn->getLayoutParams().height  = 48;
        btn->getLayoutParams().gravity = Gravity::CENTER_VERTICAL;
        btn->setVisibility(Visibility::GONE);
        return btn;
    };

    for (int fi = 0; fi < MAX_VISIBLE_FILES; ++fi)
    {
        FileRowActions fra;
        const int      fIdx = fi;

        fra.row = new LinearLayout();
        fra.row->setOrientation(Orientation::HORIZONTAL);
        fra.row->setSpacing(0.0f);
        fra.row->setBackgroundColor(Color::CardBackground);
        fra.row->setCornerRadius(20.0f);
        fra.row->setBorderWidth(0.0f);
        fra.row->getLayoutParams().width  = MATCH_PARENT;
        fra.row->getLayoutParams().height = MATCH_PARENT;
        fra.row->getLayoutParams().weight = 1.0f;
        fra.row->getLayoutParams().setPadding(0.0f);
        fra.row->setVisibility(Visibility::INVISIBLE);

        fra.nameLabel = new TextView();
        fra.nameLabel->setText("");
        fra.nameLabel->setTextSize(38.0f);
        fra.nameLabel->setTextColor(Color::TextPrimary);
        fra.nameLabel->setTextGravity(Gravity::LEFT | Gravity::CENTER_VERTICAL);
        fra.nameLabel->setMarquee(true);
        fra.nameLabel->getLayoutParams().width  = MATCH_PARENT;
        fra.nameLabel->getLayoutParams().height = MATCH_PARENT;
        fra.nameLabel->getLayoutParams().weight = 1.0f;
        fra.nameLabel->getLayoutParams().setPadding(15.0f, 0.0f);
        fra.row->addView(fra.nameLabel);

        fra.actionPanel = new LinearLayout();
        fra.actionPanel->setOrientation(Orientation::HORIZONTAL);
        fra.actionPanel->setGravity(Gravity::CENTER_VERTICAL | Gravity::RIGHT);
        fra.actionPanel->setSpacing(6.0f);
        fra.actionPanel->getLayoutParams().width  = WRAP_CONTENT;
        fra.actionPanel->getLayoutParams().height = MATCH_PARENT;
        fra.actionPanel->getLayoutParams().setPadding(8.0f, 0.0f);
        fra.row->addView(fra.actionPanel);

        fra.actionLabel = new TextView();
        fra.actionLabel->setText("");
        fra.actionLabel->setTextSize(40.0f);
        fra.actionLabel->setTextColor(Color::TextSecondary);
        fra.actionLabel->setTextGravity(Gravity::CENTER);
        fra.actionLabel->getLayoutParams().width  = WRAP_CONTENT;
        fra.actionLabel->getLayoutParams().height = MATCH_PARENT;
        fra.actionPanel->addView(fra.actionLabel);

        fra.openBtn = makeIconBtn(texOpen, Color::CardBackgroundActive);
        fra.openBtn->setOnClickListener([this, fIdx](View*)
        {
            int actualIdx = m_fileScrollOffset + fIdx;
            if (actualIdx != m_selectedFileIndex)
            {
                hideActionButtons(m_selectedFileIndex - m_fileScrollOffset);
                m_selectedFileIndex   = actualIdx;
                m_selectedActionIndex = 0;
                setSelectedFileIndex(m_selectedFileIndex);
            }
            else m_selectedActionIndex = 0;
            highlightActionButtons(fIdx);
            activateSelectedFile();
        });
        fra.actionPanel->addView(fra.openBtn);

        fra.copyBtn = makeIconBtn(texCopy, Color::CardBackgroundActive);
        fra.copyBtn->setOnClickListener([this, fIdx](View*)
        {
            int actualIdx = m_fileScrollOffset + fIdx;
            if (actualIdx != m_selectedFileIndex)
            {
                hideActionButtons(m_selectedFileIndex - m_fileScrollOffset);
                m_selectedFileIndex   = actualIdx;
                m_selectedActionIndex = 1;
                setSelectedFileIndex(m_selectedFileIndex);
            }
            else m_selectedActionIndex = 1;
            highlightActionButtons(fIdx);
            if (m_onFileCopy != nullptr) m_onFileCopy(m_selectedGroupIndex, actualIdx);
        });
        fra.actionPanel->addView(fra.copyBtn);

        fra.deleteBtn = makeIconBtn(texDelete, Color::CardBackgroundActive);
        fra.deleteBtn->setOnClickListener([this, fIdx](View*)
        {
            int actualIdx = m_fileScrollOffset + fIdx;
            if (actualIdx != m_selectedFileIndex)
            {
                hideActionButtons(m_selectedFileIndex - m_fileScrollOffset);
                m_selectedFileIndex   = actualIdx;
                m_selectedActionIndex = 2;
                setSelectedFileIndex(m_selectedFileIndex);
            }
            else m_selectedActionIndex = 2;
            highlightActionButtons(fIdx);
            if (m_onFileDelete != nullptr) m_onFileDelete(m_selectedGroupIndex, actualIdx);
        });
        fra.actionPanel->addView(fra.deleteBtn);

        m_fileRowActions.push_back(fra);
        m_fileListColumn->addView(fra.row);
    }
}

// ============================================================================
// FileMenuView – HUD helpers
// ============================================================================

void FileMenuView::showFsHud()
{
    if (m_fsHud == nullptr) return;
    refreshFsHudFileLabel();
    m_fsHud->setVisibility(Visibility::VISIBLE);
    m_fsHudVisible = true;
    m_fsHudTimer   = FS_HUD_TIMEOUT;
    updateSubtitleMargin();
}

void FileMenuView::hideFsHud()
{
    if (m_fsHud == nullptr) return;
    m_fsHud->setVisibility(Visibility::GONE);
    m_fsHudVisible = false;
    updateSubtitleMargin();
}

void FileMenuView::resetFsHudTimer()
{
    m_fsHudTimer = FS_HUD_TIMEOUT;
    if (m_previewFullscreen == true && m_fsHudVisible == false) showFsHud();
}

void FileMenuView::updateSubtitleMargin()
{
    if (m_subtitleContainer == nullptr) return;
    float botMargin = m_fsHudVisible ? (FS_HUD_HEIGHT + FS_HUD_MARGIN_BOT + 8.0f) : 12.0f;
    m_subtitleContainer->getLayoutParams().setMargin(0.0f, 0.0f, 0.0f, botMargin);
}

void FileMenuView::refreshFsHudFileLabel()
{
    if (m_fsHudFileLabel == nullptr) return;
    VPB::GroupInfo* group = (m_mgr != nullptr) ? m_mgr->getGroupAt(m_selectedGroupIndex) : nullptr;
    if (group == nullptr) { m_fsHudFileLabel->setText(""); return; }
    int globalIdx = group->startIndex + m_selectedFileIndex;
    VPB::FileInfo* file = m_mgr->getFileAt(globalIdx);
    m_fsHudFileLabel->setText((file != nullptr) ? file->name : "");
}

void FileMenuView::applyHudFocusVisuals(View* focused)
{
    for (Button* btn : m_fsHudControlButtons)
    {
        if (btn == nullptr) continue;
        btn->setBorderColor(btn == focused ? Color::BorderFocus : Color::Transparent, 8.0f);
    }
    for (int i = 0; i < static_cast<int>(m_fsHudChannelButtons.size()); ++i)
    {
        Button* btn = m_fsHudChannelButtons[i];
        if (btn == nullptr) continue;
        bool isSelected = (i == m_selectedChannelIndex);
        bool isFocused  = (btn == focused);
        bool isAvail    = (i < static_cast<int>(m_channelAvailable.size()) && m_channelAvailable[i] == true);
        btn->setBorderColor(isFocused && isSelected == false && isAvail ? Color::BorderFocus : Color::Transparent, 4.0f);
    }
    if (m_previewFsButton != nullptr)
        m_previewFsButton->setBorderColor(m_previewFsButton == focused ? Color::BorderFocus : Color::Transparent, 4.0f);
}

void FileMenuView::activateFocusedHudButton()
{
    Button* btn = dynamic_cast<Button*>(m_hudFocusManager.getFocusedView());
    if (btn != nullptr) btn->performClick();
}

// ============================================================================
// FileMenuView – public API
// ============================================================================

void FileMenuView::setMenuItem(MenuItem* item)
{
    m_currentItem = item;
    if (item == nullptr) return;

    MenuItem* grpCopy = item->findChild("group_copy").get();
    if (grpCopy != nullptr)
    {
        MenuItem* btn = grpCopy->findChild("group_copy_btn").get();
        if (btn != nullptr && m_groupCopyBtn != nullptr)
            m_groupCopyBtn->setText(btn->getName());
    }

    MenuItem* groupListItem = item->findChild("grp_list").get();
    if (groupListItem != nullptr && m_groupListLabel != nullptr)
        m_groupListLabel->setText(groupListItem->getName());

    MenuItem* ctrlItem = item->findChild("control").get();
    if (ctrlItem != nullptr)
    {
        const auto& children = ctrlItem->getChildren();
        for (size_t ci = 0; ci < m_controlButtons.size() && ci < children.size(); ++ci)
            m_controlButtons[ci]->setText(children[ci]->getName());
        m_selectedControlIndex = 1;
        for (int ci = 0; ci < static_cast<int>(m_controlButtons.size()); ++ci)
        {
            m_controlButtons[ci]->setNormalColor(Color::Transparent);
            m_controlButtons[ci]->setBorderColor(Color::Transparent, BORDER_W);
        }
    }

    MenuItem* chanItem = item->findChild("channel").get();
    if (chanItem != nullptr)
    {
        if (m_channelLabel != nullptr) m_channelLabel->setText(chanItem->getName());
        const auto& chChildren = chanItem->getChildren();
        for (size_t ci = 0; ci < m_channelButtons.size() && ci < chChildren.size(); ++ci)
            m_channelButtons[ci]->setText(chChildren[ci]->getName());
        m_selectedChannelIndex = 0;
        m_channelAvailable.assign(m_channelButtons.size(), false);
        refreshChannelHighlight(0);
    }

    MenuItem* fileListItem = item->findChild("file_list").get();
    if (fileListItem != nullptr && m_fileListLabel != nullptr)
        m_fileListLabel->setText(fileListItem->getName());

    // Reset state
    m_selectedFileIndex    = 0;
    m_selectedGroupIndex   = 0;
    m_fileScrollOffset     = 0;
    m_groupFileCount       = 0;
    m_totalFileCount       = 0;
    m_mgr                  = nullptr;
    m_fileDragging         = false;
    m_fileListMouseDown    = false;
    m_fileDragStartY       = 0.0f;
    m_fileDragStartOffset  = 0;
    m_fileDragTotalY       = 0.0f;
    m_selectedControlIndex = 1;
    m_selectedActionIndex  = -1;
    m_fileLoaded           = false;
    m_actionModeActive     = false;
    for (int s = 0; s < MAX_VISIBLE_FILES; ++s)
    {
        hideActionButtons(s);
        if (s < static_cast<int>(m_fileRowActions.size()))
        {
            FileRowActions& fra = m_fileRowActions[s];
            if (fra.actionLabel != nullptr) fra.actionLabel->setText("");
            if (fra.nameLabel   != nullptr) fra.nameLabel->setText("");
            if (fra.row         != nullptr) fra.row->setVisibility(Visibility::INVISIBLE);
        }
    }
    m_focusZone = FocusZone::GROUP_RANGE;
    highlightZone(m_focusZone);
    refreshChannelHighlight(0);
}

void FileMenuView::populateFileList(VPB::FileManager* mgr, int groupIdx, int groupCount, int fileCount)
{
    if (mgr == nullptr || m_fileListColumn == nullptr) return;

    m_selectedGroupIndex = groupIdx;
    VPB::GroupInfo* group = mgr->getGroupAt(groupIdx);

    if (group != nullptr)
    {
        char buf[128];
        snprintf(buf, sizeof(buf), "%d ~ %d  (%d / %d)",
                 group->startIndex + 1, group->getEndIndex() + 1, groupIdx + 1, groupCount);
        setGroupRangeLabel(std::string(buf));
    }
    else
    {
        setGroupRangeLabel("-- ~ --  (- / -)");
    }

    if (group != nullptr)
    {
        m_groupFileCount = group->count;
        if (m_selectedFileIndex >= group->count) m_selectedFileIndex = 0;
        m_fileScrollOffset = m_selectedFileIndex - (MAX_VISIBLE_FILES - 1);
        if (m_fileScrollOffset < 0) m_fileScrollOffset = 0;
    }
    else
    {
        m_groupFileCount = 0;
        for (auto& fra : m_fileRowActions)
            if (fra.row != nullptr) fra.row->setVisibility(Visibility::INVISIBLE);
    }

    m_mgr            = mgr;
    m_totalFileCount = fileCount;
    refreshFileListSlots();
}

void FileMenuView::setGroupRangeLabel(const std::string& text)
{
    if (m_groupRangeLabel != nullptr) m_groupRangeLabel->setText(text);
}

void FileMenuView::setPlaybackTime(const std::string& text)
{
    if (m_timeLabel      != nullptr) m_timeLabel->setText(text);
    if (m_fsHudTimeLabel != nullptr) m_fsHudTimeLabel->setText(text);
}

void FileMenuView::setVolume(float volume)
{
    if (m_fsHudVolumeSlider != nullptr) m_fsHudVolumeSlider->setValue(volume * 100.0f);
}

void FileMenuView::setPlaybackState(VPB::PlaybackState state)
{
    const char* texName = (state == VPB::PlaybackState::PLAYING) ? "08_icn_ctrl_pause" : "08_icn_ctrl_play";
    Texture* tex = TextureManager::getInstance().getTextureByName(texName);

    if (m_playBtnSlot >= 0 && m_playBtnSlot < static_cast<int>(m_controlButtons.size()))
    {
        Button* playBtn = m_controlButtons[m_playBtnSlot];
        if (playBtn != nullptr && tex != nullptr) playBtn->setTexture(tex);
    }
    if (m_playBtnSlot >= 0 && m_playBtnSlot < static_cast<int>(m_fsHudControlButtons.size()))
    {
        Button* hudPlayBtn = m_fsHudControlButtons[m_playBtnSlot];
        if (hudPlayBtn != nullptr && tex != nullptr) hudPlayBtn->setTexture(tex);
    }
}

void FileMenuView::setSubtitleLines(const std::string& line1, const std::string& line2)
{
    if (m_subtitleContainer == nullptr) return;

    const bool hasLine1 = (line1.empty() == false);
    const bool hasLine2 = (line2.empty() == false);

    if (hasLine1 == false && hasLine2 == false)
    {
        m_subtitleContainer->setVisibility(Visibility::GONE);
        return;
    }

    if (m_subtitleLine1 != nullptr)
    {
        m_subtitleLine1->setText(line1);
        m_subtitleLine1->setVisibility(hasLine1 ? Visibility::VISIBLE : Visibility::GONE);
    }
    if (m_subtitleLine2 != nullptr)
    {
        m_subtitleLine2->setText(line2);
        m_subtitleLine2->setVisibility(hasLine2 ? Visibility::VISIBLE : Visibility::GONE);
    }
    m_subtitleContainer->setVisibility(Visibility::VISIBLE);
}

void FileMenuView::setFileLoaded(bool loaded)
{
    m_fileLoaded = loaded;
    if (loaded == false)
    {
        m_channelAvailable.assign(m_channelButtons.size(), false);
        refreshChannelHighlight(0);
    }
}

void FileMenuView::onFileOpened(const std::vector<bool>& channelAvailable)
{
    setChannelAvailability(channelAvailable);

    int firstAvail = 0;
    for (int i = 0; i < static_cast<int>(m_channelAvailable.size()); ++i)
        if (m_channelAvailable[i] == true) { firstAvail = i; break; }
    refreshChannelHighlight(firstAvail);

    m_fileLoaded = true;
    hideActionButtons(m_selectedFileIndex - m_fileScrollOffset);
    m_selectedActionIndex = -1;

    m_focusZone = FocusZone::CHANNEL;
    highlightZone(m_focusZone);
    refreshChannelHighlight(m_selectedChannelIndex);
}

void FileMenuView::refreshChannelHighlight(int channelIdx)
{
    m_selectedChannelIndex = channelIdx;
    bool channelFocused = (m_focusZone == FocusZone::CHANNEL);

    for (int i = 0; i < static_cast<int>(m_channelButtons.size()); ++i)
    {
        bool isSelected  = (i == channelIdx);
        bool isAvailable = (i >= static_cast<int>(m_channelAvailable.size()) || m_channelAvailable[i] == true);

        if (isSelected == false && m_channelButtons[i]->isHovered() == true)
            m_channelButtons[i]->setHovered(false);

        m_channelButtons[i]->setEnabled(isAvailable);
        m_channelButtons[i]->setNormalColor(
            !isAvailable ? Color::CardBackground.withAlpha(0.5f)
                         : isSelected ? Color::AccentPrimary
                                      : Color::CardBackground);
        m_channelButtons[i]->setTextColor(
            !isAvailable ? Color::TextDisabled
                         : isSelected     ? Color::TextPrimary
                         : channelFocused ? Color::TextSecondary
                                          : Color::TextDisabled);

        if (i < static_cast<int>(m_fsHudChannelButtons.size()) && m_fsHudChannelButtons[i] != nullptr)
        {
            m_fsHudChannelButtons[i]->setEnabled(isAvailable);
            m_fsHudChannelButtons[i]->setNormalColor(
                !isAvailable ? Color::CardBackground.withOpacity(0.15f)
                             : isSelected ? Color::AccentPrimary
                                          : Color::CardBackground);
            m_fsHudChannelButtons[i]->setTextColor(
                !isAvailable ? Color::TextDisabled
                             : isSelected ? Color::TextPrimary
                                          : Color::TextSecondary);
        }
    }

    if (m_previewFullscreen == true)
        applyHudFocusVisuals(m_hudFocusManager.getFocusedView());
}

void FileMenuView::setChannelAvailability(const std::vector<bool>& available)
{
    m_channelAvailable = available;
    refreshChannelHighlight(m_selectedChannelIndex);
}

void FileMenuView::setSelectedFileIndex(int index)
{
    int oldSlot = m_selectedFileIndex - m_fileScrollOffset;
    if (oldSlot >= 0 && oldSlot < MAX_VISIBLE_FILES) hideActionButtons(oldSlot);

    m_selectedFileIndex   = index;
    m_actionModeActive    = false;
    m_selectedActionIndex = -1;
    if (m_previewFullscreen == true && m_fsHudVisible == true)
        refreshFsHudFileLabel();

    if (m_mgr != nullptr) { refreshFileListSlots(); return; }

    bool showFileHighlight = (m_focusZone == FocusZone::FILE_LIST
                           || m_focusZone == FocusZone::CHANNEL
                           || m_focusZone == FocusZone::CONTROL
                           || m_focusZone == FocusZone::PREVIEW);

    for (int slot = 0; slot < MAX_VISIBLE_FILES; ++slot)
    {
        if (slot >= static_cast<int>(m_fileRowActions.size())) break;
        FileRowActions& fra = m_fileRowActions[slot];
        if (fra.row == nullptr || fra.row->getVisibility() != Visibility::VISIBLE) continue;

        int  fileLocalIdx = m_fileScrollOffset + slot;
        bool isSelected   = (fileLocalIdx == m_selectedFileIndex);

        fra.row->setBackgroundColor((isSelected && showFileHighlight) ? Color::AccentPrimary : Color::CardBackground);
        if (fra.nameLabel != nullptr)
            fra.nameLabel->setTextColor(isSelected ? Color::TextPrimary : Color::TextSecondary);
        if (fra.actionLabel != nullptr && fra.isDirectory == true)
            fra.actionLabel->setTextColor(isSelected ? Color::TextPrimary : Color::TextSecondary);

        if (isSelected == true && fra.isDirectory == false && m_focusZone == FocusZone::FILE_LIST)
        { m_selectedActionIndex = 0; highlightActionButtons(slot); }
        else
            hideActionButtons(slot);
    }
}

void FileMenuView::setFocusZone(FocusZone zone)
{
    if (m_fileLoaded == false && (zone == FocusZone::CHANNEL || zone == FocusZone::CONTROL)) return;

    if (m_focusZone == FocusZone::FILE_LIST && zone != FocusZone::FILE_LIST)
    {
        hideActionButtons(m_selectedFileIndex - m_fileScrollOffset);
        m_selectedActionIndex = -1;
    }

    m_focusZone = zone;
    highlightZone(zone);
    refreshChannelHighlight(m_selectedChannelIndex);
    refreshFileListSlots();

    if (m_previewFsButton != nullptr)
    {
        if (zone == FocusZone::PREVIEW)
            m_previewFsButton->setBorderColor(Color::BorderFocus, 4.0f);
        else if (m_previewFullscreen == false)
            m_previewFsButton->setBorderColor(Color::Transparent, 4.0f);
    }
}

void FileMenuView::setContentFocused(bool focused)
{
    if (focused == true)
    {
        highlightZone(m_focusZone);
        refreshChannelHighlight(m_selectedChannelIndex);
        refreshFileListSlots();
        if (m_previewFsButton != nullptr)
            m_previewFsButton->setBorderColor(
                (m_focusZone == FocusZone::PREVIEW || m_previewFullscreen == true)
                    ? Color::BorderFocus : Color::Transparent,
                4.0f);
        return;
    }

    if (m_fileListColumn  != nullptr) m_fileListColumn->setBorderColor(Color::Transparent, BORDER_W);
    if (m_channelColumn   != nullptr) m_channelColumn->setBorderColor(Color::Transparent, BORDER_W);
    if (m_previewFsButton != nullptr && m_previewFullscreen == false)
        m_previewFsButton->setBorderColor(Color::Transparent, 4.0f);
    if (m_groupRangeRow   != nullptr) m_groupRangeRow->setBackgroundColor(Color::CardBackground);
    if (m_groupRangeLabel != nullptr) m_groupRangeLabel->setTextColor(Color::TextDisabled);
    if (m_groupRangePrev  != nullptr) m_groupRangePrev->setTextColor(Color::TextDisabled);
    if (m_groupRangeNext  != nullptr) m_groupRangeNext->setTextColor(Color::TextDisabled);
    if (m_groupCopyRow    != nullptr) m_groupCopyRow->setBackgroundColor(Color::CardBackground);
    if (m_groupCopyBtn    != nullptr) m_groupCopyBtn->setTextColor(Color::TextDisabled);
    if (m_timeLabel       != nullptr) m_timeLabel->setTextColor(Color::TextDisabled);

    for (int slot = 0; slot < MAX_VISIBLE_FILES; ++slot)
    {
        if (slot >= static_cast<int>(m_fileRowActions.size())) break;
        FileRowActions& fra = m_fileRowActions[slot];
        if (fra.row == nullptr || fra.row->getVisibility() != Visibility::VISIBLE) continue;
        fra.row->setBackgroundColor(Color::CardBackground);
        if (fra.nameLabel   != nullptr) fra.nameLabel->setTextColor(Color::TextDisabled);
        if (fra.actionLabel != nullptr && fra.isDirectory == true)
            fra.actionLabel->setTextColor(Color::TextDisabled);
    }
    for (Button* btn : m_channelButtons)
    {
        btn->setNormalColor(Color::CardBackground);
        btn->setTextColor(Color::TextDisabled);
    }
    for (Button* btn : m_controlButtons)
    {
        btn->setNormalColor(Color::Transparent);
        btn->setBorderColor(Color::Transparent, BORDER_W);
        btn->setTextColor(Color::TextDisabled);
    }
}

// ============================================================================
// FileMenuView – zone / highlight helpers
// ============================================================================

void FileMenuView::highlightZone(FocusZone zone)
{
    auto resetBorder = [&](LinearLayout* l) { if (l != nullptr) l->setBorderColor(Color::Transparent, BORDER_W); };
    auto focusBorder = [&](LinearLayout* l) { if (l != nullptr) l->setBorderColor(Color::BorderFocus,  BORDER_W); };

    resetBorder(m_fileListColumn);
    resetBorder(m_channelColumn);

    bool fileZone = (zone == FocusZone::GROUP_RANGE || zone == FocusZone::GROUP_COPY || zone == FocusZone::FILE_LIST);

    if (m_groupRangeRow   != nullptr) m_groupRangeRow->setBackgroundColor(zone == FocusZone::GROUP_RANGE ? Color::AccentPrimary : Color::CardBackground);
    if (m_groupCopyRow    != nullptr) m_groupCopyRow->setBackgroundColor(zone == FocusZone::GROUP_COPY   ? Color::AccentPrimary : Color::CardBackground);
    if (m_groupRangeLabel != nullptr) m_groupRangeLabel->setTextColor(fileZone ? Color::TextPrimary : Color::TextSecondary);
    if (m_groupRangePrev  != nullptr) m_groupRangePrev->setTextColor(zone == FocusZone::GROUP_RANGE ? Color::TextPrimary : Color::TextSecondary);
    if (m_groupRangeNext  != nullptr) m_groupRangeNext->setTextColor(zone == FocusZone::GROUP_RANGE ? Color::TextPrimary : Color::TextSecondary);
    if (m_groupCopyBtn    != nullptr) m_groupCopyBtn->setTextColor(zone == FocusZone::GROUP_COPY    ? Color::TextPrimary : Color::TextSecondary);

    bool isCtrlZone = (zone == FocusZone::CONTROL);
    for (int ci = 0; ci < static_cast<int>(m_controlButtons.size()); ++ci)
    {
        bool isSel = (ci == m_selectedControlIndex);
        m_controlButtons[ci]->setNormalColor(Color::Transparent);
        m_controlButtons[ci]->setBorderColor(isSel && isCtrlZone ? Color::BorderFocus : Color::Transparent, BORDER_W);
        m_controlButtons[ci]->setTextColor(isSel && isCtrlZone   ? Color::TextPrimary : Color::TextSecondary);
    }
    if (m_timeLabel != nullptr) m_timeLabel->setTextColor(isCtrlZone ? Color::TextPrimary : Color::TextSecondary);

    switch (zone)
    {
        case FocusZone::GROUP_RANGE:
        case FocusZone::GROUP_COPY:
        case FocusZone::FILE_LIST:  focusBorder(m_fileListColumn); break;
        case FocusZone::CHANNEL:    focusBorder(m_channelColumn);  break;
        case FocusZone::CONTROL:
        case FocusZone::PREVIEW:                                   break;
    }
}

void FileMenuView::hideActionButtons(int slot)
{
    if (slot < 0 || slot >= static_cast<int>(m_fileRowActions.size())) return;
    FileRowActions& fra = m_fileRowActions[slot];
    auto hide = [](Button* btn)
    {
        if (btn == nullptr) return;
        btn->setNormalColor(Color::Transparent);
        btn->setVisibility(Visibility::GONE);
    };
    hide(fra.openBtn);
    hide(fra.copyBtn);
    hide(fra.deleteBtn);
}

void FileMenuView::highlightActionButtons(int slot)
{
    if (slot < 0 || slot >= static_cast<int>(m_fileRowActions.size())) return;
    FileRowActions& fra = m_fileRowActions[slot];
    if (fra.isDirectory == true) return;

    if (fra.openBtn   != nullptr) fra.openBtn->setVisibility(Visibility::VISIBLE);
    if (fra.copyBtn   != nullptr) fra.copyBtn->setVisibility(Visibility::VISIBLE);
    if (fra.deleteBtn != nullptr) fra.deleteBtn->setVisibility(Visibility::VISIBLE);

    auto setFocus = [&](Button* btn, bool focused)
    {
        if (btn != nullptr) btn->setNormalColor(focused ? Color::CardBackgroundActive : Color::Transparent);
    };
    setFocus(fra.openBtn,   m_selectedActionIndex == 0);
    setFocus(fra.copyBtn,   m_selectedActionIndex == 1);
    setFocus(fra.deleteBtn, m_selectedActionIndex == 2);
}

void FileMenuView::refreshFileListSlots()
{
    VPB::GroupInfo* group = (m_mgr != nullptr) ? m_mgr->getGroupAt(m_selectedGroupIndex) : nullptr;

    bool showFileHighlight = (m_focusZone == FocusZone::FILE_LIST
                           || m_focusZone == FocusZone::CHANNEL
                           || m_focusZone == FocusZone::CONTROL
                           || m_focusZone == FocusZone::PREVIEW);

    for (int slot = 0; slot < MAX_VISIBLE_FILES; ++slot)
    {
        if (slot >= static_cast<int>(m_fileRowActions.size())) break;
        FileRowActions& fra = m_fileRowActions[slot];
        if (fra.row == nullptr) continue;

        int  fileLocalIdx = m_fileScrollOffset + slot;
        bool hasFile      = (group != nullptr && fileLocalIdx < group->count);

        if (hasFile == false)
        {
            fra.isDirectory = false;
            fra.row->setVisibility(Visibility::INVISIBLE);
            hideActionButtons(slot);
            continue;
        }

        int globalIdx = group->startIndex + fileLocalIdx;
        VPB::FileInfo* file = m_mgr->getFileAt(globalIdx);
        if (file == nullptr)
        {
            fra.isDirectory = false;
            fra.row->setVisibility(Visibility::INVISIBLE);
            hideActionButtons(slot);
            continue;
        }

        fra.isDirectory = file->isDirectory;
        bool isSelected = (fileLocalIdx == m_selectedFileIndex);

        bool        isParent    = file->isParentDir();
        std::string displayName = isParent ? "\u2B06" : file->name;
        std::string label       = "[" + std::to_string(globalIdx + 1) + "/" + std::to_string(m_totalFileCount) + "]  " + displayName;
        if (fra.nameLabel != nullptr) { fra.nameLabel->setText(label); fra.nameLabel->setTextColor(isParent ? Color::AccentWarning : (isSelected ? Color::TextPrimary : Color::TextSecondary)); }

        if (file->isDirectory == true)
        {
            std::string actionText = isParent == true && m_mgr != nullptr ? m_mgr->getCurrentDirectory() : "[DIR]";
            if (fra.actionLabel != nullptr) { fra.actionLabel->setText(actionText); fra.actionLabel->setTextColor(isParent ? Color::AccentWarning : (isSelected ? Color::TextPrimary : Color::TextSecondary)); }
            hideActionButtons(slot);
        }
        else
        {
            if (fra.actionLabel != nullptr) fra.actionLabel->setText("");
            if (isSelected == true && m_focusZone == FocusZone::FILE_LIST)
            { m_selectedActionIndex = 0; highlightActionButtons(slot); }
            else
                hideActionButtons(slot);
        }

        fra.row->setVisibility(Visibility::VISIBLE);
        fra.row->setBackgroundColor((isSelected && showFileHighlight) ? Color::AccentPrimary : Color::CardBackground);
    }
}

// ============================================================================
// FileMenuView – action helpers
// ============================================================================

void FileMenuView::activateSelectedFile()
{
    if (m_onFileSelected != nullptr) m_onFileSelected(m_selectedGroupIndex, m_selectedFileIndex);
}

void FileMenuView::activateSelectedChannel()
{
    if (m_currentItem == nullptr) return;
    MenuItem* ch = m_currentItem->findChild("channel").get();
    if (ch == nullptr) return;
    const auto& children = ch->getChildren();
    if (m_selectedChannelIndex < 0 || m_selectedChannelIndex >= static_cast<int>(children.size())) return;
    if (m_selectedChannelIndex < static_cast<int>(m_channelAvailable.size())
        && m_channelAvailable[m_selectedChannelIndex] == false) return;
    children[m_selectedChannelIndex]->activate();
}

void FileMenuView::stepChannel(int dir)
{
    if (m_fileLoaded == false) return;
    const int total = static_cast<int>(m_channelButtons.size());
    if (total == 0) return;

    int next    = m_selectedChannelIndex + dir;
    int checked = 0;
    while (checked < total)
    {
        if (next < 0)      next = total - 1;
        if (next >= total) next = 0;
        if (next < static_cast<int>(m_channelAvailable.size()) && m_channelAvailable[next] == true) break;
        next += dir;
        ++checked;
    }
    if (checked == total) return;
    refreshChannelHighlight(next);
    activateSelectedChannel();
}

void FileMenuView::stepVolume(int dir)
{
    if (m_fsHudVolumeSlider == nullptr) return;
    float current = m_fsHudVolumeSlider->getValue();
    float step    = m_fsHudVolumeSlider->getStep();
    float next    = std::max(0.0f, std::min(100.0f, current + dir * step));
    if (next == current) return;
    m_fsHudVolumeSlider->setValue(next);
    if (m_onVolumeChanged != nullptr) m_onVolumeChanged(next / 100.0f);
}

void FileMenuView::activateControl(int index)
{
    m_selectedControlIndex = index;
    bool isCtrlFocused = (m_focusZone == FocusZone::CONTROL);
    for (int ci = 0; ci < static_cast<int>(m_controlButtons.size()); ++ci)
    {
        bool isSel = (ci == index);
        m_controlButtons[ci]->setNormalColor(Color::Transparent);
        m_controlButtons[ci]->setBorderColor(isSel && isCtrlFocused ? Color::BorderFocus : Color::Transparent, BORDER_W);
        m_controlButtons[ci]->setTextColor(isSel && isCtrlFocused   ? Color::TextPrimary : Color::TextSecondary);
    }
}

// ============================================================================
// FileMenuView – fullscreen
// ============================================================================

void FileMenuView::togglePreviewFullscreen()
{
    if (m_previewFullscreen == false) enterPreviewFullscreen();
    else                              exitPreviewFullscreen();
}

void FileMenuView::enterPreviewFullscreen()
{
    if (m_previewFullscreen == true)  return;
    if (m_previewContainer  == nullptr || m_leftBlock    == nullptr) return;
    if (m_fsRootLayout      == nullptr) return;

    m_leftBlock->removeViewNoDelete(m_previewContainer);

    m_previewContainer->getLayoutParams().width   = MATCH_PARENT;
    m_previewContainer->getLayoutParams().height  = MATCH_PARENT;
    m_previewContainer->getLayoutParams().weight  = 0.0f;
    m_previewContainer->getLayoutParams().gravity = Gravity::TOP_LEFT;
    m_previewContainer->getLayoutParams().setMargin(0.0f, 0.0f, 0.0f, 0.0f);
    
    int insertIdx = static_cast<int>(m_fsRootLayout->getChildCount()) - 1;
    if (insertIdx < 0) insertIdx = 0;
    m_fsRootLayout->addViewAt(m_previewContainer, insertIdx);

    m_previewFullscreen = true;
    setFocusZone(FocusZone::PREVIEW);
    if (m_previewFsButton != nullptr) m_previewFsButton->setBorderColor(Color::Transparent, 4.0f);
    if (m_subtitleLine1 != nullptr) m_subtitleLine1->setTextSize(FS_SUBTITLE_SIZE);
    if (m_subtitleLine2 != nullptr) m_subtitleLine2->setTextSize(FS_SUBTITLE_SIZE);

    {
        int initIdx  = (m_selectedControlIndex >= 0 && m_selectedControlIndex < static_cast<int>(m_fsHudControlButtons.size()))
                       ? m_selectedControlIndex : 0;
        Button* target = (initIdx < static_cast<int>(m_fsHudControlButtons.size()))
                         ? m_fsHudControlButtons[initIdx] : nullptr;
        if (target != nullptr) m_hudFocusManager.setFocus(target);
        applyHudFocusVisuals(target);
    }
    showFsHud();
}

void FileMenuView::exitPreviewFullscreen()
{
    if (m_previewFullscreen == false) return;
    if (m_previewContainer  == nullptr || m_leftBlock    == nullptr) return;
    if (m_fsRootLayout      == nullptr) return;

    m_fsRootLayout->removeViewNoDelete(m_previewContainer);

    m_previewContainer->getLayoutParams().width   = MATCH_PARENT;
    m_previewContainer->getLayoutParams().height  = MATCH_PARENT;
    m_previewContainer->getLayoutParams().weight  = 1.0f;
    m_previewContainer->getLayoutParams().gravity = Gravity::TOP_LEFT;
    m_previewContainer->getLayoutParams().setMargin(0.0f, 0.0f, 0.0f, 0.0f);
    m_leftBlock->addViewAt(m_previewContainer, 1);  // [0]=logo [1]=preview [2]=controlBar

    m_previewFullscreen = false;
    m_hudFocusManager.clearFocus();
    applyHudFocusVisuals(nullptr);
    hideFsHud();

    if (m_subtitleLine1 != nullptr) m_subtitleLine1->setTextSize(NORMAL_SUBTITLE_SIZE);
    if (m_subtitleLine2 != nullptr) m_subtitleLine2->setTextSize(NORMAL_SUBTITLE_SIZE);
    if (m_subtitleContainer != nullptr)
        m_subtitleContainer->getLayoutParams().setMargin(0.0f, 0.0f, 0.0f, 12.0f);
    if (m_previewFsButton != nullptr) m_previewFsButton->setBorderColor(Color::Transparent, 4.0f);
    setFocusZone(FocusZone::CONTROL);
}

// ============================================================================
// FileMenuView – update (marquee + HUD auto-hide)
// ============================================================================

void FileMenuView::update(float deltaTime)
{
    if (m_previewFullscreen == true && m_fsHudVisible == true)
    {
        m_fsHudTimer -= deltaTime;
        if (m_fsHudTimer <= 0.0f) hideFsHud();
    }

    if (m_focusZone != FocusZone::FILE_LIST) return;

    int slot = m_selectedFileIndex - m_fileScrollOffset;
    if (slot < 0 || slot >= static_cast<int>(m_fileRowActions.size())) return;
    FileRowActions& fra = m_fileRowActions[slot];
    if (fra.nameLabel != nullptr) fra.nameLabel->update(deltaTime);
}

// ============================================================================
// FileMenuView – key handling
// ============================================================================

bool FileMenuView::handleKeyEvent(const IO::KeyEvent& event)
{
    if (event.action != IO::KeyAction::DOWN && event.action != IO::KeyAction::MULTIPLE) return false;

    // ── Global keys: active in CHANNEL, CONTROL, and PREVIEW zones ───────────
    if (m_focusZone == FocusZone::CHANNEL  ||
        m_focusZone == FocusZone::CONTROL  ||
        m_focusZone == FocusZone::PREVIEW)
    {
        if (event.keyCode == IO::KeyCode::MEDIA_NEXT)     { stepChannel(-1); return true; }
        if (event.keyCode == IO::KeyCode::MEDIA_PREVIOUS) { stepChannel( 1); return true; }
        if (event.keyCode == IO::KeyCode::VOLUME_UP)      { stepVolume ( 1); return true; }
        if (event.keyCode == IO::KeyCode::VOLUME_DOWN)    { stepVolume (-1); return true; }
    }

    switch (m_focusZone)
    {
        case FocusZone::GROUP_RANGE:
        {
            if (event.keyCode == IO::KeyCode::DPAD_LEFT)  { if (m_groupRangePrev != nullptr) m_groupRangePrev->performClick(); return true; }
            if (event.keyCode == IO::KeyCode::DPAD_RIGHT) { if (m_groupRangeNext != nullptr) m_groupRangeNext->performClick(); return true; }
            if (event.keyCode == IO::KeyCode::DPAD_DOWN)  { setFocusZone(FocusZone::GROUP_COPY); return true; }
            break;
        }

        case FocusZone::GROUP_COPY:
        {
            if (event.keyCode == IO::KeyCode::ENTER || event.keyCode == IO::KeyCode::DPAD_CENTER)
                { if (m_groupCopyBtn != nullptr) m_groupCopyBtn->performClick(); return true; }
            if (event.keyCode == IO::KeyCode::DPAD_UP)   { setFocusZone(FocusZone::GROUP_RANGE); return true; }
            if (event.keyCode == IO::KeyCode::DPAD_DOWN) { setFocusZone(FocusZone::FILE_LIST);   return true; }
            break;
        }

        case FocusZone::FILE_LIST:
        {
            int  slot  = m_selectedFileIndex - m_fileScrollOffset;
            bool isDir = (slot >= 0 && slot < static_cast<int>(m_fileRowActions.size()))
                      && (m_fileRowActions[slot].isDirectory == true);

            if (event.keyCode == IO::KeyCode::DPAD_UP)
            {
                if (m_actionModeActive == true)
                {
                    m_actionModeActive    = false;
                    m_selectedActionIndex = -1;
                    highlightActionButtons(slot);
                    return true;
                }
                if (m_selectedFileIndex > 0)
                {
                    if (m_selectedFileIndex <= m_fileScrollOffset && m_fileScrollOffset > 0) --m_fileScrollOffset;
                    --m_selectedFileIndex;
                    setSelectedFileIndex(m_selectedFileIndex);
                }
                else setFocusZone(FocusZone::GROUP_COPY);
                return true;
            }

            if (event.keyCode == IO::KeyCode::DPAD_DOWN)
            {
                if (m_actionModeActive == true)
                {
                    m_actionModeActive    = false;
                    m_selectedActionIndex = -1;
                    highlightActionButtons(slot);
                    return true;
                }
                if (m_selectedFileIndex + 1 < m_groupFileCount)
                {
                    ++m_selectedFileIndex;
                    if (m_selectedFileIndex >= m_fileScrollOffset + MAX_VISIBLE_FILES) ++m_fileScrollOffset;
                    setSelectedFileIndex(m_selectedFileIndex);
                }
                return true;
            }

            if (event.keyCode == IO::KeyCode::DPAD_RIGHT)
            {
                if (isDir == true) return true;
                if (m_selectedActionIndex < 0)       m_selectedActionIndex = 0;
                else if (m_selectedActionIndex < 2)  ++m_selectedActionIndex;
                else
                {
                    m_selectedActionIndex = -1;
                    m_actionModeActive    = false;
                    highlightActionButtons(slot);
                    return true;
                }
                m_actionModeActive = true;
                highlightActionButtons(slot);
                return true;
            }

            if (event.keyCode == IO::KeyCode::DPAD_LEFT)
            {
                if (m_selectedActionIndex > 0)
                {
                    --m_selectedActionIndex;
                    highlightActionButtons(slot);
                }
                else if (m_selectedActionIndex == 0)
                {
                    m_selectedActionIndex = -1;
                    m_actionModeActive    = false;
                    highlightActionButtons(slot);
                }
                else
                {
                    m_selectedActionIndex = 2;
                    m_actionModeActive    = true;
                    highlightActionButtons(slot);
                }
                return true;
            }

            if (event.keyCode == IO::KeyCode::ENTER || event.keyCode == IO::KeyCode::DPAD_CENTER)
            {
                if (isDir == true) { activateSelectedFile(); return true; }
                if (slot >= 0 && slot < static_cast<int>(m_fileRowActions.size()))
                {
                    FileRowActions& fra = m_fileRowActions[slot];
                    if      (m_selectedActionIndex == 0 && fra.openBtn   != nullptr) fra.openBtn->performClick();
                    else if (m_selectedActionIndex == 1 && fra.copyBtn   != nullptr) fra.copyBtn->performClick();
                    else if (m_selectedActionIndex == 2 && fra.deleteBtn != nullptr) fra.deleteBtn->performClick();
                    else if (fra.openBtn           != nullptr)                       fra.openBtn->performClick();
                }
                return true;
            }
            break;
        }

        case FocusZone::CHANNEL:
        {
            if (event.keyCode == IO::KeyCode::DPAD_UP || event.keyCode == IO::KeyCode::DPAD_DOWN)
            {
                stepChannel((event.keyCode == IO::KeyCode::DPAD_UP) ? -1 : 1);
                return true;
            }
            if (event.keyCode == IO::KeyCode::DPAD_RIGHT) { setFocusZone(FocusZone::FILE_LIST); if (m_onFileUnload != nullptr) m_onFileUnload(); return true; }
            if (event.keyCode == IO::KeyCode::DPAD_LEFT)  { setFocusZone(FocusZone::CONTROL); return true; }
            if (event.keyCode == IO::KeyCode::ENTER || event.keyCode == IO::KeyCode::DPAD_CENTER) { activateSelectedChannel(); return true; }
            break;
        }

        case FocusZone::CONTROL:
        {
            if (event.keyCode == IO::KeyCode::DPAD_LEFT)
            {
                if (m_selectedControlIndex > 0) activateControl(m_selectedControlIndex - 1);
                else                            activateControl(static_cast<int>(m_controlButtons.size()) - 1);
                return true;
            }
            if (event.keyCode == IO::KeyCode::DPAD_RIGHT)
            {
                if (m_selectedControlIndex < static_cast<int>(m_controlButtons.size()) - 1)
                    activateControl(m_selectedControlIndex + 1);
                else
                    setFocusZone(FocusZone::CHANNEL);
                return true;
            }
            if (event.keyCode == IO::KeyCode::DPAD_UP) { setFocusZone(FocusZone::PREVIEW); return true; }
            if (event.keyCode == IO::KeyCode::ENTER || event.keyCode == IO::KeyCode::DPAD_CENTER)
            {
                if (event.action == IO::KeyAction::DOWN)
                {
                    if (m_selectedControlIndex >= 0 && m_selectedControlIndex < static_cast<int>(m_controlButtons.size()))
                        m_controlButtons[m_selectedControlIndex]->performClick();
                }
                return true;
            }
            break;
        }

        case FocusZone::PREVIEW:
        {
            if (m_previewFullscreen == true) resetFsHudTimer();

            if (event.keyCode == IO::KeyCode::ESCAPE)
            {
                if (m_previewFullscreen == true) exitPreviewFullscreen();
                else                             setFocusZone(FocusZone::CONTROL);
                return true;
            }

            if (m_previewFullscreen == false)
            {
                if (event.keyCode == IO::KeyCode::ENTER || event.keyCode == IO::KeyCode::DPAD_CENTER || event.keyCode == IO::KeyCode::SPACE)
                    { if (event.action == IO::KeyAction::DOWN) togglePreviewFullscreen(); return true; }
                if (event.keyCode == IO::KeyCode::DPAD_DOWN) { setFocusZone(FocusZone::CONTROL); return true; }
                break;
            }

            // ── Fullscreen key routing ────────────────────────────────────
            View* focusedView = m_hudFocusManager.getFocusedView();

            // Forward to volume slider when it is focused (it handles ENTER, ESC, L/R internally)
            if (focusedView == m_fsHudVolumeSlider && m_fsHudVolumeSlider != nullptr)
            {
                if (m_fsHudVolumeSlider->onKeyEvent(event) == true) return true;
            }

            if (event.keyCode == IO::KeyCode::ENTER || event.keyCode == IO::KeyCode::DPAD_CENTER || event.keyCode == IO::KeyCode::SPACE)
            {
                if (event.action == IO::KeyAction::DOWN)
                {
                    if (focusedView == m_previewFsButton)         togglePreviewFullscreen();
                    else if (focusedView == m_fsHudVolumeSlider)  { /* handled above */ }
                    else                                          activateFocusedHudButton();
                }
                return true;
            }

            if (event.keyCode == IO::KeyCode::DPAD_LEFT || event.keyCode == IO::KeyCode::DPAD_RIGHT)
            {
                // If volume slider is adjusting, let its key handler deal with it
                if (focusedView == m_fsHudVolumeSlider && m_fsHudVolumeSlider != nullptr
                    && m_fsHudVolumeSlider->isAdjusting() == true)
                    return true;  // already forwarded above

                // Move focus, skipping m_previewFsButton (only reachable via DPAD_UP)
                View* prev = focusedView;
                m_hudFocusManager.handleKeyEvent(event);
                if (m_hudFocusManager.getFocusedView() == m_previewFsButton) m_hudFocusManager.handleKeyEvent(event);
                if (m_hudFocusManager.getFocusedView() == m_previewFsButton && prev != nullptr) m_hudFocusManager.setFocus(prev);

                View* newFocus = m_hudFocusManager.getFocusedView();
                for (int ci = 0; ci < static_cast<int>(m_fsHudControlButtons.size()); ++ci)
                    if (m_fsHudControlButtons[ci] == newFocus) { m_selectedControlIndex = ci; break; }
                applyHudFocusVisuals(newFocus);
                return true;
            }

            if (event.keyCode == IO::KeyCode::DPAD_UP)
            {
                m_hudFocusManager.setFocus(m_previewFsButton);
                applyHudFocusVisuals(m_previewFsButton);
                return true;
            }

            if (event.keyCode == IO::KeyCode::DPAD_DOWN)
            {
                if (focusedView == m_previewFsButton)
                {
                    int    idx    = (m_selectedControlIndex >= 0 && m_selectedControlIndex < static_cast<int>(m_fsHudControlButtons.size()))
                                    ? m_selectedControlIndex : 0;
                    Button* target = (idx < static_cast<int>(m_fsHudControlButtons.size()))
                                     ? m_fsHudControlButtons[idx] : nullptr;
                    if (target != nullptr) { m_hudFocusManager.setFocus(target); applyHudFocusVisuals(target); }
                }
                return true;
            }

            return true;  // consume all other keys while fullscreen
        }
    }
    return false;
}

bool FileMenuView::onKeyEvent(const IO::KeyEvent& event) { return handleKeyEvent(event); }

void FileMenuView::onDraw(ICanvas& canvas)
{
    if (isVisible() == false) return;
    FrameLayout::onDraw(canvas);
}

// ============================================================================
// FileMenuView – motion events
// ============================================================================

bool FileMenuView::onMotionEvent(const IO::MotionEvent& event)
{
    const float ex = event.x;
    const float ey = event.y;

    // ── Fullscreen: forward to reparented previewContainer + sync HUD focus on tap ──
    if (m_previewFullscreen == true)
    {
        resetFsHudTimer();
        if (m_previewContainer != nullptr) m_previewContainer->onMotionEvent(event);

        // if (event.action == IO::MotionAction::DOWN)
        // {
        //     Button* hitBtn = nullptr;
        //     if (m_previewFsButton != nullptr && m_previewFsButton->getBounds().contains(ex, ey))
        //     {
        //         hitBtn = m_previewFsButton;
        //     }
        //     else
        //     {
        //         for (Button* btn : m_fsHudControlButtons)
        //             if (btn != nullptr && btn->getBounds().contains(ex, ey)) { hitBtn = btn; break; }
        //         if (hitBtn == nullptr)
        //         {
        //             for (Button* btn : m_fsHudChannelButtons)
        //                 if (btn != nullptr && btn->isEnabled() == true && btn->getBounds().contains(ex, ey)) { hitBtn = btn; break; }
        //         }
        //     }
        //     if (hitBtn != nullptr) { m_hudFocusManager.setFocus(hitBtn); applyHudFocusVisuals(hitBtn); }
        // }

        return true;
    }

    // ── Hover → set PREVIEW zone ──────────────────────────────────────────
    if (event.canHover() == true
        && m_previewContainer != nullptr
        && m_previewContainer->getBounds().contains(ex, ey) == true
        && m_focusZone != FocusZone::PREVIEW)
    {
        setFocusZone(FocusZone::PREVIEW);
    }

    // ── Scroll ────────────────────────────────────────────────────────────
    if (event.action == IO::MotionAction::SCROLL)
    {
        if (m_fileListColumn != nullptr && m_fileListColumn->getBounds().contains(ex, ey) == true && m_groupFileCount > 0)
        {
            int delta     = (event.scrollY > 0.0f) ? 1 : -1;
            int newOffset = std::max(0, std::min(m_fileScrollOffset + delta, m_groupFileCount - MAX_VISIBLE_FILES));
            if (newOffset != m_fileScrollOffset)
            {
                m_fileScrollOffset = newOffset;
                if (m_selectedFileIndex < m_fileScrollOffset)
                    m_selectedFileIndex = m_fileScrollOffset;
                if (m_selectedFileIndex >= m_fileScrollOffset + MAX_VISIBLE_FILES)
                    m_selectedFileIndex = m_fileScrollOffset + MAX_VISIBLE_FILES - 1;
                if (m_focusZone != FocusZone::FILE_LIST) setFocusZone(FocusZone::FILE_LIST);
                refreshFileListSlots();
            }
            return true;
        }
        return FrameLayout::onMotionEvent(event);
    }

    // ── DOWN ──────────────────────────────────────────────────────────────
    if (event.action == IO::MotionAction::DOWN)
    {
        m_fileDragging        = false;
        m_fileDragStartY      = ey;
        m_fileDragStartOffset = m_fileScrollOffset;
        m_fileDragTotalY      = 0.0f;

        if (m_previewContainer != nullptr && m_previewContainer->getBounds().contains(ex, ey) == true)
        {
            if (m_focusZone != FocusZone::PREVIEW) setFocusZone(FocusZone::PREVIEW);
            return FrameLayout::onMotionEvent(event);
        }

        for (int slot = 0; slot < MAX_VISIBLE_FILES; ++slot)
        {
            if (slot >= static_cast<int>(m_fileRowActions.size())) break;
            FileRowActions& fra = m_fileRowActions[slot];
            if (fra.row == nullptr || fra.row->getVisibility() != Visibility::VISIBLE) continue;
            if (fra.row->getBounds().contains(ex, ey) == false) continue;

            m_fileListMouseDown = true;
            int fileLocalIdx    = m_fileScrollOffset + slot;
            if (fileLocalIdx != m_selectedFileIndex) setSelectedFileIndex(fileLocalIdx);
            if (m_focusZone != FocusZone::FILE_LIST) setFocusZone(FocusZone::FILE_LIST);
            return true;
        }

        for (int i = 0; i < static_cast<int>(m_channelButtons.size()); ++i)
        {
            if (m_channelButtons[i] == nullptr || m_channelButtons[i]->isEnabled() == false) continue;
            if (m_channelButtons[i]->getBounds().contains(ex, ey) == true)
            {
                refreshChannelHighlight(i);
                activateSelectedChannel();
                if (m_focusZone != FocusZone::CHANNEL) setFocusZone(FocusZone::CHANNEL);
                return true;
            }
        }

        return FrameLayout::onMotionEvent(event);
    }

    // ── MOVE ──────────────────────────────────────────────────────────────
    if (event.action == IO::MotionAction::MOVE)
    {
        if (m_fileListMouseDown == false) return FrameLayout::onMotionEvent(event);
        if (m_fileRowActions.empty() == true || m_fileRowActions[0].row == nullptr) return FrameLayout::onMotionEvent(event);
        if (m_groupFileCount <= MAX_VISIBLE_FILES) return FrameLayout::onMotionEvent(event);

        float deltaY = m_fileDragStartY - ey;
        m_fileDragTotalY = deltaY;

        float rowH      = m_fileRowActions[0].row->getBounds().height();
        if (rowH <= 0.0f) rowH = 40.0f;
        int newOffset   = std::max(0, std::min(m_fileDragStartOffset + static_cast<int>(deltaY / rowH),
                                               m_groupFileCount - MAX_VISIBLE_FILES));
        if (newOffset != m_fileScrollOffset)
        {
            m_fileDragging     = true;
            m_fileScrollOffset = newOffset;
            if (m_selectedFileIndex < m_fileScrollOffset)                    m_selectedFileIndex = m_fileScrollOffset;
            if (m_selectedFileIndex >= m_fileScrollOffset + MAX_VISIBLE_FILES) m_selectedFileIndex = m_fileScrollOffset + MAX_VISIBLE_FILES - 1;
            if (m_focusZone != FocusZone::FILE_LIST) setFocusZone(FocusZone::FILE_LIST);
            refreshFileListSlots();
        }
        return true;
    }

    // ── UP / CANCEL ───────────────────────────────────────────────────────
    if (event.action == IO::MotionAction::UP || event.action == IO::MotionAction::CANCEL)
    {
        bool wasDragging    = m_fileDragging;
        m_fileDragging      = false;
        m_fileListMouseDown = false;

        bool hasRows = (m_fileRowActions.empty() == false) && (m_fileRowActions[0].row != nullptr);
        if (wasDragging == true && hasRows == true) return true;

        if (wasDragging == false)
        {
            for (int slot = 0; slot < MAX_VISIBLE_FILES; ++slot)
            {
                if (slot >= static_cast<int>(m_fileRowActions.size())) break;
                FileRowActions& fra = m_fileRowActions[slot];
                if (fra.row == nullptr || fra.row->getVisibility() != Visibility::VISIBLE) continue;
                if (fra.row->getBounds().contains(ex, ey) == false) continue;

                int fileLocalIdx = m_fileScrollOffset + slot;
                if (fra.isDirectory == true) { m_selectedFileIndex = fileLocalIdx; activateSelectedFile(); return true; }

                auto tryFireBtn = [&](Button* btn, int actionIdx) -> bool
                {
                    if (btn == nullptr || btn->getVisibility() != Visibility::VISIBLE) return false;
                    if (btn->getBounds().contains(ex, ey) == false) return false;
                    if (fileLocalIdx != m_selectedFileIndex)
                    {
                        setSelectedFileIndex(fileLocalIdx);
                        if (m_focusZone != FocusZone::FILE_LIST) setFocusZone(FocusZone::FILE_LIST);
                    }
                    m_selectedActionIndex = actionIdx;
                    highlightActionButtons(slot);
                    btn->performClick();
                    return true;
                };

                if (tryFireBtn(fra.openBtn,   0)) return true;
                if (tryFireBtn(fra.copyBtn,   1)) return true;
                if (tryFireBtn(fra.deleteBtn, 2)) return true;
                return true;
            }
        }
        return FrameLayout::onMotionEvent(event);
    }

    return FrameLayout::onMotionEvent(event);
}

// ============================================================================
// MenuNavigator
// ============================================================================

MenuNavigator::MenuNavigator()
    : m_rootItem(nullptr)
    , m_currentItem(nullptr)
    , m_state(MenuState::MAIN_MENU)
    , m_focusManager(nullptr)
    , m_rootMenuView(nullptr)
    , m_mainMenuView(nullptr)
    , m_subMenuView(nullptr)
    , m_fileMenuView(nullptr)
    , m_onBackToApp(nullptr)
    , m_onSave(nullptr)
    , m_onFileMenuExit(nullptr)
    , m_savedMainMenuIndex(0)
    , m_savedSubMenuIndex(0)
{}

MenuNavigator::~MenuNavigator() { cleanup(); }

bool MenuNavigator::initialize(MenuItem* rootItem)
{
    if (rootItem == nullptr) return false;
    m_rootItem    = rootItem;
    m_currentItem = rootItem;

    m_rootMenuView = new FrameLayout();
    m_rootMenuView->setBackgroundColor(Color::CardBackground);
    m_rootMenuView->getLayoutParams().width  = MATCH_PARENT;
    m_rootMenuView->getLayoutParams().height = MATCH_PARENT;

    m_mainMenuView = new MainMenuView();
    m_mainMenuView->setMenuItem(rootItem);
    m_mainMenuView->getLayoutParams().width  = MATCH_PARENT;
    m_mainMenuView->getLayoutParams().height = MATCH_PARENT;
    m_mainMenuView->setVisibility(Visibility::VISIBLE);
    m_mainMenuView->setOnItemActivated([this](MenuItem* item)
    {
        if (item != nullptr && item->getChildren().empty() == false) navigateToItem(item);
        else if (item != nullptr) item->activate();
    });
    m_rootMenuView->addView(m_mainMenuView);

    m_subMenuView = new SubMenuView();
    m_subMenuView->getLayoutParams().width  = MATCH_PARENT;
    m_subMenuView->getLayoutParams().height = MATCH_PARENT;
    m_subMenuView->setVisibility(Visibility::GONE);
    m_subMenuView->setOnValueChanged([this](MenuItem*) {});
    m_rootMenuView->addView(m_subMenuView);

    m_fileMenuView = new FileMenuView();
    m_fileMenuView->getLayoutParams().width  = MATCH_PARENT;
    m_fileMenuView->getLayoutParams().height = MATCH_PARENT;
    m_fileMenuView->setVisibility(Visibility::GONE);
    m_rootMenuView->addView(m_fileMenuView);

    m_state = MenuState::MAIN_MENU;
    return true;
}

void MenuNavigator::cleanup()
{
    m_rootMenuView = nullptr;
    m_mainMenuView = nullptr;
    m_subMenuView  = nullptr;
    m_fileMenuView = nullptr;
    m_navigationStack.clear();
}

void MenuNavigator::navigateToItem(MenuItem* item)
{
    if (item == nullptr) return;
    if (m_state == MenuState::MAIN_MENU && m_mainMenuView != nullptr)
        m_savedMainMenuIndex = m_mainMenuView->getSelectedIndex();
    if (m_currentItem != nullptr) m_navigationStack.push_back(m_currentItem);
    m_currentItem = item;

    if (item->getChildren().empty() == false)
    {
        if (isFileMenuItem(item) == true)
        {
            hideAllViews();
            m_fileMenuView->setVisibility(Visibility::VISIBLE);
            m_fileMenuView->setMenuItem(item);
            item->activate();
            m_state = MenuState::FILE_MENU;
        }
        else
        {
            item->activate();  // configure SubMenuView mode before setMenuItem rebuilds content
            showSubMenu(item);
        }
    }
    else item->activate();
}

void MenuNavigator::navigateBack()
{
    if (m_state == MenuState::ADJUSTING)
    {
        m_subMenuView->setAdjusting(false);
        m_state = MenuState::SUB_MENU;
        return;
    }

    if (m_state == MenuState::FILE_MENU)
    {
        if (m_fileMenuView != nullptr) m_fileMenuView->setVisibility(Visibility::GONE);
        if (m_navigationStack.empty() == false)
        {
            m_currentItem = m_navigationStack.back();
            m_navigationStack.pop_back();
        }
        if (m_onFileMenuExit != nullptr) m_onFileMenuExit();
        showMainMenu();
        return;
    }

    if (m_state == MenuState::SUB_MENU)
    {
        if (m_navigationStack.empty() == false)
        {
            m_currentItem = m_navigationStack.back();
            m_navigationStack.pop_back();
            if (m_navigationStack.empty() == true) showMainMenu();
            else                                   showSubMenu(m_currentItem);
        }
        else showMainMenu();
        return;
    }

    if (m_state == MenuState::MAIN_MENU && m_onBackToApp != nullptr) m_onBackToApp();
}

void MenuNavigator::navigateToRoot()
{
    m_navigationStack.clear();
    m_currentItem = m_rootItem;
    showMainMenu();
}

void MenuNavigator::showMainMenu()
{
    hideAllViews();
    m_mainMenuView->setVisibility(Visibility::VISIBLE);
    m_mainMenuView->setMenuItem(m_rootItem);
    m_mainMenuView->setKeyboardActive(true);
    m_mainMenuView->setSelectedIndex(m_savedMainMenuIndex);
    m_state = MenuState::MAIN_MENU;
}

void MenuNavigator::showSubMenu(MenuItem* item)
{
    if (item == nullptr) return;
    hideAllViews();
    m_subMenuView->setVisibility(Visibility::VISIBLE);
    m_subMenuView->setKeyboardActive(true);
    m_subMenuView->setMenuItem(item);
    m_state = MenuState::SUB_MENU;
}

void MenuNavigator::hideAllViews()
{
    if (m_mainMenuView != nullptr) m_mainMenuView->setVisibility(Visibility::GONE);
    if (m_subMenuView  != nullptr) m_subMenuView->setVisibility(Visibility::GONE);
    if (m_fileMenuView != nullptr) m_fileMenuView->setVisibility(Visibility::GONE);
}

bool MenuNavigator::isFileMenuItem(MenuItem* item) const
{
    if (item == nullptr) return false;
    for (const auto& id : m_fileMenuIds)
        if (item->getId() == id) return true;
    return false;
}

bool MenuNavigator::handleKeyEvent(const IO::KeyEvent& event)
{
    if (event.action != IO::KeyAction::DOWN && event.action != IO::KeyAction::MULTIPLE) return false;

    if (event.action == IO::KeyAction::DOWN
        && (event.keyCode == IO::KeyCode::ESCAPE || event.keyCode == IO::KeyCode::BACK))
    {
        navigateBack();
        return true;
    }

    if (m_state == MenuState::FILE_MENU && m_fileMenuView != nullptr)
        return m_fileMenuView->handleKeyEvent(event);

    if (event.keyCode == IO::KeyCode::DPAD_UP)
    {
        if (m_state == MenuState::MAIN_MENU) return false;  // caller moves to Back button
        if ((m_state == MenuState::SUB_MENU || m_state == MenuState::ADJUSTING) && m_subMenuView != nullptr)
        {
            bool handled = m_subMenuView->onKeyEvent(event);
            m_state = m_subMenuView->isAdjusting() ? MenuState::ADJUSTING : MenuState::SUB_MENU;
            return handled;
        }
    }

    if (m_state == MenuState::MAIN_MENU && m_mainMenuView != nullptr)
        return m_mainMenuView->onKeyEvent(event);

    if ((m_state == MenuState::SUB_MENU || m_state == MenuState::ADJUSTING) && m_subMenuView != nullptr)
    {
        bool handled = m_subMenuView->onKeyEvent(event);
        m_state = m_subMenuView->isAdjusting() ? MenuState::ADJUSTING : MenuState::SUB_MENU;
        return handled;
    }

    return false;
}

bool MenuNavigator::handleMotionEvent(const IO::MotionEvent& event)
{
    if (m_rootMenuView == nullptr) return false;
    return m_rootMenuView->onMotionEvent(event);
}

void MenuNavigator::update(float deltaTime)
{
    if (m_state == MenuState::FILE_MENU && m_fileMenuView != nullptr)
        m_fileMenuView->update(deltaTime);
    else if ((m_state == MenuState::SUB_MENU || m_state == MenuState::ADJUSTING) && m_subMenuView != nullptr)
        m_subMenuView->update(deltaTime);
    else if (m_state == MenuState::MAIN_MENU && m_mainMenuView != nullptr)
        m_mainMenuView->update(deltaTime);
}

void MenuNavigator::setContentFocused(bool focused)
{
    if (m_state == MenuState::FILE_MENU)
    {
        if (m_fileMenuView != nullptr) m_fileMenuView->setContentFocused(focused);
        return;
    }
    if (m_state == MenuState::MAIN_MENU && m_mainMenuView != nullptr)
    {
        m_mainMenuView->setKeyboardActive(focused);
        return;
    }
    if ((m_state == MenuState::SUB_MENU || m_state == MenuState::ADJUSTING) && m_subMenuView != nullptr)
    {
        if (focused == false) { m_subMenuView->setKeyboardActive(false); m_subMenuView->clearSelection(); }
        else                  { m_subMenuView->setKeyboardActive(true);  m_subMenuView->restoreSelection(); }
    }
}

void MenuNavigator::clearMainMenuSelection()
{
    if (m_mainMenuView == nullptr) return;
    m_savedMainMenuIndex = m_mainMenuView->getSelectedIndex();
    for (size_t i = 0; i < m_mainMenuView->getItemViewCount(); ++i)
        m_mainMenuView->setItemBackground(i, Color::Transparent);
}

void MenuNavigator::restoreMainMenuSelection()
{
    if (m_mainMenuView != nullptr) m_mainMenuView->setSelectedIndex(m_savedMainMenuIndex);
}

void MenuNavigator::clearSubMenuSelection()
{
    if (m_subMenuView == nullptr) return;
    m_savedSubMenuIndex = m_subMenuView->getSelectedSubItemIndex();
    m_subMenuView->clearSelection();
}

void MenuNavigator::restoreSubMenuSelection()
{
    if (m_subMenuView == nullptr) return;
    m_subMenuView->setSelectedSubItemIndex(m_savedSubMenuIndex);
    m_subMenuView->updateContent();
}

int MenuNavigator::getSubMenuSelectedIndex() const
{
    return (m_subMenuView != nullptr) ? m_subMenuView->getSelectedSubItemIndex() : 0;
}

void MenuNavigator::setSubMenuSelectedIndex(int index)
{
    if (m_subMenuView == nullptr) return;
    m_subMenuView->setSelectedSubItemIndex(index);
    m_subMenuView->updateContent();
}

void MenuNavigator::setSubMenuAdjustmentMode(bool adjusting)
{
    if (m_subMenuView != nullptr) m_subMenuView->setAdjusting(adjusting);
}

void MenuNavigator::setSubMenuOnAdjustmentConfirm(SubMenuView::ConfirmCallback cb)
{
    if (m_subMenuView != nullptr) m_subMenuView->setOnAdjustmentConfirm(cb);
}

void MenuNavigator::refreshSubMenuContent()
{
    if (m_subMenuView != nullptr) m_subMenuView->updateContent();
}

void MenuNavigator::setSubMenuLayoutWeights(float previewWeight, float subItemsWeight)
{
    if (m_subMenuView != nullptr) m_subMenuView->setLayoutWeights(previewWeight, subItemsWeight);
}

void MenuNavigator::setSubMenuGridMode(bool enabled, int numColumns)
{
    if (m_subMenuView != nullptr) m_subMenuView->setGridMode(enabled, numColumns);
}

void MenuNavigator::setSubMenuGridColumns(const std::vector<SubMenuView::GridColumnDef>& colDefs)
{
    if (m_subMenuView != nullptr) m_subMenuView->setGridColumns(colDefs);
}

void MenuNavigator::setSubMenuNumpadCallback(SubMenuView::NumpadRequestCallback cb)
{
    if (m_subMenuView != nullptr) m_subMenuView->setNumpadRequestCallback(cb);
}

} // namespace UI

} // namespace APP