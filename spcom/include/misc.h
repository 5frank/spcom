#ifndef MISC_INCLUDE_H_
#define MISC_INCLUDE_H_

int misc_print_version(int verbose);

/// libserialport error to exit code
int misc_sp_err_to_exit_code(int sp_err);

/** use with care! directly after a failing libserialport function call. static buffer might be used and
 * overwritten on successive call */
const char *misc_sp_err_to_str(int sp_err);

/// libuv error to exit code
int misc_uv_err_to_exit_code(int uv_err);

/** use with care! static buffer used and will be overwritten on successive call */
const char *misc_uv_err_to_str(int uv_err);

const char *misc_uv_handle_type_to_str(int n);

#endif

