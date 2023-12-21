#include <string.h>

#include "defs.h"
#include "types.h"
#include "symTable.h"

// static int unresolvedPushes = 0;
// static int labelPref = 9;
// static char* data_section;
// static DbLnkList* bss_vars = NULL;
// static Parameter* curFuncParams = NULL;

// static const char* GenExpressionAsm(ASTNode* node);
// static const char* GenStatementAsm(ASTNode* node);
// static const char* GenerateAsmFromList(ASTNodeList* list);
// static const char* GenCompoundAssignment(ASTNode* node);

extern struct {
	int lbreak;
	int lcontinue;
} labels;

enum paramMode {
	P_MODE_DEFAULT	= 1,
	P_MODE_LOCAL	= 2,
	P_MODE_SHADOW	= 3,
};
// static char* CalculateParamPosition(int n, enum paramMode mode);
// static const char* GenLitInt(ASTNode* node);
// static char* GenLitStr(ASTNode* node);
// static char* GenExpressionList(ASTNode* node);
// static const char* GenFuncCall(ASTNode* node);
// static char* GenBuiltinCall(ASTNode* node);
// static const char* GenVarRef(ASTNode* node);
// static char* GenAddressOf(ASTNode* node);
// static char* GenDereference(ASTNode* node);
// static char* GenCast(ASTNode* node);
// static const char* GenUnary(ASTNode* node);
// static const char* GenLTRBinary(ASTNode* node);
// static const char* GenRTLBinary(ASTNode* node);
// static char* GenRepeatingShortCircuitingOr(ASTNode* node);
// static const char* GenShortCircuiting(ASTNode* node);
// static const char* GenTernary(ASTNode* node);
// static const char* GenAssignment(ASTNode* node);
// static const char* GenIncDec(ASTNode* node);
// static const char* GenCompoundAssignment(ASTNode* node);
// static const char* GenExpressionAsm(ASTNode* node);
// static const char* GenReturnStatementAsm(ASTNode* node);
// static const char* GenIfStatement(ASTNode* node);
// static char* GenDeclaration(ASTNode* node);
// static const char* GenWhileLoop(ASTNode* node);
// static const char* GenForLoop(ASTNode* node);
extern int switchCount;
// static char* GenSwitch(ASTNode* node);
// static const char* GenContinue(ASTNode* node);
// static char* GenBreak(ASTNode* node);
// static const char* GenBlockAsm(ASTNode* node);
// static char* GenStructDecl(ASTNode* node);
// static const char* GenStatementAsm(ASTNode* node);
// static const char* GenFunctionAsm(ASTNode* node);
// static const char* GenerateAsmFromList(ASTNodeList* list);
char* GenerateAsm(ASTNodeList* node);
