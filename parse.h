#ifndef PARSE_INCLUDED
#define PARSE_INCLUDED

#include "types.h"
#include "defs.h"

ASTNode* ParseExpression();
ASTNode* ParseStatement();
ASTNode* ParseBlock();


ASTNode* ParseFactor(){
	Token* tok = GetToken();
	ASTNode* ret;
	switch(tok->type){
		case T_LitInt:		return MakeASTLeaf(A_LitInt, FlexInt(tok->value.intVal));
		case T_Minus:		return MakeASTUnary(A_Negate,				ParseFactor(),	FlexNULL());
		case T_Bang:		return MakeASTUnary(A_LogicalNot,			ParseFactor(),	FlexNULL());
		case T_Tilde:		return MakeASTUnary(A_BitwiseComplement,	ParseFactor(),	FlexNULL());
		case T_Identifier:	return MakeASTLeaf(A_VarRef, FlexStr(tok->value.strVal));
		case T_Semicolon:	ungetc(';', fptr); return MakeASTLeaf(A_Undefined, FlexNULL());
		default:			FatalM("Invalid expression!", Line);
	}
}

ASTNode* ParseTerm(){
	ASTNode* lhs = ParseFactor();
	Token* tok = PeekToken();
	while(tok->type == T_Asterisk || tok->type == T_Divide || tok->type == T_Percent){
		GetToken();
		switch (tok->type){
			case T_Asterisk:
				lhs = MakeASTBinary(A_Multiply,	lhs,	ParseFactor(),	FlexNULL());
				break;
			case T_Divide:
				lhs = MakeASTBinary(A_Divide,	lhs,	ParseFactor(),	FlexNULL());
				break;
			case T_Percent:
				lhs = MakeASTBinary(A_Modulo,	lhs,	ParseFactor(),	FlexNULL());
				break;
		}
		tok = PeekToken();
	}
	return lhs;
}

ASTNode* ParseAdditiveExpression(){
	ASTNode* lhs = ParseTerm();
	Token* tok = PeekToken();
	while(tok->type == T_Plus || tok->type == T_Minus){
		GetToken();
		switch (tok->type){
			case T_Plus:
				lhs = MakeASTBinary(A_Add,		lhs,	ParseTerm(),	FlexNULL());
				break;
			case T_Minus:
				lhs = MakeASTBinary(A_Subtract,	lhs,	ParseTerm(),	FlexNULL());
				break;
		}
		tok = PeekToken();
	}
	return lhs;
}

ASTNode* ParseBitShiftExpression(){
	ASTNode* lhs = ParseAdditiveExpression();
	Token* tok = PeekToken();
	while(tok->type == T_DoubleLess || tok->type == T_DoubleGreater){
		GetToken();
		switch(tok->type){
			case T_DoubleLess:		lhs = MakeASTBinary(A_LeftShift,	lhs,	ParseAdditiveExpression(),	FlexNULL());	break;
			case T_DoubleGreater:	lhs = MakeASTBinary(A_RightShift,	lhs,	ParseAdditiveExpression(),	FlexNULL());	break;
		}
		tok = PeekToken();
	}
	return lhs;
}

ASTNode* ParseRelationalExpression(){
	ASTNode* lhs = ParseBitShiftExpression();
	Token* tok = PeekToken();
	while(tok->type == T_Less || tok->type == T_Greater || tok->type == T_LessEqual || tok->type == T_GreaterEqual){
		GetToken();
		switch(tok->type){
			case T_Less:			lhs = MakeASTBinary(A_LessThan,		lhs,	ParseBitShiftExpression(),	FlexNULL());	break;
			case T_Greater:			lhs = MakeASTBinary(A_GreaterThan,	lhs,	ParseBitShiftExpression(),	FlexNULL());	break;
			case T_LessEqual:		lhs = MakeASTBinary(A_LessOrEqual,	lhs,	ParseBitShiftExpression(),	FlexNULL());	break;
			case T_GreaterEqual:	lhs = MakeASTBinary(A_GreaterOrEqual,	lhs,	ParseBitShiftExpression(),	FlexNULL());	break;
		}
		tok = PeekToken();
	}
	return lhs;
}

