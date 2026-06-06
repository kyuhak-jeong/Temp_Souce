cmake_minimum_required(VERSION 3.5)

set(PROJECT_NAME            SafeView)
set(PROJECT_BRANCH          R)
set(PROJECT_VERSION         2.1.0)
set(PROJECT_SUB_VERSION     H9)
set(RELEASE_DATE            20260515)

project(
    ${PROJECT_NAME}_${PROJECT_BRANCH}${PROJECT_VERSION}${PROJECT_SUB_VERSION}
    VERSION ${PROJECT_VERSION}
    LANGUAGES CXX C)

set(TARGET_PROJECT_DIR      ${CMAKE_SYSROOT})
set(TARGET_PATH_LIB         ${TARGET_PROJECT_DIR}/usr/lib)
set(TARGET_PATH_INCLUDE     ${TARGET_PROJECT_DIR}/usr/include)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ============================================================================
# Auto-detect Platform and Build Type
# ============================================================================
message(STATUS "------------------------------------")
if(CMAKE_CROSSCOMPILING OR
   CMAKE_CXX_COMPILER MATCHES ".*arm.*|.*aarch64.*|.*rk3588.*|.*rk3576.*" OR
   CMAKE_SYSTEM_PROCESSOR MATCHES "arm.*|aarch64.*|ARM.*")
    set(EMBEDDED_DEVICE ON)
    set(PLATFORM_NAME "Embedded Linux (${CMAKE_SYSTEM_PROCESSOR})")
    message(STATUS "Auto-detected: EMBEDDED_DEVICE build")
elseif(ANDROID)
    set(PLATFORM_NAME "Android")
    message(STATUS "Auto-detected: ANDROID build")
elseif(UNIX AND NOT APPLE)
    set(EMBEDDED_DEVICE OFF)
    set(PLATFORM_NAME "Linux Desktop")
    message(STATUS "Auto-detected: DESKTOP build")
else()
    message(FATAL_ERROR "Unsupported platform")
endif()

message(STATUS "Platform:  ${PLATFORM_NAME}")
message(STATUS "Compiler:  ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER}")
message(STATUS "Processor: ${CMAKE_SYSTEM_PROCESSOR}")
message(STATUS "------------------------------------")

# ============================================================================
# Renderer Selection
# ============================================================================
option(USE_GLES  "Use raw GLES3 renderer"  TRUE)
option(USE_IMGUI "Use ImGui renderer"      FALSE)

if(NOT USE_GLES AND NOT USE_IMGUI)
    message(FATAL_ERROR "No renderer selected — enable either USE_GLES or USE_IMGUI.")
endif()

if(USE_GLES)
    message(STATUS "Renderer: USE_GLES")
    add_compile_definitions(USE_GLES)
else()
    message(STATUS "Renderer: USE_IMGUI")
    add_compile_definitions(USE_IMGUI)
    add_compile_definitions(IMGUI_IMPL_OPENGL_ES3)
    add_compile_definitions(IMGUI_DEFINE_MATH_OPERATORS)
endif()

# ============================================================================
# Board / Feature Options
# ============================================================================
if(EMBEDDED_DEVICE)
    option(DEVICE_RK3588 "Target board RK3588" FALSE)
    option(DEVICE_RK3576 "Target board RK3576" TRUE)
else()
    set(DEVICE_RK3588 FALSE)
    set(DEVICE_RK3576 FALSE)
endif()

option(USE_SVM      "Enable SVM"       FALSE)
option(USE_FORKLIFT "Enable Forklift"  FALSE)
option(USE_TAXI     "Enable Taxi"      TRUE)

if(USE_FORKLIFT OR USE_TAXI)
    set(USE_AICAM TRUE)
else()
    set(USE_AICAM FALSE)
endif()

if(EMBEDDED_DEVICE)
    add_compile_definitions(EMBEDDED_DEVICE)
    set(USE_GSTREAMER TRUE)
    set(USE_X11       FALSE)
    set(USE_DRM       TRUE)
    set(USE_CAMERA    TRUE)
    set(USE_RKNN      TRUE)
    set(USE_DVR       TRUE)
    set(USE_CAN       TRUE)
    set(USE_GPS       FALSE)        # USE_GPS means get sensor data from our own ECU Internal Sensors
    set(USE_SERIAL    TRUE)         # USE_SERIAL means get sth data from Dr.Safe UART Signal
    set(USE_BTO       FALSE)
    set(USE_GPIO      TRUE)
