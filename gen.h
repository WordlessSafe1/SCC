#include <string.h>
#include <math.h>

#include "defs.h"
#include "types.h"

static int labelPref = 9;

static const char* GenExpressionAsm(ASTNode* node);
static const char* GenStatementAsm(ASTNode* node);
const char* GenerateAsmFromList(ASTNodeList* list);


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

static const char* GenVarRef(ASTNode* node){
	if(node == NULL)			FatalM("Expected an AST node, got NULL instead! (In gen.h)", __LINE__);
	if(node->op != A_VarRef)	FatalM("Expected variable reference in expression! (In gen.h)", __LINE__);
	const char* id = node->value.strVal;
	VarEntry* var = FindVar(id, scope);
	if(var == NULL)				FatalM("Variable not defined!", Line);
	const char* format = "	mov		%s,	%%rax\n";
	int charCount = strlen(format) + strlen(var->value) + 1;
	char* str = malloc(charCount * sizeof(char));
	snprintf(str, charCount, format, var->value);
	return str;
}

static const char* GenUnary(ASTNode* node){
	if(node->lhs == NULL)	FatalM("Expected expression after unary operator!", Line);
	const char* instr = NULL;
	switch(node->op){
		case A_Negate:				instr = "	neg		%rax\n";	break;
		case A_BitwiseComplement:	instr = "	not		%rax\n";	break;
		case A_LogicalNot:
			instr =
				"	cmp		$0,		%rax\n"
				"	mov		$0,		%rax\n"
				"	sete	%al\n"
			;
			break;
	}
	const char* innerAsm = GenExpressionAsm(node->lhs);
	int charCount = strlen(instr) + strlen(innerAsm) + 1;
	char* str = malloc(charCount * sizeof(char));
	snprintf(str, charCount,"%s%s", innerAsm, instr);
	return str;
}

static const char* GenLTRBinary(ASTNode* node){
	if(node->lhs == NULL)	FatalM("Expected factor before binary operator!", Line);
	if(node->rhs == NULL)	FatalM("Expected factor after binary operator!", Line);
	const char* pushInstr = "	push	%rax\n";
	const char* popInstr = "	pop		%rcx\n";
	const char* instr = NULL;
	switch(node->op){
		case A_Multiply:			instr = "	imul	%rcx,	%rax\n";	break;
		case A_Add:					instr = "	addq	%rcx,	%rax\n";	break;
		case A_BitwiseAnd:			instr = "	and		%rcx,	%rax\n";	break;
		case A_BitwiseXor:			instr = "	xor		%rcx,	%rax\n";	break;
		case A_BitwiseOr:			instr = "	or		%rcx,	%rax\n";	break;
		case A_EqualTo:
			instr =
				"	cmp		%rcx,	%rax\n"
				"	mov		$0,		%rax\n"
				"	sete	%al\n"
			;
			break;
		case A_NotEqualTo:
			instr =
				"	cmp		%rcx,	%rax\n"
				"	mov		$0,		%rax\n"
				"	setne	%al\n"
			;
			break;
	}
	const char* lhs = GenExpressionAsm(node->lhs);
	const char* rhs = GenExpressionAsm(node->rhs);
	int charCount = strlen(instr) + strlen(pushInstr) + strlen(popInstr) + strlen(lhs) + strlen(rhs) + 1;
	char* str = malloc(charCount * sizeof(char));
	snprintf(str, charCount, "%s%s%s%s%s", lhs, pushInstr, rhs, popInstr, instr);
	return str;
}

