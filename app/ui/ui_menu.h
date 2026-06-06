#ifndef UI_MENU_H
#define UI_MENU_H

#include "ui_core.h"
#include "ui_elements.h"
#include "ui_focus.h"
#ifdef USE_DVR
    #include "vpb_file_manager.h"
    #include "vpb_file_player.h"
#endif
#include <string>
#include <vector>
#include <memory>
#include <functional>

namespace APP
{
namespace UI
{

// ── MenuItem ─────────────────────────────────────────────────────────────────

enum class MenuItemType { CATEGORY, ACTION, TOGGLE, SLIDER, CHOICE, CHECKBOX };

struct MenuOption
{
    std::string label;
    int         value;

    MenuOption() : label(), value(0) {}
    MenuOption(const std::string& l, int v) : label(l), value(v) {}
};

class MenuItem
{
public:
    using ActivateCallback     = std::function<void(MenuItem*)>;
    using ValueChangedCallback = std::function<void(MenuItem*)>;

    MenuItem(const std::string& id, const std::string& name, MenuItemType type)
        : m_id(id), m_name(name), m_type(type)
        , m_iconTexture(nullptr), m_parent(nullptr)
        , m_enabled(true), m_visible(true), m_showPreview(true)
        , m_valueInt(0), m_valueFloat(0.0f), m_valueBool(false)
        , m_minValue(0.0f), m_maxValue(100.0f), m_step(1.0f)
        , m_currentOptionIndex(0), m_selectedChildIndex(0)
        , m_zeroPad(false)
        , m_onActivate(nullptr), m_onValueChanged(nullptr) {}

    // ── Hierarchy ─────────────────────────────────────────────────────────
    void                                          addChild(std::shared_ptr<MenuItem> child);
    void                                          removeChild(const std::string& id);
    std::shared_ptr<MenuItem>                     findChild(const std::string& id) const;
    MenuItem*                                     find(const std::string& id);
    const std::vector<std::shared_ptr<MenuItem>>& getChildren() const { return m_children; }
    void      setParent(MenuItem* parent)                             { m_parent = parent; }
    MenuItem* getParent() const                                       { return m_parent; }

    // ── Identity / state ──────────────────────────────────────────────────
    const std::string& getId()   const                      { return m_id; }
    const std::string& getName() const                      { return m_name; }
    void               setName(const std::string& name)     { m_name = name; }
    MenuItemType       getType()    const                   { return m_type; }
    bool               isEnabled() const                    { return m_enabled; }
    void               setEnabled(bool v)                   { m_enabled = v; }
    bool               isVisible() const                    { return m_visible; }
    void               setVisible(bool v)                   { m_visible = v; }

    void     setIconTexture(Texture* t)                     { m_iconTexture = t; }
    Texture* getIconTexture() const                         { return m_iconTexture; }
    void     setShowPreview(bool v)                         { m_showPreview = v; }
    bool     shouldShowPreview() const                      { return m_showPreview; }

    // ── Values ────────────────────────────────────────────────────────────
    int   getValueInt()   const                             { return m_valueInt; }
    float getValueFloat() const                             { return m_valueFloat; }
    bool  getValueBool()  const                             { return m_valueBool; }
    void  setValueInt(int v)                                { m_valueInt   = v; if (m_onValueChanged != nullptr) m_onValueChanged(this); }
    void  setValueFloat(float v)                            { m_valueFloat = v; if (m_onValueChanged != nullptr) m_onValueChanged(this); }
    void  setValueBool(bool v)                              { m_valueBool  = v; if (m_onValueChanged != nullptr) m_onValueChanged(this); }

    void  setRange(float min, float max, float step = 1.0f) { m_minValue = min; m_maxValue = max; m_step = step; }
    float getMinValue() const                               { return m_minValue; }
    float getMaxValue() const                               { return m_maxValue; }
    float getStep()     const                               { return m_step; }

