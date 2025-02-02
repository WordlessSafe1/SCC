#ifndef _STDLIB_H_
#define _STDLIB_H_

#include <stddef.h>

struct div_t {
  int quot;
  int rem;
};
typedef struct div_t div_t;

struct ldiv_t {
  long quot;
  long rem;
};
typedef struct ldiv_t ldiv_t;

int atoi(char *str);
long atol(char *str);
long atoll(char *str);
long long strtoll(char* _String, char** _EndPtr, int _Radix);
void *calloc(size_t nitems, size_t size);
void free(void *ptr);
void* malloc(size_t size);
void *realloc(void *ptr, size_t size);
void abort();
void exit(int status);
char *getenv(char *name);
int system(char *string);
int abs(int x);
div_t div(int numer, int denom);
long labs(long x);
long long llabs(long long x);
ldiv_t ldiv(long numer, long denom);
int rand();
void srand(int seed);
int mblen(char *str, size_t n);

#endif