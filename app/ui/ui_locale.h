#ifndef UI_LOCALE_H
#define UI_LOCALE_H

#include <string>
#include <unordered_map>
#include <functional>
#include <vector>

// Comment out to disable localization support
#define USE_LOCALIZATION

namespace APP
{
namespace UI
{

class View;
class MenuItem;

#ifdef USE_LOCALIZATION

class LocalizationManager
{
public:
    using LanguageChangeCallback = std::function<void(const std::string&)>;
    using LocalizableView        = std::pair<View*, std::string>;
    using LocalizableMenuItem    = std::pair<MenuItem*, std::string>;

    static LocalizationManager& getInstance() { static LocalizationManager i; return i; }

    void setLanguage(const std::string& lang);
    const std::string& getLanguage() const { return m_currentLanguage; }
    std::string getString(const std::string& key) const;
    void setString(const std::string& lang, const std::string& key, const std::string& value);
    void setOnLanguageChanged(LanguageChangeCallback cb) { m_onLanguageChanged = cb; }

    void registerView(View* view, const std::string& key);
    void unregisterView(View* view);
    void registerMenuItem(MenuItem* item, const std::string& key);
    void unregisterMenuItem(MenuItem* item);
    void clearAll();
    void refreshAllViews();

private:
    LocalizationManager();
    ~LocalizationManager() = default;
    LocalizationManager(const LocalizationManager&) = delete;
    LocalizationManager& operator=(const LocalizationManager&) = delete;

    void initializeDefaultStrings();

    std::string m_currentLanguage;
    std::unordered_map<std::string, std::unordered_map<std::string, std::string>> m_translations;
    LanguageChangeCallback m_onLanguageChanged;
    std::vector<LocalizableView> m_registeredViews;
    std::vector<LocalizableMenuItem> m_registeredMenuItems;
};

#define TR(key) (UI::LocalizationManager::getInstance().getString(key))

// Set text and register view for auto-update on language change.
#define SET_LOCALIZED_TEXT(view, key) \
    do { \
        (view)->setText(TR(key)); \
        UI::LocalizationManager::getInstance().registerView((view), (key)); \
    } while(0)

// Set name and register menu item for auto-update on language change.
#define SET_LOCALIZED_MENU_ITEM(item, key) \
    do { \
        (item)->setName(TR(key)); \
        UI::LocalizationManager::getInstance().registerMenuItem((item).get(), (key)); \
    } while(0)

#else  // !USE_LOCALIZATION

class LocalizationManager
{
public:
    static LocalizationManager& getInstance() { static LocalizationManager i; return i; }

    void setLanguage(const std::string&) {}
    const std::string& getLanguage() const { static std::string lang = "en"; return lang; }
    std::string getString(const std::string& key) const { return key; }
    void setString(const std::string&, const std::string&, const std::string&) {}
    void registerView(View*, const std::string&) {}
    void unregisterView(View*) {}
    void registerMenuItem(MenuItem*, const std::string&) {}
    void unregisterMenuItem(MenuItem*) {}
    void clearAll() {}
    void refreshAllViews() {}
};

#define TR(key) (key)
#define SET_LOCALIZED_TEXT(view, key)      do { (view)->setText(key); } while(0)
#define SET_LOCALIZED_MENU_ITEM(item, key) do { (item)->setName(key); } while(0)

#endif  // USE_LOCALIZATION

} // namespace UI
} // namespace APP

#endif // UI_LOCALE_H