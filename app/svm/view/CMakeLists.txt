cmake_minimum_required(VERSION 3.5)

set(VIEWER_PROJECT_NAME "VIEWER")
project(VIEWER_PROJECT_NAME)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_VERBOSE_MAKEFILE OFF)

set(APP_OPTION ${VIEWER_PROJECT_NAME}_APP)
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/../../bin)
set(OUTPUT_ELF ${VIEWER_PROJECT_NAME})

if(MSVC)
    set(DEPENDENCIES_DIR D:/Dependencies/svm)
else()
    set(DEPENDENCIES_DIR ${PROJECT_SOURCE_DIR}/../../thirdparty)
endif()

# ── Backend selection (Linux only) ─────────────────────────────────────────────
if(NOT MSVC)
    if("${EGL_FLAVOR}" STREQUAL "x11")
        message("backend is x11")
        set(BACKEND_LINK X11 Xi Xext)
        set(BACKEND_OPTION USE_X11)
    elseif("${EGL_FLAVOR}" STREQUAL "drm")
        message("backend is drm")
        set(BACKEND_LINK drm gbm)
        set(BACKEND_OPTION USE_DRM)
    else()
        message(FATAL_ERROR "EGL_FLAVOR must be 'x11' or 'drm'")
    endif()
endif()

# ── Shared source files ────────────────────────────────────────────────────────
set(SRC_FILES
        src/svmBowl.cpp
        src/svmCamvec.cpp
        src/svmCamView.cpp
        src/svmContext.cpp
        src/svmDGS.cpp
        src/svmMOBS.cpp
        src/svmModel.cpp
        src/svmModelAnimator.cpp
        src/svmOD.cpp
        src/svmOsd.cpp
        src/svmPGS.cpp
        src/svmSceneAnimator.cpp
        src/svmVBC.cpp
        #########################################################
        ${PROJECT_SOURCE_DIR}/../core/src/svmCamera.cpp
        ${PROJECT_SOURCE_DIR}/../core/src/svmError.cpp
        ${PROJECT_SOURCE_DIR}/../core/src/svmFromFile.cpp
        ${PROJECT_SOURCE_DIR}/../core/src/svmLogger.cpp
        ${PROJECT_SOURCE_DIR}/../core/src/svmMRT.cpp
        ${PROJECT_SOURCE_DIR}/../core/src/svmOpt.cpp
        ${PROJECT_SOURCE_DIR}/../core/src/svmProject.cpp
        ${PROJECT_SOURCE_DIR}/../core/src/svmTextRenderer.cpp
        ${PROJECT_SOURCE_DIR}/../core/src/svmTime.cpp
        ${PROJECT_SOURCE_DIR}/../core/src/svmVABT.cpp
        ${PROJECT_SOURCE_DIR}/../core/src/svmWorld.cpp
        ${PROJECT_SOURCE_DIR}/../core/src/svmXML.cpp
        #########################################################
        ${PROJECT_SOURCE_DIR}/../tracker/src/svmKF.cpp
        ${PROJECT_SOURCE_DIR}/../tracker/src/svmMunkres.cpp
        ${PROJECT_SOURCE_DIR}/../tracker/src/svmTrack.cpp
        ${PROJECT_SOURCE_DIR}/../tracker/src/svmTracker.cpp
        )

