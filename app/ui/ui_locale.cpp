#include "ui_locale.h"

#ifdef USE_LOCALIZATION
    #include "ui_core.h"
    #include "ui_menu.h"
#endif

namespace APP
{

namespace UI
{

#ifdef USE_LOCALIZATION

// ============================================================================
// LocalizationManager
// ============================================================================

LocalizationManager::LocalizationManager()
    : m_currentLanguage("en")
    , m_onLanguageChanged(nullptr)
{
    initializeDefaultStrings();
}

void LocalizationManager::setLanguage(const std::string& lang)
{
    if (m_currentLanguage == lang) return;
    m_currentLanguage = lang;
    refreshAllViews();
    if (m_onLanguageChanged != nullptr) m_onLanguageChanged(lang);
}

std::string LocalizationManager::getString(const std::string& key) const
{
    auto langIt = m_translations.find(m_currentLanguage);
    if (langIt != m_translations.end())
    {
        auto keyIt = langIt->second.find(key);
        if (keyIt != langIt->second.end()) return keyIt->second;
    }
    if (m_currentLanguage != "en")
    {
        auto enIt = m_translations.find("en");
        if (enIt != m_translations.end())
        {
            auto keyIt = enIt->second.find(key);
            if (keyIt != enIt->second.end()) return keyIt->second;
        }
    }
    return key;
}

void LocalizationManager::setString(const std::string& lang, const std::string& key, const std::string& value)
{
    m_translations[lang][key] = value;
}

void LocalizationManager::registerView(View* view, const std::string& translationKey)
{
    if (view == nullptr || translationKey.empty()) return;
    for (auto& pair : m_registeredViews)
    {
        if (pair.first != view) continue;
        pair.second = translationKey;
        return;
    }
    m_registeredViews.push_back({view, translationKey});
}

void LocalizationManager::unregisterView(View* view)
{
    if (view == nullptr) return;
    m_registeredViews.erase(
        std::remove_if(m_registeredViews.begin(), m_registeredViews.end(),
            [view](const LocalizableView& pair) { return pair.first == view; }),
        m_registeredViews.end());
}

void LocalizationManager::registerMenuItem(MenuItem* item, const std::string& translationKey)
{
    if (item == nullptr || translationKey.empty()) return;
    for (auto& pair : m_registeredMenuItems)
    {
        if (pair.first != item) continue;
        pair.second = translationKey;
        return;
    }
    m_registeredMenuItems.push_back({item, translationKey});
}

void LocalizationManager::unregisterMenuItem(MenuItem* item)
{
    if (item == nullptr) return;
    m_registeredMenuItems.erase(
        std::remove_if(m_registeredMenuItems.begin(), m_registeredMenuItems.end(),
            [item](const LocalizableMenuItem& pair) { return pair.first == item; }),
        m_registeredMenuItems.end());
}

void LocalizationManager::clearAll()
{
    m_registeredViews.clear();
    m_registeredMenuItems.clear();
    std::cout << "\n LocalizationManager clear completed\n" << std::endl;
}

void LocalizationManager::refreshAllViews()
{
    for (const auto& pair : m_registeredViews)
        pair.first->setLocalizedText(getString(pair.second));
    for (const auto& pair : m_registeredMenuItems)
        pair.first->setName(getString(pair.second));
}

void LocalizationManager::initializeDefaultStrings()
{
    // =========================================================================
    // ENGLISH
    // =========================================================================

    // ── AVM ───────────────────────────────────────────────────────────────────
    m_translations["en"]["AVM Settings"]                    = "AVM Settings";
    m_translations["en"]["View"]                            = "View";
    m_translations["en"]["Front_View"]                      = "Front View";
    m_translations["en"]["Right_View"]                      = "Right View";
    m_translations["en"]["Rear_View"]                       = "Rear View";
    m_translations["en"]["Left_View"]                       = "Left View";
    m_translations["en"]["Emergency"]                       = "Emergency";
    m_translations["en"]["2D"]                              = "2D";
    m_translations["en"]["3D"]                              = "3D";
    m_translations["en"]["3D_Front"]                        = "3D Front";
    m_translations["en"]["3D_Rear"]                         = "3D Rear";
    m_translations["en"]["Activation"]                      = "Activation";
    m_translations["en"]["VBC"]                             = "Vehicle Bottom Coloring (VBC)";
    m_translations["en"]["PGS"]                             = "Parking Guidance System (PGS)";
    m_translations["en"]["DGS"]                             = "Distance Guidance System (DGS)";
    m_translations["en"]["OD"]                              = "Object Detection (OD)";
    m_translations["en"]["MOBS"]                            = "MOIS / BSIS (MOBS)";
    m_translations["en"]["OFF"]                             = "OFF";
    m_translations["en"]["ON"]                              = "ON";
    m_translations["en"]["DVR"]                             = "DVR";
    m_translations["en"]["Record_Time"]                     = "Record Time";
    m_translations["en"]["Framerate_AVM"]                   = "Framerate / AVM";
    m_translations["en"]["Framerate_Internal"]              = "Framerate / Internal";
    m_translations["en"]["Record_Audio"]                    = "Record Audio";
    m_translations["en"]["1min"]                            = "1min";
    m_translations["en"]["3min"]                            = "3min";
    m_translations["en"]["5min"]                            = "5min";
    m_translations["en"]["NA"]                              = "NA";
    m_translations["en"]["4CH"]                             = "4CH";
    m_translations["en"]["6CH"]                             = "6CH";
    m_translations["en"]["8CH"]                             = "8CH";
    m_translations["en"]["10FPS"]                           = "10FPS";
    m_translations["en"]["20FPS"]                           = "20FPS";
    m_translations["en"]["30FPS"]                           = "30FPS";
    m_translations["en"]["Menu"]                            = "Menu";
    m_translations["en"]["SafeZone"]                        = "SafeZone";
    m_translations["en"]["Front"]                           = "Front";
    m_translations["en"]["Right"]                           = "Right";
    m_translations["en"]["Rear"]                            = "Rear";
    m_translations["en"]["Left"]                            = "Left";
    m_translations["en"]["Channel"]                         = "Channel";
    m_translations["en"]["1-4"]                             = "1-4";
    m_translations["en"]["5-8"]                             = "5-8";
    m_translations["en"]["0.6m"]                            = "0.6m";
    m_translations["en"]["2m"]                              = "2m";
    m_translations["en"]["3m"]                              = "3m";
    m_translations["en"]["4m"]                              = "4m";
    m_translations["en"]["5m"]                              = "5m";
    m_translations["en"]["File"]                            = "File";
    m_translations["en"]["Control"]                         = "Control";
    m_translations["en"]["dir"]                             = "dir";
    m_translations["en"]["System"]                          = "System";
    m_translations["en"]["Language"]                        = "Language";
    m_translations["en"]["English"]                         = "English";
    m_translations["en"]["Korean"]                          = "한국어";
    m_translations["en"]["Vehicle"]                         = "Vehicle";
    m_translations["en"]["Capture_Calibration"]             = "Capture / Calibration";
    m_translations["en"]["Storage_Format"]                  = "Storage Format";
    m_translations["en"]["Format"]                          = "Format";
    m_translations["en"]["PiP_Setup"]                       = "PiP Setup";
    m_translations["en"]["PiP_Cameras"]                     = "Enabled Cameras";
    m_translations["en"]["PiP_Primary"]                     = "Primary Cameras";
    m_translations["en"]["PiP_Dock"]                        = "Dock Location";
    m_translations["en"]["PiP_Orientation"]                 = "Dock Orientation";
    m_translations["en"]["PiP_TopLeft"]                     = "TopLeft";
    m_translations["en"]["PiP_TopRight"]                    = "TopRight";
    m_translations["en"]["PiP_BotLeft"]                     = "BotLeft";
    m_translations["en"]["PiP_BotRight"]                    = "BotRight";
    m_translations["en"]["PiP_Horizontal"]                  = "Horizontal";
    m_translations["en"]["PiP_Vertical"]                    = "Vertical";

    // ── UI Elements ────────────────────────────────────────────────────────────
    m_translations["en"]["Move"]   = "Move";
    m_translations["en"]["Select"] = "Select";
    m_translations["en"]["Save"]   = "Save";
    m_translations["en"]["Back"]   = "Back";
    m_translations["en"]["Exit"]   = "Exit";
    m_translations["en"]["OK"]     = "OK";
    m_translations["en"]["Confirm"] = "Confirm";
    m_translations["en"]["Cancel"]  = "Cancel";
    m_translations["en"]["..."]    = "...";
    m_translations["en"]["-"]      = "-";
    m_translations["en"]["+"]      = "+";
    m_translations["en"]["Copying"]    = "Copying";
    m_translations["en"]["Deleting"]   = "Deleting";
    m_translations["en"]["Formating"]  = "Formating";
    m_translations["en"]["Applying"]   = "Applying";
    m_translations["en"]["Capturing"]  = "Capturing";
    m_translations["en"]["Restoring"]  = "Restoring";

    // ── Calibration ───────────────────────────────────────────────────────────
    m_translations["en"]["Calibration"]          = "Calibration";
    m_translations["en"]["Select Vehicle"]       = "Select Vehicle";
    m_translations["en"]["Vehicle List"]         = "Vehicle List";
    m_translations["en"]["Manufacturer"]         = "Manufacturer";
    m_translations["en"]["Model"]                = "Model";
    m_translations["en"]["Year"]                 = "Year";
    m_translations["en"]["Calib Params"]         = "Calib Params";
    m_translations["en"]["Width"]                = "Width";
    m_translations["en"]["Length"]               = "Length";
    m_translations["en"]["Height"]               = "Height";
    m_translations["en"]["Front Overhang"]       = "Front Overhang";
    m_translations["en"]["Wheelbase"]            = "Wheelbase";
    m_translations["en"]["Rear Overhang"]        = "Rear Overhang";
    m_translations["en"]["Front Track"]          = "Front Track";
    m_translations["en"]["Rear Track"]           = "Rear Track";
    m_translations["en"]["Min Steering Angle"]   = "Min Steering Angle";
    m_translations["en"]["Max Steering Angle"]   = "Max Steering Angle";
    m_translations["en"]["Camera Number"]        = "Camera Number";
    m_translations["en"]["Camera Index"]         = "Camera Index";
    m_translations["en"]["Pattern Number"]       = "Pattern Number";
    m_translations["en"]["Front Offset"]         = "Front Offset";
    m_translations["en"]["Right Offset"]         = "Right Offset";
    m_translations["en"]["Rear Offset"]          = "Rear Offset";
    m_translations["en"]["Left Offset"]          = "Left Offset";
    m_translations["en"]["1st Distance"]         = "1st Distance";
    m_translations["en"]["2nd Distance"]         = "2nd Distance";
    m_translations["en"]["3rd Distance"]         = "3rd Distance";
    m_translations["en"]["Auto Detect"]          = "Auto Detect";
    m_translations["en"]["Unit Space"]           = "Unit Space";
    m_translations["en"]["ROI Start X"]          = "ROI Start X";
    m_translations["en"]["ROI Start Y"]          = "ROI Start Y";
    m_translations["en"]["ROI Width"]            = "ROI Width";
    m_translations["en"]["ROI Height"]           = "ROI Height";
    m_translations["en"]["Contour Max Area"]     = "Contour Max Area";
    m_translations["en"]["Live View & Capture"]  = "Live View & Capture";
    m_translations["en"]["Restore_Default_Confirm"] = "Do you want to restore the default settings?";
    m_translations["en"]["Capture_Confirm"]      = "Do you want to capture images?";
    m_translations["en"]["Point"]                = "Point";
    m_translations["en"]["Capture"]              = "Capture";
    m_translations["en"]["Previous Captured"]    = "Previous Captured";
    m_translations["en"]["Live View"]            = "Live View";
    m_translations["en"]["AutoCalib"]            = "AutoCalib";
    m_translations["en"]["Fisheye View"]         = "Fisheye View";
    m_translations["en"]["Defisheye View"]       = "Defisheye View";
    m_translations["en"]["Feature View"]         = "Feature View";
    m_translations["en"]["Contour View"]         = "Contour View";
    m_translations["en"]["Grid View"]            = "Grid View";
    m_translations["en"]["Mask View"]            = "Mask View";
    m_translations["en"]["Result View"]          = "Result View";
    m_translations["en"]["Restore Default"]      = "Restore Default";

    // ── Messages ──────────────────────────────────────────────────────────────
    m_translations["en"]["Adj_Cam_ROI_step0"]         = "Use 4 arrow keys to adjust:\nROI 1st of 4 point";
    m_translations["en"]["Adj_Cam_ROI_step1"]         = "Use 4 arrow keys to adjust:\nROI 2nd of 4 point";
    m_translations["en"]["Adj_Cam_ROI_step2"]         = "Use 4 arrow keys to adjust:\nROI 3rd of 4 point";
    m_translations["en"]["Adj_Cam_ROI_step3"]         = "Use 4 arrow keys to adjust:\nROI 4th of 4 point";
    m_translations["en"]["Adj_CamView_Rcam2D_step0"]  = "Use Left/Right keys for Left&Right\nUse Up/Down keys for Top";
    m_translations["en"]["Adj_CamView_Rcam2D_step1"]  = "Use Left/Right keys for Left&Right\nUse Up/Down keys for Bottom";
    m_translations["en"]["Adj_CamView_Vcam3D_step0"]  = "Use Up/Down keys to adjust:\nZ height";
    m_translations["en"]["Adj_CamView_Vcam3D_step1"]  = "Use 4 arrow keys to adjust:\nHorizontal and Vertical orientation";
    m_translations["en"]["Adj_CamView_Vcam3D_step2"]  = "Use 4 arrow keys to adjust:\nX-Y position";
    m_translations["en"]["Adj_FeatureView"]           = "Use Left/Right to change camera\nUse OK to enter Adjustment mode";
    m_translations["en"]["FileView_corrupted"]        = "The selected file is corrupted";
    m_translations["en"]["FileView_copyGroup"]        = "Do you want to copy group files?";
    m_translations["en"]["FileView_copySingle"]       = "Do you want to copy the file?";
    m_translations["en"]["FileView_deleteSingle"]     = "Do you want to delete the file?";
    m_translations["en"]["System_format"]             = "Do you want to format the storage?";
    m_translations["en"]["Format_confirm"]            = "Press OK to confirm";
    m_translations["en"]["System_calibPassword"]      = "Please enter password";
    m_translations["en"]["System_correctPassword"]    = "Correct password";
    m_translations["en"]["System_incorrectPassword"]  = "Incorrect password";
    m_translations["en"]["Calib_vehicleChange"]       = "Do you want to change to";
    m_translations["en"]["Capture_confirm"]           = "Do you want to capture";
    m_translations["en"]["current_view"]              = "( current view )";
    m_translations["en"]["Calib_restore"]             = "Do you want to restore?";

    // ── TR underscore keys (buildMenuTree) ────────────────────────────────────
    m_translations["en"]["Front_Camera"]  = "Front Camera";
    m_translations["en"]["Right_Camera"]  = "Right Camera";
    m_translations["en"]["Rear_Camera"]   = "Rear Camera";
    m_translations["en"]["Left_Camera"]   = "Left Camera";
    m_translations["en"]["Time_Zone"]      = "Time Zone";
    m_translations["en"]["System_Time"]    = "Time";
    m_translations["en"]["Auto_Time"]      = "Auto";
    m_translations["en"]["Manual_Time"]    = "Manual";
    m_translations["en"]["Storage_Format"] = "Storage Format";
    m_translations["en"]["Group_List"]     = "Group List";
    m_translations["en"]["Group_Copy"]     = "Group Copy";
    m_translations["en"]["File_List"]      = "File List";

    // ── System Password (new) ─────────────────────────────────────────────────
    m_translations["en"]["System_Password"]        = "System Password";
    m_translations["en"]["Set_Password"]           = "Set Password";
    m_translations["en"]["Clear_Password"]         = "Clear Password";

    // Numpad prompts
    m_translations["en"]["Password_EnterOld"]      = "Enter current password";
    m_translations["en"]["Password_EnterNew"]      = "Enter new password (6 digits)";
    m_translations["en"]["Password_ConfirmNew"]    = "Confirm new password";

    // Result / error messages
    m_translations["en"]["Password_Incorrect"]     = "Incorrect password";
    m_translations["en"]["Password_Mismatch"]      = "Passwords do not match";
    m_translations["en"]["Password_Success"]       = "Password updated successfully";
    m_translations["en"]["Password_Cleared"]       = "Password cleared successfully";
    m_translations["en"]["Password_NoneSet"]       = "No password is currently set";
    m_translations["en"]["Password_ClearConfirm"]  = "Are you sure to clear the password?";

    // Storage format — password gate
    m_translations["en"]["Format_EnterPassword"]   = "Enter password to unlock format";
    m_translations["en"]["Format_WrongPassword"]   = "Incorrect password. Format cancelled";

    // ── Display Timeout ───────────────────────────────────────────────────────
    m_translations["en"]["Display_Timeout"]         = "Display Timeout";
    m_translations["en"]["Display_Timeout_10s"]     = "10 sec";
    m_translations["en"]["Display_Timeout_20s"]     = "20 sec";
    m_translations["en"]["Display_Timeout_30s"]     = "30 sec";
    m_translations["en"]["Display_Timeout_AlwaysOn"] = "Always On";

    // =========================================================================
    // KOREAN
    // =========================================================================

    // ── AVM ───────────────────────────────────────────────────────────────────
    m_translations["ko"]["AVM Settings"]                    = "AVM 설정";
    m_translations["ko"]["View"]                            = "화면설정";
    m_translations["ko"]["Front_View"]                      = "전방 화면";
    m_translations["ko"]["Right_View"]                      = "우측 화면";
    m_translations["ko"]["Rear_View"]                       = "후방 화면";
    m_translations["ko"]["Left_View"]                       = "좌측 화면";
    m_translations["ko"]["Emergency"]                       = "비상등 화면";
    m_translations["ko"]["2D"]                              = "2D";
    m_translations["ko"]["3D"]                              = "3D";
    m_translations["ko"]["3D_Front"]                        = "3D 전방";
    m_translations["ko"]["3D_Rear"]                         = "3D 후방";
    m_translations["ko"]["Activation"]                      = "부가기능";
    m_translations["ko"]["VBC"]                             = "차량 하부 색값 보정 (VBC)";
    m_translations["ko"]["PGS"]                             = "스티어링 가이드 라인 (PGS)";
    m_translations["ko"]["DGS"]                             = "거리 측정 가이드 라인 (DGS)";
    m_translations["ko"]["OD"]                              = "객체 감지 (OD)";
    m_translations["ko"]["MOBS"]                            = "MOIS / BSIS (MOBS)";
    m_translations["ko"]["OFF"]                             = "끄기";
    m_translations["ko"]["ON"]                              = "켜기";
    m_translations["ko"]["DVR"]                             = "녹화";
    m_translations["ko"]["Record_Time"]                     = "녹화 시간";
    m_translations["ko"]["Framerate_AVM"]                   = "프레임 속도 / AVM";
    m_translations["ko"]["Framerate_Internal"]              = "프레임 속도 / 내부";
    m_translations["ko"]["Record_Audio"]                    = "오디오 녹화";
    m_translations["ko"]["1min"]                            = "1분";
    m_translations["ko"]["3min"]                            = "3분";
    m_translations["ko"]["5min"]                            = "5분";
    m_translations["ko"]["NA"]                              = "NA";
    m_translations["ko"]["4CH"]                             = "4CH";
    m_translations["ko"]["6CH"]                             = "6CH";
    m_translations["ko"]["8CH"]                             = "8CH";
    m_translations["ko"]["10FPS"]                           = "10FPS";
    m_translations["ko"]["20FPS"]                           = "20FPS";
    m_translations["ko"]["30FPS"]                           = "30FPS";
    m_translations["ko"]["Menu"]                            = "메뉴";
    m_translations["ko"]["SafeZone"]                        = "세이프존";
    m_translations["ko"]["Front"]                           = "전방";
    m_translations["ko"]["Right"]                           = "우측";
    m_translations["ko"]["Rear"]                            = "후방";
    m_translations["ko"]["Left"]                            = "좌측";
    m_translations["ko"]["Channel"]                         = "채널";
    m_translations["ko"]["1-4"]                             = "1-4";
    m_translations["ko"]["5-8"]                             = "5-8";
    m_translations["ko"]["0.6m"]                            = "0.6m";
    m_translations["ko"]["2m"]                              = "2m";
    m_translations["ko"]["3m"]                              = "3m";
    m_translations["ko"]["4m"]                              = "4m";
    m_translations["ko"]["5m"]                              = "5m";
    m_translations["ko"]["File"]                            = "파일";
    m_translations["ko"]["Control"]                         = "제어";
    m_translations["ko"]["dir"]                             = "dir";
    m_translations["ko"]["System"]                          = "시스템";
    m_translations["ko"]["Language"]                        = "언어";
    m_translations["ko"]["English"]                         = "English";
    m_translations["ko"]["Korean"]                          = "한국어";
    m_translations["ko"]["Vehicle"]                         = "차량";
    m_translations["ko"]["Capture_Calibration"]           = "캡처 / 공차보정";
    m_translations["ko"]["Storage_Format"]                  = "저장 형식";
    m_translations["ko"]["Format"]                          = "포맷";
    m_translations["ko"]["PiP_Setup"]                       = "PiP 설정";
    m_translations["ko"]["PiP_Cameras"]                     = "활성화된 카메라";
    m_translations["ko"]["PiP_Primary"]                     = "기본 카메라";
    m_translations["ko"]["PiP_Dock"]                        = "독 위치";
    m_translations["ko"]["PiP_Orientation"]                 = "독 방향";
    m_translations["ko"]["PiP_TopLeft"]                     = "왼쪽 상단";
    m_translations["ko"]["PiP_TopRight"]                    = "오른쪽 상단";
    m_translations["ko"]["PiP_BotLeft"]                     = "왼쪽 하단";
    m_translations["ko"]["PiP_BotRight"]                    = "오른쪽 하단";
    m_translations["ko"]["PiP_Horizontal"]                  = "가로";
    m_translations["ko"]["PiP_Vertical"]                    = "세로";

    // ── UI Elements ────────────────────────────────────────────────────────────
    m_translations["ko"]["Move"]   = "이동";
    m_translations["ko"]["Select"] = "선택";
    m_translations["ko"]["Save"]   = "저장";
    m_translations["ko"]["Back"]   = "이전";
    m_translations["ko"]["Exit"]   = "나가기";
    m_translations["ko"]["OK"]     = "확인";
    m_translations["ko"]["Confirm"] = "확인";
    m_translations["ko"]["Cancel"]  = "취소";
    m_translations["ko"]["..."]    = "...";
    m_translations["ko"]["-"]      = "-";
    m_translations["ko"]["+"]      = "+";
    m_translations["ko"]["Copying"]   = "복사 중";
    m_translations["ko"]["Deleting"]  = "삭제 중";
    m_translations["ko"]["Formating"] = "포맷 중";
    m_translations["ko"]["Applying"]  = "적용 중";
    m_translations["ko"]["Capturing"] = "캡처 중";
    m_translations["ko"]["Restoring"] = "복원 중";

    // ── Calibration ───────────────────────────────────────────────────────────
    m_translations["ko"]["Calibration"]         = "공차보정";
    m_translations["ko"]["Select Vehicle"]       = "차량 선택";
    m_translations["ko"]["Vehicle List"]         = "차량 목록";
    m_translations["ko"]["Manufacturer"]         = "제조사";
    m_translations["ko"]["Model"]                = "모델명";
    m_translations["ko"]["Year"]                 = "제조년도";
    m_translations["ko"]["Calib Params"]         = "공차 보정 매개변수";
    m_translations["ko"]["Width"]                = "너비";
    m_translations["ko"]["Length"]               = "길이";
    m_translations["ko"]["Height"]               = "높이";
    m_translations["ko"]["Front Overhang"]       = "프론트 오버행";
    m_translations["ko"]["Wheelbase"]            = "휠베이스";
    m_translations["ko"]["Rear Overhang"]        = "리어 오버행";
    m_translations["ko"]["Front Track"]          = "프론트 트랙";
    m_translations["ko"]["Rear Track"]           = "리어 트랙";
    m_translations["ko"]["Min Steering Angle"]   = "최소 조향각";
    m_translations["ko"]["Max Steering Angle"]   = "최대 조향각";
    m_translations["ko"]["Camera Number"]        = "카메라 개수";
    m_translations["ko"]["Camera Index"]         = "카메라 인덱스";
    m_translations["ko"]["Pattern Number"]       = "패턴 개수";
    m_translations["ko"]["Front Offset"]         = "프론트 오프셋";
    m_translations["ko"]["Right Offset"]         = "우측 오프셋";
    m_translations["ko"]["Rear Offset"]          = "리어 오프셋";
    m_translations["ko"]["Left Offset"]          = "좌측 오프셋";
    m_translations["ko"]["1st Distance"]         = "1차 거리";
    m_translations["ko"]["2nd Distance"]         = "2차 거리";
    m_translations["ko"]["3rd Distance"]         = "3차 거리";
    m_translations["ko"]["Auto Detect"]          = "자동 감지";
    m_translations["ko"]["Unit Space"]           = "단위 공간";
    m_translations["ko"]["ROI Start X"]          = "관심 영역 시작 가로 좌표";
    m_translations["ko"]["ROI Start Y"]          = "관심 영역 시작 세로 좌표";
    m_translations["ko"]["ROI Width"]            = "관심 영역 너비";
    m_translations["ko"]["ROI Height"]           = "관심 영역 높이";
    m_translations["ko"]["Contour Max Area"]     = "윤곽선 최대 면적";
    m_translations["ko"]["Live View & Capture"]  = "라이브 뷰 & 캡처";
    m_translations["ko"]["Restore_Default_Confirm"] = "기본 설정으로 복원하시겠습니까?";
    m_translations["ko"]["Capture_Confirm"]      = "이미지를 캡처하시겠습니까?";
    m_translations["ko"]["Point"]                = "포인트";
    m_translations["ko"]["Capture"]              = "캡처";
    m_translations["ko"]["Previous Captured"]    = "이전 캡처";
    m_translations["ko"]["Live View"]            = "라이브 뷰";
    m_translations["ko"]["AutoCalib"]            = "자동 공차보정";
    m_translations["ko"]["Fisheye View"]         = "원본 뷰";
    m_translations["ko"]["Defisheye View"]       = "왜곡 보정 뷰";
    m_translations["ko"]["Feature View"]         = "피처 뷰";
    m_translations["ko"]["Contour View"]         = "컨투어 뷰";
    m_translations["ko"]["Grid View"]            = "그리드 뷰";
    m_translations["ko"]["Mask View"]            = "마스크 뷰";
    m_translations["ko"]["Result View"]          = "결과 뷰";
    m_translations["ko"]["Restore Default"]      = "기본값 복원";

    // ── Messages ──────────────────────────────────────────────────────────────
    m_translations["ko"]["Adj_Cam_ROI_step0"]         = "ROI (1 / 4)번 지점 위치를 조정을 위해\n4방향 화살표 키를 사용하세요";
    m_translations["ko"]["Adj_Cam_ROI_step1"]         = "ROI (2 / 4)번 지점 위치를 조정을 위해\n4방향 화살표 키를 사용하세요";
    m_translations["ko"]["Adj_Cam_ROI_step2"]         = "ROI (3 / 4)번 지점 위치를 조정을 위해\n4방향 화살표 키를 사용하세요";
    m_translations["ko"]["Adj_Cam_ROI_step3"]         = "ROI (4 / 4)번 지점 위치를 조정을 위해\n4방향 화살표 키를 사용하세요";
    m_translations["ko"]["Adj_CamView_Rcam2D_step0"]  = "좌우 값을 조정하려면 좌/우 키를,\n상단 값을 조정하려면 위/아래 키를 누르세요";
    m_translations["ko"]["Adj_CamView_Rcam2D_step1"]  = "좌우 값을 조정하려면 좌/우 키를,\n하단 값을 조정하려면 위/아래 키를 누르세요";
    m_translations["ko"]["Adj_CamView_Vcam3D_step0"]  = "Z 높이를 조정하려면\n위/아래 키를 누르세요";
    m_translations["ko"]["Adj_CamView_Vcam3D_step1"]  = "수평 및 수직 방향 조정을 위해\n4방향 화살표 키를 사용하세요";
    m_translations["ko"]["Adj_CamView_Vcam3D_step2"]  = "X-Y 위치를 조정하려면\n4방향 화살표 키를 사용하세요";
    m_translations["ko"]["Adj_FeatureView"]           = "카메라를 변경하려면 좌/우 키를,\n조정 모드로 진입하려면 확인 키를 누르세요";
    m_translations["ko"]["FileView_corrupted"]        = "선택한 파일이 손상되었습니다";
    m_translations["ko"]["FileView_copyGroup"]        = "그룹 파일을 복사하시겠습니까?";
    m_translations["ko"]["FileView_copySingle"]       = "파일을 복사하시겠습니까?";
    m_translations["ko"]["FileView_deleteSingle"]     = "파일을 삭제하시겠습니까?";
    m_translations["ko"]["System_format"]             = "저장소를 포맷하시겠습니까?";
    m_translations["ko"]["Format_confirm"]            = "확인하려면 확인 키를 누르세요";
    m_translations["ko"]["System_calibPassword"]      = "비밀번호를 입력해 주세요";
    m_translations["ko"]["System_correctPassword"]    = "비밀번호가 올바릅니다";
    m_translations["ko"]["System_incorrectPassword"]  = "비밀번호가 올바르지 않습니다";
    m_translations["ko"]["Calib_vehicleChange"]       = "차량을 변경하시겠습니까?";
    m_translations["ko"]["Capture_confirm"]           = "캡처하시겠습니까?";
    m_translations["ko"]["current_view"]              = "(현재 뷰)";
    m_translations["ko"]["Calib_restore"]             = "복원하시겠습니까?";

    // ── TR underscore keys (buildMenuTree) ────────────────────────────────────
    m_translations["ko"]["Front_Camera"]  = "전방 카메라";
    m_translations["ko"]["Right_Camera"]  = "우측 카메라";
    m_translations["ko"]["Rear_Camera"]   = "후방 카메라";
    m_translations["ko"]["Left_Camera"]   = "좌측 카메라";
    m_translations["ko"]["Time_Zone"]      = "시간대";
    m_translations["ko"]["System_Time"]    = "시간 설정";
    m_translations["ko"]["Auto_Time"]      = "자동";
    m_translations["ko"]["Manual_Time"]    = "수동";
    m_translations["ko"]["Storage_Format"] = "저장 형식";
    m_translations["ko"]["Group_List"]     = "그룹 목록";
    m_translations["ko"]["Group_Copy"]     = "그룹 복사";
    m_translations["ko"]["File_List"]      = "파일 목록";

    // ── System Password (new) ─────────────────────────────────────────────────
    m_translations["ko"]["System_Password"]        = "시스템 비밀번호";
    m_translations["ko"]["Set_Password"]           = "비밀번호 설정";
    m_translations["ko"]["Clear_Password"]         = "비밀번호 삭제";

    // Numpad prompts
    m_translations["ko"]["Password_EnterOld"]      = "현재 비밀번호를 입력하세요";
    m_translations["ko"]["Password_EnterNew"]      = "새 비밀번호를 입력하세요 (6자리)";
    m_translations["ko"]["Password_ConfirmNew"]    = "새 비밀번호를 다시 입력하세요";

    // Result / error messages
    m_translations["ko"]["Password_Incorrect"]     = "비밀번호가 올바르지 않습니다";
    m_translations["ko"]["Password_Mismatch"]      = "비밀번호가 일치하지 않습니다";
    m_translations["ko"]["Password_Success"]       = "비밀번호가 성공적으로 업데이트되었습니다";
    m_translations["ko"]["Password_Cleared"]       = "비밀번호가 성공적으로 삭제되었습니다";
    m_translations["ko"]["Password_NoneSet"]       = "현재 설정된 비밀번호가 없습니다";
    m_translations["ko"]["Password_ClearConfirm"]  = "비밀번호를 삭제하시겠습니까?";

    // Storage format — password gate
    m_translations["ko"]["Format_EnterPassword"]   = "포맷 잠금 해제를 위해 비밀번호를 입력하세요";
    m_translations["ko"]["Format_WrongPassword"]   = "비밀번호가 틀렸습니다. 포맷이 취소되었습니다";

    // ── Display Timeout ───────────────────────────────────────────────────────
    m_translations["ko"]["Display_Timeout"]          = "화면 자동 꺼짐";
    m_translations["ko"]["Display_Timeout_10s"]      = "10 초";
    m_translations["ko"]["Display_Timeout_20s"]      = "20 초";
    m_translations["ko"]["Display_Timeout_30s"]      = "30 초";
    m_translations["ko"]["Display_Timeout_AlwaysOn"] = "항상 켜기";
}

#endif  // USE_LOCALIZATION

} // namespace UI

} // namespace APP
