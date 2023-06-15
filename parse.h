#ifndef PARSE_INCLUDED
#define PARSE_INCLUDED

#include "types.h"
#include "defs.h"

ASTNode* ParseExpression();


ASTNode* ParseFactor(){
	Token* tok = GetToken();
	switch(tok->type){
		case T_LitInt:	return MakeASTLeaf(A_LitInt, tok->value.intVal, NULL);
		case T_Minus:	return MakeASTUnary(A_Negate,				ParseFactor(),	0,	NULL);
		case T_Bang:	return MakeASTUnary(A_LogicalNot,			ParseFactor(),	0,	NULL);
		case T_Tilde:	return MakeASTUnary(A_BitwiseComplement,	ParseFactor(),	0,	NULL);
		default:		FatalM("Invalid expression!", Line);
	}
}

ASTNode* ParseTerm(){
	ASTNode* lhs = ParseFactor();
	Token* tok = PeekToken();
	while(tok->type == T_Asterisk || tok->type == T_Divide || tok->type == T_Percent){
		GetToken();
		switch (tok->type){
			case T_Asterisk:
				lhs = MakeASTNode(A_Multiply,	lhs,	ParseFactor(),	0,	NULL);
				break;
			case T_Divide:
				lhs = MakeASTNode(A_Divide,	lhs,	ParseFactor(),	0,	NULL);
				break;
			case T_Percent:
				lhs = MakeASTNode(A_Modulo,	lhs,	ParseFactor(),	0,	NULL);
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
				lhs = MakeASTNode(A_Add,		lhs,	ParseTerm(),	0,	NULL);
				break;
			case T_Minus:
				lhs = MakeASTNode(A_Subtract,	lhs,	ParseTerm(),	0,	NULL);
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
			case T_DoubleLess:		lhs = MakeASTNode(A_LeftShift,	lhs,	ParseAdditiveExpression(),	0,	NULL);	break;
			case T_DoubleGreater:	lhs = MakeASTNode(A_RightShift,	lhs,	ParseAdditiveExpression(),	0,	NULL);	break;
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
			case T_Less:			lhs = MakeASTNode(A_LessThan,		lhs,	ParseBitShiftExpression(),	0,	NULL);	break;
			case T_Greater:			lhs = MakeASTNode(A_GreaterThan,	lhs,	ParseBitShiftExpression(),	0,	NULL);	break;
			case T_LessEqual:		lhs = MakeASTNode(A_LessOrEqual,	lhs,	ParseBitShiftExpression(),	0,	NULL);	break;
			case T_GreaterEqual:	lhs = MakeASTNode(A_GreaterOrEqual,	lhs,	ParseBitShiftExpression(),	0,	NULL);	break;
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
			case T_DoubleEqual:		lhs = MakeASTNode(A_EqualTo,	lhs,	ParseRelationalExpression(),	0,	NULL);	break;
			case T_BangEqual:		lhs = MakeASTNode(A_NotEqualTo,	lhs,	ParseRelationalExpression(),	0,	NULL);	break;
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
		lhs = MakeASTNode(A_BitwiseAnd,	lhs,	ParseEqualityExpression(),	0,	NULL);
		tok = PeekToken();
	}
	return lhs;
}

ASTNode* ParseBitwiseXorExpression(){
	ASTNode* lhs = ParseBitwiseAndExpression();
	Token* tok = PeekToken();
	while(tok->type == T_Caret){
		GetToken();
		lhs = MakeASTNode(A_BitwiseXor,	lhs,	ParseBitwiseAndExpression(),	0,	NULL);
		tok = PeekToken();
	}
	return lhs;
}

ASTNode* ParseBitwiseOrExpression(){
	ASTNode* lhs = ParseBitwiseXorExpression();
	Token* tok = PeekToken();
	while(tok->type == T_Pipe){
		GetToken();
		lhs = MakeASTNode(A_BitwiseOr,	lhs,	ParseBitwiseXorExpression(),	0,	NULL);
		tok = PeekToken();
	}
	return lhs;
}

ASTNode* ParseLogicalAndExpression(){
	ASTNode* lhs = ParseBitwiseOrExpression();
	Token* tok = PeekToken();
	while(tok->type == T_DoubleAmpersand){
		GetToken();
		lhs = MakeASTNode(A_LogicalAnd,	lhs,	ParseBitwiseOrExpression(),	0,	NULL);
		tok = PeekToken();
	}
	return lhs;
}

ASTNode* ParseLogicalOrExpression(){
	ASTNode* lhs = ParseLogicalAndExpression();
	Token* tok = PeekToken();
	while(tok->type == T_DoublePipe){
		GetToken();
		lhs = MakeASTNode(A_LogicalOr,	lhs,	ParseBitwiseOrExpression(),	0,	NULL);
		tok = PeekToken();
	}
	return lhs;
}

ASTNode* ParseExpression(){
	return ParseLogicalOrExpression();
}

ASTNode* ParseStatement(){
	if(GetToken()->type != T_Return)		FatalM("Invalid statement; Expected typename.", Line);
	ASTNode* expr = ParseExpression();
	if(GetToken()->type != T_Semicolon)		FatalM("Invalid statement; Expected semicolon.", Line);
	return MakeASTUnary(A_Return, expr, 0, NULL);
}

ASTNode* ParseFunction(){
	if(GetToken()->type != T_Type)			FatalM("Invalid function declaration; Expected typename.", Line);
	Token* tok = GetToken();
	if(tok->type != T_Identifier)			FatalM("Invalid function declaration; Expected identifier.", Line);
	Token* identifier = tok;
	if(GetToken()->type != T_OpenParen)		FatalM("Invalid function declaration; Expected open parenthesis '('.", Line);
	if(GetToken()->type != T_CloseParen)	FatalM("Invalid function declaration; Expected close parenthesis ')'.", Line);
	if(GetToken()->type != T_OpenBrace)		FatalM("Invalid function declaration; Expected open brace '{'.", Line);
	ASTNode* stmt = ParseStatement();
	if(GetToken()->type != T_CloseBrace)	FatalM("Invalid function declaration; Expected close brace '}'.", Line);
	return MakeASTUnary(A_Function, stmt, 0, identifier->value.strVal);
}

#endif