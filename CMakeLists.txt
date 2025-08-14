cmake_minimum_required(VERSION 3.30)

project(HaloKeyboard C CXX)
set(CMAKE_CXX_STANDARD 23)

if ("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
    #add_compile_options(-fsanitize=thread -fsanitize=undefined -g3 -O0)
    #add_link_options(-lubsan -ltsan)
    add_compile_definitions(DEBUG=1)
else ()
    add_compile_definitions(DEBUG=0)
    set(OPTIMIZERS
            -O3
            -march=native
            -mtune=native
            -flto -fomit-frame-pointer
            -ffast-math -fstrict-aliasing
            -fdata-sections -ffunction-sections
            -D_FORTIFY_SOURCE=2 -fno-stack-protector
            -Wl,-z,relro -Wl,-z,now -s
            -fno-rtti
    )
    add_compile_options(${OPTIMIZERS})
    add_link_options(${OPTIMIZERS})
endif ()

execute_process(
        COMMAND bash -c "echo $(dd if=/dev/random bs=32 count=1 2>/dev/null | xxd | xz | sha512sum | head -c 8)"
        OUTPUT_VARIABLE BUILD_ID_PREFIX
)

if ("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
    set(BUILD_ID_PREFIX "__${BUILD_ID_PREFIX}DEBUG")
endif ()

execute_process(
        COMMAND bash -c "echo $(date '+%s'):$(date '+%Y%m%d%H%M%S')"
        OUTPUT_VARIABLE BUILD_ID_TIMESTAMP
)

execute_process(
        COMMAND bash -c "echo $(git log | head -n 1 | awk '{print $2}' | head -c 12)"
        OUTPUT_VARIABLE BUILD_ID_GIT_HEAD
)

execute_process(
        COMMAND bash -c "echo $(date '+%Y-%m-%d %H:%M:%S')"
        OUTPUT_VARIABLE BUILD_TIME
)

execute_process(
        COMMAND bash -c "echo -n $XDG_SESSION_DESKTOP"
        OUTPUT_VARIABLE BUILD_DESKTOP
)

if ("X${BUILD_DESKTOP}" STREQUAL "X")
    set(BUILD_DESKTOP "INVALID_DESKTOP")
endif ()

message(STATUS "Build on ${BUILD_TIME}, of ID ${BUILD_ID_PREFIX}:${BUILD_ID_TIMESTAMP}:${BUILD_ID_GIT_HEAD}, active: __${BUILD_DESKTOP}__")
add_compile_definitions(
        BUILD_ID="${BUILD_ID_PREFIX}:${BUILD_ID_TIMESTAMP}:${BUILD_ID_GIT_HEAD}"
        BUILD_TIME="${BUILD_TIME}"
        __${BUILD_DESKTOP}__="${BUILD_DESKTOP}"
        VERSION="0.1.0-Release"
)

include_directories(include)
add_executable(halo_kbd
        emit_keys.cpp include/emit_keys.h
        map_reader.cpp include/map_reader.h
        entry.cpp include/key_id.h
        include/log.hpp log.cpp
        execute_command.cpp include/execute_command.h
        libmod.cpp include/libmod.h
)
target_link_libraries(halo_kbd PRIVATE input udev)
add_library(fn_keymods SHARED fn_keymods.c include/ckeyid.h)
