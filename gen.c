#include <string.h>

#include "defs.h"
#include "types.h"
#include "symTable.h"

static int unresolvedPushes = 0;
static int labelPref = 9;
static char* data_section;
static DbLnkList* bss_vars = NULL;
static Parameter* curFuncParams = NULL;

static const char* GenExpressionAsm(ASTNode* node);
static const char* GenStatementAsm(ASTNode* node);
static const char* GenerateAsmFromList(ASTNodeList* list);
static const char* GenCompoundAssignment(ASTNode* node);

struct {
	int lbreak;
	int lcontinue;
} labels;

enum paramMode {
	P_MODE_DEFAULT	= 1,
	P_MODE_LOCAL	= 2,
	P_MODE_SHADOW	= 3,
};
static char* CalculateParamPosition(int n, enum paramMode mode){
	if(n > 3){
		const char* format;
		int offset;
		if(mode == P_MODE_LOCAL){
			format = "%d(%%rbp)";
			offset = 48 + (8 * (n - 4));
		}
		else {
			format = "%d(%%rsp)";
			offset = 32 + (8 * (n - 4));
		}
		const int charCount = strlen(format) + intlen(n) + 1;
		return sngenf(charCount, format, offset);
	}
	const char* loc = NULL;
	switch(n){
		case 0:		loc = mode == P_MODE_SHADOW ? "(%rsp)"		: "%rcx";	break;
		case 1:		loc = mode == P_MODE_SHADOW ? "8(%rsp)"		: "%rdx";	break;
		case 2:		loc = mode == P_MODE_SHADOW ? "16(%rsp)"	: "%r8";	break;
		case 3:		loc = mode == P_MODE_SHADOW ? "24(%rsp)"	: "%r9";	break;
	}
	return _strdup(loc);
}

static const char* GenLitInt(ASTNode* node){
	if(node == NULL)			FatalM("Expected an AST node, got NULL instead.", Line);
	if(node->op != A_LitInt)	FatalM("Expected literal int in expression!", Line);
	const char* format = "	movq	$%lld,	%%rax\n"; // NULL;
	switch(GetPrimSize(node->type)){
		// case 1:		format = "	movb	$%lld,	%%al\n";	break;
		// case 4:		format = "	movl	$%lld,	%%eax\n";	break;
		// case 8:		format = "	movq	$%lld,	%%rax\n";	break;
		default:	format = "	movq	$%lld,	%%rax\n";	break;
	}
	long long value = node->value.intVal;
	return sngenf(strlen(format) + intlen(value) + 1, format, value);
}

static char* GenLitStr(ASTNode* node){
	if(node == NULL)			FatalM("Expected an AST node, got NULL instead.", Line);
	if(node->op != A_LitStr)	FatalM("Expected literal String in expression!", Line);
	const char* format = "	leaq	L%d(%%rip),	%%rax\n";
	char* buffer = sngenf(strlen(format) + strlen(node->value.strVal) + (2 * intlen(lVar)) + 1, format, lVar);
	{
		// .data
		const char* format =
			"L%d:\n"
			"	.ascii \"%s\\0\"\n"
		;
		char* buffer = sngenf(strlen(format) + strlen(node->value.strVal) + intlen(lVar) + 1, format, lVar, node->value.strVal);
		strapp(&data_section, buffer);
		free(buffer);
	}
	lVar++;
	return buffer;
}

static char* GenExpressionList(ASTNode* node){
	char* ret = calloc(1, sizeof(char));
	for(int i = 0; i < node->list->count; i++)
		strapp(&ret, GenExpressionAsm(node->list->nodes[i]));
	return ret;
}

static const char* GenFuncCall(ASTNode* node){
	if(node == NULL)				FatalM("Expected an AST node, got NULL instead! (In gen.h)", __LINE__);
	if(node->op != A_FunctionCall)	FatalM("Expected variable reference in expression! (In gen.h)", __LINE__);
	const char* id = node->value.strVal;
	ASTNodeList* params = node->secondaryValue.ptrVal;
	int pCount = params->count;
	char* paramInit = calloc(1, sizeof(char));
	char* paramRecall = calloc(1, sizeof(char));
	int offset = pCount < 4 ? 32 : 32 + 8 * (pCount - 4);
	offset = offset % 16 ? (offset / 16 + 1) * 16 : offset;
	for (int i = pCount - 1; i >= 0; i--) {
		ASTNode* curNode = params->nodes[i];
		const char* const format = "%s	movq	%%rax,	%s\n";
		char* pos = CalculateParamPosition(i, P_MODE_SHADOW);
		const char* inner = GenExpressionAsm(curNode);
		if(curNode->cType != NULL && GetTypeSize(curNode->type, curNode->cType) > 8){
			switch(curNode->op){
				case A_Dereference:
					if(curNode->lhs->op != A_VarRef){
						inner = GenExpressionAsm(curNode->lhs); // Loads the address into %eax
						break;
					}
					// continue
					FatalM("Composite dereference in function call!", Line);
				case A_VarRef:		FatalM("Composite variable reference in function call!", Line);
				default:			FatalM("Unhandled lvalue in function call composite! (Internal @ gen.h)", __LINE__);
			}
		}
		char* buffer = sngenf(strlen(inner) + strlen(format) + strlen(pos) + 1, format, inner, pos);
		strapp(&paramInit, buffer);
		free(pos);
		free(buffer);
	}
	int shadowPCount = pCount <= 4 ? pCount : 4;
	for (int i = 0; i < shadowPCount; i++){
		const char* format = "	movq	%s,	%s\n";
		char* shadowPos = CalculateParamPosition(i, P_MODE_SHADOW);
		char* pos = CalculateParamPosition(i, P_MODE_DEFAULT);
		char* buffer = sngenf(strlen(format) + strlen(pos) + strlen(shadowPos) + 1, format, shadowPos, pos);
		strapp(&paramRecall, buffer);
		free(pos);
		free(buffer);
	}
	strapp(&paramInit, paramRecall);
	free(paramRecall);
	if(unresolvedPushes % 2)
		offset += 8;
	const char* format =
		"	subq	$%d,	%%rsp\n"
		"%s"	// ParamInit
		"	call	%s\n"
		"	addq	$%d,	%%rsp\n"
	;
	return sngenf(strlen(format) + strlen(paramInit) + (2 * intlen(offset)) + strlen(id) + 1, format, offset, paramInit, id, offset);
}

