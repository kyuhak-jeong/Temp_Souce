cmake_minimum_required(VERSION 3.5)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_BUILD_TYPE Release)

set(LibraryName "RknnHelper")

# Create library
add_library (${LibraryName}
            src/rknn_processer.cpp
            include/rknn_helper.hpp
            include/rknn_infos.hpp
            include/rknn_struct.hpp
)
target_include_directories(${LibraryName} PRIVATE ${CMAKE_CURRENT_LIST_DIR}/include)

# Link RknnRT module
set(RKNN_API_PATH ${CMAKE_CURRENT_LIST_DIR}/core/rknnrt)
include_directories(${RKNN_API_PATH}/include)
target_link_libraries(${LibraryName} PRIVATE ${RKNN_API_PATH}/lib/aarch64/librknnrt.so)
