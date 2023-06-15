#include <string.h>

#include "defs.h"
#include "types.h"

static int labelPref = 9;

static const char* GenExpressionAsm(ASTNode* node);

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

static const char* GenNegate(ASTNode* node){
	if(node->lhs == NULL)	FatalM("Expected expression after unary negation operator!", Line);
	const char* format = "%s	neg		%rax\n";
	const char* innerAsm = GenExpressionAsm(node->lhs);
	int charCount = strlen(format) + strlen(innerAsm) + 1;
	char* str = malloc(charCount * sizeof(char));
	snprintf(str, charCount, format, innerAsm);
	return str;
}

static const char* GenBitwiseComplement(ASTNode* node){
	if(node->lhs == NULL)	FatalM("Expected expression after unary negation operator!", Line);
	const char* format = "%s	not		%rax\n";
	const char* innerAsm = GenExpressionAsm(node->lhs);
	int charCount = strlen(format) + strlen(innerAsm) + 1;
	char* str = malloc(charCount * sizeof(char));
	snprintf(str, charCount, format, innerAsm);
	return str;
}

static const char* GenLogicalNot(ASTNode* node){
	if(node->lhs == NULL)	FatalM("Expected expression after unary negation operator!", Line);
	const char* instr =
		"	cmp		$0,		%rax\n"
		"	mov		$0,		%rax\n"
		"	sete	%al\n"
	;
	const char* innerAsm = GenExpressionAsm(node->lhs);
	int charCount = strlen(instr) + strlen(innerAsm) + 1;
	char* str = malloc(charCount * sizeof(char));
	snprintf(str, charCount, "%s%s", innerAsm, instr);
	return str;
}

static const char* GenMultiply(ASTNode* node){
	if(node->lhs == NULL)	FatalM("Expected factor before binary multiplication operator!", Line);
	if(node->rhs == NULL)	FatalM("Expected factor after binary multiplication operator!", Line);
	const char* pushInstr = "	push	%rax\n";
	const char* instr =
		"	pop		%rcx\n"
		"	imul	%rcx,	%rax\n"
	;
	const char* lhs = GenExpressionAsm(node->lhs);
	const char* rhs = GenExpressionAsm(node->rhs);
	int charCount = strlen(instr) + strlen(pushInstr) + strlen(lhs) + strlen(rhs) + 1;
	char* str = malloc(charCount * sizeof(char));
	snprintf(str, charCount, "%s%s%s%s", lhs, pushInstr, rhs, instr);
	return str;
}

static const char* GenDivide(ASTNode* node){
	if(node->lhs == NULL)	FatalM("Expected factor before binary division operator!", Line);
	if(node->rhs == NULL)	FatalM("Expected factor after binary division operator!", Line);
	const char* pushInstr = "	push	%rax\n";
	const char* instr =
		"	pop		%rcx\n"
		"	cdq\n"
		"	idiv	%rcx\n"
	;
	const char* lhs = GenExpressionAsm(node->lhs);
	const char* rhs = GenExpressionAsm(node->rhs);
	int charCount = strlen(instr) + strlen(pushInstr) + strlen(lhs) + strlen(rhs) + 1;
	char* str = malloc(charCount * sizeof(char));
	snprintf(str, charCount, "%s%s%s%s", rhs, pushInstr, lhs, instr);
	return str;
}

static const char* GenModulo(ASTNode* node){
	if(node->lhs == NULL)	FatalM("Expected factor before binary division operator!", Line);
	if(node->rhs == NULL)	FatalM("Expected factor after binary division operator!", Line);
	const char* pushInstr = "	push	%rax\n";
	const char* instr =
		"	pop		%rcx\n"
		"	cdq\n"
		"	idiv	%rcx\n"
		"	mov		%rdx,	%rax\n"
	;
	const char* lhs = GenExpressionAsm(node->lhs);
	const char* rhs = GenExpressionAsm(node->rhs);
	int charCount = strlen(instr) + strlen(pushInstr) + strlen(lhs) + strlen(rhs) + 1;
	char* str = malloc(charCount * sizeof(char));
	snprintf(str, charCount, "%s%s%s%s", rhs, pushInstr, lhs, instr);
	return str;
}

