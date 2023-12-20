cmake_minimum_required(VERSION 3.5)

if (NOT DEFINED PROJECT_NAME)
    include($ENV{IDF_PATH}/tools/cmake/project.cmake)
    project(__UI_PROJECT_NAME__)
else()
    message(FATAL_ERROR "__UI_PROJECT_NAME__: This must be a project's main CMakeLists.txt.")
endif()