else()
    set(USE_GSTREAMER TRUE)
    set(USE_X11       TRUE)
    set(USE_DRM       FALSE)
    set(USE_CAMERA    FALSE)
    set(USE_RKNN      FALSE)
    set(USE_DVR       TRUE)
    set(USE_CAN       TRUE)
    set(USE_GPS       FALSE)
    set(USE_SERIAL    TRUE)
    set(USE_BTO       FALSE)
    set(USE_GPIO      FALSE)
endif()

if(USE_DVR)
    set(USE_DVR_SPLIT TRUE)
    set(USE_DVR_SUB   TRUE)
else()
    set(USE_DVR_SPLIT FALSE)
    set(USE_DVR_SUB   FALSE)
endif()

# MCU only on RK3588 + CAN
if(USE_CAN AND DEVICE_RK3588)
    set(USE_MCU TRUE)
else()
    set(USE_MCU FALSE)
endif()

# AICAM on RK3576 requires BTO
if(USE_AICAM AND DEVICE_RK3576)
    set(USE_BTO FALSE)
endif()

# Display backend
if(USE_X11)
    set(BACKEND_LINK   X11 Xi Xext)
    set(BACKEND_OPTION USE_X11)
elseif(USE_DRM)
    set(BACKEND_LINK   )
    set(BACKEND_OPTION USE_DRM)
endif()

# ============================================================================
# Compile Definitions (feature flags)
# ============================================================================
foreach(_feat USE_SVM USE_AICAM USE_FORKLIFT USE_TAXI
              USE_GSTREAMER USE_CAMERA USE_DVR USE_DVR_SPLIT USE_DVR_SUB USE_RKNN 
              USE_CAN USE_GPS USE_SERIAL USE_BTO USE_GPIO DEVICE_RK3588 DEVICE_RK3576)
    if(${_feat})
        add_compile_definitions(${_feat})
    endif()
endforeach()

# ============================================================================
# Package Config
# ============================================================================
find_package(PkgConfig)

pkg_search_module(CV       REQUIRED opencv4)
pkg_search_module(LIBXML2  REQUIRED libxml-2.0)
pkg_search_module(FREETYPE REQUIRED freetype2)

if(EMBEDDED_DEVICE)
    pkg_search_module(ASSIMP REQUIRED libassimp)
else()
    pkg_search_module(ASSIMP REQUIRED assimp)
endif()

pkg_search_module(GST         REQUIRED gstreamer-1.0)
pkg_check_modules(GST_APP     REQUIRED gstreamer-app-1.0)
pkg_search_module(GST_VIDEO   REQUIRED gstreamer-video-1.0)
pkg_search_module(GST_GL      REQUIRED gstreamer-gl-1.0)
pkg_search_module(GST_GL_EGL  REQUIRED gstreamer-gl-egl-1.0)
pkg_search_module(GST_PBUTILS REQUIRED gstreamer-pbutils-1.0)

# ============================================================================
# Include Directories
# ============================================================================
include_directories(
    ${TARGET_PATH_INCLUDE}/
    ${TARGET_PATH_INCLUDE}/libdrm/
    ${TARGET_PATH_INCLUDE}/glib-2.0/
    ${TARGET_PATH_INCLUDE}/glib-2.0/glib/

    ${CV_INCLUDE_DIRS}
    ${LIBXML2_INCLUDE_DIRS}
    ${FREETYPE_INCLUDE_DIRS}
    ${ASSIMP_INCLUDE_DIRS}

    ${GST_INCLUDE_DIRS}
    ${GST_APP_INCLUDE_DIRS}
    ${GST_VIDEO_INCLUDE_DIRS}
    ${GST_GL_INCLUDE_DIRS}
    ${GST_GL_EGL_INCLUDE_DIRS}
    ${GST_PBUTILS_INCLUDE_DIRS}

    ${PROJECT_SOURCE_DIR}/../libs/thirdparty/
    ${PROJECT_SOURCE_DIR}/../libs/thirdparty/imgui/
    ${PROJECT_SOURCE_DIR}/../libs/thirdparty/imgui/backends/
    ${PROJECT_SOURCE_DIR}/../libs/thirdparty/nlohmann/
    ${PROJECT_SOURCE_DIR}/../libs/thirdparty/stb/

    ${PROJECT_SOURCE_DIR}/../libs/aivmcu/
    ${PROJECT_SOURCE_DIR}/../libs/swin/include/
    ${PROJECT_SOURCE_DIR}/../libs/swin/target/include/
    ${PROJECT_SOURCE_DIR}/../libs/swin/target/linux/

    ${PROJECT_SOURCE_DIR}/common/
    ${PROJECT_SOURCE_DIR}/io/
    ${PROJECT_SOURCE_DIR}/ui/
    ${PROJECT_SOURCE_DIR}/main/
    ${PROJECT_SOURCE_DIR}/vpb/
)