ASTNode* ParseEqualityExpression(){
	ASTNode* lhs = ParseRelationalExpression();
	Token* tok = PeekToken();
	while(tok->type == T_DoubleEqual || tok->type == T_BangEqual){
		GetToken();
		switch(tok->type){
			case T_DoubleEqual:		lhs = MakeASTBinary(A_EqualTo,	lhs,	ParseRelationalExpression(),	FlexNULL());	break;
			case T_BangEqual:		lhs = MakeASTBinary(A_NotEqualTo,	lhs,	ParseRelationalExpression(),	FlexNULL());	break;
		}
		tok = PeekToken();
	}
	return lhs;
}

ASTNode* ParseBitwiseAndExpression(){
	ASTNode* lhs = ParseEqualityExpression();
	Token* tok = PeekToken();
	while(tok->type == T_Ampersand){
		GetToken();
		lhs = MakeASTBinary(A_BitwiseAnd,	lhs,	ParseEqualityExpression(),	FlexNULL());
		tok = PeekToken();
	}
	return lhs;
}

ASTNode* ParseBitwiseXorExpression(){
	ASTNode* lhs = ParseBitwiseAndExpression();
	Token* tok = PeekToken();
	while(tok->type == T_Caret){
		GetToken();
		lhs = MakeASTBinary(A_BitwiseXor,	lhs,	ParseBitwiseAndExpression(),	FlexNULL());
		tok = PeekToken();
	}
	return lhs;
}

ASTNode* ParseBitwiseOrExpression(){
	ASTNode* lhs = ParseBitwiseXorExpression();
	Token* tok = PeekToken();
	while(tok->type == T_Pipe){
		GetToken();
		lhs = MakeASTBinary(A_BitwiseOr,	lhs,	ParseBitwiseXorExpression(),	FlexNULL());
		tok = PeekToken();
	}
	return lhs;
}

ASTNode* ParseLogicalAndExpression(){
	ASTNode* lhs = ParseBitwiseOrExpression();
	Token* tok = PeekToken();
	while(tok->type == T_DoubleAmpersand){
		GetToken();
		lhs = MakeASTBinary(A_LogicalAnd,	lhs,	ParseBitwiseOrExpression(),	FlexNULL());
		tok = PeekToken();
	}
	return lhs;
}

ASTNode* ParseLogicalOrExpression(){
	ASTNode* lhs = ParseLogicalAndExpression();
	Token* tok = PeekToken();
	while(tok->type == T_DoublePipe){
		GetToken();
		lhs = MakeASTBinary(A_LogicalOr,	lhs,	ParseBitwiseOrExpression(),	FlexNULL());
		tok = PeekToken();
	}
	return lhs;
}

ASTNode* ParseConditionalExpression(){
	ASTNode* condition = ParseLogicalOrExpression();
	if(PeekToken()->type != T_Question)		return condition;
	GetToken();
	ASTNode* then = PeekToken()->type == T_Colon ? NULL : ParseExpression();
	if(GetToken()->type != T_Colon)			FatalM("Expected colon ':' in conditional expression!", Line);
	ASTNode* otherwise = ParseConditionalExpression();
	return MakeASTNode(A_Ternary, condition, then, otherwise, FlexNULL());
}

ASTNode* ParseAssignmentExpression(){
	ASTNode* lhs = ParseConditionalExpression();
	if(lhs == NULL)							FatalM("Got NULL instead of expression! (In parse.h)", __LINE__);
	if(lhs->op != A_VarRef || PeekToken()->type != T_Equal)
		return lhs;
	if(GetToken()->type != T_Equal)			FatalM("Expected '=' in assignment!", Line);
	ASTNode* rhs = ParseExpression();
	if(lhs == NULL)							FatalM("Expected expression!", Line);
	return MakeASTBinary(A_Assign, lhs, rhs, FlexNULL());
}

