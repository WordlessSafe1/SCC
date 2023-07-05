#ifndef DEFS_INCLUDED
#define DEFS_INCLUDED

#include <stdio.h>

#define extern_main extern

#define MAXFILES 100
#define NOLINE -1
#define true 0b1
#define false 0
#define bool unsigned

#define streq(lhs, rhs) !strcmp(lhs, rhs)
#define intlen(n) (n ? (int)floor(log10(abs(n)) + 1) : 1)

#endif