# Conditional includes — must be separate calls, not inside include_directories()
if(USE_RKNN)
    include_directories(
        ${PROJECT_SOURCE_DIR}/rknn/inc/
        ${PROJECT_SOURCE_DIR}/../libs/thirdparty/rga/RK3588/include/
    )
endif()

if(USE_DVR)
    include_directories(${PROJECT_SOURCE_DIR}/dvr/)
endif()

if(USE_SVM)
    include_directories(
        ${PROJECT_SOURCE_DIR}/svm/core/inc/
        ${PROJECT_SOURCE_DIR}/svm/view/inc/
        ${PROJECT_SOURCE_DIR}/svm/tracker/inc/
        ${PROJECT_SOURCE_DIR}/svm/calib/inc/
    )
endif()

if(USE_CAN)
    include_directories(${PROJECT_SOURCE_DIR}/canparser/)
endif()

# ============================================================================
# Source Files
# ============================================================================
set(MAIN_SRC_FILES
    ${PROJECT_SOURCE_DIR}/main/standard_main.cpp
    ${PROJECT_SOURCE_DIR}/main/nmea_parser.cpp
    ${PROJECT_SOURCE_DIR}/main/cleanup_process.cpp
    ${PROJECT_SOURCE_DIR}/main/gst_camera_process.cpp
    ${PROJECT_SOURCE_DIR}/main/app_base.cpp
)

if(USE_SVM OR USE_AICAM)
    list(APPEND MAIN_SRC_FILES ${PROJECT_SOURCE_DIR}/main/app_shell.cpp)
endif()

if(USE_SVM)
    list(APPEND MAIN_SRC_FILES
        ${PROJECT_SOURCE_DIR}/main/svm_ui_process.cpp
        ${PROJECT_SOURCE_DIR}/main/app_svm.cpp
        ${PROJECT_SOURCE_DIR}/main/calib_ui_process.cpp
        ${PROJECT_SOURCE_DIR}/main/app_calib.cpp
    )
endif()

if(USE_AICAM)
    list(APPEND MAIN_SRC_FILES
        ${PROJECT_SOURCE_DIR}/main/aicam_ui_process.cpp
        ${PROJECT_SOURCE_DIR}/main/app_aicam.cpp
    )
    if(USE_FORKLIFT)
        list(APPEND MAIN_SRC_FILES ${PROJECT_SOURCE_DIR}/main/app_forklift.cpp)
    endif()
    if(USE_TAXI)
        list(APPEND MAIN_SRC_FILES ${PROJECT_SOURCE_DIR}/main/app_taxi.cpp)
    endif()
endif()

if(USE_DVR OR USE_GPS OR USE_BTO OR USE_SERIAL)
    list(APPEND MAIN_SRC_FILES ${PROJECT_SOURCE_DIR}/main/external_sensors.cpp)
endif()