ASTNode* ParseExpression(){
	return ParseAssignmentExpression();
}

ASTNode* ParseReturnStatement(){
	if(GetToken()->type != T_Return)		FatalM("Invalid statement; Expected return.", Line);
	ASTNode* expr = ParseExpression();
	if(GetToken()->type != T_Semicolon)		FatalM("Invalid statement; Expected semicolon.", Line);
	return MakeASTUnary(A_Return, expr, FlexNULL());
}

ASTNode* ParseDeclaration(){
	Token* tok = GetToken();
	if(tok->type != T_Type)			FatalM("Expected typename!", Line);
	const char* type = tok->value.strVal;
	tok = GetToken();
	if(tok->type != T_Identifier)	FatalM("Expected identifier!", Line);
	const char* id = tok->value.strVal;
	if(PeekToken()->type != T_Equal)	return MakeASTNodeExtended(A_Declare, NULL, NULL, NULL, FlexStr(id), FlexStr(type));
	GetToken();
	ASTNode* expr = ParseExpression();
	return MakeASTNodeExtended(A_Declare, expr, NULL, NULL, FlexStr(id), FlexStr(type));
}

ASTNode* ParseIfStatement(){
	if(GetToken()->type != T_If)			FatalM("Expected 'if' to begin if statement!", Line);
	if(GetToken()->type != T_OpenParen)		FatalM("Expected open parenthesis '(' in if statement!", Line);
	ASTNode* condition = ParseExpression();
	if(GetToken()->type != T_CloseParen)	FatalM("Expected close parenthesis ')' in if statement!", Line);
	ASTNode* then = ParseStatement();
	if(PeekToken()->type != T_Else)
		return MakeASTBinary(A_If, condition, then, FlexNULL());
	GetToken();
	ASTNode* otherwise = ParseStatement();
	return MakeASTNode(A_If, condition, otherwise, then, FlexNULL());
}

ASTNode* ParseStatement(){
	Token* tok = PeekToken();
	switch(tok->type){
		case T_Return:		return ParseReturnStatement();
		case T_OpenBrace:	return ParseBlock();
		case T_If:			return ParseIfStatement();
		default:			break;
	}
	ASTNode* expr = NULL;
	switch(tok->type){
		case T_Type:		expr = ParseDeclaration();	break;
		default:			expr = ParseExpression();	break;
	}
	if(GetToken()->type != T_Semicolon)		FatalM("Expected semicolon!", Line);
	return expr;
}

ASTNode* ParseBlock(){
	if(GetToken()->type != T_OpenBrace)		FatalM("Invalid block declaration; Expected open brace '{'.", Line);
	ASTNodeList* list = MakeASTNodeList();
	while(PeekToken()->type != T_CloseBrace)
		AddNodeToASTList(list, ParseStatement());
	if(GetToken()->type != T_CloseBrace)	FatalM("Invalid block declaration; Expected close brace '}'.", Line);
	return MakeASTList(A_Block, list, FlexNULL());
}

ASTNode* ParseFunction(){
	if(GetToken()->type != T_Type)			FatalM("Invalid function declaration; Expected typename.", Line);
	Token* tok = GetToken();
	if(tok->type != T_Identifier)			FatalM("Invalid function declaration; Expected identifier.", Line);
	int idLen = strlen(tok->value.strVal) + 1;
	char* idStr = malloc(idLen * sizeof(char));
	strncpy(idStr,tok->value.strVal, idLen);
	if(GetToken()->type != T_OpenParen)		FatalM("Invalid function declaration; Expected open parenthesis '('.", Line);
	if(GetToken()->type != T_CloseParen)	FatalM("Invalid function declaration; Expected close parenthesis ')'.", Line);
	if(PeekToken()->type != T_OpenBrace)	FatalM("Invalid function declaration; Expected open brace '{'.", Line);
	ASTNode* block = ParseBlock();
	return MakeASTUnary(A_Function, block, FlexStr(idStr));
	// return MakeASTUnary(A_Function, stmt, 0, identifier->value.strVal);
}

#endif