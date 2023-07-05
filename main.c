#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <math.h>

#include "defs.h"
#include "types.h"
#include "lex.h"
#include "symTable.h"
#include "parse.h"
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
char* DumpASTTree(ASTNode* tree, int depth);
char* charStr(char c, int count);
bool noWarn = false;

int main(int argc, char** argv){
	InitVarTable();
	if(argc < 2)	FatalM("No input files specified!", NOLINE);
	const char* outputTarget = NULL; 
	const char* inputTarget = NULL; 
	bool dump		= false;
	bool print		= false;
	bool supIntl	= false;
	for(int i = 1; i < argc; i++){
		if(streq(argv[i], "-o")){
			if(i+1 >= argc)	FatalM("Trailing argument '-o'!", NOLINE);
			else			outputTarget = argv[++i];
		}
		else if(streq(argv[i], "-t"))	dump		= true;
		else if(streq(argv[i], "-p"))	print		= true;
		else if(streq(argv[i], "-sI"))	supIntl		= true;
		else if(streq(argv[i], "-q"))	noWarn		= true;
		else							inputTarget = argv[i];
	}
	if (inputTarget == NULL)	FatalM("No input file specified!", NOLINE);
	fptr = fopen(inputTarget, "r");
	ASTNodeList* ast = MakeASTNodeList();
		while(PeekToken() != NULL)
			AddNodeToASTList(ast, ParseNode());
	if(GetToken() != NULL)	FatalM("Expected EOF!", Line);
	fclose(fptr);
	Line = NOLINE;
	if(dump){
		if(outputTarget == NULL || print){
			if(supIntl)
				printf("%s", "#ifdef __INTELLISENSE__\n" "	#pragma diag_suppress 29\n" "	#pragma diag_suppress 169\n" "	#pragma diag_suppress 130\n" "#endif");
			for(int i = 0; i < ast->count; i++){
				char* tree = DumpASTTree(ast->nodes[i], 0);
				printf("%s", tree);
				free(tree);
			}
			putchar('\n');
			return 0;
		}
		fptr = fopen(outputTarget, "w");
		if(supIntl)
			fprintf(fptr, "%s", "#ifdef __INTELLISENSE__\n" "	#pragma diag_suppress 29\n" "	#pragma diag_suppress 169\n" "	#pragma diag_suppress 130\n" "#endif");
		for(int i = 0; i < ast->count; i++){
			char* tree = DumpASTTree(ast->nodes[i], 0);
			fprintf(fptr, "%s", tree);
			free(tree);
		}
		putc('\n', fptr);
		fclose(fptr);
		return 0;
	}
	ResetVarTable(0);
	char* Asm = GenerateAsm(ast);
	if(print){
		printf("%s", Asm);
		return 0;
	}
	if(outputTarget == NULL)
		outputTarget = "a.s";
	fptr = fopen(outputTarget, "w");
	fprintf(fptr, "%s", Asm);
	fclose(fptr);
	free(Asm);
	return 0;
}

void FatalM(const char* msg, int line){
	if(line == NOLINE)	printf("Fatal error encountered:\n\t%s\n", msg);
	else				printf("Fatal error encountered on ln %d:\n\t%s\n", line, msg);
	exit(-1);
}

void WarnM(const char* msg, int line){
	if(noWarn)	return;
	if(line == NOLINE)	printf("Warning:\n\t%s\n", msg);
	else				printf("Warning - on ln %d:\n\t%s\n", line, msg);
}