static const char* GenSubtract(ASTNode* node){
	if(node->lhs == NULL)	FatalM("Expected factor before binary subtraction operator!", Line);
	if(node->rhs == NULL)	FatalM("Expected factor after binary subtraction operator!", Line);
	const char* pushInstr = "	push	%rax\n";
	const char* instr =
		"	pop		%rcx\n"
		"	subq	%rcx,	%rax\n"
	;
	const char* lhs = GenExpressionAsm(node->lhs);
	const char* rhs = GenExpressionAsm(node->rhs);
	int charCount = strlen(instr) + strlen(pushInstr) + strlen(lhs) + strlen(rhs) + 1;
	char* str = malloc(charCount * sizeof(char));
	snprintf(str, charCount, "%s%s%s%s", rhs, pushInstr, lhs, instr);
	return str;
}

static const char* GenAdd(ASTNode* node){
	if(node->lhs == NULL)	FatalM("Expected factor before binary addition operator!", Line);
	if(node->rhs == NULL)	FatalM("Expected factor after binary addition operator!", Line);
	const char* pushInstr = "	push	%rax\n";
	const char* instr =
		"	pop		%rcx\n"
		"	addq	%rcx,	%rax\n"
	;
	const char* lhs = GenExpressionAsm(node->lhs);
	const char* rhs = GenExpressionAsm(node->rhs);
	int charCount = strlen(instr) + strlen(pushInstr) + strlen(lhs) + strlen(rhs) + 1;
	char* str = malloc(charCount * sizeof(char));
	snprintf(str, charCount, "%s%s%s%s", lhs, pushInstr, rhs, instr);
	return str;
}

static const char* GenLShift(ASTNode* node){
	if(node->lhs == NULL)	FatalM("Expected factor before binary left shift operator!", Line);
	if(node->rhs == NULL)	FatalM("Expected factor after binary left shift operator!", Line);
	const char* pushInstr = "	push	%rax\n";
	const char* instr =
		"	pop		%rcx\n"
		"	shl		%rcx,	%rax\n"
	;
	const char* rhs = GenExpressionAsm(node->rhs);
	const char* lhs = GenExpressionAsm(node->lhs);
	int charCount = strlen(instr) + strlen(pushInstr) + strlen(lhs) + strlen(rhs) + 1;
	char* str = malloc(charCount * sizeof(char));
	snprintf(str, charCount, "%s%s%s%s", rhs, pushInstr, lhs, instr);
	return str;
}

static const char* GenRShift(ASTNode* node){
	if(node->lhs == NULL)	FatalM("Expected factor before binary right shift operator!", Line);
	if(node->rhs == NULL)	FatalM("Expected factor after binary right shift operator!", Line);
	const char* pushInstr = "	push	%rax\n";
	const char* instr =
		"	pop		%rcx\n"
		"	sar		%rcx,	%rax\n"
	;
	const char* rhs = GenExpressionAsm(node->rhs);
	const char* lhs = GenExpressionAsm(node->lhs);
	int charCount = strlen(instr) + strlen(pushInstr) + strlen(lhs) + strlen(rhs) + 1;
	char* str = malloc(charCount * sizeof(char));
	snprintf(str, charCount, "%s%s%s%s", rhs, pushInstr, lhs, instr);
	return str;
}

static const char* GenLessThan(ASTNode* node){
	if(node->lhs == NULL)	FatalM("Expected factor before binary less than operator!", Line);
	if(node->rhs == NULL)	FatalM("Expected factor bafter binary less than operator!", Line);
	const char* pushInstr = "	push	%rax\n";
	const char* instr =
		"	pop		%rcx\n"
		"	cmp		%rcx,	%rax\n"
		"	mov		$0,		%rax\n"
		"	setl	%al\n"
	;
	const char* rhs = GenExpressionAsm(node->rhs);
	const char* lhs = GenExpressionAsm(node->lhs);
	int charCount = strlen(instr) + strlen(pushInstr) + strlen(lhs) + strlen(rhs) + 1;
	char* str = malloc(charCount * sizeof(char));
	snprintf(str, charCount, "%s%s%s%s", rhs, pushInstr, lhs, instr);
	return str;
}

