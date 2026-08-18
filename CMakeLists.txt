cmake_minimum_required(VERSION 3.24)

project(arnold-usd)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/modules" 0)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/utils" 0)
list(APPEND CMAKE_MODULE_PATH "${USD_LOCATION}/lib/cmake/" 0)

# Define the project and build options
include(options)

# Some USD builds (e.g. shared/monolithic with GL enabled) export a "garch" target whose
# link interface references OpenGL::GL, but their pxrConfig.cmake never calls
# find_dependency(OpenGL). It has to be found before find_package(USD) or that fails with
# "The link interface of target garch contains OpenGL::GL but the target was not found".
find_package(OpenGL)

# Global required packagse
find_package(USD REQUIRED)
find_package(Arnold REQUIRED)

# Include build helper functions
include(build)

# Include default compiler options 
include(defaults)

# If we are building the bundle, we don't want to build the other plugins
if (BUILD_BUNDLE AND NOT BUILD_WITH_USD_STATIC)
    message(STATUS "Building Bundle - deactivating other plugins")
    set(BUILD_RENDER_DELEGATE OFF)
    set(BUILD_NDR_PLUGIN OFF)
    set(BUILD_USD_IMAGING_PLUGIN OFF)
    set(BUILD_SCENE_INDEX_PLUGIN OFF)
    set(BUILD_PROCEDURAL OFF)
else()
    # And if we build with USD static, we don't want the bundle (for now)
    message(STATUS "Setting BUILD_BUNDLE=OFF and ENABLE_SCENE_INDEX_IN_BUNDLE=OFF")
    set(BUILD_BUNDLE OFF)
    set(ENABLE_SCENE_INDEX_IN_BUNDLE OFF)
endif()

# Adding libs and plugins
add_subdirectory(libs)
add_subdirectory(plugins)


if (BUILD_USDGENSCHEMA_ARNOLD)
    message(STATUS "Building custom usdGenSchema")
    add_subdirectory(tools/usdgenschema)
endif()

if (BUILD_TURNTABLE)
    message(STATUS "Building turntable tool")
    add_subdirectory(tools/turntable)
endif()

# Building the schemas
if (BUILD_SCHEMAS)
    add_subdirectory(schemas)
    install(FILES plugInfo.json
        DESTINATION "${PREFIX_SCHEMA}")
endif ()

if (BUILD_DOCS)
    add_subdirectory(docs)
endif ()

# We are only testing the procedural
if (BUILD_TESTSUITE AND BUILD_PROCEDURAL)
    enable_testing()
    add_subdirectory(testsuite)
endif ()

if (NOT BUILD_WITH_USD_STATIC)
    if (BUILD_RENDER_DELEGATE OR BUILD_NDR_PLUGIN OR BUILD_USD_IMAGING_PLUGIN OR BUILD_SCENE_INDEX_PLUGIN)
        install(FILES plugInfo.json
            DESTINATION "${PREFIX_PLUGINS}")
    endif ()
    if (BUILD_BUNDLE)
        install(FILES plugInfo.json
            DESTINATION "${PREFIX_BUNDLE}")
    endif()

    # A shared/monolithic USD is not embedded in what we build, so it (and Arnold) have to
    # be located at runtime. Generate a setup.sh next to this install for that, resolving
    # everything belonging to the install itself relative to the script's own location so
    # it stays relocatable; only the external Arnold and USD locations are baked in.
    set(ARNOLD_USD_SETUP_PLUGIN_DIRS_LIST "")
    if (BUILD_BUNDLE)
        list(APPEND ARNOLD_USD_SETUP_PLUGIN_DIRS_LIST "\${_arnold_usd_root}/${PREFIX_BUNDLE}")
    elseif (BUILD_RENDER_DELEGATE OR BUILD_NDR_PLUGIN OR BUILD_USD_IMAGING_PLUGIN OR BUILD_SCENE_INDEX_PLUGIN)
        list(APPEND ARNOLD_USD_SETUP_PLUGIN_DIRS_LIST "\${_arnold_usd_root}/${PREFIX_PLUGINS}")
    endif ()
    if (BUILD_SCHEMAS)
        list(APPEND ARNOLD_USD_SETUP_PLUGIN_DIRS_LIST "\${_arnold_usd_root}/${PREFIX_SCHEMA}")
    endif ()
    list(JOIN ARNOLD_USD_SETUP_PLUGIN_DIRS_LIST ":" ARNOLD_USD_SETUP_PLUGIN_DIRS)

    set(ARNOLD_USD_SETUP_PROCEDURAL_DIR "")
    if (BUILD_PROCEDURAL)
        set(ARNOLD_USD_SETUP_PROCEDURAL_DIR "\${_arnold_usd_root}/${PREFIX_PROCEDURAL}")
    endif ()

    configure_file(
        "${CMAKE_SOURCE_DIR}/cmake/templates/setup.sh.in"
        "${CMAKE_BINARY_DIR}/setup.sh"
        @ONLY)
    install(PROGRAMS "${CMAKE_BINARY_DIR}/setup.sh" DESTINATION .)
endif()

install(FILES LICENSE.md DESTINATION .)
