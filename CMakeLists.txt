cmake_minimum_required(VERSION 3.21)

project(eva LANGUAGES CXX)

option(EVA_BUILD_TESTS "Build eva tests" ${PROJECT_IS_TOP_LEVEL})

add_library(eva INTERFACE)
add_library(eva::eva ALIAS eva)

target_include_directories(eva
    INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

target_compile_features(eva
    INTERFACE
    cxx_std_23
)

if(EVA_BUILD_TESTS)
    include(CTest)
    if(BUILD_TESTING)
        add_subdirectory("tests")
    endif()
endif()