    // ── Options (CHOICE / CATEGORY) ───────────────────────────────────────
    void                           addOption(const MenuOption& o)               { m_options.push_back(o); }
    void                           setOptions(const std::vector<MenuOption>& o) { m_options = o; }
    const std::vector<MenuOption>& getOptions() const                           { return m_options; }
    int                            getCurrentOptionIndex() const                { return m_currentOptionIndex; }
    void                           setCurrentOptionIndex(int index);
    const MenuOption*              getCurrentOption() const;

    int  getSelectedChildIndex() const    { return m_selectedChildIndex; }
    void setSelectedChildIndex(int index) { m_selectedChildIndex = index; }

    // ── Checked mask (CHECKBOX) ───────────────────────────────────────────
    // m_valueInt is used as a bitmask: bit N = child N is checked.
    bool isChildChecked(int childIndex)  const { return (m_valueInt & (1 << childIndex)) != 0; }
    void setChildChecked(int childIndex, bool checked)
    {
        if (checked == true) m_valueInt |=  (1 << childIndex);
        else                 m_valueInt &= ~(1 << childIndex);
        if (m_onValueChanged != nullptr) m_onValueChanged(this);
    }
    void toggleChildChecked(int childIndex) { setChildChecked(childIndex, !isChildChecked(childIndex)); }
    void setCheckedMask(int mask)           { m_valueInt = mask; }

    // ── Callbacks ─────────────────────────────────────────────────────────
    void setOnActivate(ActivateCallback cb)         { m_onActivate     = cb; }
    void setOnValueChanged(ValueChangedCallback cb) { m_onValueChanged = cb; }
    void setZeroPad(bool v)                         { m_zeroPad        = v;  }
    bool isZeroPad() const                          { return m_zeroPad;      }
    void activate()                                 { if (m_onActivate != nullptr) m_onActivate(this); }

    // ── View binding (owned by SubMenuView, cleared on setupContent) ──────
    // Holds all UI widget pointers for the row this item is rendered into.
    // SubMenuView writes these during buildRow(); cleared in setupContent().
    struct ViewData
    {
        LinearLayout* row         = nullptr;
        TextView*     nameLabel   = nullptr;
        LinearLayout* valueWidget = nullptr;
        TextView*     valLabel    = nullptr;
        Button*       numpadBtn   = nullptr;
        int           col         = 0;
        int           rowIdx      = 0;
        Color4        tintColor   = Color::Transparent;
    };
    ViewData&       viewData()       { return m_viewData; }
    const ViewData& viewData() const { return m_viewData; }
    void            clearViewData()  { m_viewData = ViewData{}; }

private:
    std::string  m_id;
    std::string  m_name;
    MenuItemType m_type;
    Texture*     m_iconTexture;
    MenuItem*    m_parent;
    std::vector<std::shared_ptr<MenuItem>> m_children;

    bool  m_enabled;
    bool  m_visible;
    bool  m_showPreview;
    int   m_valueInt;
    float m_valueFloat;
    bool  m_valueBool;
    float m_minValue;
    float m_maxValue;
    float m_step;
    std::vector<MenuOption> m_options;
    int   m_currentOptionIndex;
    int   m_selectedChildIndex;

    bool  m_zeroPad;
    ActivateCallback     m_onActivate;
    ValueChangedCallback m_onValueChanged;
    ViewData             m_viewData;
};

// ── MenuItemView ──────────────────────────────────────────────────────────────

class MenuItemView : public FrameLayout
{
public:
    MenuItemView();

    void      setMenuItem(MenuItem* item);
    MenuItem* getMenuItem() const { return m_item; }
    void      setSelected(bool selected);
    bool      isSelected()  const { return m_selected; }
    void      update(float deltaTime);

    bool onMotionEvent(const IO::MotionEvent& event) override;
    void onFocusChanged(bool hasFocus) override;
    void onDraw(ICanvas& canvas) override;

private:
    void updateVisualState();

