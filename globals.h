#include "defs.h"
#ifdef init
	#undef init
#endif
#ifdef INIT_VARS
	#define init(val) = val;
#else
	#define init(val)
#endif

extern_main FILE* fptr;
extern_main int Line init(1);
extern_main int scope init(0);
extern_main int* stackIndex;

extern_main void FatalM(const char*, int);