static char* GenBuiltinCall(ASTNode* node){
	char* idStr_core = _strdup(node->value.strVal);
	const char* idStr = idStr_core + 15;
	ASTNodeList* params = node->secondaryValue.ptrVal;
	if(streq(idStr, "va_start")){
		Parameter* outerParams = curFuncParams;
		int i = 0;
		while(outerParams->next != NULL && !streq(outerParams->id, "...")){
			outerParams = outerParams->next;
			i++;
		}
		if(!streq(outerParams->id, "..."))
			FatalM("Failed to find variadic marker!", Line);
		char* loc;
		switch(i){
			case 0:		loc = _strdup("16(%rbp)");								break;
			case 1:		loc = _strdup("24(%rbp)");								break;
			case 2:		loc = _strdup("32(%rbp)");								break;
			case 3:		loc = _strdup("40(%rbp)");								break;
			default:	loc = CalculateParamPosition(i, P_MODE_LOCAL);	break;
		}
		const char* format = "	leaq	%s,	%%rax\n";
		char* buffer = sngenf(strlen(format) + strlen(loc) + 1, format, loc);
		free(loc);
		ASTNode* rhs = MakeASTLeaf(A_RawASM, P_Undefined, FlexStr(buffer));
		ASTNode* lhs = params->nodes[0];
		ASTNode* assign = MakeASTBinary(A_Assign, lhs->type, lhs, rhs, FlexNULL());
		const char* ret = GenExpressionAsm(assign);
		free(buffer);
		return _strdup(ret);
	}
	FatalM("Unknown built-in function! (Internal @ gen.h)", NOLINE);
}

static const char* GenVarRef(ASTNode* node){
	if(node == NULL)			FatalM("Expected an AST node, got NULL instead! (In gen.h)", __LINE__);
	if(node->op != A_VarRef)	FatalM("Expected variable reference in expression! (In gen.h)", __LINE__);
	const char* id = node->value.strVal;
	SymEntry* var = FindVar(id, scope);
	if(var == NULL)				FatalM("Variable not defined!", Line);
	const char* format = NULL;
	bool isUnsigned = IsUnsigned(node->type);
	switch(GetTypeSize(var->type, var->cType)){
		case 1:		format = isUnsigned 
						? "	movzbq	%s,	%%rax\n"
						: "	movsbq	%s,	%%rax\n";
					break;
		case 2:		format = isUnsigned 
						? "	movzwq	%s,	%%rax\n"
						: "	movswq	%s,	%%rax\n";
					break;
		case 4:		format = isUnsigned
						? "	movl	%s,	%%eax\n"
						: "	movslq	%s,	%%rax\n";
					break;
		case 8:		format = "	movq	%s,	%%rax\n";	break;
		default:	format = "	movq	%s,	%%rax\n";	break;
	};
	return sngenf(strlen(format) + strlen(var->value.strVal) + 1, format, var->value);
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
	return sngenf(strlen(format) + strlen(offset) + 1, format, offset);
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
	return sngenf(strlen(format) + strlen(offset) + 1, format, offset);
}

static char* GenCast(ASTNode* node){
	const char* format = NULL;
	switch(GetTypeSize(node->type, node->cType)){
		case 1:		format = "%s	movzbq	%%al,	%%rax\n";	break;
		case 2:		format = "%s	movzwq	%%ax,	%%rax\n";	break;
		case 4:		format = "%s	movslq	%%eax,	%%rax\n";	break;
		case 8:		format = "%s	movq	%%rax,	%%rax\n";	break;
		default:	FatalM("Unhandled cast type size! (Internal @ gen.h)", __LINE__);
	}
	const char* expr = GenExpressionAsm(node->lhs);
	return sngenf(strlen(expr) + strlen(format) + 1, format, expr);
}

static const char* GenUnary(ASTNode* node){
	if(node->lhs == NULL)	FatalM("Expected expression after unary operator!", Line);
	const char* instr = NULL;
	switch(node->op){
		case A_Negate:				instr = "	neg		%rax\n";	break;
		case A_BitwiseComplement:	instr = "	not		%rax\n";	break;
		case A_Logicize:
			instr =
				"	cmp		$0,		%rax\n"
				"	movq	$0,		%rax\n"
				"	setne	%al\n"
			;
			break;
		case A_LogicalNot:
			instr =
				"	cmp		$0,		%rax\n"
				"	movq	$0,		%rax\n"
				"	sete	%al\n"
			;
			break;
	}
	return strjoin(GenExpressionAsm(node->lhs), instr);
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
	unresolvedPushes++;
	const char* rhs = GenExpressionAsm(node->rhs);
	unresolvedPushes--;
	int charCount = strlen(instr) + strlen(pushInstr) + strlen(popInstr) + strlen(lhs) + strlen(rhs) + 1;
	return sngenf(charCount, "%s%s%s%s%s", lhs, pushInstr, rhs, popInstr, instr);
}

