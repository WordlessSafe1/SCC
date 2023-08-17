#ifndef _STDIO_H_
# define _STDIO_H_

#include <stddef.h>

#define EOF (-1)
#define SEEK_CUR 1

// This FILE definition will do for now
typedef char * FILE;
typedef long long fpos_t;

FILE *fopen(char *pathname, char *mode);
size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream);
size_t fwrite(void *ptr, size_t size, size_t nmemb, FILE *stream);
int fgetc(FILE *_Stream);
int fseek(FILE *_Stream, long _Offset, int _Origin);
int fgetpos(FILE* _Stream, fpos_t* _Position);
int fsetpos(FILE* _Stream, fpos_t* _Position);
int fclose(FILE *stream);
int printf(char *format, ...);
int fprintf(FILE *stream, char *format, ...);
void puts(char* str);
int putc(int _Character, FILE *_Stream);
void putchar(char c);

int snprintf(char * _Buffer, size_t _BufferCount, char *const _Format, ...);
int sprintf(char *_Buffer, const char *_Format, ...);

#include <stdarg.h>
int vsnprintf(char* const _Buffer, const size_t _BufferCount, const char* const _Format, va_list _ArgList);

#endif	// _STDIO_H_