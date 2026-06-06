cmake_minimum_required(VERSION 3.5)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_BUILD_TYPE Release)

set(LibraryName "ObjectDetection")

# Create library
add_library (${LibraryName}
            src/object_detection.cpp
            include/object_detection.hpp
)
target_include_directories(${LibraryName} PRIVATE ${CMAKE_CURRENT_LIST_DIR}/include)

# Link RknnHelper module
target_include_directories(${LibraryName} PRIVATE ${CMAKE_CURRENT_LIST_DIR}/../rknn_helper/include)
target_link_libraries(${LibraryName} RknnHelper)


