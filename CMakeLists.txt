cmake_minimum_required(VERSION 3.20)
project(file_integrity_toolkit VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(filetool_core
    src/file_ops.cpp
    src/sha256.cpp
)
target_include_directories(filetool_core PUBLIC include)

if(MSVC)
    target_compile_options(filetool_core PRIVATE /W4 /permissive- /utf-8)
    target_link_libraries(filetool_core PUBLIC bcrypt)
else()
    find_package(OpenSSL REQUIRED)
    target_compile_options(filetool_core PRIVATE -Wall -Wextra -Wpedantic)
    target_link_libraries(filetool_core PUBLIC OpenSSL::Crypto)
endif()

add_executable(filetool src/main.cpp)
target_link_libraries(filetool PRIVATE filetool_core)

enable_testing()
add_executable(filetool_tests tests/test_file_ops.cpp)
target_link_libraries(filetool_tests PRIVATE filetool_core)
add_test(NAME filetool_tests COMMAND filetool_tests)

install(TARGETS filetool RUNTIME DESTINATION bin)

