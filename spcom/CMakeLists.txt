cmake_minimum_required(VERSION 3.13)

project (spcom)

set(SOURCES
    src/main.c
    src/charmap.c
    src/cmd.c
    src/eol.c
    src/log.c
    src/opts.c
    src/outfmt.c
    src/port.c
    src/portinfo.c
    src/port_wait.c
    src/shell.c
    src/str.c
    src/str_split_quoted.c
    src/opq.c
    src/version.c
)


# set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c99 -Wall")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -ggdb -g3 -Os -std=gnu99 -Wall")


# kudos: Renato U. https://stackoverflow.com/a/40947954/1565079
# Remove absolute path fron `__FILE`. Usage in code:
#   `#define __FILENAME__ (__FILE__ + SOURCE_PATH_SIZE)`
#
# The additional / is important to remove the last character from the path.
# Note that it does not matter if the OS uses / or \, because we are only
# saving the path size.
string(LENGTH "${CMAKE_SOURCE_DIR}/" SOURCE_PATH_SIZE)
add_definitions("-DSOURCE_PATH_SIZE=${SOURCE_PATH_SIZE}")


add_executable(spcom ${SOURCES})

target_include_directories(spcom
    PRIVATE 
        ${PROJECT_SOURCE_DIR}/include
)


target_link_libraries(spcom readline)
target_link_libraries(spcom serialport)
target_link_libraries(spcom uv)

