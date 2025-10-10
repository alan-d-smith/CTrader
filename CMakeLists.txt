cmake_minimum_required(VERSION 3.16)
project(ctrader LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/permissive- /Zc:__cplusplus /utf-8 /W4)
endif()

# ---- ImGui setup ----
set(IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external/imgui)
if(NOT EXISTS "${IMGUI_DIR}/imgui.h")
    message(FATAL_ERROR "Dear ImGui not found in external/imgui. Did you initialise submodules?")
endif()

set(IMGUI_SOURCES
        ${IMGUI_DIR}/imgui.cpp
        ${IMGUI_DIR}/imgui_draw.cpp
        ${IMGUI_DIR}/imgui_tables.cpp
        ${IMGUI_DIR}/imgui_widgets.cpp
        ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
        ${IMGUI_DIR}/backends/imgui_impl_opengl2.cpp
        ${IMGUI_DIR}/misc/cpp/imgui_stdlib.cpp
)

# ---- Source files ----
file(GLOB_RECURSE SRC_FILES CONFIGURE_DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
)

add_executable(ctrader
        ${SRC_FILES}
        ${IMGUI_SOURCES}
)

# ---- Copy assets and certificates after build ----
add_custom_command(TARGET ctrader POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/assets"
        "$<TARGET_FILE_DIR:ctrader>/assets"
)

add_custom_command(TARGET ctrader POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/certs/cacert.pem"
        "$<TARGET_FILE_DIR:ctrader>/cacert.pem"
)

# ---- Include directories ----
target_include_directories(ctrader PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party
        ${CMAKE_CURRENT_SOURCE_DIR}/external/stb
        ${IMGUI_DIR}
        ${IMGUI_DIR}/backends
)

# ---- External deps ----
find_package(glfw3 CONFIG REQUIRED)
find_package(OpenGL REQUIRED)
find_package(CURL CONFIG REQUIRED)

target_link_libraries(ctrader PRIVATE
        glfw
        OpenGL::GL
        CURL::libcurl
)

if(WIN32)
    target_link_libraries(ctrader PRIVATE Dwmapi)
endif()

# Enable std::string overloads for ImGui::InputText
target_compile_definitions(ctrader PRIVATE IMGUI_ENABLE_STD_STRING)