static const char* GenGreaterThan(ASTNode* node){
	if(node->lhs == NULL)	FatalM("Expected factor before binary greater than operator!", Line);
	if(node->rhs == NULL)	FatalM("Expected factor after binary greater than operator!", Line);
	const char* pushInstr = "	push	%rax\n";
	const char* instr =
		"	pop		%rcx\n"
		"	cmp		%rcx,	%rax\n"
		"	mov		$0,		%rax\n"
		"	setg	%al\n"
	;
	const char* rhs = GenExpressionAsm(node->rhs);
	const char* lhs = GenExpressionAsm(node->lhs);
	int charCount = strlen(instr) + strlen(pushInstr) + strlen(lhs) + strlen(rhs) + 1;
	char* str = malloc(charCount * sizeof(char));
	snprintf(str, charCount, "%s%s%s%s", rhs, pushInstr, lhs, instr);
	return str;
}

static const char* GenEqualTo(ASTNode* node){
	if(node->lhs == NULL)	FatalM("Expected factor before binary equality operator!", Line);
	if(node->rhs == NULL)	FatalM("Expected factor after binary equality operator!", Line);
	const char* pushInstr = "	push	%rax\n";
	const char* instr =
		"	pop		%rcx\n"
		"	cmp		%rcx,	%rax\n"
		"	mov		$0,		%rax\n"
		"	sete	%al\n"
	;
	const char* rhs = GenExpressionAsm(node->rhs);
	const char* lhs = GenExpressionAsm(node->lhs);
	int charCount = strlen(instr) + strlen(pushInstr) + strlen(lhs) + strlen(rhs) + 1;
	char* str = malloc(charCount * sizeof(char));
	snprintf(str, charCount, "%s%s%s%s", lhs, pushInstr, rhs, instr);
	return str;
}

static const char* GenNotEqualTo(ASTNode* node){
	if(node->lhs == NULL)	FatalM("Expected factor before binary inequality operator!", Line);
	if(node->rhs == NULL)	FatalM("Expected factor after binary inequality operator!", Line);
	const char* pushInstr = "	push	%rax\n";
	const char* instr =
		"	pop		%rcx\n"
		"	cmp		%rcx,	%rax\n"
		"	mov		$0,		%rax\n"
		"	setne	%al\n"
	;
	const char* rhs = GenExpressionAsm(node->rhs);
	const char* lhs = GenExpressionAsm(node->lhs);
	int charCount = strlen(instr) + strlen(pushInstr) + strlen(lhs) + strlen(rhs) + 1;
	char* str = malloc(charCount * sizeof(char));
	snprintf(str, charCount, "%s%s%s%s", lhs, pushInstr, rhs, instr);
	return str;
}

static const char* GenBitwiseAnd(ASTNode* node){
	if(node->lhs == NULL)	FatalM("Expected factor before binary bitwise and operator!", Line);
	if(node->rhs == NULL)	FatalM("Expected factor after binary bitwise and operator!", Line);
	const char* pushInstr = "	push	%rax\n";
	const char* instr =
		"	pop		%rcx\n"
		"	and		%rcx,	%rax\n"
	;
	const char* rhs = GenExpressionAsm(node->rhs);
	const char* lhs = GenExpressionAsm(node->lhs);
	int charCount = strlen(instr) + strlen(pushInstr) + strlen(lhs) + strlen(rhs) + 1;
	char* str = malloc(charCount * sizeof(char));
	snprintf(str, charCount, "%s%s%s%s", lhs, pushInstr, rhs, instr);
	return str;
}

static const char* GenBitwiseXor(ASTNode* node){
	if(node->lhs == NULL)	FatalM("Expected factor before binary bitwise xor operator!", Line);
	if(node->rhs == NULL)	FatalM("Expected factor after binary bitwise xor operator!", Line);
	const char* pushInstr = "	push	%rax\n";
	const char* instr =
		"	pop		%rcx\n"
		"	xor		%rcx,	%rax\n"
	;
	const char* rhs = GenExpressionAsm(node->rhs);
	const char* lhs = GenExpressionAsm(node->lhs);
	int charCount = strlen(instr) + strlen(pushInstr) + strlen(lhs) + strlen(rhs) + 1;
	char* str = malloc(charCount * sizeof(char));
	snprintf(str, charCount, "%s%s%s%s", lhs, pushInstr, rhs, instr);
	return str;
}

