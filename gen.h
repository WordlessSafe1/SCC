#include <string.h>
#include <math.h>

#include "defs.h"
#include "types.h"

static int labelPref = 9;
static char* data_section;
static DbLnkList* bss_vars = NULL;

static const char* GenExpressionAsm(ASTNode* node);
static const char* GenStatementAsm(ASTNode* node);
static const char* GenerateAsmFromList(ASTNodeList* list);


static char* CalculateParamPosition(int n){
	if(n > 3){
		const char* format = "%d(%%rsp)";
		const int charCount = strlen(format) + intlen(n) + 1;
		char* buffer = calloc(charCount, sizeof(char));
		snprintf(buffer, charCount, format, 32 + (8 * (n - 4)));
		return buffer;
	}
	const char* loc = NULL;
	switch(n){
		case 0:		loc = "%rcx";	break;
		case 1:		loc = "%rdx";	break;
		case 2:		loc = "%r8";		break;
		case 3:		loc = "%r9";		break;
	}
	char* buffer = calloc((strlen(loc) + 1), sizeof(char));
	strncpy(buffer, loc, strlen(loc) + 1);
	return buffer;
}

// Where the params will be when passed to the current function
static char* CalculateLocalParamPosition(int n){
	if(n > 3){
		const char* format = "%d(%%rbp)";
		const int charCount = strlen(format) + intlen(n) + 1;
		char* buffer = calloc(charCount, sizeof(char));
		snprintf(buffer, charCount, format, 48 + (8 * (n - 4)));
		return buffer;
	}
	const char* loc = NULL;
	switch(n){
		case 0:		loc = "%rcx";	break;
		case 1:		loc = "%rdx";	break;
		case 2:		loc = "%r8";		break;
		case 3:		loc = "%r9";		break;
	}
	char* buffer = calloc((strlen(loc) + 1), sizeof(char));
	strncpy(buffer, loc, strlen(loc) + 1);
	return buffer;
}

static const char* GenLitInt(ASTNode* node){
	if(node == NULL)			FatalM("Expected an AST node, got NULL instead.", Line);
	if(node->op != A_LitInt)	FatalM("Expected literal int in expression!", Line);
	const char* format = "	movq	$%d,	%%rax\n"; // NULL;
	switch(GetPrimSize(node->type)){
		// case 1:		format = "	movb	$%d,	%%al\n";	break;
		// case 4:		format = "	movl	$%d,	%%eax\n";	break;
		// case 8:		format = "	movq	$%d,	%%rax\n";	break;
		default:	format = "	movq	$%d,	%%rax\n";	break;
	}
	int value = node->value.intVal;
	int charCount = strlen(format) + intlen(value) + 1;
	char* str = malloc(charCount * sizeof(char));
	snprintf(str, charCount, format, value);
	return str;
}

static char* GenLitStr(ASTNode* node){
	if(node == NULL)			FatalM("Expected an AST node, got NULL instead.", Line);
	if(node->op != A_LitStr)	FatalM("Expected literal String in expression!", Line);
	const char* format = "	leaq	L%d(%%rip),	%%rax\n";
	int charCount = strlen(format) + strlen(node->value.strVal) + (2 * intlen(lVar)) + 1;
	char* buffer = calloc(charCount, sizeof(char));
	snprintf(buffer, charCount, format, lVar);
	{
		// .data
		const char* format =
			"L%d:\n"
			"	.ascii \"%s\\0\"\n"
		;
		int charCount = strlen(format) + strlen(node->value.strVal) + intlen(lVar) + 1;
		char* buffer = calloc(charCount, sizeof(char));
		snprintf(buffer, charCount, format, lVar, node->value.strVal);
		data_section = realloc(data_section, (strlen(data_section) + charCount) * sizeof(char));
		strncat(data_section, buffer, charCount);
		free(buffer);
	}
	lVar++;
	return buffer;
}

