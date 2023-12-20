cmake_minimum_required(VERSION 3.5)

if (NOT DEFINED PROJECT_NAME)
    include($ENV{IDF_PATH}/tools/cmake/project.cmake)
    project(esp_idf_t_display_s3)
else()
    message(FATAL_ERROR "esp_idf_t_display_s3: This must be a project's main CMakeLists.txt.")
endif()
