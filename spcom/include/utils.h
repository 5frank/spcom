
#ifndef UTILS_INCLUDE_H__
#define UTILS_INCLUDE_H__

int version_print(int verbose);

/** UV_VERSION_HEX "broken" on version 1.24.1 and is 0x011801 i.e. 1.18.01 */
#define UV_VERSION_GT_OR_EQ(MAJOR, MINOR)                                      \
    ((UV_VERSION_MAJOR > MAJOR)                                                \
     || ((UV_VERSION_MAJOR == MAJOR) && (UV_VERSION_MINOR >= MINOR)))

/* This macro is commonly named `ARRAY_SIZE` - it have always anoyed me as the
 * word 'size' implies bytes (chars). compare with strlen etc... */
#define ARRAY_LEN(ARRAY) (sizeof(ARRAY) / sizeof((ARRAY)[0]))

#define STRINGIFY(X) ___STRINGIFY_VIA(X)
#define ___STRINGIFY_VIA(X) #X

#endif
