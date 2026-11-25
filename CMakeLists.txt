cmake_minimum_required(VERSION 3.16)
project(BedwarsPC LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(SDL2 CONFIG REQUIRED)

add_executable(bedwars Bedwars.cpp)
if(TARGET SDL2::SDL2)
    target_link_libraries(bedwars PRIVATE SDL2::SDL2)
elseif(TARGET SDL2::SDL2-static)
    target_link_libraries(bedwars PRIVATE SDL2::SDL2-static)
else()
    message(FATAL_ERROR "SDL2 package found, but it does not define SDL2::SDL2 or SDL2::SDL2-static")
endif()

if(WIN32 AND TARGET SDL2::SDL2)
    get_target_property(SDL2_TARGET_TYPE SDL2::SDL2 TYPE)
    if(SDL2_TARGET_TYPE STREQUAL "SHARED_LIBRARY")
        add_custom_command(TARGET bedwars POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$<TARGET_FILE:SDL2::SDL2>"
                "$<TARGET_FILE_DIR:bedwars>")
    endif()
endif()
