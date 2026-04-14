cmake_minimum_required(VERSION 3.25)

project(MonoChat LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_compile_options(-Wall -Wextra -pedantic)
if(CMAKE_BUILD_TYPE STREQUAL "Debug" OR NOT CMAKE_BUILD_TYPE)
  add_compile_options(-g)
endif()

# ---- Server logic library (reused by tests) ----
add_library(server_lib STATIC
    server/ClientPool.cpp
)
target_include_directories(server_lib PUBLIC server)

# ---- Executables ----
add_executable(server server/Server.cpp)
target_include_directories(server PRIVATE server)
target_link_libraries(server PRIVATE server_lib)

option(BUILD_CLIENT "Build the ncurses client" ON)
if(BUILD_CLIENT)
  add_executable(client client/Client.cpp client/ChatUI.cpp)
  target_include_directories(client PRIVATE client)
  find_package(Curses REQUIRED)
  target_link_libraries(client PRIVATE ${CURSES_LIBRARIES})
  target_include_directories(client PRIVATE ${CURSES_INCLUDE_DIR})
endif()

option(BUILD_TESTS "Build tests" ON)
if(BUILD_TESTS)
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
endif()

# --- OpenSSL ---
find_package(OpenSSL REQUIRED)
target_link_libraries(server_lib PRIVATE OpenSSL::SSL OpenSSL::Crypto)
target_link_libraries(server     PRIVATE OpenSSL::SSL OpenSSL::Crypto)
if(BUILD_CLIENT)
  target_link_libraries(client   PRIVATE OpenSSL::SSL OpenSSL::Crypto)
endif()

