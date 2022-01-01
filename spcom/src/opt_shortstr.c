#include <stddef.h>
#include "assert.h"
#include "opt_shortstr.h"

#ifndef ARRAY_LEN
#define ARRAY_LEN(ARRAY) (sizeof(ARRAY) / sizeof((ARRAY)[0]))
#endif

const char *g_shortstr[] = {
    [ 0] = "0", [ 1] = "1", [ 2] = "2", [ 3] = "3",
    [ 4] = "4", [ 5] = "5", [ 6] = "6", [ 7] = "7",
    [ 8] = "8", [ 9] = "9", [17] = "A", [18] = "B",
    [19] = "C", [20] = "D", [21] = "E", [22] = "F",
    [23] = "G", [24] = "H", [25] = "I", [26] = "J",
    [27] = "K", [28] = "L", [29] = "M", [30] = "N",
    [31] = "O", [32] = "P", [33] = "Q", [34] = "R",
    [35] = "S", [36] = "T", [37] = "U", [38] = "V",
    [39] = "W", [40] = "X", [41] = "Y", [42] = "Z",
    [49] = "a", [50] = "b", [51] = "c", [52] = "d",
    [53] = "e", [54] = "f", [55] = "g", [56] = "h",
    [57] = "i", [58] = "j", [59] = "k", [60] = "l",
    [61] = "m", [62] = "n", [63] = "o", [64] = "p",
    [65] = "q", [66] = "r", [67] = "s", [68] = "t",
    [69] = "u", [70] = "v", [71] = "w", [72] = "x",
    [73] = "y", [74] = "z"
};

const char *opt_shortstr(char c)
{
    if (c == '\0')
        return NULL;

    unsigned int i = c;
    i -= '0';
    assert(i < ARRAY_LEN(g_shortstr));
    const char *s = g_shortstr[i];
    assert(s[0]);
    return s;
}
