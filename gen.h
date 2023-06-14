#include <string.h>

#include "defs.h"
#include "types.h"

static const char* GenExpressionAsm(ASTNode* node){
	if(node == NULL)			FatalM("Expected an AST node, got NULL instead.", Line);
	if(node->op != A_LitInt)	FatalM("Expected Return Statement in function!", Line);
	const char* format = "	mov		$%d,	%%rax\n";
	int value = node->value.intVal;
	int charCount = strlen(format) + (int)(log10(value) + 1)/* <- # of digits in value */ + 1;
	char* str = malloc(charCount * sizeof(char));
	snprintf(str, charCount, format, value);
	return str;
}

static const char* GenStatementAsm(ASTNode* node){
	if(node == NULL)			FatalM("Expected an AST node, got NULL instead.", Line);
	if(node->op != A_Return)	FatalM("Expected Return Statement in function!", Line);
	if(node->lhs == NULL)
		return
			"	mov		$0,		%%eax\n"
			"	mov		%%rbp, %%rsp\n"
			"	pop		%%rbp\n"
			"	ret\n"
		;
	const char* innerAsm = GenExpressionAsm(node->lhs);
	const char* format =
		"%s"
		"	mov		%%rbp, %%rsp\n"
		"	pop		%%rbp\n"
		"	ret\n"
	;
	int charCount = strlen(innerAsm) + strlen(format) + 1;
	char* str = malloc(charCount * sizeof(char));
	snprintf(str, charCount, format, innerAsm);
	return str;
}

static const char* GenFunctionAsm(ASTNode* node){
	if(node == NULL)				FatalM("Expected an AST node, got NULL instead.", Line);
	if(node->op != A_Function)		FatalM("Expected function at top level statement!", Line);
	if(node->value.strVal == NULL)	FatalM("Expected a function identifier, got NULL instead.", Line);
	const char* format = 
		"	.globl %s\n"			// Identifier
		"%s:\n"						// Identifier
		"	push	%%rbp\n"
		"	mov		%%rsp, %%rbp\n"
		"%s"						// Statement ASM
	;
	const char* statementAsm = node->lhs ? GenStatementAsm(node->lhs) : "";
	int charCount =
		(2 * strlen(node->value.strVal))	// identifier x2
		+ strlen(statementAsm)				// Inner ASM
		+ strlen(format)					// format
		+ 1									// \0
	;
	char* str = malloc(charCount * sizeof(char));
	snprintf(str, charCount, format, node->value.strVal, node->value.strVal, statementAsm);
	return str;
}

const char* GenerateAsm(ASTNode* node){
	return GenFunctionAsm(node);
}