    MenuItem*     m_item;
    bool          m_selected;
    LinearLayout* m_contentLayout;
    ImageView*    m_iconView;
    TextView*     m_nameText;
};

// ── MainMenuView ──────────────────────────────────────────────────────────────

class MainMenuView : public ViewGroup
{
public:
    using ItemActivatedCallback = std::function<void(MenuItem*)>;

    MainMenuView();

    void      setMenuItem(MenuItem* rootItem);
    void      setSelectedIndex(int index);
    int       getSelectedIndex() const             { return m_selectedIndex; }
    MenuItem* getSelectedItem() const;

    void          setOnItemActivated(ItemActivatedCallback cb) { m_onItemActivated = cb; }
    void          setItemBackground(size_t index, const Color4& color);
    size_t        getItemViewCount() const         { return m_itemViews.size(); }
    MenuItemView* getItemView(size_t index)        { return (index < m_itemViews.size()) ? m_itemViews[index] : nullptr; }

    void updateSelection();
    void setKeyboardActive(bool active)            { m_keyboardActive = active; updateSelection(); }
    void update(float deltaTime);

    void onMeasure(MeasureSpec widthSpec, MeasureSpec heightSpec) override;
    bool onKeyEvent(const IO::KeyEvent& event) override;
    bool onMotionEvent(const IO::MotionEvent& event) override;
    void onDraw(ICanvas& canvas) override;

protected:
    void layoutChildren() override;

private:
    void rebuildItems();

    MenuItem*   m_rootItem;
    int         m_selectedIndex;
    bool        m_keyboardActive;
    int         m_hoveredIndex;
    std::vector<MenuItemView*> m_itemViews;
    ItemActivatedCallback      m_onItemActivated;
};

// ── SubMenuView ───────────────────────────────────────────────────────────────
// Unified grid-based sub-menu panel.  Items are laid out in a (col, row) grid.
// Default is 1 column (standard mode); setGridMode(true, N) → N columns.
//
// Row orientation rule (isVerticalRow()):
//   Grid mode OR shouldShowPreview()==false → HORIZONTAL: [nameLabel | valueWidget]
//   Standard mode AND shouldShowPreview()==true → VERTICAL: nameLabel on top,
//     valueWidget CENTER_HORIZONTAL below.
//
// UI widget pointers live on each MenuItem::ViewData (written in buildRow,
// cleared in setupContent).  m_rows is the flat ordered list of visible items.
//
// SLIDER rows always get [valLabel | numpadBtn] inside valueWidget.
// Digit-edit mode activates on DPAD_CENTER for SLIDER items only.
//
// Column layout (setGridColumns):
//   GridColumnDef.weight  — relative column width.
//   GridRowDef.fixedH > 0 — fixed-pixel row height; 0 → distributed by weight.
//   Simple form: setGridMode(true, N) auto-distributes items across N equal cols.

class SubMenuView : public LinearLayout
{
public:
    using ValueChangedCallback    = std::function<void(MenuItem*)>;
    using ConfirmCallback         = std::function<void()>;
    using SubItemSelectedCallback = std::function<void(int)>;
    using NumpadRequestCallback   = std::function<void(MenuItem*, std::function<void(float)>)>;

    // ── Column / row layout descriptors ──────────────────────────────────
    struct GridRowDef
    {
        float   fixedH  = 0.0f;               // > 0 → fixed px height; 0 → distributed by weight
        float   weight  = 1.0f;               // relative weight among distributed rows in column
        Gravity gravity = Gravity::NO_GRAVITY; // cell gravity (NO_GRAVITY = use default)

        static GridRowDef distributed(float w = 1.0f, Gravity g = Gravity::NO_GRAVITY) { return { 0.0f, w, g }; }
        static GridRowDef fixed(float h,               Gravity g = Gravity::NO_GRAVITY) { return { h,    0.0f, g }; }
    };

    struct GridColumnDef
    {
        float                   weight = 1.0f; // relative column width
        std::vector<GridRowDef> rows;           // top-to-bottom row definitions

