
#include <uv.h>
#include <libserialport.h>

#include <readline/readline.h>


int version_print(int verbose)
{
    //TODO if (verbose)
    printf("libuv: %s\n", uv_version_string());
    printf("libserialport: lib:%s, pkg:%s\n", sp_get_lib_version_string(), sp_get_package_version_string());

    //printf("readline: %d.%d\n", RL_VERSION_MAJOR, RL_VERSION_MINOR);
    printf("readline: %s\n", rl_library_version);

#ifdef __GNUC__ // || defined(__GNUG__)

#ifndef __GNUC_MAJOR__
#define __GNUC_MAJOR__  __GNUC__ 
#endif

    printf("gcc: %d.%d.%d\n", __GNUC_MAJOR__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__);
#endif

#ifdef __clang__
    printf("clang: %d.%d.%d\n", __clang_major__, __clang_minor__, __clang_patchlevel__);
#endif

#ifdef _MSC_VER
    printf("visual studio: %d\n",  _MSC_VER);
#endif
    return 0;
}