static const char* GenRTLBinary(ASTNode* node){
	if(node->lhs == NULL)	FatalM("Expected factor before binary operator!", Line);
	if(node->rhs == NULL)	FatalM("Expected factor after binary operator!", Line);
	const char* pushInstr	= "	push	%rax\n";
	const char* popInstr	= "	pop		%rcx\n";
	const char* instr = NULL;
	switch(node->op){
		case A_Subtract:	instr = "	subq	%rcx,	%rax\n";	break;
		case A_LeftShift:	instr = "	shl		%rcx,	%rax\n";	break;
		case A_RightShift:	instr = "	sar		%rcx,	%rax\n";	break;
		case A_Divide:
			instr =
				"	cqo\n"
				"	idiv	%rcx\n"
			;
			break;
		case A_Modulo:
			instr =
				"	cqo\n"
				"	idiv	%rcx\n"
				"	mov		%rdx,	%rax\n"
			;
			break;
		case A_LessThan:
			instr =
				"	cmp		%rcx,	%rax\n"
				"	mov		$0,		%rax\n"
				"	setl	%al\n"
			;
			break;
		case A_GreaterThan:
			instr =
				"	cmp		%rcx,	%rax\n"
				"	mov		$0,		%rax\n"
				"	setg	%al\n"
			;
			break;
	}
	const char* lhs = GenExpressionAsm(node->lhs);
	const char* rhs = GenExpressionAsm(node->rhs);
	int charCount = strlen(instr) + strlen(pushInstr) + strlen(popInstr) + strlen(lhs) + strlen(rhs) + 1;
	char* str = malloc(charCount * sizeof(char));
	snprintf(str, charCount, "%s%s%s%s%s", rhs, pushInstr, lhs, popInstr, instr);
	return str;
}

static const char* GenShortCircuiting(ASTNode* node){
	if(node->lhs == NULL)	FatalM("Expected factor before binary operator!", Line);
	if(node->rhs == NULL)	FatalM("Expected factor after binary operator!", Line);
	const char* format = NULL;
	switch(node->op){
		case A_LogicalAnd:
			format =
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
			break;
		case A_LogicalOr:
			format = 
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
			break;
	}
	const char* rhs = GenExpressionAsm(node->rhs);
	const char* lhs = GenExpressionAsm(node->lhs);
	int charCount = strlen(format) + strlen(lhs) + strlen(rhs) + 1;
	char* str = malloc(charCount * sizeof(char));
	labelPref++;
	snprintf(str, charCount, format, lhs, labelPref, labelPref, labelPref, rhs, labelPref);
	return str;
}

static const char* GenTernary(ASTNode* node){
	if(node->lhs == NULL)	FatalM("Expected expression before ternary operator!", Line);
	if(node->rhs == NULL)	FatalM("Expected expression after ternary operator!", Line);
	const char* format;
	if(node->mid == NULL){
		format = 
			"%s"
			"	cmp		$0,		%%rax\n"
			"	jne		%d1f\n"
			"%s"
			"%d1:\n"
		;
	}
	else{
		format =
			"%s"
			"	cmp		$0,		%%rax\n"
			"	je		%d1f\n"
			"%s"
			"	jmp		%d2f\n"
			"%d1:\n"
			"%s"
			"%d2:\n"
		;
	}
	const char* rhs = GenExpressionAsm(node->rhs);
	const char* mid = node->mid == NULL ? "" : GenExpressionAsm(node->mid);
	const char* lhs = GenExpressionAsm(node->lhs);
	int charCount = strlen(format) + strlen(lhs) + strlen(mid) + strlen(rhs) + 1;
	char* str = malloc(charCount * sizeof(char));
	labelPref++;
	if(node->mid != NULL)
		snprintf(str, charCount, format, lhs, labelPref, mid, labelPref, labelPref, rhs, labelPref);
	else
		snprintf(str, charCount, format, lhs, labelPref, rhs, labelPref);
	return str;
}

static const char* GenAssignment(ASTNode* node){
	if(node == NULL)			FatalM("Expected an AST node, got NULL instead! (In gen.h)", __LINE__);
	if(node->op != A_Assign)	FatalM("Expected assignment in expression! (In gen.h)", __LINE__);
	const char* id = node->lhs->value.strVal;
	const char* rhs = GenExpressionAsm(node->rhs);
	VarEntry* var = FindVar(id, scope);
	if(var == NULL)				FatalM("Variable not defined!", Line);
	const char* format = "%s	mov		%%rax,	%s\n";
	int charCount = strlen(format) + strlen(rhs) + strlen(var->value);
	char* str = malloc(charCount * sizeof(char));
	snprintf(str, charCount, format, rhs, var->value);
	return str;
}

