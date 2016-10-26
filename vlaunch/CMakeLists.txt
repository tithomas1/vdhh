cmake_minimum_required(VERSION 3.0)

project(vlaunchd CXX)

set(SDK "macosx")

execute_process(
    COMMAND xcrun --show-sdk-path --sdk ${SDK}
    OUTPUT_VARIABLE CMAKE_OSX_SYSROOT
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

execute_process(
    COMMAND xcrun --find --sdk ${SDK} clang++
    OUTPUT_VARIABLE CMAKE_CXX_COMPILER
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

execute_process(
    COMMAND xcrun --find --sdk ${SDK} clang
    OUTPUT_VARIABLE CMAKE_C_COMPILER
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

set(CMAKE_OSX_DEPLOYMENT_TARGET "10.10")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -g")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c11 -g")

set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -sectcreate __TEXT __info_plist ${CMAKE_CURRENT_SOURCE_DIR}/Info.plist -sectcreate __TEXT __launchd_plist ${CMAKE_CURRENT_SOURCE_DIR}/Launchd.plist")

include_directories("${PROJECT_SOURCE_DIR}/..")

add_executable(vlaunchd
    main.cpp
    vobj.cpp
    vlaunch.cpp
    vmsg.cpp
    log.cpp
)

enable_testing()
add_subdirectory(test)
