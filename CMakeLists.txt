cmake_minimum_required(VERSION 3.27)
project(Sonic3Atlus)

set(CMAKE_CXX_STANDARD 23)
add_subdirectory(vendored/SDL EXCLUDE_FROM_ALL)
add_executable(Sonic3Atlus main.cpp
        draw.cpp
        consts.cpp
        common.h
        textureManager.cpp
)
target_link_libraries(Sonic3Atlus PRIVATE SDL3::SDL3)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

#target_compile_options(Sonic3Atlus PRIVATE -fsanitize=address -fno-omit-frame-pointer -g)
#target_link_options(Sonic3Atlus PRIVATE -fsanitize=address)

#add_custom_target(run_my_script
#        COMMAND ${CMAKE_COMMAND} -E echo "Compiling Shaders"
#        COMMAND ${CMAKE_COMMAND} -E env LD_LIBRARY_PATH=/home/alexoxorn/installs/lib:/usr/local/lib bash ${CMAKE_CURRENT_SOURCE_DIR}/shaders/compile.sh
#        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/shaders
#        COMMENT "Executing script before compiling targets..."
#)
#add_dependencies(Sonic3Atlus run_my_script)