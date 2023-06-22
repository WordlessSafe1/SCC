#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <math.h>

#include "defs.h"
#include "types.h"
#include "lex.h"
#include "parse.h"
#include "varTable.h"
#include "gen.h"

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
	InitVarTable();
	if(argc < 2)	FatalM("No input files specified!", NOLINE);
	const char* outputTarget = "a.s"; 
	const char* inputTarget = NULL; 
	for(int i = 1; i < argc; i++){
		if(streq(argv[i], "-o")){
			if(i+1 >= argc)	FatalM("Trailing argument '-o'!", NOLINE);
			else			outputTarget = argv[++i];
		}
		else
			inputTarget = argv[i];
	}
	if (inputTarget == NULL)	FatalM("No input file specified!", NOLINE);
	fptr = fopen(inputTarget, "r");
	ASTNodeList* ast = MakeASTNodeList();
		while(PeekToken() != NULL)
			AddNodeToASTList(ast, ParseFunction());
	// ASTNode* ast = ParseFunction();
	if(GetToken() != NULL)	FatalM("Expected EOF!", Line);
	fclose(fptr);
	Line = NOLINE;
	const char* Asm = GenerateAsm(ast);
	fptr = fopen(outputTarget, "w");
	fprintf(fptr, "%s", Asm);
	fclose(fptr);
	return 0;
}

void FatalM(const char* msg, int line){
	if(line == NOLINE)	printf("Fatal error encountered:\n\t%s\n", msg);
	else				printf("Fatal error encountered on ln %d:\n\t%s\n", line, msg);
	exit(-1);
}
