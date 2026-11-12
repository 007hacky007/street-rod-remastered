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

# The launcher's check of a game folder, Street Rod's or Street Rod 2's: wxBase only.
if(TARGET srelauncher)
    add_executable(test_gamefolder test_gamefolder.cpp ../launcher/gamefolder.cpp ../launcher/gameargs.cpp)
    target_include_directories(test_gamefolder PRIVATE ../launcher)
    target_compile_features(test_gamefolder PRIVATE cxx_std_17)
    if(TARGET wx::wxbase)
        target_link_libraries(test_gamefolder PRIVATE wx::wxbase)
    else()
        target_link_libraries(test_gamefolder PRIVATE ${wxWidgets_LIBRARIES})
    endif()
    add_test(NAME gamefolder COMMAND test_gamefolder)
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
add_executable(test_side_mirror test_side_mirror.c)
target_link_libraries(test_side_mirror PRIVATE sre_core)
add_test(NAME side_mirror COMMAND test_side_mirror)
# The enhanced view's bands against one thread, the band count set by each case.
add_executable(test_enh_bands test_enh_bands.c)
target_link_libraries(test_enh_bands PRIVATE sre_core)
foreach(t identity_counts full_list arena_full thread_fail cache_overflow shutdown_runs_pending restart)
    add_test(NAME enh_bands_${t} COMMAND test_enh_bands bands_${t})
    set_tests_properties(enh_bands_${t} PROPERTIES TIMEOUT 120)
endforeach()

# The shared host: Street Rod 1's audio pinned, the AdLib chip, voice volumes, the mouse space.
add_executable(test_host test_host.c)
target_link_libraries(test_host PRIVATE sre_host)
foreach(t sr1_audio_unchanged opl_renders opl_voice_gain opl_music_muted_by_synth opl_key_retrigger mouse_space mouse_space_set fps_parse fps_interval fps_next_present fps_pump_cap)
    add_test(NAME host_${t} COMMAND test_host ${t})
endforeach()

# Street Rod 2; needs its game files: -DSRE_TEST_GAME2_DIR=<folder with SR2's SR.EXE>.
if(SRE_TEST_GAME2_DIR)
    add_executable(test_sr2_music test_sr2_music.c)
    target_link_libraries(test_sr2_music PRIVATE sr2_core sre_host)
    add_test(NAME sr2_music COMMAND test_sr2_music ${SRE_TEST_GAME2_DIR})
    add_executable(test_sr2_band test_sr2_band.c)
    target_link_libraries(test_sr2_band PRIVATE sr2_core sre_host)
    add_test(NAME sr2_band COMMAND test_sr2_band ${SRE_TEST_GAME2_DIR})
    add_executable(test_sr2_enh test_sr2_enh.c)
    target_link_libraries(test_sr2_enh PRIVATE sr2_core sre_host)
    add_test(NAME sr2_enh COMMAND test_sr2_enh ${SRE_TEST_GAME2_DIR})
    add_executable(test_sr2_tags test_sr2_tags.c)
    target_link_libraries(test_sr2_tags PRIVATE sr2_core sre_host)
    add_test(NAME sr2_tags COMMAND test_sr2_tags ${SRE_TEST_GAME2_DIR})
    add_test(NAME sr2_enh_compare COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/test_sr2_enh_compare.sh
             $<TARGET_FILE:streetrod2-enhanced> ${SRE_TEST_GAME2_DIR} ${CMAKE_SOURCE_DIR})
    set_tests_properties(sr2_enh_compare PROPERTIES TIMEOUT 600 LABELS slow)
    add_test(NAME sr2_volumes COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/test_sr2_volumes.sh
             $<TARGET_FILE:streetrod2-enhanced> ${SRE_TEST_GAME2_DIR})
    set_tests_properties(sr2_volumes PROPERTIES TIMEOUT 240)
    # The remastered pictures: an identity pack (and a half one, a missing one) and a marker pack.
    foreach(t identity marker)
        add_test(NAME sr2_${t} COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/test_sr2_${t}.sh
                 $<TARGET_FILE:streetrod2-enhanced> ${SRE_TEST_GAME2_DIR} ${CMAKE_SOURCE_DIR})
        set_tests_properties(sr2_${t} PROPERTIES TIMEOUT 900 LABELS slow)
    endforeach()
    # The 15 smoke paths of upstream (title, demo tour, every race kind, jail, garage, save/load, endings).
    find_package(Python3 COMPONENTS Interpreter)
    if(Python3_FOUND)
        add_test(NAME sr2_smoke COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/sr2_smoke.py
                 --exe $<TARGET_FILE:streetrod2-enhanced> --game ${SRE_TEST_GAME2_DIR}
                 --work ${CMAKE_CURRENT_BINARY_DIR}/sr2_smoke --jobs 4)
        set_tests_properties(sr2_smoke PROPERTIES TIMEOUT 1800 LABELS slow)
    endif()
    if(SRE_TEST_GAME_DIR)
        add_test(NAME sr2_check COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/test_sr2_check.sh
                 $<TARGET_FILE:streetrod2-enhanced> ${SRE_TEST_GAME2_DIR} $<TARGET_FILE:streetrod-enhanced>
                 ${SRE_TEST_GAME_DIR})
        # The remaster's picture dump, and the pictures both games share against Street Rod 1's dump.
        add_test(NAME sr2_dump COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/test_sr2_dump.sh
                 $<TARGET_FILE:streetrod2-enhanced> ${SRE_TEST_GAME2_DIR} $<TARGET_FILE:streetrod-enhanced>
                 ${SRE_TEST_GAME_DIR})
    else()
        add_test(NAME sr2_check COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/test_sr2_check.sh
                 $<TARGET_FILE:streetrod2-enhanced> ${SRE_TEST_GAME2_DIR})
        add_test(NAME sr2_dump COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/test_sr2_dump.sh
                 $<TARGET_FILE:streetrod2-enhanced> ${SRE_TEST_GAME2_DIR})
    endif()
endif()

# The enhanced view's bands on worker threads against one thread, byte for byte (both games when given).
if(SRE_TEST_GAME_DIR OR SRE_TEST_GAME2_DIR)
    if(SRE_TEST_GAME_DIR)
        set(enh_threads_sr1 $<TARGET_FILE:streetrod-enhanced> ${SRE_TEST_GAME_DIR})
    else()
        set(enh_threads_sr1 - -)
    endif()
    if(SRE_TEST_GAME2_DIR)
        set(enh_threads_sr2 $<TARGET_FILE:streetrod2-enhanced> ${SRE_TEST_GAME2_DIR})
    else()
        set(enh_threads_sr2 - -)
    endif()
    add_test(NAME enh_threads COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/test_enh_threads.sh ${enh_threads_sr1}
             ${enh_threads_sr2} ${CMAKE_SOURCE_DIR})
    set_tests_properties(enh_threads PROPERTIES TIMEOUT 600 LABELS slow)
endif()

# --fps and --vsync (the frame pacing in common/host.c), both games.
add_test(NAME frame_options COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/test_frame_options.sh $<TARGET_FILE:streetrod-enhanced>)
add_test(NAME sr2_frame_options COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/test_frame_options.sh $<TARGET_FILE:streetrod2-enhanced>)

# The unit tests read the sample buffer right after a drawing call: drawn directly, on one thread (the bands
# against one thread: enh_threads).
foreach(t compose side_mirror tags sr2_enh sr2_tags)
    if(TEST ${t})
        set_tests_properties(${t} PROPERTIES ENVIRONMENT SR_ENH_THREADS=1)
    endif()
endforeach()