# ── Windows ────────────────────────────────────────────────────────────────────
if(MSVC)
    add_executable(${OUTPUT_ELF}
            ${SRC_FILES}
            ${DEPENDENCIES_DIR}/gl3w/src/gl3w.c
            src/win/svmViewMainWin.cpp
            )
    target_include_directories(${OUTPUT_ELF} PUBLIC
            ${DEPENDENCIES_DIR}/opencv/build/include
            ${DEPENDENCIES_DIR}/gl3w/include/gl3w
            ${DEPENDENCIES_DIR}/glfw/include
            ${DEPENDENCIES_DIR}/glm
            ${DEPENDENCIES_DIR}/libxml2/include/libxml2
            ${DEPENDENCIES_DIR}/stb
            ${DEPENDENCIES_DIR}/assimp/include
            ${DEPENDENCIES_DIR}/eigen/include
            ${DEPENDENCIES_DIR}/freetype/include/freetype2
            ${DEPENDENCIES_DIR}/dirent
            ${PROJECT_SOURCE_DIR}/../core/inc
            ${PROJECT_SOURCE_DIR}/../tracker/inc
            ${PROJECT_SOURCE_DIR}/../../common
            inc
            inc/win
            "C:/Program Files (x86)/Windows Kits/10/Include/10.0.22621.0/cppwinrt"
            "C:/Program Files (x86)/Windows Kits/10/Include/10.0.22621.0/shared"
            "C:/Program Files (x86)/Windows Kits/10/Include/10.0.22621.0/ucrt"
            "C:/Program Files (x86)/Windows Kits/10/Include/10.0.22621.0/um"
            "C:/Program Files (x86)/Windows Kits/10/Include/10.0.22621.0/winrt"
            )
    target_link_directories(${OUTPUT_ELF} PUBLIC
            ${DEPENDENCIES_DIR}/opencv/build/x64/vc15/lib
            ${DEPENDENCIES_DIR}/libxml2/lib
            ${DEPENDENCIES_DIR}/glfw/lib
            ${DEPENDENCIES_DIR}/assimp/lib
            ${DEPENDENCIES_DIR}/freetype/lib
            "C:/Program Files (x86)/Windows Kits/10/Lib/10.0.22621.0/um/x64"
            "C:/Windows/System32"
            )
    target_link_libraries(${OUTPUT_ELF} PUBLIC
            opengl32 glfw3 assimpd opencv_world454d libxml2d freetyped
            )
    target_compile_definitions(${OUTPUT_ELF} PUBLIC
            PROJECT_SRC_DIR="${CMAKE_SOURCE_DIR}"
            ${APP_OPTION}
            )
    target_compile_options(${OUTPUT_ELF} PUBLIC /W3 -DGENERATE_OD)

# ── Linux ──────────────────────────────────────────────────────────────────────
else()
    set(CMAKE_C_COMPILER /usr/bin/gcc)
    set(CMAKE_CXX_COMPILER /usr/bin/g++)
    set(TARGET_PROJECT_DIR ${CMAKE_SYSROOT})
    set(TARGET_PATH_LIB ${TARGET_PROJECT_DIR}/usr/lib)
    set(TARGET_PATH_INCLUDE ${TARGET_PROJECT_DIR}/usr/include)

    add_executable(${OUTPUT_ELF}
            ${SRC_FILES}
            ${DEPENDENCIES_DIR}/gl3w/src/gl3w.c
            ${PROJECT_SOURCE_DIR}/../../io/io_platform.cpp
            src/linux/svmViewMainLinux.cpp
            )

    target_include_directories(${OUTPUT_ELF} PUBLIC
            ${TARGET_PATH_LIB}
            ${TARGET_PATH_INCLUDE}
            ${TARGET_PATH_INCLUDE}/apriltag/common
            inc
            inc/linux
            ${PROJECT_SOURCE_DIR}/../core/inc
            ${PROJECT_SOURCE_DIR}/../tracker/inc
            ${PROJECT_SOURCE_DIR}/../../common
            ${PROJECT_SOURCE_DIR}/../../io
            ${TARGET_PATH_INCLUDE}/opencv4
            ${DEPENDENCIES_DIR}/gl3w/include/gl3w
            ${TARGET_PATH_INCLUDE}/glm
            ${TARGET_PATH_INCLUDE}/libxml2
            ${TARGET_PATH_INCLUDE}/stb
            ${TARGET_PATH_INCLUDE}/assimp
            ${TARGET_PATH_INCLUDE}/eigen3
            ${TARGET_PATH_INCLUDE}/freetype2
            )

    target_link_directories(${OUTPUT_ELF} PUBLIC
            /lib
            /usr/lib
            /usr/local/lib
            /usr/lib/x86_64-linux-gnu
            ${TARGET_PROJECT_DIR}/lib
            ${TARGET_PROJECT_DIR}/usr/lib
            ${TARGET_PROJECT_DIR}/usr/local/lib
            ${TARGET_PROJECT_DIR}/usr/lib/x86_64-linux-gnu
            )

    target_link_libraries(${OUTPUT_ELF} PUBLIC
            -Wl,--library-path=${TARGET_PATH_LIB},-rpath-link=${TARGET_PATH_LIB}
            m stdc++ pthread dl
            xml2 assimp freetype
            opencv_core opencv_imgcodecs opencv_imgproc opencv_highgui opencv_videoio opencv_calib3d
            GLESv2 EGL
            ${BACKEND_LINK}
            )

    target_compile_definitions(${OUTPUT_ELF} PUBLIC
            PROJECT_SRC_DIR="${CMAKE_SOURCE_DIR}"
            ${BACKEND_OPTION} ${APP_OPTION}
            )

    target_compile_options(${OUTPUT_ELF} PUBLIC -Wall -O3 -DGENERATE_OD)
endif()