static const char* GenRTLBinary(ASTNode* node){
	if(node->lhs == NULL)	FatalM("Expected factor before binary operator!", Line);
	if(node->rhs == NULL)	FatalM("Expected factor after binary operator!", Line);
	const char* pushInstr	= "	push	%rax\n";
	const char* popInstr	= "	pop		%rcx\n";
	const char* instr = NULL;
	bool isUnsigned = IsUnsigned(node->lhs->type) || IsUnsigned(node->rhs->type);
	switch(node->op){
		case A_Subtract:	instr = "	subq	%rcx,	%rax\n";	break;
		case A_LeftShift:	instr = "	shl		%rcx,	%rax\n";	break;
		case A_RightShift:
			instr = IsUnsigned(node->lhs->type) ? "	shr		%rcx,	%rax\n" : "	sar		%rcx,	%rax\n";
			break;
		case A_Divide:
			instr = isUnsigned ? "	movq	$0,	%rdx\n	divq	%rcx\n" : "	cqo\n""	idiv	%rcx\n";
			break;
		case A_Modulo:
			instr = isUnsigned
				? "	movq	$0,	%rdx\n	divq	%rcx\n	movq	%rdx,	%rax\n"
				: "	cqo\n""	idiv	%rcx\n""	movq	%rdx,	%rax\n";
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
	unresolvedPushes++;
	const char* lhs = GenExpressionAsm(node->lhs);
	unresolvedPushes--;
	const char* rhs = GenExpressionAsm(node->rhs);
	int charCount = strlen(instr) + strlen(pushInstr) + strlen(popInstr) + strlen(lhs) + strlen(rhs) + 1;
	return sngenf(charCount, "%s%s%s%s%s", rhs, pushInstr, lhs, popInstr, instr);
}

static char* GenRepeatingShortCircuitingOr(ASTNode* node){
	if(node->lhs == NULL)					FatalM("Got NULL as lhs of node! (Internal @ gen.h)", __LINE__);
	if(node->rhs == NULL)					FatalM("Got NULL as rhs of node! (Internal @ gen.h)", __LINE__);
	if(node->rhs->op != A_ExpressionList)	FatalM("Expected rhs of node to be A_ExpressionList! (Internal @ gen.h)", __LINE__);
	unresolvedPushes++;
	int endLabel = lVar++;
	char* Asm = _strdup(GenExpressionAsm(node->lhs));
	strapp(&Asm, "	pushq	%rax\n");
	ASTNodeList* list = node->rhs->list;
	int count = list->count;
	int localLabelPref = ++labelPref;
	char* caseStart = sngenf(intlen(localLabelPref) + 4, "%d1:\n", localLabelPref);
	const char* scFormat = 
		"	cmpq	%%rax,	(%%rsp)\n"
		"	jne		%d1f\n"				// Next case
		"	movq	$1,		%%rax\n"
		"	jmp		L%d\n"				// End
	;
	char* shortCircuit = sngenf(strlen(scFormat) + intlen(localLabelPref) + intlen(endLabel) + 1, scFormat, localLabelPref, endLabel);
	for(int i = 0; i < count; i++){
		strapp(&Asm, caseStart);
		strapp(&Asm, GenExpressionAsm(list->nodes[i]));
		strapp(&Asm, shortCircuit);
	}
	strapp(&Asm, caseStart);
	strapp(&Asm, "	movq	$0,		%rax\n");
	char* buffer = sngenf(intlen(endLabel) + 4, "L%d:\n", endLabel);
	strapp(&Asm, buffer);
	free(buffer);
	strapp(&Asm, "	subq	$8,		%rsp\n"); // Remove pushed item
	unresolvedPushes--;
	return Asm;
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
	labelPref++;
	return sngenf(strlen(format) + strlen(lhs) + (4 * intlen(labelPref)) + strlen(rhs) + 1, format, lhs, labelPref, labelPref, labelPref, rhs, labelPref);
}

static const char* GenTernary(ASTNode* node){
	if(node->lhs == NULL)	FatalM("Expected expression before ternary operator!", Line);
	if(node->rhs == NULL)	FatalM("Expected expression after ternary operator!", Line);
	const char* format;
	int charCount;
	if(node->mid == NULL){
		format = 
			"%s"
			"	cmp		$0,		%%rax\n"
			"	jne		%d1f\n"
			"%s"
			"%d1:\n"
		;
		charCount = strlen(format) + (2 * intlen(labelPref));
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
		charCount = strlen(format) + (4 * intlen(labelPref));
	}
	const char* rhs = GenExpressionAsm(node->rhs);
	const char* mid = node->mid == NULL ? "" : GenExpressionAsm(node->mid);
	const char* lhs = GenExpressionAsm(node->lhs);
	charCount += strlen(lhs) + strlen(mid) + strlen(rhs);
	labelPref++;
	return node->mid != NULL
		? sngenf(charCount, format, lhs, labelPref, mid, labelPref, labelPref, rhs, labelPref)
		: sngenf(charCount, format, lhs, labelPref, rhs, labelPref);
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
		return sngenf(strlen(format) + strlen(rhs) + strlen(offset) + 1, format, rhs, offset);
	}
	if(node->lhs->op != A_Dereference)	FatalM("Unsupported assignment target! (In gen.h)", __LINE__);
	const char* derefASM = GenExpressionAsm(node->lhs->lhs);
	const char* format = 
		"%s" // lhs
		"	push	%%rax\n"
		"%s" // rhs
		"	pop		%%rcx\n"
		"%s" // instr
	;
	const char* instr = NULL;
	switch(GetTypeSize(node->lhs->type, node->lhs->cType)){
		case 1:		instr = "	movb	%al,	(%rcx)\n";	break;
		case 4:		instr = "	movl	%eax,	(%rcx)\n";	break;
		case 8:		instr = "	movq	%rax,	(%rcx)\n";	break;
		default:	FatalM("Non-standard sizes not yet supported in assignments! (Internal @ gen.h)", __LINE__);
	}
	unresolvedPushes++;
	const char* innerASM = GenExpressionAsm(node->rhs);
	unresolvedPushes--;
	return sngenf(strlen(format) + strlen(derefASM) + strlen(innerASM) + strlen(instr) + 1, format, derefASM, innerASM, instr);
}