        GridColumnDef& addRow(float w = 1.0f, Gravity g = Gravity::NO_GRAVITY)
            { rows.push_back(GridRowDef::distributed(w, g)); return *this; }
        GridColumnDef& addFixedRow(float h, Gravity g = Gravity::NO_GRAVITY)
            { rows.push_back(GridRowDef::fixed(h, g)); return *this; }
    };

    static constexpr float DEFAULT_PREVIEW_WEIGHT  = 2.8f;
    static constexpr float DEFAULT_SUBITEMS_WEIGHT = 1.0f;

    static constexpr int   SUBMENU_MIN_ROWS            = 5;
    static constexpr int   SUBMENU_MAX_ROWS            = 10;
    static constexpr float SUBMENU_FIXED_ROW_H         = 128.0f;
    static constexpr float SUBMENU_FIXED_ROW_H_GRID    = 140.0f;

    SubMenuView();

    // ── MenuItem binding ──────────────────────────────────────────────────
    void      setMenuItem(MenuItem* item);
    MenuItem* getMenuItem() const { return m_currentItem; }

    // ── Layout weight control ─────────────────────────────────────────────
    void setLayoutWeights(float previewWeight, float subItemsWeight);
    void getLayoutWeights(float& outPreview, float& outSubItems) const;

    // ── Grid mode ─────────────────────────────────────────────────────────
    void setGridMode(bool enabled, int numColumns = 3);
    void setGridColumns(const std::vector<GridColumnDef>& colDefs);
    bool isGridMode()                   const { return m_gridMode; }
    void setNumpadRequestCallback(NumpadRequestCallback cb);

    // ── Content / selection ───────────────────────────────────────────────
    void setupContent();
    void updateContent();
    void clearSelection();
    void restoreSelection();

    // ── Adjustment ────────────────────────────────────────────────────────
    void setAdjusting(bool adjusting);
    bool isAdjusting()             const { return m_isAdjusting; }

    // ── Keyboard / selection state ────────────────────────────────────────
    void setKeyboardActive(bool active);
    int  getSelectedSubItemIndex() const { return m_selRow; }
    int  getSelectedCol()          const { return m_selCol; }
    void setSelectedSubItemIndex(int index);

    // ── Callbacks ─────────────────────────────────────────────────────────
    void setOnValueChanged(ValueChangedCallback cb)       { m_onValueChanged      = cb; }
    void setOnAdjustmentConfirm(ConfirmCallback cb)       { m_onAdjustmentConfirm = cb; }
    void setOnSubItemSelected(SubItemSelectedCallback cb) { m_onSubItemSelected   = cb; }

    // ── Preview accessors ─────────────────────────────────────────────────
    FrameLayout* getPreviewContainer() const { return m_previewContainer; }
    ImageView*   getPreviewImage()     const { return m_previewImage; }
    void         applyAllRowStyles();

    // ── Overrides ─────────────────────────────────────────────────────────
    void onMeasure(MeasureSpec widthSpec, MeasureSpec heightSpec) override;
    bool onKeyEvent(const IO::KeyEvent& event) override;
    bool onMotionEvent(const IO::MotionEvent& event) override;
    void onDraw(ICanvas& canvas) override;
    void update(float deltaTime);

protected:
    void layoutChildren() override;

private:
    // ── UI setup ──────────────────────────────────────────────────────────
    void setupUI();

    // ── Content build / update ────────────────────────────────────────────
    void        buildRow(LinearLayout* colLayout, MenuItem* item, int col, int rowIdx, const GridRowDef& def);
    void        updateContent(MenuItem* item);
    void        applyRowStyle(MenuItem* item);
    void        refreshSliderDisplay(MenuItem* item);
    void        refreshAllSliderDisplays();

    // ── Selection helpers ─────────────────────────────────────────────────
    MenuItem*   selectedItem()                   const;
    int         flatIndexOf(int col, int rowIdx) const;
    int         maxRowInCol(int col)             const;
    int         maxCol()                         const;

