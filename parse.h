#ifndef PARSE_INCLUDED
#define PARSE_INCLUDED

#include "types.h"
#include "defs.h"

ASTNode* ParseStatement();
ASTNode* ParseExpression();

ASTNode* ParseFunction(){
	if(GetToken()->type != T_Type)			FatalM("Invalid function declaration; Expected typename.", Line);
	Token* tok = GetToken();
	if(tok->type != T_Identifier)			FatalM("Invalid function declaration; Expected identifier.", Line);
	Token* identifier = tok;
	if(GetToken()->type != T_OpenParen)		FatalM("Invalid function declaration; Expected open parenthesis '('.", Line);
	if(GetToken()->type != T_CloseParen)	FatalM("Invalid function declaration; Expected close parenthesis ')'.", Line);
	if(GetToken()->type != T_OpenBrace)		FatalM("Invalid function declaration; Expected open brace '{'.", Line);
	ASTNode* stmt = ParseStatement();
	if(GetToken()->type != T_CloseBrace)		FatalM("Invalid function declaration; Expected close brace '}'.", Line);
	return MakeASTUnary(A_Function, stmt, 0);
}

ASTNode* ParseStatement(){
	if(GetToken()->type != T_Return)		FatalM("Invalid statement; Expected typename.", Line);
	ASTNode* expr = ParseExpression();
	if(GetToken()->type != T_Semicolon)		FatalM("Invalid statement; Expected semicolon.", Line);
	return MakeASTUnary(A_Return, expr, 0);
}

ASTNode* ParseExpression(){
	Token* tok = GetToken();
	if(tok->type != T_LitInt)				FatalM("Invalid expression; Expected integer literal.", Line);
	return MakeASTLeaf(A_LitInt, tok->value.intVal);
}


#endif