static const char* GenIncDec(ASTNode* node){
	if(node == NULL)										FatalM("Expected an AST node, got NULL instead! (In gen.h)", __LINE__);
	if(node->op != A_Increment && node->op != A_Decrement)	FatalM("Expected increment or decrement in expression! (In gen.h)", __LINE__);
	if(node->secondaryValue.intVal){
		if(node->secondaryValue.intVal < 0)	FatalM("Increment / decrement scale amount was negative! (Internal @ gen.h)", __LINE__);
		if(node->rhs == NULL)				FatalM("RHS of scaled increment was NULL! (Internal @ gen.h)", __LINE__);
		if(node->value.intVal)
			return GenCompoundAssignment(node->rhs);
		char* val = _strdup(GenExpressionAsm(node->lhs));
		strapp(&val, "	push	%rax\n");
		unresolvedPushes++;
		strapp(&val, GenCompoundAssignment(node->rhs));
		unresolvedPushes--;
		strapp(&val, "	pop		%rax\n");
		return val;
	}
	switch(node->lhs->op){
		case A_VarRef:{
			const char* move = NULL;
			const char* action =  NULL;
			switch(GetTypeSize(node->lhs->type, node->lhs->cType)){
				case 1:
					move = "	movb	%s,	%%al\n";
					action = (node->op == A_Increment) ? "	incb	%s\n" : "	decb	%s\n";	break;
					break;
				case 2:
					move = "	movw	%s,	%%ax\n";
					action = (node->op == A_Increment) ? "	incw	%s\n" : "	decw	%s\n";	break;
					break;
				case 4:
					move = "	movl	%s,	%%eax\n";
					action = (node->op == A_Increment) ? "	incl	%s\n" : "	decl	%s\n";	break;
					break;
				case 8:
					move = "	movq	%s,	%%rax\n";
					action = (node->op == A_Increment) ? "	incq	%s\n" : "	decq	%s\n";	break;
					break;
				default:	FatalM("Unhandled type size! (Internal @ gen.h)", __LINE__);
			}
			char* format = (node->value.intVal) ? strjoin(action, move) : strjoin(move, action);
			const char* id = node->lhs->value.strVal;
			SymEntry* var = FindVar(id, scope);
			if(var == NULL)	FatalM("Variable not defined!", Line);
			const char* location = var->value.strVal;
			return sngenf(strlen(format) + (2 * strlen(location)) + 1, format, location, location);
		}
		case A_Dereference:{
			ASTNode* innerNode = node->lhs->lhs;
			if(innerNode == NULL)	FatalM("Expected inner node, got NULL instead! (Internal @ gen.h)", __LINE__);
			const char* format = 
				"%s"	// ASM of dereference's lhs
				"	movq	%%rax,	%%rcx\n"	// clear %rax for return
				"%s"	// mov		OR inc/dec
				"%s"	// inc/dec	OR mov
			;
			const char* move = NULL;
			const char* action = NULL;
			switch(GetTypeSize(innerNode->type, innerNode->cType)){
				case 1:
					move	= "	movb	(%rcx),	%al\n";
					action	= (node->op == A_Increment) ? "	incb	(%rcx)\n" : "	decb	(%rcx)\n";
					break;
				case 2:
					move	= "	movw	(%rcx),	%ax\n";
					action	= (node->op == A_Increment) ? "	incw	(%rcx)\n" : "	decw	(%rcx)\n";
					break;
				case 4:
					move	= "	movl	(%rcx),	%eax\n";
					action	= (node->op == A_Increment) ? "	incl	(%rcx)\n" : "	decl	(%rcx)\n";
					break;
				case 8:
					move	= "	movq	(%rcx),	%rax\n";
					action	= (node->op == A_Increment) ? "	incq	(%rcx)\n" : "	decq	(%rcx)\n";
					break;
				default:	FatalM("Unhandled type size! (Internal @ gen.h)", __LINE__);
			}
			const char* inner = GenExpressionAsm(innerNode);
			int charCount = strlen(action) + strlen(move) + strlen(format) + strlen(inner) + 1;
			return (node->value.intVal)
				? sngenf(charCount, format, inner, action, move)
				: sngenf(charCount, format, inner, move, action);
		}
		default:
			FatalM("Unsupported lvalue in increment / decrement!", Line);
	}
}