    // ── Navigation ────────────────────────────────────────────────────────
    void  moveRow(int delta);
    void  moveCol(int delta);
    void  moveOption(int delta);
    void  moveDigit(int delta);
    void  adjustDigit(int delta);
    void  adjustValue(float delta);
    void  showNumpad();

    // ── Helpers ───────────────────────────────────────────────────────────
    // true when row should be VERTICAL (standard mode + preview visible)
    bool               isVerticalRow()              const;
    static std::string formatValue(float v, float minV, float maxV, bool zeroPad = false);
    static float       clampValue(float v, float minV, float maxV);

    // ── Members ───────────────────────────────────────────────────────────
    MenuItem*     m_currentItem;
    bool          m_keyboardActive;
    bool          m_isAdjusting;
    bool          m_gridMode;
    int           m_gridColumns;

    int           m_selCol;
    int           m_selRow;
    int           m_digitIndex;

    float         m_previewWeight;
    float         m_subItemsWeight;

    FrameLayout*  m_previewContainer;
    ImageView*    m_previewImage;
    LinearLayout* m_subItemsContainer;

    std::vector<GridColumnDef> m_gridColDefs;
    std::vector<LinearLayout*> m_colLayouts;
    std::vector<MenuItem*>     m_rows;

    static constexpr int MAX_GRID_COLS = 8;

    ValueChangedCallback    m_onValueChanged;
    ConfirmCallback         m_onAdjustmentConfirm;
    SubItemSelectedCallback m_onSubItemSelected;
    NumpadRequestCallback   m_onNumpadRequest;
};

// ── FileMenuView ──────────────────────────────────────────────────────────────
// DVR file-browser / playback layout.
// Left block (55%): logo, preview image, control bar.
// Right block (45%): channel column | file list column.
//
//  ┌─────────────────────────────┬──────────┬───────────────────────────┐
//  │       LEFT BLOCK (~55%)     │ CHANNEL  │       FILE LIST           │
//  │ ┌─────────────────────────┐ │  COLUMN  │  COLUMN                   │
//  │ │       LOGO IMAGE        │ │          │ ┌─────────────────────┐   │
//  │ └─────────────────────────┘ │ [  1  ]  │ │  Group Range        │   │
//  │ ┌─────────────────────────┐ │ [  2  ]  │ │  < 101~200 (2/N) >  │   │
//  │ │                         │ │ [  3  ]  │ └─────────────────────┘   │
//  │ │     PREVIEW IMAGE       │ │ [  4  ]  │ ┌─────────────────────┐   │
//  │ │   (video texture here)  │ │ [ 1-4 ]  │ │  Group Copy Btn     │   │
//  │ │                         │ │ [  5  ]  │ └─────────────────────┘   │
//  │ └─────────────────────────┘ │ [  6  ]  │ ┌─────────────────────┐   │
//  │ ┌─────────────┬───────────┐ │ [  7  ]  │ │  File List          │   │
//  │ │  TIME LABEL │ << ▶ ■ >>│ │ [  8  ]  │ │  (scrollable)       │   │
//  │ └─────────────┴───────────┘ │ [ 5-8 ]  │ └─────────────────────┘   │
//  └─────────────────────────────┴──────────┴───────────────────────────┘

class FileMenuView : public FrameLayout
{
public:
    enum class FocusZone { GROUP_RANGE, GROUP_COPY, FILE_LIST, CHANNEL, CONTROL, PREVIEW };

    static constexpr float FS_HUD_HEIGHT        = 120.0f;
    static constexpr float FS_HUD_MARGIN_BOT    = 16.0f;
    static constexpr float FS_HUD_TIMEOUT       = 4.0f;
    static constexpr float FS_SUBTITLE_SIZE     = 40.0f;
    static constexpr float NORMAL_SUBTITLE_SIZE = 24.0f;
    static constexpr int   MAX_VISIBLE_FILES    = 7;

    FileMenuView();