static const char* GenExpressionAsm(ASTNode* node){
	if(node == NULL)					FatalM("Expected an AST node, got NULL instead.", Line);
	switch(node->op){
		case A_LitInt:				return GenLitInt(node);
		case A_VarRef:				return GenVarRef(node);
		case A_Assign:				return GenAssignment(node);
		case A_Ternary:				return GenTernary(node);
		// Unary Operators
		case A_Negate:
		case A_BitwiseComplement:
		case A_LogicalNot:			return GenUnary(node);
		// Left-To-Right Operators
		case A_Add:
		case A_Multiply:
		case A_EqualTo:
		case A_NotEqualTo:
		case A_BitwiseAnd:
		case A_BitwiseXor:
		case A_BitwiseOr:			return GenLTRBinary(node);
		// Right-To-Left Operators
		case A_Divide:
		case A_Modulo:
		case A_Subtract:
		case A_LeftShift:
		case A_RightShift:
		case A_LessThan:
		case A_GreaterThan:			return GenRTLBinary(node);
		// Short-Circuiting Operators
		case A_LogicalAnd:
		case A_LogicalOr:			return GenShortCircuiting(node);
		// Unhandled
		case A_Undefined:			return "";
		default:					FatalM("Invalid expression in generation stage!", Line);
	}
}

static const char* GenReturnStatementAsm(ASTNode* node){
	if(node == NULL)			FatalM("Expected an AST node, got NULL instead.", Line);
	if(node->op != A_Return)	FatalM("Expected Return Statement in function!", Line);
	if(node->lhs == NULL)
		return
			"	mov		$0,		%eax\n"
			"	mov		%rbp,	%rsp\n"
			"	pop		%rbp\n"
			"	ret\n"
		;
	const char* innerAsm = GenExpressionAsm(node->lhs);
	const char* format =
		"%s"
		"	mov		%%rbp,	%%rsp\n"
		"	pop		%%rbp\n"
		"	ret\n"
	;
	int charCount = strlen(innerAsm) + strlen(format) + 1;
	char* str = malloc(charCount * sizeof(char));
	snprintf(str, charCount, format, innerAsm);
	return str;
}

static const char* GenIfStatement(ASTNode* node){
	if(node->lhs == NULL)	FatalM("Expected condition in if statement!", Line);
	if(node->rhs == NULL)	FatalM("Expected action in if statememt!", Line);
	const char* format;
	if(node->mid == NULL){
		format =
			"%s"
			"	cmp		$0,		%%rax\n"
			"	je		%d1f\n"
			"%s"
			"%d1:\n"
		;
	}
	else{
		format =
			"%s"
			"	cmp		$0,		%%rax\n"
			"	je		%d1f\n"
			"%s"
			"	jmp		%d2f\n"
			"%d1:\n"
			"%s"
			"%d2:\n"
		;
	}
	const char* rhs = GenStatementAsm(node->rhs);
	const char* mid = node->mid == NULL ? "" : GenStatementAsm(node->mid);
	const char* lhs = GenExpressionAsm(node->lhs);
	int charCount = strlen(format) + strlen(lhs) + strlen(mid) + strlen(rhs) + 1;
	char* str = malloc(charCount * sizeof(char));
	labelPref++;
	if(node->mid != NULL)
		snprintf(str, charCount, format, lhs, labelPref, rhs, labelPref, labelPref, mid, labelPref);
	else
		snprintf(str, charCount, format, lhs, labelPref, rhs, labelPref);
	return str;
}

static const char* GenDeclaration(ASTNode* node){
	if(node == NULL)									FatalM("Expected an AST Node, got NULL instead", Line);
	if(node->op != A_Declare)							FatalM("Expected declaration!", Line);
	if(FindLocalVar(node->value.strVal, scope) != NULL)	FatalM("Local variable redeclaration!", Line);
	char* varLoc = malloc(10 * sizeof(char));
	snprintf(varLoc, 10, "%d(%%rbp)", stackIndex[scope] -= 8);
	char* expr = "";
	if(node->lhs != NULL){
		const char* rhs = GenExpressionAsm(node->lhs);
		const char* format = "%s	mov		%%rax,	%s\n";
		int len = (strlen(format) + strlen(rhs) + strlen(varLoc) + 4);
		expr = malloc(len * sizeof(char));
		snprintf(expr, len, format, rhs, varLoc);
	}
	InsertVar(node->value.strVal, varLoc, scope);
	return expr;
}