static const char* GenCompoundAssignment(ASTNode* node){
	if(node == NULL)	FatalM("Expected an AST node, got NULL instead! (In gen.h)", __LINE__);
	const char* offset = NULL;
	const char* preface = GenExpressionAsm(node->rhs);
	switch(node->lhs->op){
		case A_Dereference:{
			char* buffer = _strdup(GenExpressionAsm(node->lhs->lhs));
			strapp(&buffer, "	push	%rax\n"); // Deref's addr => stack
			unresolvedPushes++;
			strapp(&buffer, preface);
			unresolvedPushes--;
			strapp(&buffer, "	pop		%r8\n"); // Load deref's addr => r8
			preface = buffer;
			offset = "(%r8)";
			break;
		}
		case A_VarRef:{
			const char* id = node->lhs->value.strVal;
			
			SymEntry* var = FindVar(id, scope);
			if(var == NULL)		FatalM("Variable not defined!", Line);
			offset = var->value.strVal;
			break;
		}
		default:			FatalM("Unsupported lvalue in compound assignment! (Internal @ gen.h)", __LINE__);
	}
	// val, op, mov -> offset
	char* format = NULL;
	const char* fb_format = NULL;
	const char* reg_a	= NULL;	const char* reg_c	= NULL;	const char* reg_d	= NULL;
	char opc			= '\0';	char opcl			= '\0';	char nxopcl			= '\0';
	switch(GetTypeSize(node->lhs->type, node->lhs->cType)){
		case 1:
			reg_a	= "%%al";	reg_c	= "%%cl";	reg_d	= "%%dl";
			opc		= 'b';		opcl	= 'b';		nxopcl	= 'w';
			break;
		case 2:
			reg_a	= "%%ax";	reg_c	= "%%cx";	reg_d	= "%%dx";
			opc		= 'w';		opcl	= 'w';		nxopcl	= 'd';
			break;
		case 4:
			reg_a	= "%%eax";	reg_c	= "%%ecx";	reg_d	= "%%edx";
			opc		= 'l';		opcl	= 'd';		nxopcl	= 'q';
			break;
		case 8:
			reg_a	= "%%rax";	reg_c	= "%%rcx";	reg_d	= "%%rdx";
			opc		= 'q';		opcl	= 'q';		nxopcl	= 'o';
			break;
		default:	FatalM("Unhandled type size in compound assignment generation! (Internal @ gen.h)", __LINE__);
	}
	switch(node->op){
		//region simple execution
		case A_AssignSum:			fb_format = "%%s	add%c	%s,	%%s\n	mov%c	%%s,	%s\n";		break;
		case A_AssignDifference:	fb_format = "%%s	sub%c	%s,	%%s\n	mov%c	%%s,	%s\n";		break;
		case A_AssignProduct:		fb_format = "%%s	imul%c	%%s,	%s\n	mov%c	%s,	%%s\n";		break;
		case A_AssignBitwiseAnd:	fb_format = "%%s	and%c	%s,	%%s\n	mov%c	%%s,	%s\n";		break;
		case A_AssignBitwiseXor:	fb_format = "%%s	xor%c	%s,	%%s\n	mov%c	%%s,	%s\n";		break;
		case A_AssignBitwiseOr:		fb_format = "%%s	or%c		%s,	%%s\n	mov%c	%%s,	%s\n";	break;
		//endregion simple execution
		case A_AssignQuotient:{
				const char* fb_format = 
					"%%s"						// preface
					"	mov%c	%s,	%s\n"		// opc, reg_a, reg_c
					"	mov%c	%%s,	%s\n"	// opc, reg_a
					"	c%c%c\n"				// opcl, nxopcl
					"	idiv%c	%s\n"			// opc, reg_c
					"	mov%c	%s,	%%s\n"		// opc, reg_a
				;
				int charCount =
					strlen(fb_format)
					+ sizeof(opcl)
					+ sizeof(nxopcl)
					+ (4 * sizeof(opc))
					+ (3 * strlen(reg_a))
					+ (2 * strlen(reg_c))
					+ 1
				;
				format = sngenf(charCount, fb_format, opc, reg_a, reg_c, opc, reg_a, opcl, nxopcl, opc, reg_c, opc, reg_a);
			}
			break;
		case A_AssignModulus:{
				const char* fb_format = 
					"%%s"						// preface
					"	mov%c	%s,	%s\n"		// opc, reg_a, reg_c
					"	mov%c	%%s,	%s\n"	// opc, reg_a
					"	c%c%c\n"				// opcl, nxopcl
					"	idiv%c	%s\n"			// opc, reg_c
					"	mov%c	%s,	%s\n"		// opc, reg_d, reg_a
					"	mov%c	%s,	%%s\n"		// opc, reg_a
				;
				int charCount = 
					strlen(fb_format)
					+ sizeof(opcl)
					+ sizeof(nxopcl)
					+ (3 * sizeof(opc))
					+ (4 * strlen(reg_a))
					+ (2 * strlen(reg_c))
					+ strlen(reg_d)
					+ 1
				;
				format = sngenf(charCount, fb_format, opc, reg_a, reg_c, opc, reg_a, opcl, nxopcl, opc, reg_c, opc, reg_d, reg_a, opc, reg_a);
			}
			break;
		case A_AssignLeftShift:{
				const char* fb_format = 
					"%%s"						// preface
					"	mov%c	%s,	%s\n"		// opc, reg_a, reg_c
					"	mov%c	%%s,	%s\n"	// opc, reg_a
					"	shl%c	%s,	%s\n"		// opc, reg_c, reg_a
					"	mov%c	%s,	%%s\n"		// opc, reg_a
				;
				int charCount =
					strlen(fb_format)
					+ (4 * sizeof(opc))
					+ (4 * strlen(reg_a))
					+ (2 * strlen(reg_c))
					+ 1
				;
				format = sngenf(charCount, fb_format, opc, reg_a, reg_c, opc, reg_a, opc, reg_c, reg_a, opc, reg_a);
			}
			break;
		case A_AssignRightShift:{
				const char* fb_format = NULL;
				if(IsUnsigned(node->lhs->type))
						fb_format =
						"%%s"						// preface
						"	mov%c	%s,	%s\n"		// opc, reg_a, reg_c
						"	mov%c	%%s,	%s\n"	// opc, reg_a
						"	shr%c	%s, %s\n"		// opc, reg_c, reg_a
						"	mov%c	%s,	%%s\n"		// opc, reg_a
					;
				else
					fb_format =
						"%%s"						// preface
						"	mov%c	%s,	%s\n"		// opc, reg_a, reg_c
						"	mov%c	%%s,	%s\n"	// opc, reg_a
						"	sar%c	%s, %s\n"		// opc, reg_c, reg_a
						"	mov%c	%s,	%%s\n"		// opc, reg_a
					;
				int charCount =
					strlen(fb_format)
					+ (4 * sizeof(opc))
					+ (4 * strlen(reg_a))
					+ (2 * strlen(reg_c))
					+ 1
				;
				format = sngenf(charCount, fb_format, opc, reg_a, reg_c, opc, reg_a, opc, reg_c, reg_a, opc, reg_a);
			}
			break;
		default:					FatalM("Unexpected operation in compound assignment! (In gen.h)", __LINE__);
	}
	// If Format is NULL, try to build it using the common format builder
	if(format == NULL){
		if(fb_format == NULL)		FatalM("Got NULL format and NULL format builder! (Internal @ gen.h)", __LINE__);
		int charCount = strlen(fb_format) + (2 * sizeof(opc)) + (2 * strlen(reg_a)) + 1;
		format = sngenf(charCount, fb_format, opc, reg_a, opc, reg_a);
	}
	int charCount = strlen(format) + strlen(preface) + (2 * strlen(offset)) + 1;
	char* str = sngenf(charCount, format, preface, offset, offset);
	free(format);
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
		case A_BuiltinCall:			return GenBuiltinCall(node);
		case A_Dereference:			return GenDereference(node);
		case A_AddressOf:			return GenAddressOf(node);
		case A_Cast:				return GenCast(node);
		case A_ExpressionList:		return GenExpressionList(node);
		case A_RepeatLogicalOr:		return GenRepeatingShortCircuitingOr(node);
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
		case A_Logicize:
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
		case A_RawASM:				return node->value.strVal;
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
	return sngenf(strlen(innerAsm) + strlen(format) + 1, format, innerAsm);
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
	labelPref++;
	return (node->mid != NULL)
		? sngenf(charCount, format, lhs, labelPref, rhs, labelPref, labelPref, mid, labelPref)
		: sngenf(charCount, format, lhs, labelPref, rhs, labelPref);
}

