#include <string.h>

#include "defs.h"
#include "types.h"
#include "symTable.h"

extern struct {
	int lbreak;
	int lcontinue;
} labels;

enum paramMode {
	P_MODE_DEFAULT	= 1,
	P_MODE_LOCAL	= 2,
	P_MODE_SHADOW	= 3,
};

extern int switchCount;

char* GenerateAsm(ASTNodeList* node);