char* DumpASTTree(ASTNode* tree, int depth){
	char* lhs = tree->lhs == NULL ? calloc(1, sizeof(char)) : DumpASTTree(tree->lhs, depth + 1);
	char* mid = tree->mid == NULL ? calloc(1, sizeof(char)) : DumpASTTree(tree->mid, depth + 1);
	char* rhs = tree->rhs == NULL ? calloc(1, sizeof(char)) : DumpASTTree(tree->rhs, depth + 1);
	char* list = calloc(1, sizeof(char));
	if(tree->op == A_FunctionCall)
		tree->list = tree->secondaryValue.ptrVal;
	if (tree->list != NULL) {
		list = calloc(3, sizeof(char));
		list[0] = ' ';
		list[1] = '{';
		for(int i = 0; i < tree->list->count; i++){
			char* buffer = DumpASTTree(tree->list->nodes[i], depth + 1);
			list = realloc(list, strlen(list) + strlen(buffer) + 1);
			strncat(list, buffer, strlen(list) + strlen(buffer) + 1);
			free(buffer);
		}
		list = realloc(list, strlen(list) + (depth * 2) + 3);
		char* tabs = charStr(' ', depth * 2);
		strcat(list, "\n");
		strcat(list, tabs);
		free(tabs);
		strcat(list, "}");
	}
	const char* name = calloc(1, sizeof(char));
	const char* val = calloc(1, sizeof(char));
	switch(tree->op){
		case A_Function:			val = tree->value.strVal;	break;
		case A_Declare:				val = tree->value.strVal;	break;
		case A_VarRef:				val = tree->value.strVal;	break;
		case A_Assign:				val = tree->value.strVal == NULL ? "expr" : tree->value.strVal;	break;
		case A_FunctionCall:		val = tree->value.strVal;	break;
		case A_LitInt:{
			const int charCount = intlen(tree->value.intVal) + (tree->value.intVal < 0) + 1;
			char* buffer = malloc(charCount * sizeof(char));
			snprintf(buffer, charCount, "%d", tree->value.intVal);
			val = buffer;
			break;
		}
		default:					break;
	}
	switch(tree->op){
		case A_Undefined:			name = "Undefined";			break;
		case A_Glue:				name = "Glue";				break;
		case A_Function:			name = "Function";			break;
		case A_Return:				name = "Return";			break;
		case A_LitInt:				name = "LitInt";			break;
		case A_Negate:				name = "Negate";			break;
		case A_LogicalNot:			name = "LogicalNot";		break;
		case A_BitwiseComplement:	name = "BitwiseComplement";	break;
		case A_Multiply:			name = "Multiply";			break;
		case A_Divide:				name = "Divide";			break;
		case A_Modulo:				name = "Modulo";			break;
		case A_Add:					name = "Add";				break;
		case A_Subtract:			name = "Subtract";			break;
		case A_LeftShift:			name = "LeftShift";			break;
		case A_RightShift:			name = "RightShift";		break;
		case A_LessThan:			name = "LessThan";			break;
		case A_GreaterThan:			name = "GreaterThan";		break;
		case A_LessOrEqual:			name = "LessOrEqual";		break;
		case A_GreaterOrEqual:		name = "GreaterOrEqual";	break;
		case A_EqualTo:				name = "EqualTo";			break;
		case A_NotEqualTo:			name = "NotEqualTo";		break;
		case A_BitwiseAnd:			name = "BitwiseAnd";		break;
		case A_BitwiseXor:			name = "BitwiseXor";		break;
		case A_BitwiseOr:			name = "BitwiseOr";			break;
		case A_LogicalAnd:			name = "LogicalAnd";		break;
		case A_LogicalOr:			name = "LogicalOr";			break;
		case A_Declare:				name = "Declare";			break;
		case A_VarRef:				name = "VarRef";			break;
		case A_Assign:				name = "Assign";			break;
		case A_Block:				name = "Block";				break;
		case A_Ternary:				name = "Ternary";			break;
		case A_If:					name = "If";				break;
		case A_While:				name = "While";				break;
		case A_Do:					name = "Do";				break;
		case A_For:					name = "For";				break;
		case A_Continue:			name = "Continue";			break;
		case A_Break:				name = "Break";				break;
		case A_Increment:			name = "Increment";			break;
		case A_Decrement:			name = "Decrement";			break;
		case A_AssignSum:			name = "AssignSum";			break;
		case A_AssignDifference:	name = "AssignDifference";	break;
		case A_AssignProduct:		name = "AssignProduct";		break;
		case A_AssignQuotient:		name = "AssignQuotient";	break;
		case A_AssignModulus:		name = "AssignModulus";		break;
		case A_AssignLeftShift:		name = "AssignLeftShift";	break;
		case A_AssignRightShift:	name = "AssignRightShift";	break;
		case A_AssignBitwiseAnd:	name = "AssignBitwiseAnd";	break;
		case A_AssignBitwiseXor:	name = "AssignBitwiseXor";	break;
		case A_AssignBitwiseOr:		name = "AssignBitwiseOr";	break;
		case A_FunctionCall:		name = "FunctionCall";		break;
		case A_AddressOf:			name = "AddressOf";			break;
		case A_Dereference:			name = "Dereference";		break;
	}
	const char* type = calloc(1, sizeof(char));
	switch(tree->type & 0xF0){
		case P_Undefined:	break;
		case P_Void:		type = "void";	break;
		case P_Char:		type = "char";	break;
		case P_Int:			type = "int";	break;
		case P_Long:		type = "long";	break;
	}
	if(tree->type & 0x0F){
		int deref = tree->type & 0x0F;
		int tlen = strlen(type);
		char* buffer = calloc(tlen + deref + 1, sizeof(char));
		strncpy(buffer, type, tlen);
		buffer[tlen + deref + 1] = '\0';
		for(int i = deref + tlen - 1; i >= tlen; i--)
			buffer[i] = '*';
		type = buffer;
	}
	const char* format =
		"\n%s%s(%s)[%s]" // Tabs, Name, val, type
		"%s" // lhs
		"%s" // mid
		"%s" // rhs
		"%s" // list
	;
	char* tabs = charStr(' ', depth * 2);
	const int charCount = strlen(name) + strlen(lhs) + strlen(mid) + strlen(rhs) + strlen(val) + strlen(type) + strlen(format) + strlen(list) + (2 * strlen(tabs)) + 1;
	char* buffer = malloc(charCount * sizeof(char));
	snprintf(buffer, charCount, format, tabs, name, val, type, lhs, mid, rhs, list, tabs);
	free(lhs);
	free(mid);
	free(rhs);
	free(list);
	free(tabs);
	return buffer;
}

char* charStr(char c, int count){
	char* buffer = calloc(count+1, sizeof(char));
	for(int i = 0; i < count; i++)
		buffer[i] = c;
	return buffer;
}