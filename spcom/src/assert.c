#include "misc.h"
#include "assert.h"

void ___assert_uv_fail(const char *file,
                       unsigned int line,
                       const char *func,
                       const char *msg,
                       int uv_err)
{

    int exit_code = misc_uv_err_to_exit_code(uv_err);
    const char *uv_err_str = misc_uv_err_to_str(uv_err);

    spcom_exit(exit_code, file, line,
               "libuv, %s, error %d, '%s'",
               msg, uv_err, uv_err_str);
}

void ___assert_sp_fail(const char *file,
                       unsigned int line,
                       const char *func,
                       const char *msg,
                       int sp_err)
{
    int exit_code = misc_sp_err_to_exit_code(sp_err);
    const char *sp_err_str = misc_sp_err_to_str(sp_err);

    spcom_exit(exit_code, file, line,
               "libserialport, %s, error %d, '%s'",
                msg, sp_err, sp_err_str);
}

