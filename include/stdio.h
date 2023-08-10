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

#endif	// _STDIO_H_