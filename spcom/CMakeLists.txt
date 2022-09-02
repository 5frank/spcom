cmake_minimum_required(VERSION 3.13)

project (spcom)

set(SOURCES
    src/assert.c
    src/main.c
    src/main_opts.c
    src/misc.c
    src/charmap.c
    src/cmd.c
    src/ctohex.c
    src/btree.c
    src/eol.c
    src/ipipe.c
    src/log.c
    src/opt.c
    src/opt_argviter.c
    src/opt_parse.c
    src/outfmt.c
    src/port.c
    src/port_info.c
    src/port_wait.c
    src/port_opts.c
    src/shell.c
    src/shell_rl.c
    #src/shell_mode_cmd.c
    src/shell_mode_cooked.c
    src/shell_mode_raw.c
    src/str.c
    src/strto_r.c
    src/str_split_quoted.c
    src/strbuf.c
    src/strerrorname_np.c
    src/keybind.c
    src/timeout.c
    src/termios_debug.c
    src/opq.c
)
# TODO use gcc `-I` to include pre_defs.h before any other headers'
#add_compile_options ( -Wno-reorder )
#add_definitions( "-Iinclude/pre_defs.h"  )

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
#TODO configurable target
add_definitions("-DDEBUG=1")

add_compile_definitions(CONFIG_TERMIOS_DEBUG=1)

add_executable(spcom ${SOURCES})

target_include_directories(spcom
    PRIVATE 
        ${PROJECT_SOURCE_DIR}/include
)


target_link_libraries(spcom readline)
target_link_libraries(spcom serialport)
target_link_libraries(spcom uv)

