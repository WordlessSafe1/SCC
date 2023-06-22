#ifndef PARSE_INCLUDED
#define PARSE_INCLUDED

#include "types.h"
#include "defs.h"

ASTNode* ParseExpression();
ASTNode* ParseStatement();
ASTNode* ParseBlock();


PrimordialType GetType(Token* t){
	switch(t->type){
		case T_Int:		return P_Int;
		case T_Char:	return P_Char;
		case T_Void:	return P_Void;
		default:		return P_Undefined;
	}
}

ASTNode* ParseFactor(){
	Token* tok = GetToken();

	switch(tok->type){
		case T_LitInt:{
			PrimordialType type = tok->value.intVal >= 0 && tok->value.intVal < 256 ? P_Char : P_Int;
			return MakeASTLeaf(A_LitInt, type, FlexInt(tok->value.intVal));
		}
		case T_Minus:		return MakeASTUnary(A_Negate,				ParseFactor(),	FlexNULL());
		case T_Bang:		return MakeASTUnary(A_LogicalNot,			ParseFactor(),	FlexNULL());
		case T_Tilde:		return MakeASTUnary(A_BitwiseComplement,	ParseFactor(),	FlexNULL());
		case T_Semicolon:	ungetc(';', fptr); return MakeASTLeaf(A_Undefined, P_Undefined, FlexNULL());
		case T_Identifier:{
			SymEntry* varInfo = FindVar(tok->value.strVal, scope);
			PrimordialType type = varInfo == NULL ? P_Undefined : varInfo->type;
			ASTNode* ref = MakeASTLeaf(A_VarRef, type, FlexStr(tok->value.strVal));
			Token* tok = PeekToken();
			if(tok->type != T_PlusPlus && tok->type != T_MinusMinus)
				return ref;
			tok = GetToken();
			if(tok->type == T_PlusPlus)
				return MakeASTUnary(A_Increment, ref, FlexInt(0));
			return MakeASTUnary(A_Decrement, ref, FlexInt(0));
		}
		case T_PlusPlus:{
			ASTNode* ref = ParseFactor();
			if(ref->op != A_VarRef)		FatalM("The increment prefix operator '++' may only be used before a variable name!", Line);
			return MakeASTUnary(A_Increment, ref, FlexInt(1));
		}
		case T_MinusMinus:{
			ASTNode* ref = ParseFactor();
			if(ref->op != A_VarRef)		FatalM("The decrement prefix operator '--' may only be used before a variable name!", Line);
			return MakeASTUnary(A_Decrement, ref, FlexInt(1));
		}
		case T_OpenParen:{
			ASTNode* expr = ParseExpression();
			if(GetToken()->type != T_CloseParen)	FatalM("Expected close parenthesis!", Line);
			return expr;
		}
		default:			FatalM("Invalid expression!", Line);
	}
}

