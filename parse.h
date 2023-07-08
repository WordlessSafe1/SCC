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

PrimordialType ParseType(){
	PrimordialType type;
	switch(PeekToken()->type){
		case T_Int:		type = P_Int;		break;
		case T_Char:	type = P_Char;		break;
		case T_Void:	type = P_Void;		break;
		case T_Long:	type = P_Long;		break;
		case T_Union:
		case T_Struct:	return P_Composite;	// Structs must be parsed with ParseCompRef()
		default:		return P_Undefined;
	}
	GetToken();
	while(PeekToken()->type == T_Asterisk){
		GetToken();
		if((type & 0xF) == 0xF)	FatalM("Indirection limit exceeded!", Line);
		type++;
	}
	return type;
}

PrimordialType PeekType(){
	PrimordialType type;
	switch(PeekToken()->type){
		case T_Int:		type = P_Int;		break;
		case T_Char:	type = P_Char;		break;
		case T_Void:	type = P_Void;		break;
		case T_Long:	type = P_Long;		break;
		case T_Union:
		case T_Struct:	return P_Composite;	// Structs must be parsed with ParseCompRef()
		default:		return P_Undefined;
	}
	int i = 1;
	while(PeekTokenN(i++)->type == T_Asterisk){
		if((type & 0xF) == 0xF)	FatalM("Indirection limit exceeded!", Line);
		type++;
	}
	return type;
}

/// @brief Parse a composite reference.
/// @param type [OUT] Address of type; will be modified to account for pointers.
/// @return A pointer to the struct definition.
SymEntry* ParseCompRef(PrimordialType* type){
	Token* cTok = GetToken();
	if(cTok->type != T_Struct && cTok->type != T_Union)
		FatalM("Aliased composites not yet supported! (Internal @ parse.h)", __LINE__);
	Token* tok = GetToken();
	if(tok->type != T_Identifier)		FatalM("Expected identifier after 'struct' keyword!", Line);
	const char* id = tok->value.strVal;
	while(PeekToken()->type == T_Asterisk) {
		GetToken();
		(*type)++;
	}
	return FindStruct(id);
}

ASTNode* ParseFunctionCall(Token* tok){
	GetToken();
	SymEntry* func = FindFunc(tok->value.strVal);
	if (func == NULL)
		FatalM("Implicit function declarations not yet supported! (In parse.h)", Line);
	Parameter* paramPrototype = (Parameter*)func->value.ptrVal;
	ASTNodeList* params = MakeASTNodeList();
	while(PeekToken()->type != T_CloseParen){
		if(PeekToken()->type == T_Comma)
			GetToken();
		if(paramPrototype == NULL)	FatalM("Too many arguments in function call!", Line);
		ASTNode* expr = ParseExpression();
		if(expr == NULL)	FatalM("Invalid expression used as parameter!", Line);
		int typeCheck = CheckTypeCompatibility(paramPrototype->type, expr->type);
		if(typeCheck != TYPES_COMPATIBLE && typeCheck != TYPES_WIDEN_RHS)	FatalM("Incompatible type in function call!", Line);
		paramPrototype = paramPrototype->next;
		AddNodeToASTList(params, expr);
	}
	if(paramPrototype != NULL)				FatalM("Too few arguments in function call!", Line);
	if(GetToken()->type != T_CloseParen)	FatalM("Missing close parenthesis in function call!", Line);
	PrimordialType type = P_Undefined;
	SymEntry* cType = NULL;
	if(func == NULL)
		WarnM("Implicit function declaration!", Line);
	else{
		type = func->type;
		cType = func->cType;
	}
	return MakeASTNodeEx(A_FunctionCall, type, NULL, NULL, NULL, FlexStr(tok->value.strVal), FlexPtr(params), cType);
}

ASTNode* ParseVariableReference(Token* outerTok){
	SymEntry* varInfo = FindVar(outerTok->value.strVal, scope);
	PrimordialType type = varInfo == NULL ? P_Undefined : varInfo->type;
	ASTNode* ref = MakeASTNode(A_VarRef, type, NULL, NULL, NULL, FlexStr(outerTok->value.strVal), varInfo->cType);
	Token* tok = PeekToken();
	if(tok->type != T_PlusPlus && tok->type != T_MinusMinus)
		return ref;
	tok = GetToken();
	if(tok->type == T_PlusPlus)
		return MakeASTUnary(A_Increment, ref, FlexInt(0));
	return MakeASTUnary(A_Decrement, ref, FlexInt(0));
}

