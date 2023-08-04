#ifndef _STRING_H_
#define _STRING_H_

#include <stddef.h>

void* memchr(void *str, int c, size_t n);
int memcmp(void *str1, void *str2, size_t n);
void* memcpy(void *dest, void *src, size_t n);
void* memmove(void *dest, void *src, size_t n);
void* memset(void *str, int c, size_t n);
char* strcat(char *dest, char *src);
char* strncat(char *dest, char *src, size_t n);
char* strchr(char *str, int c);
int strcmp(char *str1, char *str2);
int strncmp(char *str1, char *str2, size_t n);
int strcoll(char *str1, char *str2);
char* strcpy(char *dest, char *src);
char* strncpy(char *dest, char *src, size_t n);
size_t strcspn(char *str1, char *str2);
char* strerror(int errnum);
size_t strlen(char *str);
char* strpbrk(char *str1, char *str2);
char* strrchr(char *str, int c);
size_t strspn(char *str1, char *str2);
char* strstr(char *haystack, char *needle);
char* strtok(char *str, char *delim);
size_t strxfrm(char *dest, char *src, size_t n);

int snprintf(char * _Buffer, size_t _BufferCount, char *const _Format, ...);
int sprintf(char *_Buffer, const char *_Format, ...);

char* _strdup(char* source);

#endif