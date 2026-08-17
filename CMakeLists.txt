cmake_minimum_required(VERSION 3.27)
project(Sonic3Atlus)

set(CMAKE_CXX_STANDARD 23)
add_subdirectory(vendored/SDL EXCLUDE_FROM_ALL)


file(GLOB SUB_SRCS1 CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/vendored/imgui/*.cpp")

find_package(OpenGL REQUIRED)
add_executable(Sonic3Atlus
        main.cpp
        draw.cpp
        consts.cpp
        common.h
        textureManager.cpp
        main_renderer.cpp
        main_renderer.h
        main_menu.cpp
        main_menu.h
)
target_link_libraries(Sonic3Atlus PRIVATE SDL3::SDL3)
target_link_libraries(Sonic3Atlus PRIVATE OpenGL::GL)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

target_include_directories(Sonic3Atlus PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/vendored/imgui)
target_include_directories(Sonic3Atlus PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/vendored/imgui/backends)

target_sources(Sonic3Atlus
        PRIVATE
        ${SUB_SRCS1}
        ${CMAKE_CURRENT_SOURCE_DIR}/vendored/imgui/backends/imgui_impl_sdl3.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/vendored/imgui/backends/imgui_impl_opengl3.cpp
)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    # Add compiler flag
    target_compile_options(Sonic3Atlus PRIVATE -stdlib=libc++)

    # Add linker flag
    target_link_options(Sonic3Atlus PRIVATE -stdlib=libc++)
endif()

#target_compile_options(Sonic3Atlus PRIVATE -fsanitize=address -fno-omit-frame-pointer -g)
#target_link_options(Sonic3Atlus PRIVATE -fsanitize=address)

#add_custom_target(run_my_script
#        COMMAND ${CMAKE_COMMAND} -E echo "Compiling Shaders"
#        COMMAND ${CMAKE_COMMAND} -E env LD_LIBRARY_PATH=/home/alexoxorn/installs/lib:/usr/local/lib bash ${CMAKE_CURRENT_SOURCE_DIR}/shaders/compile.sh
#        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/shaders
#        COMMENT "Executing script before compiling targets..."
#)
#add_dependencies(Sonic3Atlus run_my_script)