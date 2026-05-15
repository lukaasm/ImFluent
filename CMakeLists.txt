cmake_minimum_required(VERSION 4.0 FATAL_ERROR)

# Read project metadata from vcpkg.json so the manifest is the single source of truth.
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg.json" _imfluent_manifest)
string(JSON _imfluent_version     GET "${_imfluent_manifest}" "version")
string(JSON _imfluent_description GET "${_imfluent_manifest}" "description")
string(JSON _imfluent_homepage    GET "${_imfluent_manifest}" "homepage")

project(ImFluent
    VERSION       ${_imfluent_version}
    DESCRIPTION   ${_imfluent_description}
    HOMEPAGE_URL  ${_imfluent_homepage}
    LANGUAGES     CXX
)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

find_package(imgui CONFIG REQUIRED)

add_library(ImFluent STATIC)
add_library(ImFluent::ImFluent ALIAS ImFluent)

target_sources(ImFluent
    PUBLIC
        FILE_SET HEADERS
            BASE_DIRS src
            FILES
                src/imfluent.h
                src/imfluent_icons.h
                src/imfluent_internal.h
    PRIVATE
        src/imfluent.cpp
        src/imfluent_demo.cpp
)

target_link_libraries(ImFluent PUBLIC imgui::imgui)

set_target_properties(ImFluent PROPERTIES
    CXX_STANDARD 11
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

if (VCPKG_MANIFEST_FEATURES MATCHES "examples")
    add_executable(ImFluentGallery src/main.cpp)
    target_link_libraries(ImFluentGallery PRIVATE ImFluent)
endif()

# ---------------------------------------------------------------------------
# Install / package config
# ---------------------------------------------------------------------------

install(TARGETS ImFluent
    EXPORT ImFluentTargets
    ARCHIVE          DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY          DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME          DESTINATION ${CMAKE_INSTALL_BINDIR}
    FILE_SET HEADERS DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    INCLUDES         DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

set(ImFluent_CMAKE_INSTALL_DIR ${CMAKE_INSTALL_LIBDIR}/cmake/ImFluent)

install(EXPORT ImFluentTargets
    FILE        ImFluentTargets.cmake
    NAMESPACE   ImFluent::
    DESTINATION ${ImFluent_CMAKE_INSTALL_DIR}
)

# Embedded config template - written to the build tree at configure time so
# configure_package_config_file() can do the @PACKAGE_INIT@ substitution.
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/ImFluentConfig.cmake.in" [[
@PACKAGE_INIT@

include(CMakeFindDependencyMacro)
find_dependency(imgui CONFIG)

include("${CMAKE_CURRENT_LIST_DIR}/ImFluentTargets.cmake")

check_required_components(ImFluent)
]])

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/ImFluentConfigVersion.cmake"
    VERSION       ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

configure_package_config_file(
    "${CMAKE_CURRENT_BINARY_DIR}/ImFluentConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/ImFluentConfig.cmake"
    INSTALL_DESTINATION ${ImFluent_CMAKE_INSTALL_DIR}
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/ImFluentConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/ImFluentConfigVersion.cmake"
    DESTINATION ${ImFluent_CMAKE_INSTALL_DIR}
)

export(EXPORT ImFluentTargets
    FILE      "${CMAKE_CURRENT_BINARY_DIR}/ImFluentTargets.cmake"
    NAMESPACE ImFluent::
)
