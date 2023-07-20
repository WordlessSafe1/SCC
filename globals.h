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
extern_main int lVar init(0);
extern_main int* stackIndex;
extern_main const char* curFile;
extern_main int switchDepth init(0);
extern_main int loopDepth init(0);
extern_main bool USE_SUB_SWITCH init(false);

extern_main void FatalM(const char*, int);
extern_main void WarnM(const char*, int);

extern_main char* strjoin(const char* lhs, const char* rhs);
// Append the rhs string to the string referenced by lhs
// - Frees the string referenced by lhs
extern_main char* strapp(char** lhs, const char* rhs);
extern_main char* sngenf(int bufferSize, const char* format, ...);