static char* GenDeclaration(ASTNode* node){
	if(node == NULL)											FatalM("Expected an AST Node, got NULL instead", Line);
	if(node->op != A_Declare)									FatalM("Expected declaration!", Line);
	SymEntry* existing = FindLocalVar(node->value.strVal, scope);
	if(existing != NULL && existing->sValue.intVal != C_Extern)	FatalM("Local variable redeclaration!", Line);
	char* varLoc = malloc(10 * sizeof(char));
	char* expr = _strdup("");
	if(!scope){
		// Global variable
		free(varLoc);
		const char* id = node->value.strVal;
		varLoc = _strdup(id);
		strapp(&varLoc, "(%rip)");
		InsertVar(node->value.strVal, varLoc, node->type, node->cType, (StorageClass)node->secondaryValue.intVal, scope);
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
		if(node->sClass == C_Extern)
			return calloc(1, sizeof(char));
		if(node->lhs == NULL){
			DbLnkList* bss = MakeDbLnkList((void*)id, NULL, bss_vars);
			bss_vars->prev = bss;
			bss_vars = bss;
			return calloc(1, sizeof(char));
		}
		if (node->lhs->op != A_LitInt)
			FatalM("Non-constant expression used in global variable declaration!", Line);
		const char* format = NULL;
		if(node->sClass == C_Static){
			switch(GetTypeSize(node->type, node->cType)){
				case 1:		format = "%s:\n	.byte	%lld\n"; break;
				case 4:		format = "%s:\n	.long	%lld\n"; break;
				case 8:		format = "%s:\n	.quad	%lld\n"; break;
				default:	FatalM("Unsupported type size! (Internal @ gen.h)", __LINE__);
			}
			int charCount = strlen(format) + strlen(id) + intlen(node->lhs->value.intVal) + 1;
			char* buffer = sngenf(charCount, format, id, node->lhs->value.intVal);
			strapp(&data_section, buffer);
			free(buffer);
			return calloc(1, sizeof(char));
		}
		switch(GetTypeSize(node->type, node->cType)){
			case 1:		format = "	.globl %s\n%s:\n	.byte	%lld\n"; break;
			case 4:		format = "	.globl %s\n%s:\n	.long	%lld\n"; break;
			case 8:		format = "	.globl %s\n%s:\n	.quad	%lld\n"; break;
			default:	FatalM("Unsupported type size! (Internal @ gen.h)", __LINE__);
		}
		const int charCount = strlen(format) + (2 * strlen(id)) + intlen(node->lhs->value.intVal) + 1;
		char* buffer = sngenf(charCount, format, id, id, node->lhs->value.intVal);
		strapp(&data_section, buffer);
		free(buffer);
		return calloc(1, sizeof(char));
	}
	{
		int n = stackIndex[scope] -= GetTypeSize(node->type, node->cType);
		free(varLoc);
		varLoc = sngenf(7 + intlen(n), "%d(%%rbp)", n);
	}
	if(node->lhs != NULL){
		const char* rhs = GenExpressionAsm(node->lhs);
		const char* format = "%s	movq	%%rax,	%s\n";
		switch(GetTypeSize(node->type, node->cType)){
			case 1:		format = "%s	movb	%%al,	%s\n";	break;
			case 4:		format = "%s	movl	%%eax,	%s\n";	break;
			case 8:		break;
			default:	break;
		}
		free(expr);
		expr = sngenf(strlen(format) + strlen(rhs) + strlen(varLoc) + 4, format, rhs, varLoc);
	}
	InsertVar(node->value.strVal, varLoc, node->type, node->cType, C_Default, scope);
	return expr;
}

