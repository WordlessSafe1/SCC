#ifndef DEFS_INCLUDED
#define DEFS_INCLUDED

#include <stdio.h>

#define extern_main extern

#define MAXFILES 100
#define NOLINE -1
#define true 0b1
#define false 0
#define bool char

#define streq(lhs, rhs) !strcmp(lhs, rhs)
/**
 * String begins with
 * @param needle The substring to search for. Should be pre-evaluated.
 * @param haystack The string to search in
*/
#define strbeg(haystack, needle) (strncmp(haystack, needle, strlen(needle)) == 0)
#define align(value, alignment) ((value + alignment - 1) & (-alignment))

#endif