    // ── MenuItem binding ──────────────────────────────────────────────────
    void      setMenuItem(MenuItem* item);
    MenuItem* getMenuItem()      const { return m_currentItem; }

    // ── View accessors ────────────────────────────────────────────────────
    ImageView* getPreviewImage() const { return m_previewImage; }
    ImageView* getLogoImage()    const { return m_logoImage; }

    // ── Callbacks ─────────────────────────────────────────────────────────
    void setOnFileSelected(std::function<void(int,int)> cb)       { m_onFileSelected       = std::move(cb); }
    void setOnFileUnload(std::function<void()> cb)                { m_onFileUnload         = std::move(cb); }
    void setOnDirectoryNavigated(std::function<void(int,int)> cb) { m_onDirectoryNavigated = std::move(cb); }
    void setOnFileCopy(std::function<void(int,int)> cb)           { m_onFileCopy           = std::move(cb); }
    void setOnFileDelete(std::function<void(int,int)> cb)         { m_onFileDelete         = std::move(cb); }
    void setOnGroupCopy(std::function<void(int)> cb)              { m_onGroupCopy          = std::move(cb); }

    void setOnVolumeChanged(std::function<void(float)> cb) { m_onVolumeChanged = std::move(cb); }
    void setVolume(float volume);

    // ── File / playback state ─────────────────────────────────────────────
    void setFileLoaded(bool loaded);
    bool isFileLoaded() const                  { return m_fileLoaded; }
    void onFileOpened(const std::vector<bool>& channelAvailable);

    void populateFileList(VPB::FileManager* mgr, int groupIdx, int groupCount, int fileCount);
    void setGroupRangeLabel(const std::string& text);
    void setPlaybackTime(const std::string& text);
    void setPlaybackState(VPB::PlaybackState state);
    void setSubtitleLines(const std::string& line1, const std::string& line2);

    // ── Channel ───────────────────────────────────────────────────────────
    void refreshChannelHighlight(int channelIdx);
    void setChannelAvailability(const std::vector<bool>& available);
    int  getChannelButtonCount() const         { return static_cast<int>(m_channelButtons.size()); }

    // ── Focus zone ────────────────────────────────────────────────────────
    void      setFocusZone(FocusZone zone);
    FocusZone getFocusZone() const             { return m_focusZone; }
    void      setContentFocused(bool focused);

    // ── Fullscreen preview ────────────────────────────────────────────────
    void setFullscreenRootLayout(FrameLayout* rootLayout) { m_fsRootLayout = rootLayout; }
    void togglePreviewFullscreen();
    void enterPreviewFullscreen();
    void exitPreviewFullscreen();
    bool isPreviewFullscreen() const           { return m_previewFullscreen; }
    void resetFsHudTimer();

    // ── File list selection ───────────────────────────────────────────────
    int  getSelectedFileIndex()    const       { return m_selectedFileIndex; }
    int  getSelectedGroupIndex()   const       { return m_selectedGroupIndex; }
    int  getSelectedChannelIndex() const       { return m_selectedChannelIndex; }
    void setSelectedFileIndex(int index);

    // ── Event handling ────────────────────────────────────────────────────
    bool handleKeyEvent(const IO::KeyEvent& event);
    bool onKeyEvent(const IO::KeyEvent& event) override;
    bool onMotionEvent(const IO::MotionEvent& event) override;
    void onDraw(ICanvas& canvas) override;
    void update(float deltaTime);

private:
    // ── Per-row file action widgets ────────────────────────────────────────
    struct FileRowActions
    {
        LinearLayout* row         = nullptr;
        TextView*     nameLabel   = nullptr;
        LinearLayout* actionPanel = nullptr;
        TextView*     actionLabel = nullptr;
        Button*       openBtn     = nullptr;
        Button*       copyBtn     = nullptr;
        Button*       deleteBtn   = nullptr;
        bool          isDirectory = false;
    };

    // ── Layout construction ───────────────────────────────────────────────
    void buildLayout();
    void buildLeftBlock();
    void buildControlBar();
    void buildFsHud();
    void buildRightBlock();
    void buildChannelColumn();
    void buildFileListColumn();