static const char* GenWhileLoop(ASTNode* node){
	if(node == NULL)							FatalM("Expected an AST Node, got NULL instead", Line);
	if(node->op != A_While && node->op != A_Do)	FatalM("Expected While or Do-While loop!", Line);
	const char* condition	= GenExpressionAsm(node->lhs);
	int localLabelPref = labelPref++;
	int lbreak =	labels.lbreak;
	int lcontinue =	labels.lcontinue;
	labels.lbreak		= (localLabelPref * 10) + 9;
	labels.lcontinue	= (localLabelPref * 10) + 8;
	const char* action		= GenStatementAsm(node->rhs);
	labels.lbreak		= lbreak;
	labels.lcontinue	= lcontinue;
	char* buffer;
	if(node->op == A_Do){
		const char* format =
			"%d0:\n"
			"0:\n"
			"%s"						// Action
			"%d8:\n"
			"%s"						// Condition
			"	cmp		$0,		%%rax\n"
			"	jne		%d0b\n"
			"%d9:\n"
			"9:\n"
		;
		int charCount = strlen(condition) + strlen(action) + strlen(format) + (4 * intlen(localLabelPref)) + 1;
		return sngenf(charCount, format, localLabelPref, action, localLabelPref, condition, localLabelPref, localLabelPref);
	}
	const char* format = 
		"%d0:\n"
		"0:\n"
		"%s"						// Condition
		"	cmp		$0,		%%rax\n"
		"	je		%d9f\n"
		"%s"						// Action
		"%d8:\n"
		"	jmp		%d0b\n"
		"%d9:\n"
		"9:\n"
	;
	int charCount = strlen(condition) + strlen(action) + strlen(format) + (5 * intlen(localLabelPref)) + 1;
	return sngenf(charCount, format, localLabelPref, condition, localLabelPref, action, localLabelPref, localLabelPref, localLabelPref);
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
	const char* condition	= node->lhs->mid == NULL ? NULL : GenExpressionAsm(node->lhs->mid);
	const char* modifier	= node->lhs->rhs == NULL ? "" : GenExpressionAsm(node->lhs->rhs);
	int lbreak			= labels.lbreak;
	int lcontinue		= labels.lcontinue;
	int localLabelPref	= labelPref++;
	labels.lbreak		= (localLabelPref * 10) + 9;
	labels.lcontinue	= localLabelPref * 10 + 8;
	const char* action	= GenStatementAsm(node->rhs);
	labels.lbreak		= lbreak;
	labels.lcontinue	= lcontinue;
	const char* format =
		"%s"				// Allocate Stack Space for vars
		"%s"				// Initializer
		"%d0:\n"
		"0:\n"
		"%s"				// Condition
		"%s"				// Action
		"%d8:\n"
		"8:\n"
		"%s"				// Modifier
		"	jmp		%d0b\n"
		"%d9:\n"
		"9:\n"
		"%s"				// Deallocate Stack Space for vars
	;
	// Beyond this point, don't generate any more ASM using other functions
	int stackSize = align(GetLocalVarCount(scope) * 8, 16);
	char* stackAlloc = malloc(1 * sizeof(char));
	*stackAlloc = '\0';
	char* stackDealloc = malloc(1 * sizeof(char));
	*stackDealloc = '\0';
	if(stackSize){
		free(stackAlloc);
		free(stackDealloc);
		const char* format = "	sub		$%d,		%%rsp\n";
		stackAlloc = sngenf(strlen(format) + intlen(stackSize) + 1, format, stackSize);
		format = "	add		$%d,		%%rsp\n";
		stackDealloc = sngenf(strlen(format) + intlen(stackSize) + 1, format, stackSize);
	}
	if(condition != NULL){
		const char* condCheck = 
			"%s"
			"	cmp		$0,		%%rax\n"
			"	je		%d9f\n"
		;
		int charCount = strlen(condCheck) + strlen(condition) + intlen(localLabelPref) + 1;
		condition = sngenf(charCount, condCheck, condition, localLabelPref);
	}
	int charCount = strlen(stackAlloc) + strlen(initializer) + strlen(modifier) + strlen(format) + strlen(condition) + strlen(action) + (4 * intlen(localLabelPref)) + strlen(stackDealloc) + 1;
	char* str = sngenf(charCount, format, stackAlloc, initializer, localLabelPref, condition, action, localLabelPref, modifier, localLabelPref, localLabelPref, stackDealloc);
	free((void*)condition);
	free(stackAlloc);
	ExitScope();
	return str;
}
int switchCount = 0;
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
		"%d9:\n"							// labelpref
	;
	char* casesASM = calloc(1, sizeof(char));
	const char* declLabelFormat =
		"L%d:\n"
		"	.quad	%d\n" // Count
	;
	char* tableASM = calloc(1, sizeof(char));
	const char* exprAsm = GenExpressionAsm(node->lhs);

	int localLabelPref = labelPref++;
	int lbreak = labels.lbreak;
	labels.lbreak = (localLabelPref * 10) + 9;
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
		int charCount = strlen(jmpFrmt) + intlen(caseValue[i]) + intlen(caseLabel[i]) + 1;
		char* jmpASM = sngenf(charCount, jmpFrmt, caseValue[i], caseLabel[i]);
		strapp(&tableASM, jmpASM);
		free(jmpASM);
	}
	labels.lbreak = lbreak;
	int tpCharCount = strlen(declLabelFormat) + intlen(lJmp) + intlen(caseCount) + 1;
	char* tablePreASM = sngenf(tpCharCount, declLabelFormat, lJmp, caseCount);
	strapp(&tablePreASM, tableASM);
	free(tableASM);
	tableASM = tablePreASM;
	const char* defFrmt = "	.quad	L%d\n";
	int djCharCount = strlen(jmpFrmt) + intlen(lDef) + 1;
	char* defJmpLbl = sngenf(djCharCount, defFrmt, lDef);
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
		+ intlen(localLabelPref)
		+ 1
	;
	char* ret = sngenf(charCount, format, exprAsm, lTop, casesASM, lEnd, tableASM, lTop, lJmp, lEnd, localLabelPref);
	free(casesASM);
	free(tableASM);
	return ret;
}

static const char* GenContinue(ASTNode* node){
	if(labels.lcontinue == -1)	FatalM("A 'continue' statement may only be used inside of a loop!", Line);
	const char* format = "	jmp		%df\n";
	return sngenf(strlen(format) + intlen(labels.lcontinue) + 1, format, labels.lcontinue);
}

static char* GenBreak(ASTNode* node){
	if(labels.lbreak == -1)		FatalM("A 'break' statement may only be used inside of a switch or loop!", Line);
	const char* format = "	jmp		%df\n";
	return sngenf(strlen(format) + intlen(labels.lbreak) + 1, format, labels.lbreak);
}

static const char* GenBlockAsm(ASTNode* node){
	if(node == NULL)				FatalM("Expected an AST node, got NULL instead.", Line);
	if(node->op != A_Block)			FatalM("Expected function at top level statement!", Line);
	if(!node->list)					FatalM("Expected an ASTNodeList* member 'list'! (In gen.h)", __LINE__);
	if(!node->list->count)			return "";
	EnterScope();
	const char* statementAsm = GenerateAsmFromList(node->list);
	int stackSize = align(GetLocalStackSize(scope), 16);
	char* stackAlloc = calloc(1, sizeof(char));
	char* stackDealloc = calloc(1, sizeof(char));
	if(stackSize){
		free(stackAlloc);
		free(stackDealloc);
		const char* format = "	subq	$%d,		%%rsp\n";
		stackAlloc = sngenf(strlen(format) + 6, format, stackSize);
		format = "	addq	$%d,		%%rsp\n";
		stackDealloc = sngenf(strlen(format) + 6, format, stackSize);
	}
	char* str = strjoin(stackAlloc, statementAsm);
	strapp(&str, stackDealloc);
	free(stackAlloc);
	free(stackDealloc);
	ExitScope();
	return str;
}