static const char* GenWhileLoop(ASTNode* node){
	if(node == NULL)							FatalM("Expected an AST Node, got NULL instead", Line);
	if(node->op != A_While && node->op != A_Do)	FatalM("Expected While or Do-While loop!", Line);
	const char* condition	= GenExpressionAsm(node->lhs);
	const char* action		= GenStatementAsm(node->rhs);
	char* buffer;
	labelPref++;
	if(node->op == A_Do){
		const char* format =
			"%d0:\n"
			"0:\n"
			"%s"						// Action
			"%s"						// Condition
			"	cmp		$0,		%%rax\n"
			"	jne		%d0b\n"
			"%d9:\n"
			"9:\n"
		;
		int charCount = strlen(condition) + strlen(action) + strlen(format) + (3 * intlen(labelPref)) + 1;
		buffer = malloc(charCount * sizeof(char));
		snprintf(buffer, charCount, format, labelPref, action, condition, labelPref);
		return buffer;
	}
	const char* format = 
		"%d0:\n"
		"0:\n"
		"%s"						// Condition
		"	cmp		$0,		%%rax\n"
		"	je		%d9f\n"
		"%s"						// Action
		"	jmp		%d0b\n"
		"%d9:\n"
		"9:\n"
	;
	int charCount = strlen(condition) + strlen(action) + strlen(format) + (4 * intlen(labelPref)) + 1;
	buffer = malloc(charCount * sizeof(char));
	snprintf(buffer, charCount, format, labelPref, condition, labelPref, action, labelPref, labelPref);
	return buffer;
}

static const char* GenForLoop(ASTNode* node){
	if(node == NULL)									FatalM("Expected an AST Node, got NULL instead", Line);
	if(node->op != A_For)								FatalM("Expected for loop!", Line);
	if(node->rhs == NULL)								FatalM("Expected a statement folowing for loop!", Line);
	EnterScope();
	const char* initializer = NULL;
	if(node->lhs->lhs == NULL)					initializer = "";
	else if(node->lhs->lhs->op == A_Declare)	initializer = GenDeclaration(node->lhs->lhs);
	else										initializer = GenExpressionAsm(node->lhs->lhs);
	const char* modifier	= node->lhs->rhs == NULL ? "" : GenExpressionAsm(node->lhs->rhs);
	const char* action		= GenStatementAsm(node->rhs);
	const char* format =
		"%s"				// Allocate Stack Space for vars
		"%s"				// Initializer
		"%d0:\n"
		"0:\n"
		"%s"				// Condition
		"%s"				// Action
		"8:\n"
		"%s"				// Modifier
		"	jmp		%d0b\n"
		"%d9:\n"
		"9:\n"
		"%s"				// Deallocate Stack Space for vars
	;
	const char* condition	= node->lhs->mid == NULL ? NULL : GenExpressionAsm(node->lhs->mid);
	// Beyond this point, don't generate any more ASM using other functions
	int stackSize = GetLocalVarCount(scope) * 8;
	char* stackAlloc = malloc(1 * sizeof(char));
	*stackAlloc = '\0';
	char* stackDealloc = malloc(1 * sizeof(char));
	*stackDealloc = '\0';
	if(stackSize){
		free(stackAlloc);
		free(stackDealloc);
		const char* format = "	sub		$%d,		%%rsp\n";
		int len = strlen(format) + intlen(stackSize) + 1;
		stackAlloc = malloc(len * sizeof(char));
		snprintf(stackAlloc, len, format, stackSize);
		format = "	add		$%d,		%%rsp\n";
		stackDealloc = malloc(len * sizeof(char));
		snprintf(stackDealloc, len, format, stackSize);
	}
	labelPref++;
	if(condition != NULL){
		const char* condCheck = 
			"%s"
			"	cmp		$0,		%%rax\n"
			"	je		%d9f\n"
		;
		int charCount = strlen(condCheck) + strlen(condition);
		char* buffer = malloc(charCount * sizeof(char));
		snprintf(buffer, charCount, condCheck, condition, labelPref);
		condition = buffer;
	}
	int charCount = strlen(stackAlloc) + strlen(initializer) + strlen(modifier) + strlen(format) + strlen(condition) + strlen(action) + (3 * intlen(labelPref)) + strlen(stackDealloc) + 1;
	char* str = malloc(charCount * sizeof(char));
	snprintf(str, charCount, format, stackAlloc, initializer, labelPref, condition, action, modifier, labelPref, labelPref, stackDealloc);
	free((void*)condition);
	free(stackAlloc);
	ExitScope();
	return str;
}

