cmake_minimum_required(VERSION 3.0)

include(ExternalProject)

ExternalProject_Add(googletest
    GIT_REPOSITORY "https://github.com/google/googletest.git"
    GIT_TAG "release-1.7.0"
    UPDATE_COMMAND ""
    INSTALL_COMMAND ""
    CMAKE_ARGS -DCMAKE_BINARY_DIR=${CMAKE_BINARY_DIR}
        -DCMAKE_OSX_DEPLOYMENT_TARGET=${CMAKE_OSX_DEPLOYMENT_TARGET}
        -DCMAKE_OSX_SYSROOT=${CMAKE_OSX_SYSROOT}
        -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
        -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
)
ExternalProject_Get_Property(googletest SOURCE_DIR BINARY_DIR)
SET(googletest_SOURCE_DIR ${SOURCE_DIR})
SET(googletest_BINARY_DIR ${BINARY_DIR})

include_directories("${googletest_SOURCE_DIR}/include")
link_directories("${googletest_BINARY_DIR}")

add_executable(vtest
    test.cpp
    test_vobj.cpp
    test_vmsg.cpp
    test_vlaunch.cpp

    ../vobj.cpp
    ../vmsg.cpp
    ../vlaunch.cpp
    ../log.cpp
)

target_link_libraries(vtest
    gtest
)

add_test(vtest "${CMAKE_CURRENT_BINARY_DIR}/vtest")
