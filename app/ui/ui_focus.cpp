#include "ui_focus.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace APP
{

namespace UI
{

// ============================================================================
// FocusContext
// ============================================================================

FocusContext::FocusContext(FocusContextType type)
    : m_type(type)
    , m_zOrder(type == FocusContextType::POPUP ? 100 : (type == FocusContextType::OVERLAY ? 50 : 0))
    , m_blocking(type == FocusContextType::POPUP || type == FocusContextType::OVERLAY)
    , m_active(true)
    , m_navStrategy(NavigationStrategy::GEOMETRIC)
    , m_gridColumns(3)
    , m_wrapAround(false)
{
}

void FocusContext::addView(View* view)
{
    if (view == nullptr) return;
    for (View* existing : m_views)
        if (existing == view) return;
    m_views.push_back(view);
}

void FocusContext::removeView(View* view)
{
    if (view == nullptr) return;
    for (auto it = m_views.begin(); it != m_views.end(); ++it)
    {
        if (*it == view) { m_views.erase(it); return; }
    }
}

bool FocusContext::hasView(View* view) const
{
    if (view == nullptr) return false;
    for (View* existing : m_views)
        if (existing == view) return true;
    return false;
}

void FocusContext::clearViews() { m_views.clear(); }

// ============================================================================
// FocusManager
// ============================================================================

FocusManager::FocusManager()
    : m_focusedView(nullptr)
    , m_defaultStrategy(NavigationStrategy::GEOMETRIC)
    , m_keyRepeatDelay(0.5f)
    , m_keyRepeatInterval(0.1f)
    , m_onFocusChanged(nullptr)
    , m_lastInputSource(IO::Source::KEYBOARD)
{
}

FocusManager::~FocusManager()
{
    clearFocus();
    m_focusables.clear();
    m_contexts.clear();
    m_focusLinks.clear();
}

void FocusManager::setFocus(View* view)
{
    if (m_focusedView == view) return;
    if (view != nullptr && canInteractWithView(view) == false) return;

    View* oldFocus = m_focusedView;
    m_focusedView  = view;

    if (oldFocus != nullptr)    oldFocus->onFocusChanged(false);
    if (m_focusedView != nullptr) m_focusedView->onFocusChanged(true);
    if (m_onFocusChanged != nullptr) m_onFocusChanged(oldFocus, m_focusedView);
}

void FocusManager::clearFocus() { setFocus(nullptr); }

std::shared_ptr<FocusContext> FocusManager::createContext(FocusContextType type)
{
    auto context = std::make_shared<FocusContext>(type);
    m_contexts.push_back(context);
    std::sort(m_contexts.begin(), m_contexts.end(),
        [](const std::shared_ptr<FocusContext>& a, const std::shared_ptr<FocusContext>& b) {
            return a->getZOrder() > b->getZOrder();
        });
    return context;
}

void FocusManager::removeContext(std::shared_ptr<FocusContext> context)
{
    if (context == nullptr) return;
    for (auto it = m_contexts.begin(); it != m_contexts.end(); ++it)
    {
        if (*it != context) continue;
        for (auto& info : m_focusables)
            if (info.context == context) info.context = nullptr;
        m_contexts.erase(it);
        return;
    }
}

std::shared_ptr<FocusContext> FocusManager::getActiveContext() const
{
    for (const auto& context : m_contexts)
        if (context->isActive() == true) return context;
    return nullptr;
}

std::shared_ptr<FocusContext> FocusManager::getContextForView(View* view) const
{
    const FocusableInfo* info = findFocusableInfo(view);
    return (info != nullptr) ? info->context : nullptr;
}

std::shared_ptr<FocusContext> FocusManager::getHighestPriorityBlockingContext() const
{
    for (const auto& context : m_contexts)
        if (context->isActive() == true && context->isBlocking() == true) return context;
    return nullptr;
}

void FocusManager::registerView(View* view, std::shared_ptr<FocusContext> context)
{
    if (view == nullptr) return;
    unregisterView(view);

    FocusableInfo info;
    info.view    = view;
    info.context = context;
    updateFocusableInfo(view);
    m_focusables.push_back(info);

    if (context != nullptr) context->addView(view);
}

void FocusManager::unregisterView(View* view)
{
    if (view == nullptr) return;
    if (m_focusedView == view) clearFocus();

    for (auto it = m_focusables.begin(); it != m_focusables.end(); ++it)
    {
        if (it->view != view) continue;
        if (it->context != nullptr) it->context->removeView(view);
        m_focusables.erase(it);
        break;
    }
    clearFocusLinks(view);
}

bool FocusManager::isViewRegistered(View* view) const { return findFocusableInfo(view) != nullptr; }

void FocusManager::setFocusLink(View* source, View* target, IO::KeyCode direction)
{
    if (source == nullptr || target == nullptr) return;
    for (auto& link : m_focusLinks)
    {
        if (link.source == source && link.direction == direction)
        {
            link.target = target;
            return;
        }
    }
    m_focusLinks.push_back(FocusLink(source, target, direction));
}

void FocusManager::clearFocusLinks(View* view)
{
    if (view == nullptr) return;
    for (auto it = m_focusLinks.begin(); it != m_focusLinks.end();)
    {
        if (it->source == view || it->target == view) it = m_focusLinks.erase(it);
        else ++it;
    }
}

View* FocusManager::findNextFocusable(IO::KeyCode direction)
{
    if (m_focusedView == nullptr) return nullptr;

    View* customNext = findNextCustom(m_focusedView, direction);
    if (customNext != nullptr) return customNext;

    std::shared_ptr<FocusContext> context = getContextForView(m_focusedView);
    if (context != nullptr)
    {
        switch (context->getNavigationStrategy())
        {
            case NavigationStrategy::GRID:
                return findNextGrid(m_focusedView, direction, context.get());
            case NavigationStrategy::LINEAR:
            {
                std::vector<View*> views = getVisibleFocusablesInContext(context);
                return findNextLinear(m_focusedView, direction, views);
            }
            case NavigationStrategy::GEOMETRIC:
                return findNextGeometric(m_focusedView, direction);
            case NavigationStrategy::CUSTOM:
                return nullptr;
        }
    }
    return findNextGeometric(m_focusedView, direction);
}

View* FocusManager::findFocusableAt(float x, float y)
{
    auto blockingContext = getHighestPriorityBlockingContext();
    for (auto it = m_focusables.rbegin(); it != m_focusables.rend(); ++it)
    {
        View* view = it->view;
        if (view == nullptr || view->isVisible() == false || view->isFocusable() == false) continue;
        if (blockingContext != nullptr && it->context != blockingContext) continue;
        if (view->getBounds().contains(x, y) == true)
        {
            updateFocusableInfo(view);
            return view;
        }
    }
    return nullptr;
}

void FocusManager::update(float deltaTime)
{
    for (auto& info : m_focusables)
        if (info.view != nullptr && info.view->isVisible() == true) updateFocusableInfo(info.view);
}

bool FocusManager::handleKeyEvent(const IO::KeyEvent& event)
{
    if (event.action != IO::KeyAction::DOWN && event.action != IO::KeyAction::MULTIPLE) return false;
    setLastInputSource(event.source);

    IO::KeyCode dir = event.keyCode;
    if (dir != IO::KeyCode::DPAD_UP   && dir != IO::KeyCode::DPAD_DOWN &&
        dir != IO::KeyCode::DPAD_LEFT && dir != IO::KeyCode::DPAD_RIGHT)
        return false;

    View* nextView = findNextFocusable(dir);
    if (nextView != nullptr) { setFocus(nextView); return true; }
    return false;
}

bool FocusManager::handleMotionEvent(const IO::MotionEvent& event)
{
    setLastInputSource(event.source);
    if (shouldAutoFocusOnPointerDown(event) == true && event.action == IO::MotionAction::DOWN)
    {
        View* viewAtPoint = findFocusableAt(event.x, event.y);
        if (viewAtPoint != nullptr) { setFocus(viewAtPoint); return true; }
    }
    return false;
}

bool FocusManager::canInteractWithView(View* view) const
{
    if (view == nullptr || view->isVisible() == false || view->isEnabled() == false) return false;
    const FocusableInfo* info = findFocusableInfo(view);
    if (info == nullptr) return false;

    auto blockingContext = getHighestPriorityBlockingContext();
    if (blockingContext != nullptr) return info->context == blockingContext;
    return true;
}

FocusManager::FocusableInfo* FocusManager::findFocusableInfo(View* view)
{
    for (auto& info : m_focusables)
        if (info.view == view) return &info;
    return nullptr;
}

const FocusManager::FocusableInfo* FocusManager::findFocusableInfo(View* view) const
{
    for (const auto& info : m_focusables)
        if (info.view == view) return &info;
    return nullptr;
}

void FocusManager::updateFocusableInfo(View* view)
{
    FocusableInfo* info = findFocusableInfo(view);
    if (info != nullptr)
    {
        info->bounds         = view->getBounds();
        info->hierarchyLevel = calculateHierarchyLevel(view);
    }
}

int FocusManager::calculateHierarchyLevel(View* view) const
{
    int level = 0;
    ViewGroup* parent = view->getParent();
    while (parent != nullptr) { level++; parent = parent->getParent(); }
    return level;
}

View* FocusManager::findNextGeometric(View* current, IO::KeyCode direction)
{
    if (current == nullptr) return nullptr;
    updateFocusableInfo(current);

    const FocusableInfo* currentInfo = findFocusableInfo(current);
    if (currentInfo == nullptr) return nullptr;

    Vec2  currentCenter  = currentInfo->bounds.center();
    View* bestCandidate  = nullptr;
    float bestScore      = std::numeric_limits<float>::max();
    auto  blockingContext = getHighestPriorityBlockingContext();

    for (auto& info : m_focusables)
    {
        View* candidate = info.view;
        if (candidate == current || candidate == nullptr ||
            candidate->isVisible() == false || candidate->isFocusable() == false) continue;
        if (blockingContext != nullptr && info.context != blockingContext) continue;

        Vec2 candidateCenter = info.bounds.center();
        if (isInDirection(currentCenter, candidateCenter, direction) == false) continue;

        float score = calculateDirectionalScore(currentCenter, candidateCenter, direction);
        if (score < bestScore) { bestScore = score; bestCandidate = candidate; }
    }
    return bestCandidate;
}

View* FocusManager::findNextLinear(View* current, IO::KeyCode direction, const std::vector<View*>& views)
{
    if (current == nullptr || views.empty() == true) return nullptr;

    int currentIndex = -1;
    for (size_t i = 0; i < views.size(); ++i)
        if (views[i] == current) { currentIndex = static_cast<int>(i); break; }
    if (currentIndex == -1) return nullptr;

    int nextIndex = currentIndex;
    int size      = static_cast<int>(views.size());

    if (direction == IO::KeyCode::DPAD_RIGHT || direction == IO::KeyCode::DPAD_DOWN)
        nextIndex = (currentIndex + 1) % size;
    else if (direction == IO::KeyCode::DPAD_LEFT || direction == IO::KeyCode::DPAD_UP)
        nextIndex = (currentIndex - 1 + size) % size;

    if (nextIndex != currentIndex && nextIndex >= 0 && nextIndex < size) return views[nextIndex];
    return nullptr;
}

View* FocusManager::findNextGrid(View* current, IO::KeyCode direction, FocusContext* context)
{
    if (current == nullptr || context == nullptr) return nullptr;

    const std::vector<View*>& views = context->getViews();
    if (views.empty() == true) return nullptr;

    int currentIndex = -1;
    for (size_t i = 0; i < views.size(); ++i)
        if (views[i] == current) { currentIndex = static_cast<int>(i); break; }
    if (currentIndex == -1) return nullptr;

    int cols       = context->getGridColumns();
    int size       = static_cast<int>(views.size());
    int rows       = (size + cols - 1) / cols;
    int currentRow = currentIndex / cols;
    int currentCol = currentIndex % cols;
    int nextIndex  = -1;

    switch (direction)
    {
        case IO::KeyCode::DPAD_RIGHT:
            if (currentCol < cols - 1)
            {
                nextIndex = currentIndex + 1;
                if (nextIndex >= size) nextIndex = -1;
            }
            else if (context->isWrapAround() == true) nextIndex = currentRow * cols;
            break;

        case IO::KeyCode::DPAD_LEFT:
            if (currentCol > 0)
                nextIndex = currentIndex - 1;
            else if (context->isWrapAround() == true)
                nextIndex = std::min(currentRow * cols + cols - 1, size - 1);
            break;

        case IO::KeyCode::DPAD_DOWN:
            if (currentRow < rows - 1)
            {
                nextIndex = currentIndex + cols;
                if (nextIndex >= size) nextIndex = -1;
            }
            else if (context->isWrapAround() == true)
            {
                nextIndex = currentCol;
                if (nextIndex >= size) nextIndex = -1;
            }
            break;

        case IO::KeyCode::DPAD_UP:
            if (currentRow > 0)
                nextIndex = currentIndex - cols;
            else if (context->isWrapAround() == true)
            {
                int lastRowStart = (rows - 1) * cols;
                nextIndex = lastRowStart + currentCol;
                if (nextIndex >= size)
                    nextIndex = lastRowStart + std::min(currentCol, size - 1 - lastRowStart);
            }
            break;

        default: break;
    }

    if (nextIndex >= 0 && nextIndex < size)
    {
        View* nextView = views[nextIndex];
        if (nextView != nullptr && nextView->isVisible() == true && nextView->isFocusable() == true)
            return nextView;
    }
    return nullptr;
}

View* FocusManager::findNextCustom(View* current, IO::KeyCode direction)
{
    for (const auto& link : m_focusLinks)
    {
        if (link.source != current || link.direction != direction) continue;
        if (link.target != nullptr &&
            link.target->isVisible() == true  &&
            link.target->isFocusable() == true &&
            canInteractWithView(link.target) == true)
            return link.target;
    }
    return nullptr;
}

float FocusManager::calculateDistance(const Vec2& from, const Vec2& to) const
{
    float dx = to.x - from.x;
    float dy = to.y - from.y;
    return std::sqrt(dx * dx + dy * dy);
}

bool FocusManager::isInDirection(const Vec2& from, const Vec2& to, IO::KeyCode direction) const
{
    float dx = to.x - from.x;
    float dy = to.y - from.y;
    constexpr float threshold = 0.01f;
    switch (direction)
    {
        case IO::KeyCode::DPAD_UP:    return dy < -threshold;
        case IO::KeyCode::DPAD_DOWN:  return dy >  threshold;
        case IO::KeyCode::DPAD_LEFT:  return dx < -threshold;
        case IO::KeyCode::DPAD_RIGHT: return dx >  threshold;
        default: return false;
    }
}

float FocusManager::calculateDirectionalScore(const Vec2& from, const Vec2& to, IO::KeyCode direction) const
{
    float dx = to.x - from.x;
    float dy = to.y - from.y;
    float mainAxis  = 0.0f;
    float crossAxis = 0.0f;

    switch (direction)
    {
        case IO::KeyCode::DPAD_UP:
        case IO::KeyCode::DPAD_DOWN:  mainAxis = std::abs(dy); crossAxis = std::abs(dx); break;
        case IO::KeyCode::DPAD_LEFT:
        case IO::KeyCode::DPAD_RIGHT: mainAxis = std::abs(dx); crossAxis = std::abs(dy); break;
        default: return std::numeric_limits<float>::max();
    }
    return mainAxis + (crossAxis * 2.0f);
}

std::vector<View*> FocusManager::getVisibleFocusablesInContext(std::shared_ptr<FocusContext> context) const
{
    std::vector<View*> result;
    if (context == nullptr) return result;
    for (View* view : context->getViews())
    {
        if (view != nullptr && view->isEnabled() == true &&
            view->isVisible() == true && view->isFocusable() == true)
            result.push_back(view);
    }
    return result;
}

bool FocusManager::canInteractWithContext(std::shared_ptr<FocusContext> context) const
{
    if (context == nullptr) return false;
    auto blockingContext = getHighestPriorityBlockingContext();
    if (blockingContext != nullptr) return context == blockingContext;
    return context->isActive() == true;
}

bool FocusManager::shouldAutoFocusOnPointerDown(const IO::MotionEvent& event) const
{
    return event.source == IO::Source::MOUSE || event.source == IO::Source::TOUCHSCREEN;
}

} // namespace UI

} // namespace APP