static const char* GenBitwiseOr(ASTNode* node){
	if(node->lhs == NULL)	FatalM("Expected factor before binary bitwise or operator!", Line);
	if(node->rhs == NULL)	FatalM("Expected factor after binary bitwise or operator!", Line);
	const char* pushInstr = "	push	%rax\n";
	const char* instr =
		"	pop		%rcx\n"
		"	or		%rcx,	%rax\n"
	;
	const char* rhs = GenExpressionAsm(node->rhs);
	const char* lhs = GenExpressionAsm(node->lhs);
	int charCount = strlen(instr) + strlen(pushInstr) + strlen(lhs) + strlen(rhs) + 1;
	char* str = malloc(charCount * sizeof(char));
	snprintf(str, charCount, "%s%s%s%s", lhs, pushInstr, rhs, instr);
	return str;
}

static const char* GenLogicalAnd(ASTNode* node){
	if(node->lhs == NULL)	FatalM("Expected factor before binary logical and operator!", Line);
	if(node->rhs == NULL)	FatalM("Expected factor after binary logical and operator!", Line);
	const char* format =
		"%s"
		"	cmp		$0,		%%rax\n"
		"	jne		%d1f\n"
		"	jmp		%d2f\n"
		"%d1:\n"
		"%s"
		"	cmp		$0,		%%rax\n"
		"	mov		$0,		%%rax\n"
		"	setne	%%al\n"
		"%d2:\n"
	;
	const char* rhs = GenExpressionAsm(node->rhs);
	const char* lhs = GenExpressionAsm(node->lhs);
	int charCount = strlen(format) + strlen(lhs) + strlen(rhs) + 1;
	char* str = malloc(charCount * sizeof(char));
	labelPref++;
	snprintf(str, charCount, format, lhs, labelPref, labelPref, labelPref, rhs, labelPref);
	return str;
}

static const char* GenLogicalOr(ASTNode* node){
	if(node->lhs == NULL)	FatalM("Expected factor before binary logical and operator!", Line);
	if(node->rhs == NULL)	FatalM("Expected factor after binary logical and operator!", Line);
	const char* format =
		"%s"
		"	cmp		$0,		%%rax\n"
		"	je		%d1f\n"
		"	mov		$1,		%%rax\n"
		"	jmp		%d2f\n"
		"%d1:\n"
		"%s"
		"	cmp		$0,		%%rax\n"
		"	mov		$0,		%%rax\n"
		"	setne	%%al\n"
		"%d2:\n"
	;
	const char* rhs = GenExpressionAsm(node->rhs);
	const char* lhs = GenExpressionAsm(node->lhs);
	int charCount = strlen(format) + strlen(lhs) + strlen(rhs) + 1;
	char* str = malloc(charCount * sizeof(char));
	labelPref++;
	snprintf(str, charCount, format, lhs, labelPref, labelPref, labelPref, rhs, labelPref);
	return str;
}

static const char* GenExpressionAsm(ASTNode* node){
	if(node == NULL)					FatalM("Expected an AST node, got NULL instead.", Line);
	switch(node->op){
		case A_LitInt:				return GenLitInt(node);
		case A_Negate:				return GenNegate(node);
		case A_BitwiseComplement:	return GenBitwiseComplement(node);
		case A_LogicalNot:			return GenLogicalNot(node);
		case A_Multiply:			return GenMultiply(node);
		case A_Divide:				return GenDivide(node);
		case A_Modulo:				return GenModulo(node);
		case A_Subtract:			return GenSubtract(node);
		case A_Add:					return GenAdd(node);
		case A_LeftShift:			return GenLShift(node);
		case A_RightShift:			return GenRShift(node);
		case A_LessThan:			return GenLessThan(node);
		case A_GreaterThan:			return GenGreaterThan(node);
		case A_EqualTo:				return GenEqualTo(node);
		case A_NotEqualTo:			return GenNotEqualTo(node);
		case A_BitwiseAnd:			return GenBitwiseAnd(node);
		case A_BitwiseXor:			return GenBitwiseXor(node);
		case A_BitwiseOr:			return GenBitwiseOr(node);
		case A_LogicalAnd:			return GenLogicalAnd(node);
		case A_LogicalOr:			return GenLogicalOr(node);
		default:					FatalM("Invalid expression in generation stage!", Line);
	}
}

static const char* GenStatementAsm(ASTNode* node){
	if(node == NULL)			FatalM("Expected an AST node, got NULL instead.", Line);
	if(node->op != A_Return)	FatalM("Expected Return Statement in function!", Line);
	if(node->lhs == NULL)
		return
			"	mov		$0,		%eax\n"
			"	mov		%rbp, %rsp\n"
			"	pop		%rbp\n"
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