static char* GenStructDecl(ASTNode* node){
	InsertStruct(node->value.strVal, MakeCompMembers(node->list));
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
	Parameter* prevParams = curFuncParams;
	curFuncParams = params;
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
	char* globl = calloc(1, sizeof(char));
	if(node->sClass != C_Static){
		free(globl);
		const char* format = "	.globl	%s\n";
		int charCount = strlen(format) + strlen(node->value.strVal) + 1;
		globl = sngenf(charCount, format, node->value.strVal);
	}
	const char* format = 
		"%s"						// Global Identifier (If applicable)
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
		char* paramPos = CalculateParamPosition(i, P_MODE_LOCAL);
		char* varLoc = NULL;
		if(i > 3)
			varLoc = paramPos;
		else {
			int offset = stackIndex[scope] -= 8;
			const char* const format = "%d(%%rbp)";
			varLoc = sngenf(intlen(offset) + strlen(format) + 1, format, offset);
		}
		InsertVar(params->id, varLoc, params->type, params->cType, C_Default, scope);
		const char* const format = "	movq	%s,	%s\n";
		const int charCount = strlen(format) + strlen(varLoc) + strlen(paramPos) + 1;
		char* buffer = sngenf(charCount, format, paramPos, varLoc);
		if(i < 4)
			strapp(&paramPlacement, buffer);
		free(buffer);
		params = params->prev;
	}
	char* stackAlloc = calloc(1, sizeof(char));
	char* stackDealloc = calloc(1, sizeof(char));
	if(paramCount){
			const char* format = "	subq	$%d,	%%rsp\n";
			const int allocSize = paramCount * 8;
			stackAlloc = sngenf(strlen(format) + intlen(allocSize) + 1, format, allocSize);
			format = "	addq	$%d,	%%rsp\n";
			stackDealloc = sngenf(strlen(format) + intlen(allocSize) + 1, format, allocSize);
	}
	const char* statementAsm = GenBlockAsm(node->lhs);
	int charCount =
		strlen(globl)						// Global Identifier
		+ strlen(node->value.strVal)		// Identifier
		+ strlen(stackAlloc)				// Stack Allocation ASM
		+ strlen(paramPlacement)			// Parameter Placement ASM
		+ strlen(statementAsm)				// Inner ASM
		+ strlen(format)					// format
		+ strlen(stackDealloc)				// Stack Deallocation ASM
		+ 1									// \0
	;
	char* str = sngenf(charCount, format, globl, node->value.strVal, stackAlloc, paramPlacement, statementAsm, stackDealloc);
	free(paramPlacement);
	free(stackAlloc);
	if(node->sClass != C_Static)	free(globl);
	if(paramCount)					ExitScope();
	curFuncParams = prevParams;
	return str;
}

static const char* GenerateAsmFromList(ASTNodeList* list){
	if(list->count < 1)	return "";
	const char* generated = NULL;
	char* buffer = calloc(1, sizeof(char));
	int i = 0;
	while(i < list->count){
		ASTNode* node = list->nodes[i];
		switch(node->op){
			case A_Function:	generated = GenFunctionAsm(node);	break;
			default:			generated = GenStatementAsm(node);	break;
		}
		strapp(&buffer, generated);
		i++;
	}
	return buffer;
}

char* GenerateAsm(ASTNodeList* node){
	labels.lbreak = -1;
	labels.lcontinue = -1;
	data_section = calloc(1, sizeof(char));
	bss_vars = MakeDbLnkList("", NULL, NULL);
	char* bss_section = calloc(1, sizeof(char));
	char* Asm = _strdup(GenerateAsmFromList(node));
	if(USE_SUB_SWITCH){
		const char* switchPreamble =
			"switch:\n"
			"	pushq	%rsi\n"			// Save %rsi
			"	movq	%rdx,	%rsi\n"	// Base of jump table => %rsi
			"	movq	%rax,	%r8\n"	// Expression Value => %r8
			"	cld\n"					// Clear direction flag
			"	lodsq\n"				// Case Count => %rax AND increment %rsi
			"	movq	%rax,	%rcx\n"	// Case Count => %rcx
			"	cmp		$0,		%rcx\n"	// Check if there are 0 cases except default
			"	jne		1f\n"			// If cases exist, enter start of loop
			"	inc		%rcx\n"			// Else, set cases to 1
			"	jmp		2f\n"			// Then jump to end of loop
			"1:\n"						// Start of loop
			"	lodsq\n"				// Case Value => %rax
			"	movq	%rax,	%rdx\n"	// Case Value => %rdx
			"	lodsq\n"				// Case Label => %rax
			"	cmpq	%rdx,	%r8\n"	// Case Value <=> Expression Value
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
		char* buffer = sngenf(dslen + 1, format, data_section);
		free(data_section);
		data_section = buffer;
	}
	if(bss_vars->next != NULL){
		bss_section = _strdup("	.bss\n	.align	16\n");
		const char* const format =
			"%s" // bss_section
			"	.globl	%s\n" // id - Any bss variable is global by definition
			"%s:\n" // id
			"	.zero	%d\n" // size
		;
		for(DbLnkList* bss = bss_vars; bss != NULL; bss = bss->next){
			if(streq(bss->val, ""))	continue;
			SymEntry* var = FindVar(bss->val, 0);
			if(var == NULL)	FatalM("Failed to find global variable! (In gen.h)", __LINE__);
			int charCount = strlen(bss_section) + 2*strlen(bss->val) + strlen(format) + 1;
			char* buffer = sngenf(charCount, format, bss_section, bss->val, bss->val, GetTypeSize(var->type, var->cType));
			free(bss_section);
			bss_section = buffer;
			if(bss->prev != NULL)
				free(bss->prev);
		}
	}
	char* buffer = _strdup(data_section);
	strapp(&buffer, bss_section);
	strapp(&buffer, "	.text\n");
	strapp(&buffer, Asm);
	free(data_section);
	free(bss_section);
	free(Asm);
	return buffer;
}