    // ── HUD helpers ───────────────────────────────────────────────────────
    void showFsHud();
    void hideFsHud();
    void updateSubtitleMargin();
    void refreshFsHudFileLabel();
    void applyHudFocusVisuals(View* focused);
    void activateFocusedHudButton();

    // ── Zone / highlight helpers ──────────────────────────────────────────
    void highlightZone(FocusZone zone);
    void highlightActionButtons(int slot);
    void hideActionButtons(int slot);
    void refreshFileListSlots();

    // ── Action helpers ────────────────────────────────────────────────────
    void activateSelectedFile();
    void activateSelectedChannel();
    void activateControl(int index);
    void stepChannel(int dir);
    void stepVolume(int dir);

    // ── State ─────────────────────────────────────────────────────────────
    MenuItem*         m_currentItem;
    VPB::FileManager* m_mgr;
    FocusZone         m_focusZone;

    int  m_selectedFileIndex;
    int  m_selectedGroupIndex;
    int  m_selectedChannelIndex;
    int  m_selectedControlIndex;
    int  m_fileScrollOffset;
    int  m_groupFileCount;
    int  m_totalFileCount;
    int  m_selectedActionIndex;   // -1 = none; 0 = Open, 1 = Copy, 2 = Delete
    int  m_playBtnSlot;

    bool  m_fileDragging;
    bool  m_fileListMouseDown;
    bool  m_fileLoaded;
    bool  m_actionModeActive;
    bool  m_previewFullscreen;
    bool  m_fsHudVisible;
    float m_fileDragStartY;
    float m_fileDragTotalY;
    float m_fsHudTimer;
    int   m_fileDragStartOffset;

    // ── Layout views ──────────────────────────────────────────────────────
    LinearLayout* m_rootRow;
    LinearLayout* m_leftBlock;
    LinearLayout* m_rightBlock;
    LinearLayout* m_controlBar;
    LinearLayout* m_channelColumn;
    LinearLayout* m_fileListColumn;
    LinearLayout* m_groupRangeRow;
    LinearLayout* m_groupCopyRow;

    ImageView*   m_logoImage;
    FrameLayout* m_previewContainer;
    ImageView*   m_previewImage;
    Button*      m_previewFsButton;
    FrameLayout* m_fsRootLayout;
    FrameLayout* m_fsHud;

    TextView* m_timeLabel;
    LinearLayout* m_subtitleContainer;
    TextView*     m_subtitleLine1;
    TextView*     m_subtitleLine2;
    TextView* m_channelLabel;
    TextView* m_groupListLabel;
    TextView* m_fileListLabel;
    TextView* m_groupRangeLabel;
    TextView*   m_fsHudFileLabel;
    TextView*   m_fsHudTimeLabel;
    SliderView* m_fsHudVolumeSlider;

    Button* m_groupRangePrev;
    Button* m_groupRangeNext;
    Button* m_groupCopyBtn;

    std::vector<Button*>        m_controlButtons;
    std::vector<Button*>        m_channelButtons;
    std::vector<bool>           m_channelAvailable;
    std::vector<Button*>        m_fsHudControlButtons;
    std::vector<Button*>        m_fsHudChannelButtons;
    std::vector<FileRowActions> m_fileRowActions;

    FocusManager                  m_hudFocusManager;
    std::shared_ptr<FocusContext> m_hudFocusContext;

    // ── Callbacks ─────────────────────────────────────────────────────────
    std::function<void(int,int)> m_onFileSelected;
    std::function<void()>        m_onFileUnload;
    std::function<void(int,int)> m_onDirectoryNavigated;
    std::function<void(int,int)> m_onFileCopy;
    std::function<void(int,int)> m_onFileDelete;
    std::function<void(int)>     m_onGroupCopy;
    std::function<void(float)>   m_onVolumeChanged;
};

// ── MenuNavigator ─────────────────────────────────────────────────────────────

enum class MenuState { MAIN_MENU, SUB_MENU, FILE_MENU, ADJUSTING };

class MenuNavigator
{
public:
    using BackCallback = std::function<void()>;
    using SaveCallback = std::function<void()>;

