# Unit tests (SRE_TESTS): ctest --test-dir <build>.

add_executable(test_mackeys test_mackeys.c ../common/host_mackeys.c)
target_include_directories(test_mackeys PRIVATE ../common)
target_link_libraries(test_mackeys PRIVATE SDL3::Headers)
add_test(NAME mackeys COMMAND test_mackeys)

# The launcher's game files installer: wxBase only, no GUI.
if(TARGET srelauncher)
    enable_language(CXX)
    add_executable(test_gamefiles test_gamefiles.cpp ../launcher/gamefiles.cpp ../launcher/sha256.cpp)
    target_include_directories(test_gamefiles PRIVATE ../launcher)
    target_compile_features(test_gamefiles PRIVATE cxx_std_17)
    # As in launcher/CMakeLists.txt; its imported targets aren't visible here.
    find_package(wxWidgets 3.2 CONFIG QUIET COMPONENTS base)
    if(wxWidgets_FOUND AND TARGET wx::wxbase)
        target_link_libraries(test_gamefiles PRIVATE wx::wxbase)
    else()
        find_package(wxWidgets 3.2 REQUIRED COMPONENTS base)
        include(${wxWidgets_USE_FILE})
        target_link_libraries(test_gamefiles PRIVATE ${wxWidgets_LIBRARIES})
    endif()
    add_test(NAME gamefiles COMMAND test_gamefiles)
endif()

# The launcher's art pack download: the manifest, the update offer and the installer; wxBase only.
if(TARGET srelauncher)
    add_executable(test_artpack test_artpack.cpp ../launcher/artpack.cpp ../launcher/gamefiles.cpp
                   ../launcher/sha256.cpp)
    target_include_directories(test_artpack PRIVATE ../launcher)
    target_compile_features(test_artpack PRIVATE cxx_std_17)
    if(TARGET wx::wxbase)
        target_link_libraries(test_artpack PRIVATE wx::wxbase)
    else()
        target_link_libraries(test_artpack PRIVATE ${wxWidgets_LIBRARIES})
    endif()
    add_test(NAME artpack COMMAND test_artpack)
endif()

# The launcher's command line for the game: wxBase only.
if(TARGET srelauncher)
    add_executable(test_args test_args.cpp ../launcher/gameargs.cpp)
    target_include_directories(test_args PRIVATE ../launcher)
    target_compile_features(test_args PRIVATE cxx_std_17)
    if(TARGET wx::wxbase)
        target_link_libraries(test_args PRIVATE wx::wxbase)
    else()
        target_link_libraries(test_args PRIVATE ${wxWidgets_LIBRARIES})
    endif()
    add_test(NAME args COMMAND test_args)
endif()

# The downloader for real (see the file): run by hand, not by ctest.
if(TARGET srelauncher)
    find_package(wxWidgets 3.2 CONFIG QUIET COMPONENTS net base)
    add_executable(test_download test_download.cpp ../launcher/download.cpp ../launcher/gamefiles.cpp
                   ../launcher/artpack.cpp ../launcher/sha256.cpp)
    target_include_directories(test_download PRIVATE ../launcher)
    target_compile_features(test_download PRIVATE cxx_std_17)
    if(TARGET wx::wxnet)
        target_link_libraries(test_download PRIVATE wx::wxnet wx::wxbase)
    else()
        find_package(wxWidgets 3.2 REQUIRED COMPONENTS net base)
        target_link_libraries(test_download PRIVATE ${wxWidgets_LIBRARIES})
    endif()
endif()

# The remaster's picture dump; needs the original game files: -DSRE_TEST_GAME_DIR=<folder with SR.EXE>.
if(SRE_TEST_GAME_DIR)
    add_test(NAME dump COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/test_dump.sh $<TARGET_FILE:streetrod-enhanced>
             ${SRE_TEST_GAME_DIR})
    # Opens a window for 100 s.
    add_test(NAME identity COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/test_identity.sh $<TARGET_FILE:streetrod-enhanced>
             ${SRE_TEST_GAME_DIR} ${CMAKE_SOURCE_DIR})
    set_tests_properties(identity PROPERTIES TIMEOUT 220)
endif()

# The remaster's per-pixel tags, linked against the whole engine.
add_executable(test_tags test_tags.c)
target_link_libraries(test_tags PRIVATE sre_core)
add_test(NAME tags COMMAND test_tags)
add_executable(test_color test_color.c)
target_link_libraries(test_color PRIVATE sre_core)
add_test(NAME color COMMAND test_color)
add_executable(test_music test_music.c)
target_link_libraries(test_music PRIVATE sre_core)
add_test(NAME music COMMAND test_music)
add_executable(test_compose test_compose.c)
target_link_libraries(test_compose PRIVATE sre_core)
add_test(NAME compose COMMAND test_compose)

# The shared host: Street Rod 1's audio pinned, the AdLib chip, voice volumes, the mouse space.
add_executable(test_host test_host.c)
target_link_libraries(test_host PRIVATE sre_host)
foreach(t sr1_audio_unchanged opl_renders opl_voice_gain mouse_space mouse_space_set)
    add_test(NAME host_${t} COMMAND test_host ${t})
endforeach()

# Street Rod 2; needs its game files: -DSRE_TEST_GAME2_DIR=<folder with SR2's SR.EXE>.
if(SRE_TEST_GAME2_DIR)
    if(SRE_TEST_GAME_DIR)
        add_test(NAME sr2_check COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/test_sr2_check.sh
                 $<TARGET_FILE:streetrod2-enhanced> ${SRE_TEST_GAME2_DIR} $<TARGET_FILE:streetrod-enhanced>
                 ${SRE_TEST_GAME_DIR})
    else()
        add_test(NAME sr2_check COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/test_sr2_check.sh
                 $<TARGET_FILE:streetrod2-enhanced> ${SRE_TEST_GAME2_DIR})
    endif()
endif()
