#ifndef _STDARG_H_
	#define _STDARG_H_

	// struct _va_list {
	// 	unsigned int gp_offset;
	// 	unsigned int fp_offset;
	// 	void* overflow_arg_area;
	// 	void* reg_save_area;
	// };
	// typedef struct _va_list* va_list;
	typedef void* va_list;

	#define va_start(valist, param) __SCC_BUILTIN__va_start(valist, param)
	#define va_end(valist) valist = NULL
	#define va_arg(valist, type) (*(type*)((void**)valist)++)
#endif