static const char* GenFuncCall(ASTNode* node){
	if(node == NULL)				FatalM("Expected an AST node, got NULL instead! (In gen.h)", __LINE__);
	if(node->op != A_FunctionCall)	FatalM("Expected variable reference in expression! (In gen.h)", __LINE__);
	const char* id = node->value.strVal;
	ASTNodeList* params = node->secondaryValue.ptrVal;
	int pCount = params->count;
	char* paramInit = calloc(1, sizeof(char));
	for (int i = pCount - 1; i >= 0; i--) {
		const char* const format = "%s	movq	%%rax,	%s\n";
		char* pos = CalculateParamPosition(i);
		const char* inner = GenExpressionAsm(params->nodes[i]);
		const int charCount = strlen(inner) + strlen(format) + strlen(pos) + 1;
		char* buffer = malloc(charCount * sizeof(char));
		snprintf(buffer, charCount, format, inner, pos);
		paramInit = realloc(paramInit, strlen(paramInit) + charCount);
		strncat(paramInit, buffer, strlen(paramInit) + charCount);
		free(pos);
		free(buffer);
	}
	int offset = pCount < 4 ? 32 : 32 + 8 * (pCount - 4);
	offset = offset % 16 ? (offset / 16 + 1) * 16 : offset;
	const char* format =
		"	subq	$%d,	%%rsp\n"
		"%s"	// ParamInit
		"	call	%s\n"
		"	addq	$%d,	%%rsp\n"
	;
	int charCount = strlen(format) + strlen(paramInit) + (2 * intlen(offset)) + strlen(id) + 1;
	char* buffer = malloc(charCount * sizeof(char));
	snprintf(buffer, charCount, format, offset, paramInit, id, offset);
	return buffer;
}

static const char* GenVarRef(ASTNode* node){
	if(node == NULL)			FatalM("Expected an AST node, got NULL instead! (In gen.h)", __LINE__);
	if(node->op != A_VarRef)	FatalM("Expected variable reference in expression! (In gen.h)", __LINE__);
	const char* id = node->value.strVal;
	SymEntry* var = FindVar(id, scope);
	if(var == NULL)				FatalM("Variable not defined!", Line);
	const char* format = NULL;
	// if(strchr(var->key, 'i') != NULL){
		// // Var is global
	switch(GetTypeSize(var->type, var->cType)){
		case 1:		format = "	movzbq	%s,	%%rax\n";	break;
		case 4:		format = "	movslq	%s,	%%rax\n";	break;
		case 8:		format = "	movq	%s,	%%rax\n";	break;
		default:	format = "	movq	%s,	%%rax\n";	break;
	};
	// }
	// else
	// 	format = "	movq	%s,	%%rax\n";
	int charCount = strlen(format) + strlen(var->value.strVal) + 1;
	char* str = malloc(charCount * sizeof(char));
	snprintf(str, charCount, format, var->value);
	return str;
}

static char* GenAddressOf(ASTNode* node){
	if(node->lhs->op != A_VarRef){
		if(node->lhs->op == A_Dereference)
			return _strdup(GenExpressionAsm(node->lhs->lhs));
		FatalM("Unsupported lvalue! (Internal @ gen.h)", __LINE__);
	}
	const char* format = "	leaq	%s,	%%rax\n";
	SymEntry* varInfo = FindVar(node->lhs->value.strVal, scope);
	const char* offset = varInfo->value.strVal;
	const int charCount = strlen(format) + strlen(offset) + 1;
	char* buffer = malloc(charCount * sizeof(char));
	snprintf(buffer, charCount, format, offset);
	return buffer;
}