set(SWIN_SRC_FILES
    ${PROJECT_SOURCE_DIR}/../libs/swin/core/skernel.cpp
    ${PROJECT_SOURCE_DIR}/../libs/swin/core/sptrarray.cpp
    ${PROJECT_SOURCE_DIR}/../libs/swin/core/smainwin.cpp
    ${PROJECT_SOURCE_DIR}/../libs/swin/core/sdialogbox.cpp
    ${PROJECT_SOURCE_DIR}/../libs/swin/core/sgfx.cpp
    ${PROJECT_SOURCE_DIR}/../libs/swin/core/smsgctrl.cpp
    ${PROJECT_SOURCE_DIR}/../libs/swin/core/sobject.cpp
    ${PROJECT_SOURCE_DIR}/../libs/swin/core/smessage.cpp
    ${PROJECT_SOURCE_DIR}/../libs/swin/core/smessagequeue.cpp
    ${PROJECT_SOURCE_DIR}/../libs/swin/core/srect.cpp
    ${PROJECT_SOURCE_DIR}/../libs/swin/core/ssemaphore.cpp
    ${PROJECT_SOURCE_DIR}/../libs/swin/core/strace.cpp
    ${PROJECT_SOURCE_DIR}/../libs/swin/core/swin.cpp
    ${PROJECT_SOURCE_DIR}/../libs/swin/core/swincall.cpp
    ${PROJECT_SOURCE_DIR}/../libs/swin/core/swinchild.cpp
    ${PROJECT_SOURCE_DIR}/../libs/swin/core/swinmain.cpp
    ${PROJECT_SOURCE_DIR}/../libs/swin/core/swintask.cpp
    ${PROJECT_SOURCE_DIR}/../libs/swin/core/swintick.cpp
    ${PROJECT_SOURCE_DIR}/../libs/swin/target/linux/tgfxfunc.cpp
    ${PROJECT_SOURCE_DIR}/../libs/swin/target/linux/tsemaphore.cpp
    ${PROJECT_SOURCE_DIR}/../libs/swin/target/linux/tsystem.cpp
    ${PROJECT_SOURCE_DIR}/../libs/swin/target/linux/ttask.cpp
    ${PROJECT_SOURCE_DIR}/../libs/swin/target/linux/tutil.cpp
)


set(COMMON_SRC_FILES
    ${PROJECT_SOURCE_DIR}/common/app_vars.cpp
    ${PROJECT_SOURCE_DIR}/common/app_process.cpp
)

if(USE_SVM OR USE_AICAM OR USE_DVR)
    list(APPEND COMMON_SRC_FILES
        ${PROJECT_SOURCE_DIR}/common/app_conf.cpp
    )
endif()

if(USE_IMGUI)
    list(APPEND COMMON_SRC_FILES
        ${PROJECT_SOURCE_DIR}/../libs/thirdparty/imgui/imgui.cpp
        ${PROJECT_SOURCE_DIR}/../libs/thirdparty/imgui/imgui_draw.cpp
        ${PROJECT_SOURCE_DIR}/../libs/thirdparty/imgui/imgui_tables.cpp
        ${PROJECT_SOURCE_DIR}/../libs/thirdparty/imgui/imgui_widgets.cpp
        ${PROJECT_SOURCE_DIR}/../libs/thirdparty/imgui/backends/imgui_impl_opengl3.cpp
    )
endif()


set(DVR_SRC_FILES)
if(USE_DVR)
    list(APPEND DVR_SRC_FILES
        ${PROJECT_SOURCE_DIR}/dvr/dvr_config.cpp
        ${PROJECT_SOURCE_DIR}/dvr/dvr_manager.cpp
        ${PROJECT_SOURCE_DIR}/dvr/file_writer.cpp
        ${PROJECT_SOURCE_DIR}/dvr/qdvr_manager.cpp
    )
endif()


set(VPB_SRC_FILES
    ${PROJECT_SOURCE_DIR}/vpb/vpb_file_manager.cpp
    ${PROJECT_SOURCE_DIR}/vpb/vpb_file_player.cpp
)


set(RKNN_SRC_FILES)
if(USE_RKNN)
    list(APPEND RKNN_SRC_FILES
        ${PROJECT_SOURCE_DIR}/rknn/src/detection.cpp
        ${PROJECT_SOURCE_DIR}/rknn/src/process_detection.cpp
    )
endif()


