cmake_minimum_required(VERSION 3.20)
project(mtm LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23) # -std=c++23, cmake should correctly add this
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()


file(GLOB ALL_SOURCES "src/*.cpp")

foreach(source_file ${ALL_SOURCES})
    get_filename_component(exe_name ${source_file} NAME_WE)
    
    add_executable(${exe_name} ${source_file})
    
    target_include_directories(${exe_name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
    
    target_compile_options(${exe_name} PRIVATE
        # debug build flags
        $<$<CONFIG:Debug>:
            -pedantic-errors
            -ggdb 
            -Wall 
            -Weffc++ 
            -Wextra 
            -Wconversion 
            -Wsign-conversion 
            -Werror
            -Wshadow 
            -Wnon-virtual-dtor 
            -pedantic 
            -Wunused 
            -Wold-style-cast 
            -Wpedantic
            -Wimplicit-fallthrough 
            -Wformat=2
        >
        # release build flags
        $<$<CONFIG:Release>:
            -O2
            # -O3 -march=native
        >
    )

    target_compile_definitions(${exe_name} PRIVATE
        $<$<CONFIG:Release>:NDEBUG>
    )
endforeach()
