#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#include "defs.h"
#include "types.h"
#include "lex.h"
#include "parse.h"
#ifdef extern_main
	#undef extern_main
#endif
#define extern_main
#define INIT_VARS

#include "globals.h"

char* TokenTypeNames[] = {
	"Undefined",
	"Type",
	"Identifier",
	"OpenParen",
	"CloseParen",
	"OpenBrace",
	"CloseBrace",
	"Return",
	"Semicolon",
	"LitInt",
};

int main(int argc, char** argv){
	if(argc < 2)	FatalM("No input files specified!", NOLINE);
	fptr = fopen(argv[1], "r");
	ASTNode* a = ParseFunction();
	if(GetToken() != NULL)	FatalM("Expected EOF!", Line);
	puts("EOF");
	fclose(fptr);
}

void FatalM(const char* msg, int line){
	if(line == NOLINE)	printf("Fatal error encountered:\n\t%s", msg);
	else				printf("Fatal error encountered on ln %d:\n\t%s", line, msg);
	exit(-1);
}

