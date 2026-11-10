# Unit tests (SRE_TESTS): ctest --test-dir <build>.

add_executable(test_mackeys test_mackeys.c ../src/host_mackeys.c)
target_include_directories(test_mackeys PRIVATE ../src)
target_link_libraries(test_mackeys PRIVATE SDL3::Headers)
add_test(NAME mackeys COMMAND test_mackeys)
