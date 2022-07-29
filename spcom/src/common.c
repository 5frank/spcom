#include <uv.h>
#include <libserialport.h>
#include <sysexits.h>

#include "common.h"

int sp_err_to_ex(int sp_err)
{
    switch (sp_err) {
        case SP_OK:
            return EX_OK;
        // Invalid arguments. This implies a bug in the caller
        case SP_ERR_ARG:
            return EX_SOFTWARE;
        // A system error occurred
        case SP_ERR_FAIL:
            return EX_OSERR;
        // A memory allocation failed
        case SP_ERR_MEM:
            return EX_OSERR;
        // operation not supported by OS, driver or device. 
        case SP_ERR_SUPP:
            return EX_UNAVAILABLE;
        default:
            return EX_SOFTWARE;
    }
}


int uv_err_to_ex(int uv_err)
{
    switch (uv_err) {
        case 0:
            return EX_OK;
            // TODO
        default:
            return EX_SOFTWARE;
    }
}
