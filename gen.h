#include <string.h>

#include "defs.h"
#include "types.h"

static const char* GenLitInt(ASTNode* node){
	if(node == NULL)			FatalM("Expected an AST node, got NULL instead.", Line);
	if(node->op != A_LitInt)	FatalM("Expected literal int in expression!", Line);
	const char* format = "	mov		$%d,	%%rax\n";
	int value = node->value.intVal;
	int charCount = strlen(format) + (value ? (int)(log10(value) + 1) : 1 )/* <- # of digits in value */ + 1;
	char* str = malloc(charCount * sizeof(char));
	snprintf(str, charCount, format, value);
	return str;
}

static const char* GenExpressionAsm(ASTNode* node){
	if(node == NULL)			FatalM("Expected an AST node, got NULL instead.", Line);
	if(node->op == A_LitInt)	return GenLitInt(node);
	if(node->op == A_Negate){
		if(node->lhs == NULL)	FatalM("Expected expression after unary negation operator!", Line);
		const char* format = "%s	neg		%rax\n";
		const char* innerAsm = GenExpressionAsm(node->lhs);
		int charCount = strlen(format) + strlen(innerAsm) + 1;
		char* str = malloc(charCount * sizeof(char));
		snprintf(str, charCount, format, innerAsm);
		return str;
	}
	if(node->op == A_BitwiseComplement){
		if(node->lhs == NULL)	FatalM("Expected expression after unary negation operator!", Line);
		const char* format = "%s	not		%rax\n";
		const char* innerAsm = GenExpressionAsm(node->lhs);
		int charCount = strlen(format) + strlen(innerAsm) + 1;
		char* str = malloc(charCount * sizeof(char));
		snprintf(str, charCount, format, innerAsm);
		return str;
	}
	if(node->op == A_LogicalNot){
		if(node->lhs == NULL)	FatalM("Expected expression after unary negation operator!", Line);
		const char* instr =
			"	cmp		$0,		%%rax\n"
			"	mov		$0,		%%rax\n"
			"	sete	%%al\n"
		;
		const char* innerAsm = GenExpressionAsm(node->lhs);
		int charCount = strlen(instr) + strlen(innerAsm) + 1;
		char* str = malloc(charCount * sizeof(char));
		snprintf(str, charCount, "%s%s", innerAsm, instr);
		return str;
	}
	FatalM("Invalid expression!", Line);
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