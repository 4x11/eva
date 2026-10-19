find_package(Catch2 3 QUIET CONFIG)

if(NOT Catch2_FOUND)
    include(FetchContent)
    FetchContent_Declare(Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG v3.8.1
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(Catch2)
    list(APPEND CMAKE_MODULE_PATH "${catch2_SOURCE_DIR}/extras")
endif()

add_executable(eva_tests
    "binary_reader_tests.cpp"
    "binary_writer_tests.cpp"
)

target_link_libraries(eva_tests
    PRIVATE
    eva::eva
    Catch2::Catch2WithMain
)

include(Catch)
catch_discover_tests(eva_tests)
