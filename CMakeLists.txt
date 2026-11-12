cmake_minimum_required(VERSION 3.24)
project(streetrod_enhanced VERSION 0.4.2 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Street Rod Enhanced: the faithful port of SR.EXE (VGA 16-colour mode 0Dh, PC speaker; ENGINE.md) with
# the enhancements in src/enhanced (ENHANCED.md).

find_package(SDL3 REQUIRED CONFIG)

file(GLOB_RECURSE SR_SOURCES CONFIGURE_DEPENDS src/*.c)
file(GLOB_RECURSE SR_ENHANCED_SOURCES CONFIGURE_DEPENDS src/enhanced/*.c)
# The enhanced view's drawing core (common/enhanced): built into each game's core, like the band.
file(GLOB SR_ENH_CORE_SOURCES CONFIGURE_DEPENDS common/enhanced/*.c)
list(APPEND SR_ENHANCED_SOURCES ${SR_ENH_CORE_SOURCES})

# Nuked-OPL3 (LGPL-2.1+, third_party/nuked-opl3/LICENSE): Street Rod 2's AdLib chip, in the shared host.
add_library(nuked_opl3 STATIC third_party/nuked-opl3/opl3.c)
target_include_directories(nuked_opl3 PUBLIC third_party/nuked-opl3)

# The host and what else both games share (common/README.md).
file(GLOB SR_HOST_SOURCES CONFIGURE_DEPENDS common/*.c)
add_library(sre_host STATIC ${SR_HOST_SOURCES})
target_include_directories(sre_host PUBLIC common)
target_link_libraries(sre_host PUBLIC SDL3::SDL3 nuked_opl3)

# The remastered music's band (common/band): built into each game's core, because its score reader
# (arrange.c) reads the game's memory through that game's mem.h.
file(GLOB SR_BAND_SOURCES CONFIGURE_DEPENDS common/band/*.c)
# The remastered pictures' core (common/remaster: tags, pack, colour): built into each game's core, because
# it reads the game's memory and VGA model through that game's mem.h and platform/ headers.
file(GLOB SR_RM_CORE_SOURCES CONFIGURE_DEPENDS common/remaster/*.c)

# Everything but main.c in an object library, so the unit tests can link the engine.
list(FILTER SR_SOURCES EXCLUDE REGEX "/src/main\\.c$")
add_library(sre_core OBJECT ${SR_SOURCES} ${SR_BAND_SOURCES} ${SR_RM_CORE_SOURCES} ${SR_ENH_CORE_SOURCES})
target_include_directories(sre_core PUBLIC src common/band common/enhanced)
target_link_libraries(sre_core PUBLIC sre_host SDL3::SDL3)
add_executable(streetrod-enhanced src/main.c)
target_link_libraries(streetrod-enhanced PRIVATE sre_core)
foreach(t sre_host sre_core streetrod-enhanced)
if(MSVC)
    target_compile_options(${t} PRIVATE /W3)
else()
    # The memory model reads multi-byte values through byte pointers (see src/mem.h).
    target_compile_options(${t} PRIVATE -Wall -Wextra -Wno-unused-parameter -fno-strict-aliasing)
    if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
        target_compile_options(${t} PRIVATE -Wno-format-truncation)
    endif()
    # The enhanced renderer does per-pixel work every frame; keep it optimised in Debug builds.
    if(SR_ENHANCED_SOURCES)
        set_source_files_properties(${SR_ENHANCED_SOURCES} PROPERTIES COMPILE_OPTIONS -O2)
    endif()
endif()
endforeach()
if(WIN32)
    add_custom_command(TARGET streetrod-enhanced POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_RUNTIME_DLLS:streetrod-enhanced>
                $<TARGET_FILE_DIR:streetrod-enhanced>
        COMMAND_EXPAND_LISTS)
    # And what those load in turn (MSYS2's SDL3.dll needs libiconv-2.dll), so the game starts outside
    # the MSYS2 shell, e.g. from the launcher.
    get_filename_component(SR_DLL_DIR "${CMAKE_C_COMPILER}" DIRECTORY)
    add_custom_command(TARGET streetrod-enhanced POST_BUILD
        COMMAND ${CMAKE_COMMAND} -DEXE=$<TARGET_FILE:streetrod-enhanced> "-DSEARCH=${SR_DLL_DIR}"
                -DCMAKE_OBJDUMP=${CMAKE_OBJDUMP} -P ${CMAKE_CURRENT_SOURCE_DIR}/launcher/copy_dlls.cmake)
endif()

# Street Rod 2: The Next Generation, its own engine (src2/, ENGINE2.md) on the shared host.
file(GLOB_RECURSE SR2_SOURCES CONFIGURE_DEPENDS src2/*.c)
list(FILTER SR2_SOURCES EXCLUDE REGEX "/src2/main\\.c$")
add_library(sr2_core OBJECT ${SR2_SOURCES} ${SR_BAND_SOURCES} ${SR_RM_CORE_SOURCES} ${SR_ENH_CORE_SOURCES})
target_include_directories(sr2_core PUBLIC src2 common/band common/enhanced)
target_link_libraries(sr2_core PUBLIC sre_host SDL3::SDL3)
add_executable(streetrod2-enhanced src2/main.c)
target_link_libraries(streetrod2-enhanced PRIVATE sr2_core sre_host)
foreach(t sr2_core streetrod2-enhanced)
if(MSVC)
    target_compile_options(${t} PRIVATE /W3)
else()
    target_compile_options(${t} PRIVATE -Wall -Wextra -Wno-unused-parameter -fno-strict-aliasing)
    if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
        target_compile_options(${t} PRIVATE -Wno-format-truncation)
    endif()
endif()
endforeach()
if(WIN32)
    add_custom_command(TARGET streetrod2-enhanced POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_RUNTIME_DLLS:streetrod2-enhanced>
                $<TARGET_FILE_DIR:streetrod2-enhanced>
        COMMAND_EXPAND_LISTS)
    add_custom_command(TARGET streetrod2-enhanced POST_BUILD
        COMMAND ${CMAKE_COMMAND} -DEXE=$<TARGET_FILE:streetrod2-enhanced> "-DSEARCH=${SR_DLL_DIR}"
                -DCMAKE_OBJDUMP=${CMAKE_OBJDUMP} -P ${CMAKE_CURRENT_SOURCE_DIR}/launcher/copy_dlls.cmake)
endif()

# The launcher ("Street Rod Enhanced"): the game folder, the original or the data disk's cars, the
# game's own switches and the display options; built beside streetrod-enhanced.exe. It needs
# wxWidgets 3.2 (MSYS2: mingw-w64-ucrt-x86_64-wxwidgets3.2-msw) and a C++17 compiler.
option(SRE_LAUNCHER "Also build the launcher, Street Rod Enhanced.exe (wxWidgets)" ON)
if(SRE_LAUNCHER)
    add_subdirectory(launcher)
endif()

option(SRE_TESTS "Also build the unit tests (ctest)" OFF)
if(SRE_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