static char* GenDereference(ASTNode* node){
	const char* format;
	const char* offset;
	if(node->lhs->op == A_VarRef){
		format =
			"	movq	%s,	%%rax\n"	// Offset
			"	movq	(%%rax),	%%rax\n"
		;
		switch(GetTypeSize(node->type, node->cType)){
			case 1:
				format = 
					"	movq	%s,	%%rax\n"	// Offset
					"	movzbq	(%%rax),	%%rax\n"
				;
				break;
			case 4:
				format = 
					"	movq	%s,	%%rax\n"	// Offset
					"	movslq	(%%rax),	%%rax\n"
				;
				break;
			case 8:		break;
			default:	break;
		}
		SymEntry* varInfo = FindVar(node->lhs->value.strVal, scope);
		if(varInfo == NULL)	FatalM("Failed to find variable!", Line);
		offset = varInfo->value.strVal;
	}
	else{
		format =
			"%s"						// Inner ASM
			"	movq	(%%rax),	%%rax\n"
		;
		switch(GetTypeSize(node->type, node->cType)){
			case 1:
				format = 
					"%s"
					"	movzbq	(%%rax),	%%rax\n"
				;
				break;
			case 4:
				format = 
					"%s"
					"	movslq	(%%rax),	%%rax\n"
				;
				break;
			case 8:		break;
			default:	break;
		}
		offset = GenExpressionAsm(node->lhs);
	}
	const int charCount = strlen(format) + strlen(offset) + 1;
	char* buffer = malloc(charCount * sizeof(char));
	snprintf(buffer, charCount, format, offset);
	return buffer;
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
				"	movq	$0,		%rax\n"
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
				"	movq	$0,		%rax\n"
				"	sete	%al\n"
			;
			break;
		case A_NotEqualTo:
			instr =
				"	cmp		%rcx,	%rax\n"
				"	movq	$0,		%rax\n"
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
				"	movq	%rdx,	%rax\n"
			;
			break;
		case A_LessThan:
			instr =
				"	cmp		%rcx,	%rax\n"
				"	movq	$0,		%rax\n"
				"	setl	%al\n"
			;
			break;
		case A_GreaterThan:
			instr =
				"	cmp		%rcx,	%rax\n"
				"	movq	$0,		%rax\n"
				"	setg	%al\n"
			;
			break;
		case A_LessOrEqual:
			instr =
				"	cmp		%rcx,	%rax\n"
				"	movq	$0,		%rax\n"
				"	setle	%al\n"
			;
			break;
		case A_GreaterOrEqual:
			instr =
				"	cmp		%rcx,	%rax\n"
				"	movq	$0,		%rax\n"
				"	setge	%al\n"
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
				"	movq	$0,		%%rax\n"
				"	setne	%%al\n"
				"%d2:\n"
			;
			break;
		case A_LogicalOr:
			format = 
				"%s"
				"	cmp		$0,		%%rax\n"
				"	je		%d1f\n"
				"	movq	$1,		%%rax\n"
				"	jmp		%d2f\n"
				"%d1:\n"
				"%s"
				"	cmp		$0,		%%rax\n"
				"	movq	$0,		%%rax\n"
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
	if(node == NULL)				FatalM("Expected an AST node, got NULL instead! (In gen.h)", __LINE__);
	if(node->op != A_Assign)		FatalM("Expected assignment in expression! (In gen.h)", __LINE__);
	if (node->lhs->op == A_VarRef) {
		const char* id = node->lhs->value.strVal;
		const char* rhs = GenExpressionAsm(node->rhs);
		SymEntry* var = FindVar(id, scope);
		if (var == NULL)				FatalM("Variable not defined!", Line);
		const char* offset = var->value.strVal;
		const char* format = NULL;
		switch(GetPrimSize(var->type)){
			case 1:		format = "%s	movb	%%al,	%s\n";	break;
			case 4:		format = "%s	movl	%%eax,	%s\n";	break;
			case 8:		format = "%s	movq	%%rax,	%s\n";	break;
			default:	FatalM("Non-standard sizes not yet supported in assignments! (Internal @ gen.h)", __LINE__);
		}
		int charCount = strlen(format) + strlen(rhs) + strlen(offset);
		char* str = malloc(charCount * sizeof(char));
		snprintf(str, charCount, format, rhs, offset);
		return str;
	}
	if(node->lhs->op != A_Dereference)	FatalM("Unsupported assignment target! (In gen.h)", __LINE__);
	const char* derefASM = GenExpressionAsm(node->lhs->lhs);
	const char* format = 
		"%s" // lhs
		"	push	%%rax\n"
		"%s" // rhs
		"	pop		%%rcx\n"
		"%s"
		// "	mov		%%rax,	(%%rcx)\n"
	;
	const char* instr = NULL;
	switch(GetTypeSize(node->lhs->type, node->lhs->cType)){
		case 1:		instr = "	movb	%al,	(%rcx)\n";	break;
		case 4:		instr = "	movl	%eax,	(%rcx)\n";	break;
		case 8:		instr = "	movq	%rax,	(%rcx)\n";	break;
		default:	FatalM("Non-standard sizes not yet supported in assignments! (Internal @ gen.h)", __LINE__);
	}
	const char* innerASM = GenExpressionAsm(node->rhs);
	const int charCount = strlen(format) + strlen(derefASM) + strlen(innerASM) + strlen(instr) + 1;
	char* buffer = malloc(charCount * sizeof(char));
	snprintf(buffer, charCount, format, derefASM, innerASM, instr);
	return buffer;
}