ASTNode* ParseFactor(){
	Token* tok = GetToken();
	switch(tok->type){
		case T_LitInt:{
			PrimordialType type = tok->value.intVal >= 0 && tok->value.intVal < 256 ? P_Char : P_Int;
			return MakeASTLeaf(A_LitInt, type, FlexInt(tok->value.intVal));
		}
		case T_LitStr:		return MakeASTLeaf(A_LitStr, P_Char + 1, tok->value);
		case T_Minus:		return MakeASTUnary(A_Negate,				ParseFactor(),	FlexNULL());
		case T_Bang:		return MakeASTUnary(A_LogicalNot,			ParseFactor(),	FlexNULL());
		case T_Tilde:		return MakeASTUnary(A_BitwiseComplement,	ParseFactor(),	FlexNULL());
		case T_Semicolon:	ungetc(';', fptr); return MakeASTLeaf(A_Undefined, P_Undefined, FlexNULL());
		case T_Ampersand:{
			ASTNode* fctr = ParseFactor();
			if(fctr->op != A_VarRef)	FatalM("The Address operator may only be used on variable references!", Line);
			PrimordialType t = fctr->type;
			if(t == P_Undefined)		FatalM("Expression type not determined!", Line);
			if((t & 0xF) == 0xF)		FatalM("Indirection limit exceeded!", Line);
			return MakeASTNode(A_AddressOf,		fctr->type + 1,	fctr,	NULL,	NULL,	FlexNULL(), fctr->cType);
		}
		case T_Asterisk:{
			ASTNode* fctr = ParseFactor();
			PrimordialType t = fctr->type;
			if(t == P_Undefined)		FatalM("Expression type not determined!", Line);
			if(!(t & 0xF))				FatalM("Can't dereference a non-pointer!", Line);
			return MakeASTNode(A_Dereference,	fctr->type - 1,	fctr,	NULL,	NULL,	FlexNULL(), fctr->cType);
		}
		case T_Identifier:
			if(PeekToken()->type == T_OpenParen)
				return ParseFunctionCall(tok);
			if(PeekToken()->type == T_OpenBracket){
				ASTNode* varRef = ParseVariableReference(tok);
				if(GetToken()->type != T_OpenBracket)	FatalM("Expected open bracket in array access!", Line);
				ASTNode* expr = ParseExpression();
				if(GetToken()->type != T_CloseBracket)	FatalM("Expected close bracket in array access!", Line);
				ASTNode* scale = MakeASTBinary(A_Multiply, expr->type, expr, MakeASTLeaf(A_LitInt, P_Int, FlexInt(GetPrimSize(varRef->type - 1))), FlexNULL());
				ASTNode* add = MakeASTBinary(A_Add, varRef->type - 1, varRef, scale, FlexNULL());
				ASTNode* deref = MakeASTUnary(A_Dereference, add, FlexNULL());
				return deref;
			}
			if(PeekToken()->type == T_Arrow){
				ASTNode* varRef = ParseVariableReference(tok);
				if(GetToken()->type != T_Arrow)	FatalM("Expected arrow '->' in struct pointer member access!", Line);
				Token* idTok = GetToken();
				if(idTok->type != T_Identifier)	FatalM("Expected member name!", Line);
				SymEntry* member = GetMember(varRef->cType, idTok->value.strVal);
				ASTNode* offset = MakeASTLeaf(A_LitInt, P_Int, FlexInt(member->value.intVal));
				ASTNode* add = MakeASTBinary(A_Add, member->type, varRef, offset, FlexNULL());
				return MakeASTUnary(A_Dereference, add, FlexNULL());
			}
			if(PeekToken()->type == T_Period){
				ASTNode* varRef = ParseVariableReference(tok);
				if(GetToken()->type != T_Period)	FatalM("Expected period '.' in value struct member access!", Line);
				Token* idTok = GetToken();
				if(idTok->type != T_Identifier)	FatalM("Expected member name!", Line);
				SymEntry* member = GetMember(varRef->cType, idTok->value.strVal);
				ASTNode* offset = MakeASTLeaf(A_LitInt, P_Int, FlexInt(member->value.intVal));
				ASTNode* address = MakeASTUnary(A_AddressOf, varRef, FlexNULL());
				ASTNode* add = MakeASTBinary(A_Add, member->type, address, offset, FlexNULL());
				return MakeASTUnary(A_Dereference, add, FlexNULL());
			}
			return ParseVariableReference(tok);
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
		if(IsPointer(lhs->type) && !IsPointer(rhs->type))
			rhs = ScaleNode(rhs, lhs->type);
		else if(IsPointer(rhs->type) && ! IsPointer(lhs->type))
			lhs = ScaleNode(lhs, rhs->type);
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
	return MakeASTNode(A_Ternary, type, condition, then, otherwise, FlexNULL(), NULL);
}

ASTNode* ParseAssignmentExpression(){
	ASTNode* lhs = ParseConditionalExpression();
	if(lhs == NULL)												FatalM("Got NULL instead of expression! (In parse.h)", __LINE__);
	if(lhs->op != A_VarRef && lhs->op != A_Dereference)			return lhs;
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
	PrimordialType type = ParseType();
	if(type == P_Undefined)			FatalM("Expected typename!", Line);
	SymEntry* cType = (type == P_Composite) ? ParseCompRef(&type) : NULL;
	Token* tok = GetToken();
	if(tok->type != T_Identifier)	FatalM("Expected identifier!", Line);
	const char* id = tok->value.strVal;
	InsertVar(id, NULL, type, cType, scope);
	if(PeekToken()->type != T_Equal)	return MakeASTNode(A_Declare, type, NULL, NULL, NULL, FlexStr(id), cType);
	GetToken();
	ASTNode* expr = ParseExpression();
	int typeCompat = CheckTypeCompatibility(type, expr->type);
	switch(typeCompat){
		case TYPES_INCOMPATIBLE:	FatalM("Types incompatible!", Line);
		case TYPES_WIDEN_LHS:		WarnM("Truncating right hand side of declaration!", Line); break;
		default:					break;
	}
	if(!scope){
		if(expr->op != A_LitInt)	FatalM("Non-constant expression in global varibale declaration!", Line);
		int typeSize = GetTypeSize(type, cType);
		// If expression result is too large to fit in variable, truncate it
		if(expr->value.intVal >= (1 << (8 * typeSize)))
			expr->value.intVal &= (1 << (8 * typeSize)) - 1;
		return MakeASTNodeEx(A_Declare, type, expr, NULL, NULL, FlexStr(id), FlexInt(expr->value.intVal), cType);
	}
	return MakeASTNode(A_Declare, type, expr, NULL, NULL, FlexStr(id), cType);
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
	return MakeASTNode(A_If, P_Undefined, condition, otherwise, then, FlexNULL(), NULL);
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
	ASTNode* header = MakeASTNode(A_Glue, P_Undefined, initializer, condition, modifier, FlexNULL(), NULL);
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
	ASTNode* expr = (PeekType() == P_Undefined) ? ParseExpression() : ParseDeclaration();
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
	PrimordialType type = ParseType();
	if(type == P_Undefined)					FatalM("Invalid function declaration; Expected typename.", Line);
	SymEntry* cType = type == P_Composite ? ParseCompRef(&type) : NULL;
	Token* tok = GetToken();
	if(tok->type != T_Identifier)			FatalM("Invalid function declaration; Expected identifier.", Line);
	int idLen = strlen(tok->value.strVal) + 1;
	char* idStr = malloc(idLen * sizeof(char));
	strncpy(idStr,tok->value.strVal, idLen);
	if(GetToken()->type != T_OpenParen)		FatalM("Invalid function declaration; Expected open parenthesis '('.", Line);
	Parameter* params = NULL;
	while(PeekToken()->type != T_CloseParen){
		if(PeekToken()->type == T_Comma)
			GetToken();
		PrimordialType paramType = ParseType();
		if(paramType == P_Undefined)		FatalM("Invalid type in parameter list!", Line);
		Token* t = GetToken();
		if(t->type != T_Identifier)			FatalM("Expected identifier in parameter list!", Line);
		int charCount = strlen(t->value.strVal) + 1;
		char* paramName = malloc(charCount * sizeof(char));
		paramName = strncpy(paramName, t->value.strVal, charCount);
		free(t);
		params = MakeParam(paramName, paramType, params);
	}
	if(params != NULL)
		while (params->prev != NULL)
			params = params->prev;
	if(GetToken()->type != T_CloseParen)	FatalM("Invalid function declaration; Expected close parenthesis ')'.", Line);
	InsertFunc(idStr, FlexPtr(params), type, NULL);
	if(PeekToken()->type == T_Semicolon){
		GetToken();
		return MakeASTNodeEx(A_Function, type, NULL, NULL, NULL, FlexStr(idStr), FlexPtr(params), cType);
	}
	EnterScope();
	if(params != NULL){
		Parameter* p = params;
		do {
			InsertVar(p->id, NULL, p->type, NULL, scope);
			p = p->next;
		} while( p != NULL);
	}
	if(PeekToken()->type != T_OpenBrace)	FatalM("Invalid function declaration; Expected open brace '{'.", Line);
	ASTNode* block = ParseBlock();
	ExitScope();
	return MakeASTNodeEx(A_Function, type, block, NULL, NULL, FlexStr(idStr), FlexPtr(params), cType);
	// return MakeASTUnary(A_Function, stmt, 0, identifier->value.strVal);
}

ASTNode* ParseCompositeDeclaration(){
	Token* cTok = GetToken();
	if(cTok->type != T_Struct && cTok->type != T_Union)
		FatalM("Expected composite type!", Line);
	Token* tok = GetToken();
	if(tok->type != T_Identifier)			FatalM("Anonymous composites not yet implemented!", Line);
	const char* identifier = tok->value.strVal;
	if(PeekToken()->type == T_Semicolon){
		// incomplete type
		FatalM("Incomplete composite declarations not yet supported!", Line);
	}
	if(GetToken()->type != T_OpenBrace)		FatalM("Expected open brace '{' in composite declaration!", Line);
	ASTNodeList* memberNodes = MakeASTNodeList();
	while(PeekToken()->type != T_CloseBrace){
		ASTNode* decl = ParseDeclaration();
		if(decl->lhs != NULL)				FatalM("Composite member initializers not yet supported!", Line);
		AddNodeToASTList(memberNodes, decl);
		if(GetToken()->type != T_Semicolon)	FatalM("Expected semicolon following composite member declaration!", Line);
	}
	if(GetToken()->type != T_CloseBrace)	FatalM("Expected close brace '}' in composite declaration!", Line);
	SymList* list = cTok->type == T_Struct
		? InsertStruct(identifier, MakeCompMembers(memberNodes))
		: InsertUnion(identifier, MakeCompMembers(memberNodes));
	if(list == NULL)						FatalM("Failed to create composite definition! (In parse.h)", __LINE__);
	if(PeekToken()->type != T_Identifier){
		if(GetToken()->type != T_Semicolon)	FatalM("Expected semicolon after composite declaration!", Line);
		return MakeASTList(A_StructDecl, memberNodes, FlexStr(identifier));
	}
	while(!streq(list->item->key, identifier) && list->next != NULL)
		list = list->next;
	if(!streq(list->item->key, identifier))	FatalM("Failed to find composite definition after creation! (In parse.h)", __LINE__);
	SymEntry* entry = list->item;
	ASTNode* varDecl = MakeASTLeaf(A_Declare, P_Composite, GetToken()->value);
	varDecl->cType = entry;
	if(GetToken()->type != T_Semicolon)		FatalM("Expected semicolon after struct declaratioin!", Line);
	ASTNode* ret = MakeASTList(A_StructDecl, memberNodes, FlexStr(identifier));
	ret->lhs = varDecl;
	return ret;
}

ASTNode* ParseNode(){
	int i = 0;
	Token* tok = PeekTokenN(i);
	while(tok->type != T_OpenParen && tok->type != T_Equal && tok->type != T_Semicolon && tok->type != T_OpenBrace)
		tok = PeekTokenN(++i);
	switch(tok->type){
		case T_Equal:
		case T_Semicolon:{
			ASTNode* decl = ParseDeclaration();
			if(GetToken()->type != T_Semicolon)
				FatalM("Expected semicolon after declaration!", Line);
			return decl;
		}
		case T_OpenBrace:	return ParseCompositeDeclaration();
		default:			return ParseFunction();
	}
}

#endif