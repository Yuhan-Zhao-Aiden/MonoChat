cmake_minimum_required(VERSION 3.25)

project(MonoChat LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_compile_options(-Wall -Wextra -pedantic -g)

# ---- Server logic library (reused by tests) ----
add_library(server_lib STATIC
    server/ClientPool.cpp
)
target_include_directories(server_lib PUBLIC server)

# ---- Executables ----
add_executable(server server/Server.cpp)
target_include_directories(server PRIVATE server)
target_link_libraries(server PRIVATE server_lib)

add_executable(client client/Client.cpp client/ChatUI.cpp)
target_include_directories(client PRIVATE client)
find_package(Curses REQUIRED)
target_link_libraries(client PRIVATE ${CURSES_LIBRARIES})
target_include_directories(client PRIVATE ${CURSES_INCLUDE_DIR})

# ---- Google Test ----
include(FetchContent)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

enable_testing()
add_subdirectory(tests)