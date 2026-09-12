cmake_minimum_required(VERSION 3.30)

project(HaloKeyboard C CXX)
set(CMAKE_CXX_STANDARD 20)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBINPUT  REQUIRED libinput)
pkg_check_modules(LIBUDEV   REQUIRED libudev)
pkg_check_modules(LIBCAP    REQUIRED libcap)

if ("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
    add_compile_options(
#            -fsanitize=thread
#            -fsanitize=undefined -fsanitize=address
            -g3 -O0
    )
    add_link_options(
#            -lubsan -lasan
#            -ltsan
    )
    add_compile_definitions(DEBUG=1)
else ()
    add_compile_definitions(DEBUG=0)
    set(OPTIMIZERS
            -O3
            -flto -fomit-frame-pointer
            -ffast-math -fstrict-aliasing
            -fdata-sections -ffunction-sections
            -D_FORTIFY_SOURCE=2 -fno-stack-protector
            -Wl,-z,relro -Wl,-z,now -s
            -fno-rtti -fno-exceptions -fwhole-program -fuse-linker-plugin -fdevirtualize-at-ltrans
    )
    add_compile_options(${OPTIMIZERS})
    add_link_options(${OPTIMIZERS})
endif ()

execute_process(
        COMMAND bash -c "echo -n $(git log | head -n 1 | awk '{print $2}' | head -c 12)"
        OUTPUT_VARIABLE BUILD_ID_GIT_HEAD
)

execute_process(
        COMMAND bash -c "echo -n $(date '+%Y-%m-%d %H:%M:%S')"
        OUTPUT_VARIABLE BUILD_TIME
)

add_compile_definitions(
        BUILD_ID="${BUILD_ID_GIT_HEAD}"
        BUILD_TIME="${BUILD_TIME}"
        VERSION="1.0.4-Release"
)

include_directories(include)
add_executable(halo_kbd
        src/map_reader.cpp      include/map_reader.h
        src/entry.cpp
                                include/key_id.h
        src/EmitKeys.cpp        include/EmitKeys.h
        src/HaloKeyboard.cpp    include/HaloKeyboard.h
        src/ExecuteCommands.cpp include/ExecuteCommands.h
)
target_link_libraries(halo_kbd PRIVATE input udev)
target_compile_options(halo_kbd PRIVATE -std=gnu++20)
target_include_directories(halo_kbd PRIVATE ${LIBINPUT_INCLUDE_DIRS} ${LIBUDEV_INCLUDE_DIRS} ${LIBCAP_INCLUDE_DIRS})
target_link_directories(halo_kbd PRIVATE ${LIBINPUT_LIBRARY_DIRS} ${LIBUDEV_LIBRARY_DIRS} ${LIBCAP_LIBRARY_DIRS})
target_link_libraries(halo_kbd PRIVATE ${LIBINPUT_LIBRARIES} ${LIBUDEV_LIBRARIES} ${LIBCAP_LIBRARIES})

find_program(PATCHELF_EXEC NAMES patchelf REQUIRED)
add_custom_target(AppImage_Stage1 ALL
        COMMAND ${PATCHELF_EXEC} --set-rpath $ORIGIN halo_kbd
        COMMAND ${PATCHELF_EXEC} --add-rpath $ORIGIN/libs halo_kbd
        COMMAND ${PATCHELF_EXEC} --add-rpath /usr/lib halo_kbd
        COMMAND ${PATCHELF_EXEC} --add-rpath /lib halo_kbd
        COMMAND /bin/sh -c "file halo_kbd | sed -E 's/.*interpreter[[:space:]]([^,]+),.*/\\1/g' > interpreter.txt"
        COMMAND /bin/sh -c "cp $(cat interpreter.txt) ${CMAKE_BINARY_DIR}/"
        COMMAND /bin/sh -c "strip -g -S -d --strip-debug --strip-unneeded ${CMAKE_BINARY_DIR}/$(basename $(cat interpreter.txt))"
        COMMAND /bin/sh -c "ldd halo_kbd | grep -v 'linux-vdso.so' | grep '=>' | sed -E \"$(cat ${CMAKE_SOURCE_DIR}/rule.txt)\" > libs.txt"
        COMMAND /bin/sh -c "rm -rf libs && mkdir -p libs && for file in $(cat libs.txt); do cp $file libs/; done"
        DEPENDS halo_kbd
        BYPRODUCTS interpreter.txt libs.txt
        VERBATIM
)

set(APPIMAGE_WORK_DIR "${CMAKE_BINARY_DIR}/halo_kbd.AppDir/")

add_custom_target(AppImage_Stage2 ALL
        COMMAND rm -rf ${APPIMAGE_WORK_DIR}
        COMMAND /bin/sh -c "if ! [ -e appimagetool.finish ]; then curl -L https://github.com/AppImage/appimagetool/releases/download/continuous/appimagetool-x86_64.AppImage -o appimagetool && chmod +x appimagetool && touch appimagetool.finish; fi"
        COMMAND mkdir -p ${APPIMAGE_WORK_DIR}
        COMMAND cp ${CMAKE_SOURCE_DIR}/resources/halo_kbd.desktop ${APPIMAGE_WORK_DIR}
        COMMAND cp ${CMAKE_SOURCE_DIR}/resources/halo_kbd.svg     ${APPIMAGE_WORK_DIR}
        COMMAND mv libs ${APPIMAGE_WORK_DIR}
        COMMAND cp halo_kbd ${APPIMAGE_WORK_DIR}
        COMMAND /bin/sh -c "cp $(basename $(cat interpreter.txt)) ${APPIMAGE_WORK_DIR}"
        COMMAND /bin/sh -c "cat ${CMAKE_SOURCE_DIR}/resources/AppRun | sed s/INTERPRETER/$(basename $(cat interpreter.txt))/g > ${APPIMAGE_WORK_DIR}/AppRun"
        COMMAND chmod +x ${APPIMAGE_WORK_DIR}/AppRun
        COMMAND ./appimagetool ${APPIMAGE_WORK_DIR}
        DEPENDS AppImage_Stage1
        VERBATIM
)