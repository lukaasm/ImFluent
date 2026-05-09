cmake_minimum_required(VERSION 4.0 FATAL_ERROR)

project(ImFluent LANGUAGES CXX)

find_package(imgui CONFIG REQUIRED)

add_library(ImFluent STATIC)
target_sources(ImFluent
    PUBLIC
        src/imfluent.h
        src/imfluent_icons.h
        src/imfluent_internal.h
    PRIVATE
        src/imfluent.cpp
        src/imfluent_demo.cpp
)
target_include_directories(ImFluent PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(ImFluent PUBLIC imgui::imgui)

set_target_properties(ImFluent PROPERTIES
    CXX_STANDARD 11
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

if (VCPKG_MANIFEST_FEATURES MATCHES "examples")
    add_executable(ImFluentGallery src/main.cpp)

    target_link_libraries(ImFluentGallery
        PUBLIC
            ImFluent
    )
endif()