static const char* GenIncDec(ASTNode* node){
	if(node == NULL)										FatalM("Expected an AST node, got NULL instead! (In gen.h)", __LINE__);
	if(node->op != A_Increment && node->op != A_Decrement)	FatalM("Expected increment or decrement in expression! (In gen.h)", __LINE__);
	const char* id = node->lhs->value.strVal;
	SymEntry* var = FindVar(id, scope);
	if(var == NULL)				FatalM("Variable not defined!", Line);
	const char* format;
	if(node->op == A_Increment){
		if(node->value.intVal)
			format =
				"	movq	%s,	%%rax\n"
				"	incq	%s\n"
			;
		else
			format =
				"	incq	%s\n"
				"	movq	%s,	%%rax\n"
			;
	}
	else{
		if(node->value.intVal)
			format =
				"	movq	%s,	%%rax\n"
				"	decq	%s\n"
			;
		else
			format =
				"	decq	%s\n"
				"	movq	%s,	%%rax\n"
			;
	}
	int charCount = strlen(format) + (2 * strlen(var->value.strVal));
	char* str = malloc(charCount * sizeof(char));
	snprintf(str, charCount, format, var->value, var->value);
	return str;
}

static const char* GenCompoundAssignment(ASTNode* node){
	if(node == NULL)	FatalM("Expected an AST node, got NULL instead! (In gen.h)", __LINE__);
	const char* id = node->lhs->value.strVal;
	const char* rhs = GenExpressionAsm(node->rhs);
	SymEntry* var = FindVar(id, scope);
	if(var == NULL)		FatalM("Variable not defined!", Line);
	// val, op, mov -> offset
	const char* format = NULL;
	switch(node->op){
		case A_AssignSum:			format = "%s	add		%%rax,	%s\n	movq	%s,	%%rax\n";				break;
		case A_AssignDifference:	format = "%s	sub		%%rax,	%s\n	movq	%s,	%%rax\n";				break;
		case A_AssignProduct:		format = "%s	imul	%s,	%%rax\n	movq	%%rax,	%s\n";					break;
		case A_AssignBitwiseAnd:	format = "%s	and		%%rax,	%s\n	movq	%s,	%%rax\n";				break;
		case A_AssignBitwiseXor:	format = "%s	xor		%%rax,	%s\n	movq	%s,	%%rax\n";				break;
		case A_AssignBitwiseOr:		format = "%s	or		%%rax,	%s\n	movq	%s,	%%rax\n";				break;
		case A_AssignQuotient:
			format = 
				"%s"
				"	movq	%%rax,	%%rcx\n"
				"	movq	%s,	%%rax\n"
				"	cqo\n"
				"	idiv	%%rcx\n"
				"	movq	%%rax,	%s\n"
			;
			break;
		case A_AssignModulus:
			format = 
				"%s"
				"	movq	%%rax,	%%rcx\n"
				"	movq	%s,	%%rax\n"
				"	cqo\n"
				"	idiv	%%rcx\n"
				"	movq	%%rdx,	%%rax\n"
				"	movq	%%rax,	%s\n"
			;
			break;
		case A_AssignLeftShift:
			format = 
				"%s"
				"	movq	%%rax,	%%rcx\n"
				"	movq	%s,	%%rax\n"
				"	shl		%%rcx,	%%rax\n"
				"	movq	%%rax,	%s\n"
			;
			break;
		case A_AssignRightShift:
			format = 
				"%s"
				"	movq	%%rax,	%%rcx\n"
				"	movq	%s,	%%rax\n"
				"	sar		%%rcx,	%%rax\n"
				"	movq	%%rax,	%s\n"
			;
			break;
		default:					FatalM("Unexpected operation in compound assignment! (In gen.h)", __LINE__);
	}
	int charCount = strlen(format) + strlen(rhs) + (2 * strlen(var->value.strVal)) + 1;
	char* str = malloc(charCount * sizeof(char));
	snprintf(str, charCount, format, rhs, var->value, var->value);
	return str;
}