set(SVM_SRC_FILES)
if(USE_SVM)
    list(APPEND SVM_SRC_FILES
        ${PROJECT_SOURCE_DIR}/svm/core/src/svmCamera.cpp
        ${PROJECT_SOURCE_DIR}/svm/core/src/svmError.cpp
        ${PROJECT_SOURCE_DIR}/svm/core/src/svmFromFile.cpp
        ${PROJECT_SOURCE_DIR}/svm/core/src/svmLogger.cpp
        ${PROJECT_SOURCE_DIR}/svm/core/src/svmMRT.cpp
        ${PROJECT_SOURCE_DIR}/svm/core/src/svmOpt.cpp
        ${PROJECT_SOURCE_DIR}/svm/core/src/svmProject.cpp
        ${PROJECT_SOURCE_DIR}/svm/core/src/svmTextRenderer.cpp
        ${PROJECT_SOURCE_DIR}/svm/core/src/svmTime.cpp
        ${PROJECT_SOURCE_DIR}/svm/core/src/svmVABT.cpp
        ${PROJECT_SOURCE_DIR}/svm/core/src/svmWorld.cpp
        ${PROJECT_SOURCE_DIR}/svm/core/src/svmXML.cpp

        ${PROJECT_SOURCE_DIR}/svm/calib/src/svmCalibBowl.cpp
        ${PROJECT_SOURCE_DIR}/svm/calib/src/svmCalibCal.cpp
        ${PROJECT_SOURCE_DIR}/svm/calib/src/svmCalibContext.cpp
        ${PROJECT_SOURCE_DIR}/svm/calib/src/svmCalibContour.cpp
        ${PROJECT_SOURCE_DIR}/svm/calib/src/svmCalibDefisheye.cpp
        ${PROJECT_SOURCE_DIR}/svm/calib/src/svmCalibFisheye.cpp
        ${PROJECT_SOURCE_DIR}/svm/calib/src/svmCalibGrid.cpp
        ${PROJECT_SOURCE_DIR}/svm/calib/src/svmCalibMask.cpp

        ${PROJECT_SOURCE_DIR}/svm/view/src/svmBowl.cpp
        ${PROJECT_SOURCE_DIR}/svm/view/src/svmCamvec.cpp
        ${PROJECT_SOURCE_DIR}/svm/view/src/svmCamView.cpp
        ${PROJECT_SOURCE_DIR}/svm/view/src/svmContext.cpp
        ${PROJECT_SOURCE_DIR}/svm/view/src/svmDGS.cpp
        ${PROJECT_SOURCE_DIR}/svm/view/src/svmMOBS.cpp
        ${PROJECT_SOURCE_DIR}/svm/view/src/svmModel.cpp
        ${PROJECT_SOURCE_DIR}/svm/view/src/svmModelAnimator.cpp
        ${PROJECT_SOURCE_DIR}/svm/view/src/svmOD.cpp
        ${PROJECT_SOURCE_DIR}/svm/view/src/svmOsd.cpp
        ${PROJECT_SOURCE_DIR}/svm/view/src/svmPGS.cpp
        ${PROJECT_SOURCE_DIR}/svm/view/src/svmSceneAnimator.cpp
        ${PROJECT_SOURCE_DIR}/svm/view/src/svmVBC.cpp

        ${PROJECT_SOURCE_DIR}/svm/tracker/src/svmKF.cpp
        ${PROJECT_SOURCE_DIR}/svm/tracker/src/svmMunkres.cpp
        ${PROJECT_SOURCE_DIR}/svm/tracker/src/svmTrack.cpp
        ${PROJECT_SOURCE_DIR}/svm/tracker/src/svmTracker.cpp
    )
endif()


set(CAN_SRC_FILES)
if(USE_CAN)
    list(APPEND CAN_SRC_FILES
        ${PROJECT_SOURCE_DIR}/canparser/can_parser.cc
        ${PROJECT_SOURCE_DIR}/canparser/can_parser_sim.cc
        ${PROJECT_SOURCE_DIR}/canparser/can_parser_kia_bongo3ev.cc
        ${PROJECT_SOURCE_DIR}/canparser/can_parser_tatadaewoo.cc
        ${PROJECT_SOURCE_DIR}/canparser/can_parser_kgm_electric_bus.cc
        ${PROJECT_SOURCE_DIR}/canparser/can_parser_hyundai_eleccity.cc
        ${PROJECT_SOURCE_DIR}/canparser/can_parser_tatamotors_ebus.cc
        ${PROJECT_SOURCE_DIR}/canparser/can_parser_hyundai_new_county.cc
        ${PROJECT_SOURCE_DIR}/canparser/can_parser_main.cpp
        ${PROJECT_SOURCE_DIR}/canparser/san_gpio.c
    )
endif()