static const char* GenContinue(ASTNode* node){
	return "	jmp		8f\n";
}

static const char* GenBreak(ASTNode* node){
	return "	jmp		9f\n";
}

static const char* GenBlockAsm(ASTNode* node){
	if(node == NULL)				FatalM("Expected an AST node, got NULL instead.", Line);
	if(node->op != A_Block)			FatalM("Expected function at top level statement!", Line);
	if(!node->list)					FatalM("Expected an ASTNodeList* member 'list'! (In gen.h)", __LINE__);
	if(!node->list->count)			return "";
	EnterScope();
	const char* statementAsm = GenerateAsmFromList(node->list);
	int stackSize = GetLocalVarCount(scope) * 8;
	char* stackAlloc = malloc(1 * sizeof(char));
	*stackAlloc = '\0';
	if(stackSize){
		free(stackAlloc);
		const char* format = "	sub		$%d,		%%rsp\n";
		int len = (strlen(format) + 6);
		stackAlloc = malloc(len * sizeof(char));
		snprintf(stackAlloc, len, format, stackSize);
	}
	int charCount =
		+ strlen(statementAsm)	// Inner ASM
		+ strlen(stackAlloc)	// Stack Allocation ASM
		+ 1						// '\0'
	;
	char* str = malloc(charCount * sizeof(char));
	snprintf(str, charCount, "%s%s", stackAlloc, statementAsm);
	free(stackAlloc);
	ExitScope();
	return str;
}

static const char* GenStatementAsm(ASTNode* node){
	if(node == NULL)			FatalM("Expected an AST node, got NULL instead.", Line);
	switch(node->op){
		case A_Return:		return GenReturnStatementAsm(node);
		case A_Declare:		return GenDeclaration(node);
		case A_Block:		return GenBlockAsm(node);
		case A_If:			return GenIfStatement(node);
		case A_For:			return GenForLoop(node);
		case A_Do:
		case A_While:		return GenWhileLoop(node);
		case A_Continue:	return GenContinue(node);
		case A_Break:		return GenBreak(node);
		default:			return GenExpressionAsm(node);
	}
}

static const char* GenFunctionAsm(ASTNode* node){
	if(node == NULL)				FatalM("Expected an AST node, got NULL instead.", Line);
	if(node->op != A_Function)		FatalM("Expected function at top level statement!", Line);
	if(node->value.strVal == NULL)	FatalM("Expected a function identifier, got NULL instead.", Line);
	const char* format = 
		"	.globl %s\n"			// Identifier
		"%s:\n"						// Identifier
		"	push	%%rbp\n"
		"	mov		%%rsp,	%%rbp\n"
		"%s"						// Statement ASM
	;
	const char* statementAsm = GenBlockAsm(node->lhs);
	if(statementAsm == NULL){
		FatalM("Implicit returns not yet handled. In gen.h", __LINE__);
	}
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

const char* GenerateAsmFromList(ASTNodeList* list){
	if(list->count < 1)	return "";
	const char* generated = GenStatementAsm(list->nodes[0]);
	char* buffer = malloc(strlen(generated) + 1);
	strcpy(buffer, generated);
	int i = 1;
	while(i < list->count){
		generated = GenStatementAsm(list->nodes[i]);
		buffer = realloc(buffer, strlen(buffer) + strlen(generated) + 1);
		strcat(buffer, generated);
		i++;
	}
	return buffer;
}