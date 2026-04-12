add_executable(MonoChatTests
    ClientPoolTest.cpp
)

target_link_libraries(MonoChatTests PRIVATE
    server_lib
    GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(MonoChatTests)
