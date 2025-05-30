cmake_minimum_required(VERSION 3.30)

project(HaloKeyboard CXX)
set(CMAKE_CXX_STANDARD 23)

if (${CMAKE_BUILD_TYPE} STREQUAL "Debug")
#    add_compile_options(-fsanitize=thread -fsanitize=undefined -g3 -O0)
#    add_link_options(-lubsan -ltsan)
    add_compile_definitions(DEBUG=1)
else ()
    add_compile_definitions(DEBUG=0)
    set(OPTIMIZERS
            -O3
            -flto -fomit-frame-pointer
            -ffast-math -fstrict-aliasing
            -fdata-sections -ffunction-sections
            -D_FORTIFY_SOURCE=2 -fstack-protector-strong
            -Wl,-z,relro -Wl,-z,now -s
    )
    add_compile_options(${OPTIMIZERS})
    add_link_options(${OPTIMIZERS})
endif ()

include_directories(include)
add_executable(halo_kbd
        emit_keys.cpp map_reader.cpp
        entry.cpp
        include/log.hpp log.cpp
        include/map_reader.h
        include/emit_keys.h)
target_link_libraries(halo_kbd PRIVATE input udev)