static const char* GenExpressionAsm(ASTNode* node){
	if(node == NULL)					FatalM("Expected an AST node, got NULL instead! (In gen.h)", __LINE__);
	switch(node->op){
		case A_LitInt:				return GenLitInt(node);
		case A_LitStr:				return GenLitStr(node);
		case A_VarRef:				return GenVarRef(node);
		case A_Ternary:				return GenTernary(node);
		case A_Assign:				return GenAssignment(node);
		case A_FunctionCall:		return GenFuncCall(node);
		case A_Dereference:			return GenDereference(node);
		case A_AddressOf:			return GenAddressOf(node);
		// Compound Assignment
		case A_AssignSum:
		case A_AssignDifference:
		case A_AssignProduct:
		case A_AssignQuotient:
		case A_AssignModulus:
		case A_AssignLeftShift:
		case A_AssignRightShift:
		case A_AssignBitwiseAnd:
		case A_AssignBitwiseXor:
		case A_AssignBitwiseOr:		return GenCompoundAssignment(node);
		// Increment / Decrement
		case A_Increment:
		case A_Decrement:			return GenIncDec(node);
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
		case A_LessOrEqual:
		case A_GreaterOrEqual:
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
			"	movq	$0,		%eax\n"
			"	jmp		7f\n"
		;
	const char* innerAsm = GenExpressionAsm(node->lhs);
	const char* format =
		"%s"
		"	jmp		7f\n"
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

static char* GenDeclaration(ASTNode* node){
	if(node == NULL)									FatalM("Expected an AST Node, got NULL instead", Line);
	if(node->op != A_Declare)							FatalM("Expected declaration!", Line);
	if(FindLocalVar(node->value.strVal, scope) != NULL)	FatalM("Local variable redeclaration!", Line);
	char* varLoc = malloc(10 * sizeof(char));
	char* expr = "";
	if(!scope){
		// Global variable
		free(varLoc);
		const char* id = node->value.strVal;
		varLoc = malloc((strlen(id) + 7) * sizeof(char));
		snprintf(varLoc, strlen(id) + 7, "%s(%%rip)", id);
		InsertVar(node->value.strVal, varLoc, node->type, node->cType, node->secondaryValue.intVal, scope);
		for(DbLnkList* bss = bss_vars; bss != NULL; bss = bss->next){
			if(!streq(bss->val, id))
				continue;
			bss->prev->next = bss->next;
			bss->next->prev = bss->prev;
			free(bss);
		}
		// If external, we don't want to allocate space or generate ASM for it.
		// This is after the BSS removal, because we want to be sure we aren't allocating space for a previously defined reference.
		// int a; extern int a;
		// is treated the same as 
		// extern int a;
		if(node->secondaryValue.intVal == C_Extern)
			return calloc(1, sizeof(char));
		if(node->lhs == NULL){
			DbLnkList* bss = MakeDbLnkList((void*)id, NULL, bss_vars);
			bss_vars->prev = bss;
			bss_vars = bss;
			return calloc(1, sizeof(char));
		}
		if (node->lhs->op != A_LitInt)
			FatalM("Non-constant expression used in global variable declaration!", Line);
		const char* format =
			"	.globl %s\n"
			"%s:\n"
			"	.quad	%d\n"
		;
		switch(GetTypeSize(node->type, node->cType)){
			case 1:		format = "	.globl %s\n%s:\n	.byte	%d\n"; break;
			case 4:		format = "	.globl %s\n%s:\n	.long	%d\n"; break;
			case 8:		format = "	.globl %s\n%s:\n	.quad	%d\n"; break;
			default:	break;
		}
		const int charCount = strlen(format) + (2 * strlen(id)) + intlen(node->lhs->value.intVal) + 1;
		char* buffer = malloc(charCount * sizeof(char));
		snprintf(buffer, charCount, format, id, id, node->lhs->value.intVal);
		data_section = realloc(data_section, (strlen(data_section) + charCount) * sizeof(char));
		strncat(data_section, buffer, charCount);
		free(buffer);
		return calloc(1, sizeof(char));
	}
	snprintf(varLoc, 10, "%d(%%rbp)", stackIndex[scope] -= GetTypeSize(node->type, node->cType));
	if(node->lhs != NULL){
		const char* rhs = GenExpressionAsm(node->lhs);
		const char* format = "%s	movq	%%rax,	%s\n";
		switch(GetTypeSize(node->type, node->cType)){
			case 1:		format = "%s	movb	%%al,	%s\n";	break;
			case 4:		format = "%s	movl	%%eax,	%s\n";	break;
			case 8:		break;
			default:	break;
		}
		int len = (strlen(format) + strlen(rhs) + strlen(varLoc) + 4);
		expr = malloc(len * sizeof(char));
		snprintf(expr, len, format, rhs, varLoc);
	}
	InsertVar(node->value.strVal, varLoc, node->type, node->cType, C_Default, scope);
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
			"8:\n"
			"%s"						// Condition
			"	cmp		$0,		%%rax\n"
			"	jne		%d0b\n"
			"%d9:\n"
			"9:\n"
		;
		int charCount = strlen(condition) + strlen(action) + strlen(format) + (3 * intlen(labelPref)) + 1;
		buffer = malloc(charCount * sizeof(char));
		snprintf(buffer, charCount, format, labelPref, action, condition, labelPref, labelPref);
		return buffer;
	}
	const char* format = 
		"%d0:\n"
		"0:\n"
		"%s"						// Condition
		"	cmp		$0,		%%rax\n"
		"	je		%d9f\n"
		"%s"						// Action
		"8:\n"
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

static char* GenSwitch(ASTNode* node){
	ASTNodeList* list = node->list;
	int childCount = list->count;
	int* caseLabel = malloc(childCount * sizeof(int));
	int* caseValue = malloc(childCount * sizeof(int));
	int lJmp = lVar++;
	int lTop = lVar++;
	int lEnd = lVar++;
	int lDef = lEnd;
	int caseCount = childCount;
	USE_SUB_SWITCH = true;
	const char* format =
		"%s"								// Expression ASM
		"	jmp		L%d\n"					// lTop
		"%s"								// Cases ASM
		"	jmp		L%d\n"					// lEnd
		"%s"								// Jump Table ASM
		"L%d:\n"							// lTop
		"	leaq	L%d(%%rip),	%%rdx\n"	// lJmp
		"	jmp		switch\n"				// Jump to switch subroutine
		"L%d:\n"							// lEnd
	;
	char* casesASM = calloc(1, sizeof(char));
	const char* declLabelFormat =
		"L%d:\n"
		"	.quad	%d\n" // Count
	;
	char* tableASM = calloc(1, sizeof(char));
	const char* exprAsm = GenExpressionAsm(node->lhs);


	const char* caseFrmt = "L%d:\n" "%s";
	const char* jmpFrmt = "	.quad	%d,	L%d\n";
	for(int i = 0; i < childCount; i++){
		ASTNode* inner = list->nodes[i];
		caseLabel[i] = lVar++;
		caseValue[i] = inner->value.intVal;
		const char* innerASM = GenerateAsmFromList(inner->list);
		char* caseASM = sngenf(strlen(caseFrmt) + intlen(caseLabel[i]) + strlen(innerASM) + 1, caseFrmt, caseLabel[i], innerASM);
		strapp(&casesASM, caseASM);
		free(caseASM);
		if(inner->op == A_Default){
			lDef = caseLabel[i];
			caseCount--;
			continue;
		}
		char* jmpASM = sngenf(strlen(jmpFrmt) + intlen(caseValue[i]) + intlen(caseLabel[i]) + 1, jmpFrmt, caseValue[i], caseLabel[i]);
		strapp(&tableASM, jmpASM);
		free(jmpASM);
	}
	char* tablePreASM = sngenf(strlen(declLabelFormat) + intlen(lJmp) + intlen(childCount) + 1, declLabelFormat, lJmp, childCount);
	strapp(&tablePreASM, tableASM);
	free(tableASM);
	tableASM = tablePreASM;
	const char* defFrmt = "	.quad	L%d\n";
	char* defJmpLbl = sngenf(strlen(jmpFrmt) + intlen(lDef) + 1, defFrmt, lDef);
	strapp(&tableASM, defJmpLbl);
	free(defJmpLbl);
	int charCount = 
		strlen(format)
		+ strlen(exprAsm)
		+ intlen(lTop)
		+ strlen(casesASM)
		+ intlen(lEnd)
		+ strlen(tableASM)
		+ intlen(lTop)
		+ intlen(lJmp)
		+ intlen(lEnd)
		+ 1
	;
	char* ret = sngenf(charCount, format, exprAsm, lTop, casesASM, lEnd, tableASM, lTop, lJmp, lEnd);
	free(casesASM);
	free(tableASM);
	return ret;
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
	int stackSize = align(GetLocalStackSize(scope), 8);
	char* stackAlloc = calloc(1, sizeof(char));
	char* stackDealloc = calloc(1, sizeof(char));
	if(stackSize){
		free(stackAlloc);
		free(stackDealloc);
		const char* format = "	subq	$%d,		%%rsp\n";
		int len = (strlen(format) + 6);
		stackAlloc = malloc(len * sizeof(char));
		snprintf(stackAlloc, len, format, stackSize);
		format = "	addq	$%d,		%%rsp\n";
		len = (strlen(format) + 6);
		stackDealloc = malloc(len * sizeof(char));
		snprintf(stackDealloc, len, format, stackSize);
	}
	int charCount =
		+ strlen(statementAsm)	// Inner ASM
		+ strlen(stackAlloc)	// Stack Allocation ASM
		+ strlen(stackDealloc)	// Stack Deallocation ASM
		+ 1						// '\0'
	;
	char* str = malloc(charCount * sizeof(char));
	snprintf(str, charCount, "%s%s%s", stackAlloc, statementAsm, stackDealloc);
	free(stackAlloc);
	free(stackDealloc);
	ExitScope();
	return str;
}

static char* GenStructDecl(ASTNode* node){
	SymList* list = InsertStruct(node->value.strVal, MakeCompMembers(node->list));
	if(node->lhs == NULL)				return calloc(1, sizeof(char));
	if(node->lhs->op != A_Declare)		FatalM("Expected child node of struct to be declaration! (In gen.h)", __LINE__);
	return GenDeclaration(node->lhs);
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
		case A_StructDecl:	return GenStructDecl(node);
		case A_Switch:		return GenSwitch(node);
		case A_EnumDecl:	return "";
		default:			return GenExpressionAsm(node);
	}
}

static const char* GenFunctionAsm(ASTNode* node){
	if(node == NULL)				FatalM("Expected an AST node, got NULL instead.", Line);
	if(node->op != A_Function)		FatalM("Expected function at top level statement!", Line);
	if(node->value.strVal == NULL)	FatalM("Expected a function identifier, got NULL instead.", Line);
	if(node->lhs == NULL)			return "";
	Parameter* params = (Parameter*)node->secondaryValue.ptrVal;
	int paramCount = 0;
	if (params != NULL) {
		paramCount++;
		while (params->next != NULL) {
			paramCount++;
			params = params->next;
		}
	}
	if(paramCount)
		EnterScope();
	const char* format = 
		"	.globl %s\n"			// Identifier
		"%s:\n"						// Identifier
		"	push	%%rbp\n"
		"	movq	%%rsp,	%%rbp\n"
		"%s"						// Stack Allocation ASM
		"%s"						// Parameter Placement ASM
		"%s"						// Statement ASM
		"%s"						// Stack Deallocation ASM
		"7:\n"
		"	movq	%%rbp,	%%rsp\n"
		"	pop		%%rbp\n"
		"	ret\n"
	;
	char* paramPlacement = calloc(1, sizeof(char));
	for(int i = paramCount - 1; i >= 0; i--){
		char* paramPos = CalculateLocalParamPosition(i);
		char* varLoc = NULL;
		if(i > 3)
			varLoc = paramPos;
		else {
			int offset = stackIndex[scope] -= 8;
			const char* const format = "%d(%%rbp)";
			int charCount = intlen(offset) + strlen(format) + 1;
			varLoc = malloc(charCount * sizeof(char));
			snprintf(varLoc, charCount, format, offset);
		}
		InsertVar(params->id, varLoc, params->type, NULL, C_Default, scope);
		const char* const format = "	movq	%s,	%s\n";
		const int charCount = strlen(format) + strlen(varLoc) + strlen(paramPos) + 1;
		char* buffer = calloc(charCount, sizeof(char));
		snprintf(buffer, charCount, format, paramPos, varLoc);
		if(i < 4){
			paramPlacement = realloc(paramPlacement, charCount + strlen(paramPlacement));
			strncat(paramPlacement, buffer, charCount + strlen(paramPlacement));
		}
		free(buffer);
		params = params->prev;
	}
	char* stackAlloc = calloc(1, sizeof(char));
	char* stackDealloc = calloc(1, sizeof(char));
	if(paramCount){
			const char* format = "	subq	$%d,	%%rsp\n";
			const int allocSize = paramCount * 8;
			int charCount = strlen(format) + intlen(allocSize) + 1;
			stackAlloc = malloc(charCount * sizeof(char));
			snprintf(stackAlloc, charCount, format, allocSize);
			format = "	addq	$%d,	%%rsp\n";
			charCount = strlen(format) + intlen(allocSize) + 1;
			stackDealloc = malloc(charCount * sizeof(char));
			snprintf(stackDealloc, charCount, format, allocSize);
	}
	const char* statementAsm = GenBlockAsm(node->lhs);
	int charCount =
		(2 * strlen(node->value.strVal))	// identifier x2
		+ strlen(stackAlloc)				// Stack Allocation ASM
		+ strlen(paramPlacement)			// Parameter Placement ASM
		+ strlen(statementAsm)				// Inner ASM
		+ strlen(format)					// format
		+ strlen(stackDealloc)				// Stack Deallocation ASM
		+ 1									// \0
	;
	char* str = malloc(charCount * sizeof(char));
	snprintf(str, charCount, format, node->value.strVal, node->value.strVal, stackAlloc, paramPlacement, statementAsm, stackDealloc);
	free(paramPlacement);
	free(stackAlloc);
	if(paramCount)
		ExitScope();
	return str;
}

static const char* GenerateAsmFromList(ASTNodeList* list){
	if(list->count < 1)	return "";
	const char* generated = NULL;
	char* buffer = malloc(1);
	*buffer = '\0';
	int i = 0;
	while(i < list->count){
		ASTNode* node = list->nodes[i];
		switch(node->op){
			case A_Function:	generated = GenFunctionAsm(node);	break;
			default:			generated = GenStatementAsm(node);	break;
		}
		// generated = GenStatementAsm(node);
		buffer = realloc(buffer, strlen(buffer) + strlen(generated) + 1);
		strcat(buffer, generated);
		i++;
	}
	return buffer;
}

char* GenerateAsm(ASTNodeList* node){
	data_section = calloc(1, sizeof(char));
	bss_vars = MakeDbLnkList("", NULL, NULL);
	char* bss_section = calloc(1, sizeof(char));
	char* Asm = _strdup(GenerateAsmFromList(node));
	if(USE_SUB_SWITCH){
		const char* switchPreamble =
			"switch:\n"
			"	pushq	%rsi\n"			// Save %rsi
			"	movq	%rdx,	%rsi\n"	// Base of jump table => %rsi
			"	movq	%rax,	%rbx\n"	// Expression Value => %rbx
			"	cld\n"					// Clear direction flag
			"	lodsq\n"				// Case Count => %rax AND increment %rsi
			"	movq	%rax,	%rcx\n"	// Case Count => %rcx
			"1:\n"						// Start of loop
			"	lodsq\n"				// Case Value => %rax
			"	movq	%rax,	%rdx\n"	// Case Value => %rdx
			"	lodsq\n"				// Case Label => %rax
			"	cmpq	%rdx,	%rbx\n"	// Case Value <=> Expression Value
			"	jne		2f\n"			// If != jmp forward to 2
			"	popq	%rsi\n"			// Restore %rsi
			"	jmp		*%rax\n"		// Jump to label
			"2:\n"						// End of loop
			"	loop	1b\n"			// If %rcx != 0, jmp back to 1
			"	lodsq\n"				// Cases Exhausted, Default Label => %rax
			"	popq	%rsi\n"			// Restore %rsi
			"	jmp		*%rax\n"		// Jump to default
		;
		strapp(&Asm, switchPreamble);
	}
	int dslen = strlen(data_section);
	if(dslen){
		const char* format =
			"	.data\n"
			"	.align 16\n"
			"%s"
		;
		dslen += strlen(format);
		char* buffer = malloc((dslen + 1) * sizeof(char));
		snprintf(buffer, dslen + 1, format, data_section);
		free(data_section);
		data_section = buffer;
	}
	if(bss_vars->next != NULL){
		bss_section = realloc(bss_section, 18);
		sprintf(bss_section, "	.bss\n	.align	16\n");
		const char* const format =
			"%s" // bss_section
			"%s:\n" // id
			"	.zero	%d\n" // size
		;
		for(DbLnkList* bss = bss_vars; bss != NULL; bss = bss->next){
			if(streq(bss->val, ""))	continue;
			char* buffer = malloc(strlen(bss_section) + strlen(bss->val) + strlen(format) + 1);
			SymEntry* var = FindVar(bss->val, 0);
			if(var == NULL)	FatalM("Failed to find global variable! (In gen.h)", __LINE__);
			sprintf(buffer, format, bss_section, bss->val, GetTypeSize(var->type, var->cType));
			free(bss_section);
			bss_section = buffer;
			if(bss->prev != NULL)
				free(bss->prev);
		}
	}
	const char* format =
		"%s" // Data section
		"%s" // BSS section
		"	.text\n"
		"%s" // ASM
	;
	data_section = realloc(data_section, dslen + strlen(Asm) + 1);
	char* buffer = malloc(dslen + strlen(Asm) + strlen(format) + strlen(bss_section) + 1);
	snprintf(buffer, dslen + strlen(Asm) + strlen(format) + strlen(bss_section) + 1, format, data_section, bss_section, Asm);
	free(Asm);
	return buffer;
}