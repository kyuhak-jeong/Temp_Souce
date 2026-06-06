#ifndef UI_FOCUS_H
#define UI_FOCUS_H

#include "ui_core.h"
#include "constants.h"
#include <vector>
#include <functional>
#include <memory>

namespace APP
{

namespace UI
{

class FocusManager;
class FocusContext;

enum class FocusContextType { NORMAL, POPUP, OVERLAY };
enum class NavigationStrategy { GEOMETRIC, LINEAR, GRID, CUSTOM };

class FocusContext
{
public:
    FocusContext(FocusContextType type = FocusContextType::NORMAL);
    
    FocusContextType getType() const { return m_type; }
    int getZOrder() const { return m_zOrder; }
    void setZOrder(int z) { m_zOrder = z; }
    bool isBlocking() const { return m_blocking; }
    void setBlocking(bool blocking) { m_blocking = blocking; }
    bool isActive() const { return m_active; }
    void setActive(bool active) { m_active = active; }
    
    void setNavigationStrategy(NavigationStrategy strategy) { m_navStrategy = strategy; }
    NavigationStrategy getNavigationStrategy() const { return m_navStrategy; }
    void setGridColumns(int columns) { m_gridColumns = columns; }
    int getGridColumns() const { return m_gridColumns; }
    void setWrapAround(bool wrap) { m_wrapAround = wrap; }
    bool isWrapAround() const { return m_wrapAround; }
    
    void addView(View* view);
    void removeView(View* view);
    bool hasView(View* view) const;
    void clearViews();
    const std::vector<View*>& getViews() const { return m_views; }
    
private:
    FocusContextType m_type;
    int m_zOrder;
    bool m_blocking;
    bool m_active;
    NavigationStrategy m_navStrategy;
    int m_gridColumns;
    bool m_wrapAround;
    std::vector<View*> m_views;
};

struct FocusLink
{
    View* source;
    View* target;
    IO::KeyCode direction;
    
    FocusLink() : source(nullptr), target(nullptr), direction(IO::KeyCode::UNKNOWN) {}
    FocusLink(View* src, View* tgt, IO::KeyCode dir) : source(src), target(tgt), direction(dir) {}
};

class FocusManager
{
public:
    using FocusChangeCallback = std::function<void(View* oldFocus, View* newFocus)>;
    
    FocusManager();
    ~FocusManager();
    
    void setFocus(View* view);
    View* getFocusedView() const { return m_focusedView; }
    void clearFocus();
    
    std::shared_ptr<FocusContext> createContext(FocusContextType type = FocusContextType::NORMAL);
    void removeContext(std::shared_ptr<FocusContext> context);
    std::shared_ptr<FocusContext> getActiveContext() const;
    std::shared_ptr<FocusContext> getContextForView(View* view) const;
    std::shared_ptr<FocusContext> getHighestPriorityBlockingContext() const;
    
    void registerView(View* view, std::shared_ptr<FocusContext> context = nullptr);
    void unregisterView(View* view);
    bool isViewRegistered(View* view) const;
    
    void setFocusLink(View* source, View* target, IO::KeyCode direction);
    void clearFocusLinks(View* view);
    
    View* findNextFocusable(IO::KeyCode direction);
    View* findFocusableAt(float x, float y);
    
    void setLastInputSource(IO::Source source) { m_lastInputSource = source; }
    IO::Source getLastInputSource() const { return m_lastInputSource; }
    bool isKeyboardNavigation() const { return m_lastInputSource == IO::Source::KEYBOARD || m_lastInputSource == IO::Source::DPAD; }
    
    void setDefaultStrategy(NavigationStrategy strategy) { m_defaultStrategy = strategy; }
    NavigationStrategy getDefaultStrategy() const { return m_defaultStrategy; }
    
    void update(float deltaTime);
    bool handleKeyEvent(const IO::KeyEvent& event);
    bool handleMotionEvent(const IO::MotionEvent& event);
    
    void setOnFocusChanged(FocusChangeCallback callback) { m_onFocusChanged = callback; }
    
    bool canInteractWithView(View* view) const;
    
private:
    struct FocusableInfo
    {
        View* view;
        RectF bounds;
        int hierarchyLevel;
        std::shared_ptr<FocusContext> context;
        
        FocusableInfo() : view(nullptr), bounds(), hierarchyLevel(0), context(nullptr) {}
    };
    
    FocusableInfo* findFocusableInfo(View* view);
    const FocusableInfo* findFocusableInfo(View* view) const;
    void updateFocusableInfo(View* view);
    int calculateHierarchyLevel(View* view) const;
    
    View* findNextGeometric(View* current, IO::KeyCode direction);
    View* findNextLinear(View* current, IO::KeyCode direction, const std::vector<View*>& views);
    View* findNextGrid(View* current, IO::KeyCode direction, FocusContext* context);
    View* findNextCustom(View* current, IO::KeyCode direction);
    
    float calculateDistance(const Vec2& from, const Vec2& to) const;
    bool isInDirection(const Vec2& from, const Vec2& to, IO::KeyCode direction) const;
    float calculateDirectionalScore(const Vec2& from, const Vec2& to, IO::KeyCode direction) const;
    
    std::vector<View*> getVisibleFocusablesInContext(std::shared_ptr<FocusContext> context) const;
    bool canInteractWithContext(std::shared_ptr<FocusContext> context) const;
    bool shouldAutoFocusOnPointerDown(const IO::MotionEvent& event) const;
    
    View* m_focusedView;
    std::vector<FocusableInfo> m_focusables;
    std::vector<std::shared_ptr<FocusContext>> m_contexts;
    std::vector<FocusLink> m_focusLinks;
    NavigationStrategy m_defaultStrategy;
    float m_keyRepeatDelay;
    float m_keyRepeatInterval;
    FocusChangeCallback m_onFocusChanged;
    IO::Source m_lastInputSource;
};

} // namespace UI

} // namespace APP

#endif // UI_FOCUS_H