set(IO_SRC_FILES  ${PROJECT_SOURCE_DIR}/io/io_platform.cpp)
set(UI_SRC_FILES
    ${PROJECT_SOURCE_DIR}/ui/ui_core.cpp
    ${PROJECT_SOURCE_DIR}/ui/ui_elements.cpp
    ${PROJECT_SOURCE_DIR}/ui/ui_focus.cpp
    ${PROJECT_SOURCE_DIR}/ui/ui_menu.cpp
    ${PROJECT_SOURCE_DIR}/ui/ui_locale.cpp
    ${PROJECT_SOURCE_DIR}/ui/ui3d_core.cpp
)

# ============================================================================
# Executable
# ============================================================================
add_executable(${PROJECT_NAME}
    ${SWIN_SRC_FILES}
    ${COMMON_SRC_FILES}
    ${RKNN_SRC_FILES}
    ${DVR_SRC_FILES}
    ${SVM_SRC_FILES}
    ${CAN_SRC_FILES}
    ${VPB_SRC_FILES}
    ${IO_SRC_FILES}
    ${UI_SRC_FILES}
    ${MAIN_SRC_FILES}
)

# ============================================================================
# RKNN Submodules
# ============================================================================
if(USE_RKNN)
    add_subdirectory(${CMAKE_SOURCE_DIR}/rknn/util/rknn_helper       ${CMAKE_BINARY_DIR}/rknn_helper_build)
    add_subdirectory(${CMAKE_SOURCE_DIR}/rknn/util/object_detection  ${CMAKE_BINARY_DIR}/object_detection_build)
    target_include_directories(${PROJECT_NAME} PUBLIC
        ${CMAKE_SOURCE_DIR}/rknn/util/rknn_helper/include
        ${CMAKE_SOURCE_DIR}/rknn/util/object_detection/include
    )
    set(RKNN_LIB RknnHelper ObjectDetection)
else()
    set(RKNN_LIB)
endif()

# ============================================================================
# Link Libraries
# ============================================================================
if(EMBEDDED_DEVICE)
    set(ECU_LIB libmali.so librga.so librknnrt.so)
else()
    set(ECU_LIB)
endif()

if(USE_MCU)
    set(MCU_LIB "${PROJECT_SOURCE_DIR}/../libs/aivmcu/libaivmcu.so.1.3.3")
else()
    set(MCU_LIB)
endif()

target_link_directories(${PROJECT_NAME} PUBLIC
    ${TARGET_PROJECT_DIR}/lib/
    ${TARGET_PROJECT_DIR}/usr/lib/
    ${TARGET_PROJECT_DIR}/usr/local/lib/
)

target_link_libraries(${PROJECT_NAME} PUBLIC
    m stdc++ pthread dl
    libdrm.so
    libgbm.so
    libglib-2.0.so.0
    libgobject-2.0.so.0
    libgthread-2.0.so
    libEGL.so
    libpthread.so
    libGLESv2.so
    ${MCU_LIB}
    ${ECU_LIB}
    ${BACKEND_LINK}
    ${GST_LIBRARIES}
    ${GST_APP_LIBRARIES}
    ${GST_VIDEO_LIBRARIES}
    ${GST_GL_LIBRARIES}
    ${GST_GL_EGL_LIBRARIES}
    ${GST_PBUTILS_LIBRARIES}
    ${CV_LIBRARIES}
    ${LIBXML2_LIBRARIES}
    ${FREETYPE_LIBRARIES}
    ${ASSIMP_LIBRARIES}
    ${RKNN_LIB}
)

# ============================================================================
# Compile Definitions and Options
# ============================================================================
target_compile_definitions(${PROJECT_NAME} PUBLIC
    PROJECT_SRC_DIR=".."
    ${BACKEND_OPTION}
    ${SRC}
    CALIBRATION_APP
    VIEWER_APP
)

option(DEBUG_BUILD "Enable debug symbols (-g -O0)" OFF)

if(DEBUG_BUILD)
    target_compile_options(${PROJECT_NAME} PUBLIC
        -Wall
        -O0
        -g
        -rdynamic
    )
    message(STATUS "Build type: DEBUG (-O0 -g)")
else()
    target_compile_options(${PROJECT_NAME} PUBLIC
        -Wall
        -O3
        -g          # keep symbols for addr2line; strip before push
        -rdynamic
    )
    message(STATUS "Build type: RELEASE (-O3 -g)")
endif()