ASTNode* ParseTerm(){
	ASTNode* lhs = ParseFactor();
	Token* tok = PeekToken();
	while(tok->type == T_Asterisk || tok->type == T_Divide || tok->type == T_Percent){
		GetToken();
		ASTNode* rhs = ParseFactor();
		PrimordialType type = NodeWidestType(lhs, rhs);
		if(type == P_Undefined)
			FatalM("Types of expression members are incompatible!", Line);
		switch (tok->type){
			case T_Asterisk:
				lhs = MakeASTBinary(A_Multiply,	type, lhs, rhs, FlexNULL());
				break;
			case T_Divide:
				lhs = MakeASTBinary(A_Divide,	type, lhs, rhs, FlexNULL());
				break;
			case T_Percent:
				lhs = MakeASTBinary(A_Modulo,	type, lhs, rhs, FlexNULL());
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
		ASTNode* rhs = ParseTerm();
		PrimordialType type = NodeWidestType(lhs, rhs);
		if(type == P_Undefined)
			FatalM("Types of expression members are incompatible!", Line);
		switch (tok->type){
			case T_Plus:
				lhs = MakeASTBinary(A_Add,		type, lhs, rhs, FlexNULL());
				break;
			case T_Minus:
				lhs = MakeASTBinary(A_Subtract,	type, lhs, rhs, FlexNULL());
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
		ASTNode* rhs = ParseAdditiveExpression();
		PrimordialType type = NodeWidestType(lhs, rhs);
		if(type == P_Undefined)
			FatalM("Types of expression members are incompatible!", Line);
		switch(tok->type){
			case T_DoubleLess:		lhs = MakeASTBinary(A_LeftShift,	type, lhs, rhs, FlexNULL());	break;
			case T_DoubleGreater:	lhs = MakeASTBinary(A_RightShift,	type, lhs, rhs, FlexNULL());	break;
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
		ASTNode* rhs = ParseBitShiftExpression();
		PrimordialType type = NodeWidestType(lhs, rhs);
		if(type == P_Undefined)
			FatalM("Types of expression members are incompatible!", Line);
		switch(tok->type){
			case T_Less:			lhs = MakeASTBinary(A_LessThan,			type, lhs, rhs, FlexNULL());	break;
			case T_Greater:			lhs = MakeASTBinary(A_GreaterThan,		type, lhs, rhs, FlexNULL());	break;
			case T_LessEqual:		lhs = MakeASTBinary(A_LessOrEqual,		type, lhs, rhs, FlexNULL());	break;
			case T_GreaterEqual:	lhs = MakeASTBinary(A_GreaterOrEqual,	type, lhs, rhs, FlexNULL());	break;
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
		ASTNode* rhs = ParseRelationalExpression();
		PrimordialType type = NodeWidestType(lhs, rhs);
		if(type == P_Undefined)
			FatalM("Types of expression members are incompatible!", Line);
		switch(tok->type){
			case T_DoubleEqual:		lhs = MakeASTBinary(A_EqualTo,		type, lhs, rhs, FlexNULL());	break;
			case T_BangEqual:		lhs = MakeASTBinary(A_NotEqualTo,	type, lhs, rhs, FlexNULL());	break;
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
		ASTNode* rhs = ParseEqualityExpression();
		PrimordialType type = NodeWidestType(lhs, rhs);
		if(type == P_Undefined)
			FatalM("Types of expression members are incompatible!", Line);
		lhs = MakeASTBinary(A_BitwiseAnd,	type, lhs, rhs, FlexNULL());
		tok = PeekToken();
	}
	return lhs;
}

ASTNode* ParseBitwiseXorExpression(){
	ASTNode* lhs = ParseBitwiseAndExpression();
	Token* tok = PeekToken();
	while(tok->type == T_Caret){
		GetToken();
		ASTNode* rhs = ParseBitwiseAndExpression();
		PrimordialType type = NodeWidestType(lhs, rhs);
		if(type == P_Undefined)
			FatalM("Types of expression members are incompatible!", Line);
		lhs = MakeASTBinary(A_BitwiseXor,	type, lhs, rhs, FlexNULL());
		tok = PeekToken();
	}
	return lhs;
}

ASTNode* ParseBitwiseOrExpression(){
	ASTNode* lhs = ParseBitwiseXorExpression();
	Token* tok = PeekToken();
	while(tok->type == T_Pipe){
		GetToken();
		ASTNode* rhs = ParseBitwiseXorExpression();
		PrimordialType type = NodeWidestType(lhs, rhs);
		if(type == P_Undefined)
			FatalM("Types of expression members are incompatible!", Line);
		lhs = MakeASTBinary(A_BitwiseOr,	type, lhs, rhs, FlexNULL());
		tok = PeekToken();
	}
	return lhs;
}

ASTNode* ParseLogicalAndExpression(){
	ASTNode* lhs = ParseBitwiseOrExpression();
	Token* tok = PeekToken();
	while(tok->type == T_DoubleAmpersand){
		GetToken();
		ASTNode* rhs = ParseBitwiseOrExpression();
		PrimordialType type = NodeWidestType(lhs, rhs);
		if(type == P_Undefined)
			FatalM("Types of expression members are incompatible!", Line);
		lhs = MakeASTBinary(A_LogicalAnd,	type, lhs, rhs, FlexNULL());
		tok = PeekToken();
	}
	return lhs;
}

ASTNode* ParseLogicalOrExpression(){
	ASTNode* lhs = ParseLogicalAndExpression();
	Token* tok = PeekToken();
	while(tok->type == T_DoublePipe){
		GetToken();
		ASTNode* rhs = ParseLogicalAndExpression();
		PrimordialType type = NodeWidestType(lhs, rhs);
		if(type == P_Undefined)
			FatalM("Types of expression members are incompatible!", Line);
		lhs = MakeASTBinary(A_LogicalOr,	type, lhs, rhs, FlexNULL());
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
	PrimordialType type = NodeWidestType((then != NULL ? then : condition), otherwise);
	if(type == P_Undefined)					FatalM("Types of expression members are incompatible!", Line);
	return MakeASTNode(A_Ternary, type, condition, then, otherwise, FlexNULL());
}

ASTNode* ParseAssignmentExpression(){
	ASTNode* lhs = ParseConditionalExpression();
	if(lhs == NULL)							FatalM("Got NULL instead of expression! (In parse.h)", __LINE__);
	if(lhs->op != A_VarRef)		return lhs;
	NodeType nt = A_Undefined;
	switch(PeekToken()->type){
		case T_Equal:				nt = A_Assign;				break;
		case T_PlusEqual:			nt = A_AssignSum;			break;
		case T_MinusEqual:			nt = A_AssignDifference;	break;
		case T_AsteriskEqual:		nt = A_AssignProduct;		break;
		case T_DivideEqual:			nt = A_AssignQuotient;		break;
		case T_PercentEqual:		nt = A_AssignModulus;		break;
		case T_DoubleLessEqual:		nt = A_AssignLeftShift;		break;
		case T_DoubleGreaterEqual:	nt = A_AssignRightShift;	break;
		case T_AmpersandEqual:		nt = A_AssignBitwiseAnd;	break;
		case T_CaretEqual:			nt = A_AssignBitwiseXor;	break;
		case T_PipeEqual:			nt = A_AssignBitwiseOr;		break;
		default:					return lhs;
	}
	GetToken();
	ASTNode* rhs = ParseExpression();
	if(lhs == NULL)							FatalM("Expected expression!", Line);
	PrimordialType type;
	switch(NodeTypesCompatible(lhs, rhs)){
		case TYPES_INCOMPATIBLE:	FatalM("Types of expression members are incompatible!", Line);
		case TYPES_WIDEN_LHS:		WarnM("Truncating right hand side of expression!", Line);
		default:					type = lhs->type;
	}

	// PrimordialType type = NodeWidestType(lhs, rhs);
	// if(type == P_Undefined)					FatalM("Types of expression members are incompatible!", Line);
	return MakeASTBinary(nt, type, lhs, rhs, FlexNULL());
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
	PrimordialType type = GetType(tok);
	if(type == P_Undefined)			FatalM("Expected typename!", Line);
	tok = GetToken();
	if(tok->type != T_Identifier)	FatalM("Expected identifier!", Line);
	const char* id = tok->value.strVal;
	if(PeekToken()->type != T_Equal)	return MakeASTLeaf(A_Declare, type, FlexStr(id));
	GetToken();
	ASTNode* expr = ParseExpression();
	if(!CheckTypeCompatibility(type, expr->type))	FatalM("Types incompatible!", Line);
	InsertVar(id, NULL, type, scope);
	return MakeASTNode(A_Declare, type, expr, NULL, NULL, FlexStr(id));
}

ASTNode* ParseIfStatement(){
	if(GetToken()->type != T_If)			FatalM("Expected 'if' to begin if statement!", Line);
	if(GetToken()->type != T_OpenParen)		FatalM("Expected open parenthesis '(' in if statement!", Line);
	ASTNode* condition = ParseExpression();
	if(GetToken()->type != T_CloseParen)	FatalM("Expected close parenthesis ')' in if statement!", Line);
	ASTNode* then = ParseStatement();
	if(PeekToken()->type != T_Else)
		return MakeASTBinary(A_If, P_Undefined, condition, then, FlexNULL());
	GetToken();
	ASTNode* otherwise = ParseStatement();
	return MakeASTNode(A_If, P_Undefined, condition, otherwise, then, FlexNULL());
}

ASTNode* ParseWhileLoop(){
	if(GetToken()->type != T_While)			FatalM("Expected 'while' to begin while loop!", Line);
	if(GetToken()->type != T_OpenParen)		FatalM("Expected open parenthesis '(' in while loop!", Line);
	ASTNode* condition = ParseExpression();
	if(GetToken()->type != T_CloseParen)	FatalM("Expected close parenthesis ')' in while loop!", Line);
	ASTNode* stmt = ParseStatement();
	return MakeASTBinary(A_While, P_Undefined, condition, stmt, FlexNULL());
}

ASTNode* ParseDoLoop(){
	if(GetToken()->type != T_Do)			FatalM("Expected 'do' to begin do-while loop!", Line);
	ASTNode* stmt = ParseStatement();
	if(GetToken()->type != T_While)			FatalM("Expected 'while' clause in do-while loop!", Line);
	if(GetToken()->type != T_OpenParen)		FatalM("Expected open parenthesis '(' in do-while loop!", Line);
	ASTNode* condition = ParseExpression();
	if(GetToken()->type != T_CloseParen)	FatalM("Expected close parenthesis ')' in do-while loop!", Line);
	return MakeASTBinary(A_Do, P_Undefined, condition, stmt, FlexNULL());
}

ASTNode* ParseForLoop(){
	if(GetToken()->type != T_For)			FatalM("Expected 'for' to begin for loop!", Line);
	EnterScope();
	if(GetToken()->type != T_OpenParen)		FatalM("Expected open parenthesis '(' in for loop!", Line);
	ASTNode* initializer = NULL;
	switch(PeekToken()->type){
		case T_Semicolon:	break;
		case T_Char:
		case T_Int:			initializer = ParseDeclaration();	break;
		default:			initializer = ParseExpression();	break;
	}
	if(GetToken()->type != T_Semicolon)	FatalM("Expected semicolon in for loop!", Line);
	ASTNode* condition	= PeekToken()->type == T_Semicolon ? NULL : ParseExpression();
	if(GetToken()->type != T_Semicolon)	FatalM("Expected semicolon in for loop!", Line);
	ASTNode* modifier	= PeekToken()->type == T_CloseParen ? NULL : ParseExpression();
	if(GetToken()->type != T_CloseParen)	FatalM("Expected close parenthesis ')' in for loop!", Line);
	ASTNode* stmt = ParseStatement();
	ASTNode* header = MakeASTNode(A_Glue, P_Undefined, initializer, condition, modifier, FlexNULL());
	ExitScope();
	return MakeASTBinary(A_For, P_Undefined, header, stmt, FlexNULL());
}

ASTNode* ParseStatement(){
	Token* tok = PeekToken();
	switch(tok->type){
		case T_Return:		return ParseReturnStatement();
		case T_OpenBrace:	return ParseBlock();
		case T_If:			return ParseIfStatement();
		case T_For:			return ParseForLoop();
		case T_While:		return ParseWhileLoop();
		case T_Do:			return ParseDoLoop();
		case T_Continue:	GetToken(); return MakeASTLeaf(A_Continue, P_Undefined, FlexNULL());
		case T_Break:		GetToken(); return MakeASTLeaf(A_Break, P_Undefined, FlexNULL());
		default:			break;
	}
	ASTNode* expr = NULL;
	switch(tok->type){
		case T_Char:
		case T_Int:		expr = ParseDeclaration();	break;
		default:			expr = ParseExpression();	break;
	}
	if(GetToken()->type != T_Semicolon)		FatalM("Expected semicolon!", Line);
	return expr;
}

ASTNode* ParseBlock(){
	if(GetToken()->type != T_OpenBrace)		FatalM("Invalid block declaration; Expected open brace '{'.", Line);
	EnterScope();
	ASTNodeList* list = MakeASTNodeList();
	while(PeekToken()->type != T_CloseBrace)
		AddNodeToASTList(list, ParseStatement());
	if(GetToken()->type != T_CloseBrace)	FatalM("Invalid block declaration; Expected close brace '}'.", Line);
	ExitScope();
	return MakeASTList(A_Block, list, FlexNULL());
}

ASTNode* ParseFunction(){
	if(GetType(GetToken()) == P_Undefined)	FatalM("Invalid function declaration; Expected typename.", Line);
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