    MenuNavigator();
    ~MenuNavigator();

    bool initialize(MenuItem* rootItem);
    void cleanup();

    void         setFocusManager(FocusManager* fm) { m_focusManager = fm; }
    FrameLayout* getRootMenuView()                 { return m_rootMenuView; }

    // ── Navigation ────────────────────────────────────────────────────────
    void navigateToItem(MenuItem* item);
    void navigateBack();
    void navigateToRoot();
    void showSubMenu(MenuItem* item);

    // ── State ─────────────────────────────────────────────────────────────
    MenuItem* getCurrentItem() const               { return m_currentItem; }
    MenuState getState()       const               { return m_state; }
    bool      isAtRoot()       const               { return m_navigationStack.empty() == true && m_state == MenuState::MAIN_MENU; }
    bool      isFileMenuActive() const             { return m_state == MenuState::FILE_MENU; }

    // ── Focus / content ───────────────────────────────────────────────────
    void setContentFocused(bool focused);
    void clearMainMenuSelection();
    void restoreMainMenuSelection();
    void clearSubMenuSelection();
    void restoreSubMenuSelection();

    // ── Sub-menu helpers ──────────────────────────────────────────────────
    int  getSubMenuSelectedIndex() const;
    void setSubMenuSelectedIndex(int index);
    void setSubMenuAdjustmentMode(bool adjusting);
    void setSubMenuOnAdjustmentConfirm(SubMenuView::ConfirmCallback cb);
    void setSubMenuNumpadCallback(SubMenuView::NumpadRequestCallback cb);
    void refreshSubMenuContent();

    // ── Sub-menu layout weight control ────────────────────────────────────
    void setSubMenuLayoutWeights(float previewWeight, float subItemsWeight);
    void setSubMenuGridMode(bool enabled, int numColumns = 3);
    void setSubMenuGridColumns(const std::vector<SubMenuView::GridColumnDef>& colDefs);

    // ── Sub-menu view accessor ────────────────────────────────────────────
    SubMenuView* getSubMenuView() const            { return m_subMenuView; }

    // ── File menu ─────────────────────────────────────────────────────────
    FileMenuView* getFileMenuView() const              { return m_fileMenuView; }
    void          addFileMenuId(const std::string& id) { m_fileMenuIds.push_back(id); }

    // ── Callbacks ─────────────────────────────────────────────────────────
    void setOnBackToApp(BackCallback cb)           { m_onBackToApp    = cb; }
    void setOnSave(SaveCallback cb)                { m_onSave         = cb; }
    void setOnFileMenuExit(BackCallback cb)        { m_onFileMenuExit = cb; }

    // ── Event / update ────────────────────────────────────────────────────
    bool handleKeyEvent(const IO::KeyEvent& event);
    bool handleMotionEvent(const IO::MotionEvent& event);
    void update(float deltaTime);

private:
    void showMainMenu();
    void hideAllViews();
    bool isFileMenuItem(MenuItem* item) const;

    MenuItem*     m_rootItem;
    MenuItem*     m_currentItem;
    MenuState     m_state;
    FocusManager* m_focusManager;

    FrameLayout*  m_rootMenuView;
    MainMenuView* m_mainMenuView;
    SubMenuView*  m_subMenuView;
    FileMenuView* m_fileMenuView;

    std::vector<std::string> m_fileMenuIds;
    std::vector<MenuItem*>   m_navigationStack;

    BackCallback  m_onBackToApp;
    SaveCallback  m_onSave;
    BackCallback  m_onFileMenuExit;
    int           m_savedMainMenuIndex;
    int           m_savedSubMenuIndex;
};

} // namespace UI
} // namespace APP

#endif // UI_MENU_H