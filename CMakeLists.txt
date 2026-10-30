cmake_minimum_required(VERSION 3.21)

project(eva VERSION 0.1.0 LANGUAGES CXX)

option(EVA_BUILD_TESTS "Build eva tests" ${PROJECT_IS_TOP_LEVEL})
option(EVA_INSTALL "Generate eva install rules" ${PROJECT_IS_TOP_LEVEL})

include(GNUInstallDirs)

add_library(eva INTERFACE)
add_library(eva::eva ALIAS eva)

target_include_directories(eva
    INTERFACE
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
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

if(EVA_INSTALL)
    include(CMakePackageConfigHelpers)

    set(EVA_INSTALL_CMAKEDIR "${CMAKE_INSTALL_DATADIR}/cmake/eva")

    install(TARGETS eva EXPORT evaTargets)

    install(DIRECTORY "include/"
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
    )

    install(EXPORT evaTargets
        NAMESPACE eva::
        DESTINATION "${EVA_INSTALL_CMAKEDIR}"
    )

    configure_package_config_file(
        "cmake/evaConfig.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/evaConfig.cmake"
        INSTALL_DESTINATION "${EVA_INSTALL_CMAKEDIR}"
    )

    write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/evaConfigVersion.cmake"
        COMPATIBILITY SameMajorVersion
        ARCH_INDEPENDENT
    )

    install(
        FILES
        "${CMAKE_CURRENT_BINARY_DIR}/evaConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/evaConfigVersion.cmake"
        DESTINATION "${EVA_INSTALL_CMAKEDIR}"
    )
endif()
