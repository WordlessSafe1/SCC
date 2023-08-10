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
		case T_Long:	return P_Long;
		case T_Void:	return P_Void;
		default:		return P_Undefined;
	}
}

PrimordialType ParseType(StorageClass* sc){
	PrimordialType type;
	Token* tok = PeekToken();
	bool isUnsigned = false;
	// If SC is NULL, then storage classes are not supported; Parsing should be skipped in order to force a fail later on.
	if(sc != NULL){
		switch(tok->type){
			case T_Static:	GetToken(); *sc = C_Static;	break;
			case T_Extern:	GetToken();	*sc = C_Extern;	break;
			default:		break;
		}
		tok = PeekToken();
		switch(tok->type){
			case T_Static:
			case T_Extern:	FatalM("Cannot use more than one storage class!", Line);
			default:		break;
		}
	}
	if(PeekToken()->type == T_Unsigned){
		GetToken();
		tok = PeekToken();
		isUnsigned = true;
	}
	switch(tok->type){
		case T_Identifier:{
			SymEntry* tdef = FindGlobal(tok->value.strVal, S_Typedef);
			if(tdef == NULL)				return P_Undefined;
			if((tdef->type & 0xF0) == P_Composite)	
				if(isUnsigned)				FatalM("Unsigned is not valid for composite types!", Line);
				else						return P_Composite;
			type = tdef->type;
			break;
		}
		case T_Int:			type = P_Int;		break;
		case T_Char:		type = P_Char;		break;
		case T_Void:		type = P_Void;		break;
		case T_Long:
			if(PeekTokenN(1)->type == T_Long){
				GetToken();
				type = P_LongLong;
			}
			else
				type = P_Long;
			break;
		case T_Enum:
		case T_Union:
		case T_Struct:
			if(isUnsigned)	FatalM("Unsigned is not valid for composite types!", Line);
			return P_Composite;	// Structs must be parsed with ParseCompRef()
		default:			return P_Undefined;
	}
	if(isUnsigned)
		type += P_UNSIGNED_DIFF;
	GetToken();
	while(PeekToken()->type == T_Asterisk){
		GetToken();
		if((type & 0xF) == 0xF)	FatalM("Indirection limit exceeded!", Line);
		type++;
	}
	return type;
}

PrimordialType PeekTypeN(int n){
	fpos_t* fpos = malloc(sizeof(fpos_t));
	int ln = Line;
	fgetpos(fptr, fpos);
	for(int i = 0; i < n; i++)
		if(ShiftToken() == NULL)
			break;
	PrimordialType t = ParseType(NULL);
	fsetpos(fptr, fpos);
	free(fpos);
	Line = ln;
	return t;
}

PrimordialType PeekType(){
	return PeekTypeN(0);
}

/// @brief Parse a composite reference.
/// @param type [OUT] Address of type; will be modified to account for pointers.
/// @return A pointer to the struct definition.
SymEntry* ParseCompRef(PrimordialType* type){
	Token* cTok = GetToken();
	const char* id = "";
	SymEntry* tdef = NULL;
	switch(cTok->type){
		case T_Identifier:
			tdef = FindGlobal(cTok->value.strVal, S_Typedef);
			if (tdef == NULL)
				FatalM("Expected typename!", Line);
			*type = tdef->type;
			break;
		case T_Enum:
			*type = P_Int;
			// Fall through
		case T_Struct:
		case T_Union:
			{
				Token* tok = GetToken();
				if(tok->type != T_Identifier)		FatalM("Expected identifier after 'struct' keyword!", Line);
				id = tok->value.strVal;
			}
			break;
		default:
			// Should never be able to hit this
			FatalM("Expected typename!", Line);
	}
	while(PeekToken()->type == T_Asterisk) {
		GetToken();
		(*type)++;
	}
	if(cTok->type == T_Enum)
		return NULL;
	SymEntry* cType = cTok->type == T_Identifier ? tdef->cType : FindStruct(id);
	return cType;
}

ASTNode* ParseFunctionCall(Token* tok){
	GetToken();
	SymEntry* func = FindFunc(tok->value.strVal);
	if (func == NULL)
		FatalM("Implicit function declarations not yet supported!", Line);
	Parameter* paramPrototype = (Parameter*)func->value.ptrVal;
	ASTNodeList* params = MakeASTNodeList();
	bool variadic = false;
	while(PeekToken()->type != T_CloseParen){
		if(PeekToken()->type == T_Comma)
			GetToken();
		if(!variadic && paramPrototype == NULL)	FatalM("Too many arguments in function call!", Line);
		if(paramPrototype != NULL && paramPrototype->type == P_Void && streq(paramPrototype->id, "..."))
			variadic = true;
		ASTNode* expr = ParseExpression();
		if(expr == NULL)	FatalM("Invalid expression used as parameter!", Line);
		if(!variadic){
			int typeCheck = CheckTypeCompatibility(paramPrototype->type, expr->type);
			if(typeCheck != TYPES_COMPATIBLE && typeCheck != TYPES_WIDEN_RHS)	FatalM("Incompatible type in function call!", Line);
		}
		if(paramPrototype != NULL)
			paramPrototype = paramPrototype->next;
		AddNodeToASTList(params, expr);
	}
	if(paramPrototype != NULL && (paramPrototype->type != P_Void || !streq(paramPrototype->id, "...")))
		FatalM("Too few arguments in function call!", Line);
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
	if (varInfo == NULL)				return NULL;
	PrimordialType type = varInfo == NULL ? P_Undefined : varInfo->type;
	ASTNode* ref = MakeASTNode(A_VarRef, type, NULL, NULL, NULL, FlexStr(outerTok->value.strVal), varInfo->cType);
	return ref;
}

ASTNode* ParseBase(){
	Token* tok = GetToken();
	switch(tok->type){
		case T_Identifier:{
			if(PeekToken()->type == T_OpenParen)
				return ParseFunctionCall(tok);
			ASTNode* varRef = ParseVariableReference(tok);
			if(varRef != NULL)
				return varRef;
			SymEntry* enumVal = FindEnumValue(tok->value.strVal);
			if(enumVal != NULL)
				return MakeASTLeaf(A_LitInt, enumVal->value.intVal <= 255 ? P_Char : P_Int, enumVal->value);
			FatalM("Unknown identifier!", Line);
			break;
		}
		case T_OpenParen:{
			ASTNode* expr = ParseExpression();
			if(GetToken()->type != T_CloseParen)	FatalM("Expected close parenthesis!", Line);
			return expr;
		}
		case T_LitInt:{
			PrimordialType type = P_Undefined;
			if(tok->value.intVal >= 0 && tok->value.intVal < 256)
				type = P_Char;
			else if(tok->value.intVal >= -2147483647 && tok->value.intVal <= 2147483647)
				type = P_Int;
			else
				type = P_LongLong;
			return MakeASTLeaf(A_LitInt, type, FlexInt(tok->value.intVal));
		}
		case T_LitStr:		return MakeASTLeaf(A_LitStr, P_Char + 1, tok->value);
		default:	FatalM("Invalid Expression!", Line);
	}
}

ASTNode* ParseArraySubscript(ASTNode* node){
	GetToken();
	ASTNode* expr = ParseExpression();
	if(GetToken()->type != T_CloseBracket)	FatalM("Expected close bracket in array access!", Line);
	ASTNode* scale = MakeASTBinary(A_Multiply, expr->type, expr, MakeASTLeaf(A_LitInt, P_Int, FlexInt(GetPrimSize(node->type - 1))), FlexNULL());
	ASTNode* add = MakeASTBinary(A_Add, node->type - 1, node, scale, FlexNULL());
	ASTNode* deref = MakeASTUnary(A_Dereference, add, FlexNULL(), node->cType);
	return deref;
}

ASTNode* ParseValueAccessor(ASTNode* node){
	GetToken();
	Token* idTok = GetToken();
	if(idTok->type != T_Identifier)	FatalM("Expected member name!", Line);
	SymEntry* member = GetMember(node->cType, idTok->value.strVal);
	ASTNode* offset = MakeASTLeaf(A_LitInt, P_Int, FlexInt(member->value.intVal));
	ASTNode* address = MakeASTUnary(A_AddressOf, node, FlexNULL(), NULL);
	ASTNode* add = MakeASTBinary(A_Add, member->type, address, offset, FlexNULL());
	return MakeASTUnary(A_Dereference, add, FlexNULL(), member->cType);
}

ASTNode* ParsePointerAccessor(ASTNode* node){
	GetToken();
	Token* idTok = GetToken();
	if(idTok->type != T_Identifier)	FatalM("Expected member name!", Line);
	SymEntry* member = GetMember(node->cType, idTok->value.strVal);
	ASTNode* offset = MakeASTLeaf(A_LitInt, P_Int, FlexInt(member->value.intVal));
	ASTNode* add = MakeASTBinary(A_Add, member->type, node, offset, FlexNULL());
	return MakeASTUnary(A_Dereference, add, FlexNULL(), member->cType);
}

ASTNode* ParsePost(ASTNode* node){
	Token* tok = PeekToken();
	switch(tok->type){
		case T_PlusPlus:{
			if(!node->lvalue)				FatalM("The increment postfix operator may only be preceded by an lvalue!", Line);
			if(node->type == P_Composite)	FatalM("The increment postfix operator may not be used on composite types!", Line);
			GetToken();
			int size = 0;
			ASTNode* rhs = NULL;
			if(node->type & 0xF){
				size = GetTypeSize(node->type - 1, node->cType);
				rhs = MakeASTBinary(A_AssignSum, node->type, node, MakeASTLeaf(A_LitInt, P_LongLong, FlexInt(size)), FlexNULL());
			}
			return MakeASTNodeEx(A_Increment, node->type, node, NULL, rhs, FlexNULL(), FlexInt(size), NULL);
		}
		case T_MinusMinus:{
			if(!node->lvalue)				FatalM("The decrement postfix operator may only be preceded by an lvalue!", Line);
			if(node->type == P_Composite)	FatalM("The decrement postfix operator may not be used on composite types!", Line);
			GetToken();
			int size = 0;
			ASTNode* rhs = NULL;
			if(node->type & 0xF){
				size = GetTypeSize(node->type - 1, node->cType);
				rhs = MakeASTBinary(A_AssignDifference, node->type, node, MakeASTLeaf(A_LitInt, P_LongLong, FlexInt(size)), FlexNULL());
			}
			return MakeASTNodeEx(A_Decrement, node->type, node, NULL, rhs, FlexNULL(), FlexInt(size), NULL);
		}
		case T_OpenParen:
			return NULL; // Function calls are currently only handled alongside identifiers. Function pointers are not yet supported.
		case T_OpenBracket:
			if(!node->lvalue)				FatalM("The array accessor operator may only be preceded by an lvalue!", Line);
			return ParseArraySubscript(node);
		case T_Period:
			if(!node->lvalue)				FatalM("The member accessor may only be preceded by an lvalue!", Line);
			if(node->type != P_Composite)	FatalM("The member accessor may only be used on a composite!", Line);
			if(node->type & 0x0F)			FatalM("The member accessor may not be used on a pointer!", Line);
			if(node->cType == NULL)			FatalM("Can only access members of a composite!", Line);
			return ParseValueAccessor(node);
		case T_Arrow:
			if(!node->lvalue)						FatalM("The dereferencing member accessor may only be preceded by an lvalue!", Line);
			if(!(node->type & 0x0F))				FatalM("The dereferencing member accessor may only be used on a pointer!", Line);
			if((node->type & 0xF0) != P_Composite)	FatalM("The dereferencing member accessor may only be used on a composite pointer!", Line);
			if(node->cType == NULL)					FatalM("Can only access members of a composite!", Line);
			return ParsePointerAccessor(node);
		default:
			return NULL;
	}
}

ASTNode* ParsePrimary(){
	ASTNode* node = ParseBase();
	ASTNode* post = ParsePost(node);
	while(post != NULL){
		node = post;
		post = ParsePost(node);
	}
	return node;
}

ASTNode* ParseFactor(){
	Token* tok = PeekToken();
	switch(tok->type){
		case T_Minus:{
			GetToken();
			ASTNode* Factor = ParseFactor();
			if(FOLD_INLINE && Factor->op == A_LitInt){
				int val = -Factor->value.intVal;
				free(Factor);
				return MakeASTLeaf(A_LitInt, P_Int, FlexInt(val));
			}
			return MakeASTUnary(A_Negate, Factor,	FlexNULL(), NULL);
		}
		case T_Bang:		GetToken(); return MakeASTUnary(A_LogicalNot,			ParseFactor(),	FlexNULL(), NULL);
		case T_Tilde:		GetToken(); return MakeASTUnary(A_BitwiseComplement,	ParseFactor(),	FlexNULL(), NULL);
		case T_Semicolon:	return MakeASTLeaf(A_Undefined, P_Undefined, FlexNULL());
		case T_PlusPlus:{
			GetToken();
			ASTNode* node = ParseFactor();
			if(!node->lvalue)				FatalM("The increment prefix operator '++' may only be used before an lvalue!", Line);
			if(node->type == P_Composite)	FatalM("Composite increments not supported!", Line);
			int size = 0;
			ASTNode* rhs = NULL;
			if(node->type & 0xF){
				size = GetTypeSize(node->type - 1, node->cType);
				rhs = MakeASTBinary(A_AssignSum, node->type, node, MakeASTLeaf(A_LitInt, P_LongLong, FlexInt(size)), FlexNULL());
			}
			return MakeASTNodeEx(A_Increment, node->type, node, NULL, rhs, FlexInt(1), FlexInt(size), NULL);
		}
		case T_MinusMinus:{
			GetToken();
			ASTNode* node = ParseFactor();
			if(!node->lvalue)				FatalM("The decrement prefix operator '--' may only be used before an lvalue!", Line);
			if(node->type == P_Composite)	FatalM("Composite decrements not supported!", Line);
			int size = 0;
			ASTNode* rhs = NULL;
			if(node->type & 0xF){
				size = GetTypeSize(node->type - 1, node->cType);
				rhs = MakeASTBinary(A_AssignDifference, node->type, node, MakeASTLeaf(A_LitInt, P_LongLong, FlexInt(size)), FlexNULL());
			}
			return MakeASTNodeEx(A_Decrement, node->type, node, NULL, rhs, FlexInt(1), FlexInt(size), NULL);
		}
		case T_Ampersand:{
			GetToken();
			ASTNode* fctr = ParseFactor();
			if(!fctr->lvalue)		FatalM("The Address operator may only be used on variable references!", Line);
			PrimordialType t = fctr->type;
			if(t == P_Undefined)	FatalM("Expression type not determined!", Line);
			if((t & 0xF) == 0xF)	FatalM("Indirection limit exceeded!", Line);
			return MakeASTNode(A_AddressOf,		fctr->type + 1,	fctr,	NULL,	NULL,	FlexNULL(), fctr->cType);
		}
		case T_Asterisk:{
			GetToken();
			ASTNode* fctr = ParseFactor();
			PrimordialType t = fctr->type;
			if(t == P_Undefined)		FatalM("Expression type not determined!", Line);
			if(!(t & 0xF))				FatalM("Can't dereference a non-pointer!", Line);
			return MakeASTNode(A_Dereference,	fctr->type - 1,	fctr,	NULL,	NULL,	FlexNULL(), fctr->cType);
		}
		case T_Sizeof:{
			GetToken();
			if(GetToken()->type != T_OpenParen)		FatalM("Expected open parenthesis after 'sizeof'!", Line);
			PrimordialType type = PeekType();
			if(type == P_Undefined){
				ASTNode* expr = ParseExpression();
				if(GetToken()->type != T_CloseParen)	FatalM("Expected close parenthesis after 'sizeof'!", Line);
				return MakeASTLeaf(A_LitInt, P_Char, FlexInt(GetTypeSize(expr->type, expr->cType)));
			}
			type = ParseType(NULL);
			if(type == P_Undefined)					FatalM("Expected typename!", Line);
			SymEntry* cType = (type == P_Composite) ? ParseCompRef(&type) : NULL;
			if(GetToken()->type != T_CloseParen)	FatalM("Expected close parenthesis after 'sizeof'!", Line);
			return MakeASTLeaf(A_LitInt, P_Char, FlexInt(GetTypeSize(type, cType)));
		}
		case T_OpenParen:{
			fpos_t* fpos = malloc(sizeof(fpos_t));
			fgetpos(fptr, fpos);
			int ln = Line;
			const char* file = curFile;
			GetToken();
			PrimordialType type = ParseType(NULL);
			bool failed = type == P_Undefined;
			while(!failed){
				SymEntry* cType = (type == P_Composite) ? ParseCompRef(&type) : NULL;
				if(GetToken()->type != T_CloseParen) {	failed = true;	break; }
				ASTNode* expr = ParseExpression();
				if(expr == NULL)		FatalM("Got NULL instead of expression! (Internal @ parse.h)", __LINE__);
				// Still need to handle narrowing manually... :'(
				if(expr->op == A_LitInt){
					size_t maxValue = (size_t)~0 >> 8 * (sizeof(size_t) - GetTypeSize(type, cType));
					if(expr->value.intVal > maxValue)
						expr->value.intVal &= maxValue;
					expr->type = type;
					expr->cType = cType;
					return expr;
				}
				return MakeASTNode(A_Cast, type, expr, NULL, NULL, FlexNULL(), cType);
			}
			if(failed){
				curFile = file;
				Line = ln;
				fsetpos(fptr, fpos);
				free(fpos);
			}
			// FALL THROUGH
		}
		default:	return ParsePrimary();
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
	PrimordialType type = GetWidestType((then != NULL ? then->type : condition->type), otherwise->type);
	if(type == P_Undefined)					FatalM("Types of expression members are incompatible!", Line);
	return MakeASTNode(A_Ternary, type, condition, then, otherwise, FlexNULL(), NULL);
}

ASTNode* ParseAssignmentExpression(){
	ASTNode* lhs = ParseConditionalExpression();
	if(lhs == NULL)					FatalM("Got NULL instead of expression! (In parse.h)", __LINE__);
	if(!lhs->lvalue)				return lhs;
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
	if(IsPointer(lhs->type))
		if(nt == A_AssignSum || nt == A_AssignDifference)	rhs = ScaleNode(rhs, lhs->type);
		else if(nt != A_Assign)								FatalM("Invalid operands to compound assignment!", Line);
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
	return MakeASTUnary(A_Return, expr, FlexNULL(), expr->cType);
}

ASTNode* ParseDeclaration(){
	StorageClass sc = C_Default;
	PrimordialType type = ParseType(&sc);
	if(sc && scope)					FatalM("External locals not yet supported!", Line);
	if(type == P_Undefined)			FatalM("Expected typename!", Line);
	SymEntry* cType = NULL;
	if((type & 0xF0) == P_Composite){
		cType = ParseCompRef(&type);
		if(cType == NULL && (type & 0xF0) == P_Composite)
			FatalM("Undefined composite!", Line);
	}
	Token* tok = GetToken();
	if(tok->type != T_Identifier)	FatalM("Expected identifier!", Line);
	const char* id = tok->value.strVal;
	InsertVar(id, NULL, type, cType, sc, scope);
	if (PeekToken()->type != T_Equal){
		ASTNode* n = MakeASTNodeEx(A_Declare, type, NULL, NULL, NULL, FlexStr(id), FlexInt(sc), cType);
		n->sClass = sc;
		return n;
	}
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
		if(typeSize < 8) // If expression result is too large to fit in variable, truncate it
			if(expr->value.intVal >= ((long long)1 << (8 * typeSize)))
				expr->value.intVal &= ((long long)1 << (8 * typeSize)) - 1;
		ASTNode* n = MakeASTNodeEx(A_Declare, type, expr, NULL, NULL, FlexStr(id), FlexInt(sc), cType);
		n->sClass = sc;
		return n;
	}
	ASTNode* n = MakeASTNodeEx(A_Declare, type, expr, NULL, NULL, FlexStr(id), FlexInt(sc), cType);
	n->sClass = sc;
	return n;
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
	loopDepth++;
	ASTNode* stmt = ParseStatement();
	loopDepth--;
	return MakeASTBinary(A_While, P_Undefined, condition, stmt, FlexNULL());
}

ASTNode* ParseDoLoop(){
	if(GetToken()->type != T_Do)			FatalM("Expected 'do' to begin do-while loop!", Line);
	loopDepth++;
	ASTNode* stmt = ParseStatement();
	loopDepth--;
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
	if(PeekType() != P_Undefined)
		initializer = ParseDeclaration();
	else switch(PeekToken()->type){
		case T_Semicolon:	break;
		default:			initializer = ParseExpression();	break;
	}
	if(GetToken()->type != T_Semicolon)	FatalM("Expected semicolon in for loop!", Line);
	ASTNode* condition	= PeekToken()->type == T_Semicolon ? NULL : ParseExpression();
	if(GetToken()->type != T_Semicolon)	FatalM("Expected semicolon in for loop!", Line);
	ASTNode* modifier	= PeekToken()->type == T_CloseParen ? NULL : ParseExpression();
	if(GetToken()->type != T_CloseParen)	FatalM("Expected close parenthesis ')' in for loop!", Line);
	loopDepth++;
	ASTNode* stmt = ParseStatement();
	loopDepth--;
	ASTNode* header = MakeASTNode(A_Glue, P_Undefined, initializer, condition, modifier, FlexNULL(), NULL);
	ExitScope();
	return MakeASTBinary(A_For, P_Undefined, header, stmt, FlexNULL());
}

ASTNode* ParseSwitch(){
	if(GetToken()->type != T_Switch)		FatalM("Expected 'switch' keyword to begin switch statement!", Line);
	if(GetToken()->type != T_OpenParen)		FatalM("Expected open parenthesis '(' in switch statement!", Line);
	ASTNode* expr = ParseExpression();
	int typeCompat = CheckTypeCompatibility(expr->type, P_Int);
	if(typeCompat == TYPES_INCOMPATIBLE || typeCompat == TYPES_WIDEN_RHS)
		FatalM("Incompatible expression type in switch statement! Expression must be of integral type!", Line);
	if(expr == NULL)						FatalM("Expected expression in switch statement!", Line);
	if(GetToken()->type != T_CloseParen)	FatalM("Expected close parenthesis ')' in switch statement!", Line);
	if(GetToken()->type != T_OpenBrace)		FatalM("Expected open brace '{' to denote body of switch statement!", Line);
	ASTNodeList* cases = MakeASTNodeList();
	switchDepth++;
	bool seenDefault = false;
	while(PeekToken()->type != T_CloseBrace){
		int* caseValue = NULL;
		int op = A_Undefined;
		switch(GetToken()->type){
			case T_Case:{
				ASTNode* caseValExpr = ParseExpression();
				if(caseValExpr->op != A_LitInt)				FatalM("Case condition must be a literal integer!", Line);
				caseValue = malloc(sizeof(int));
				*caseValue = caseValExpr->value.intVal;
				op = A_Case;
				for(int i = 0; i < cases->count; i++){
					ASTNode* node = cases->nodes[i];
					if(node->op != A_Case)					continue;
					if(node->value.intVal == *caseValue)	FatalM("Duplicate case labels are not permitted in switch statement!", Line);
				}
				break;
			}
			case T_Default:{
				if(seenDefault)		FatalM("Multiple 'default' cases in switch statement!", Line);
				op = A_Default;
				seenDefault = true;
				break;
			}
			default:						FatalM("Expected either 'case' or 'default' in switch statement!", Line);
		}
		if(GetToken()->type != T_Colon)		FatalM("Expected colon ':' following case!", Line);
		Token* tok = PeekToken();
		ASTNodeList* caseActions = MakeASTNodeList();
		while(tok->type != T_Case && tok->type != T_Default && tok->type != T_CloseBrace){
			ASTNode* stmt = ParseStatement();
			if(stmt->op == A_Declare)		FatalM("Declarations not allowed directly in switches! Use a compound statement!", Line);
			AddNodeToASTList(caseActions, stmt);
			tok = PeekToken();
		}
		AddNodeToASTList(cases, MakeASTList(op, caseActions, caseValue ? FlexInt(*caseValue) : FlexNULL()));
		if(caseValue != NULL)
			free(caseValue);
	}
	switchDepth--;
	if(GetToken()->type != T_CloseBrace)	FatalM("Expected close brace '}' after switch statement!", Line);
	if(cases->count == 0){
		WarnM("No cases provided to switch!", Line);
		return expr;
	}
	ASTNode* switchNode = MakeASTList(A_Switch, cases, FlexNULL());
	switchNode->lhs = expr;
	return switchNode;
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
		case T_Continue:
			if(!loopDepth)					FatalM("A 'continue' statement may only be used inside of a loop!", Line);
			GetToken();
			return MakeASTLeaf(A_Continue, P_Undefined, FlexNULL());
		case T_Break:
			if(!loopDepth && !switchDepth)	FatalM("A 'break' statement may only be used inside of a switch or loop!", Line);
			GetToken();
			return MakeASTLeaf(A_Break, P_Undefined, FlexNULL());
		case T_Switch:		return ParseSwitch();
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
	StorageClass sc = C_Default;
	PrimordialType type = ParseType(&sc);
	if(type == P_Undefined)					FatalM("Invalid function declaration; Expected typename.", Line);
	SymEntry* cType = NULL;
	if((type & 0xF0) == P_Composite){
		cType = ParseCompRef(&type);
		if(cType == NULL && (type & 0xF0) == P_Composite)
			FatalM("Undefined composite!", Line);
	}
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
		if(PeekToken()->type == T_Ellipsis){
			GetToken();
			if(PeekToken()->type != T_CloseParen)	FatalM("Varaidic parameters must be the last parameter in a function prototype!", Line);
			params = MakeParam("...", P_Void, NULL, params);
			break;
		}
		PrimordialType paramType = ParseType(NULL);
		if(paramType == P_Undefined)		FatalM("Invalid type in parameter list!", Line);
		SymEntry* cType = NULL;
		if(paramType == P_Composite){
			cType = ParseCompRef(&paramType);
			if(paramType == P_Composite){
				int size = GetTypeSize(paramType, cType);
				if(size > 8){
					// FatalM("Value composite parameters >64bit in size are not yet supported!", Line);
				}
			}
		}
		Token* t = GetToken();
		if(t->type != T_Identifier)			FatalM("Expected identifier in parameter list!", Line);
		int charCount = strlen(t->value.strVal) + 1;
		char* paramName = malloc(charCount * sizeof(char));
		paramName = strncpy(paramName, t->value.strVal, charCount);
		free(t);
		params = MakeParam(paramName, paramType, cType, params);
	}
	if(params != NULL)
		while (params->prev != NULL)
			params = params->prev;
	if(GetToken()->type != T_CloseParen)	FatalM("Invalid function declaration; Expected close parenthesis ')'.", Line);
	InsertFunc(idStr, FlexPtr(params), type, cType);
	if(PeekToken()->type == T_Semicolon){
		GetToken();
		ASTNode* n = MakeASTNodeEx(A_Function, type, NULL, NULL, NULL, FlexStr(idStr), FlexPtr(params), cType);
		n->sClass = sc;
		return n;
	}
	EnterScope();
	if(params != NULL){
		Parameter* p = params;
		do {
			InsertVar(p->id, NULL, p->type, p->cType, sc, scope);
			p = p->next;
		} while( p != NULL);
	}
	if(PeekToken()->type != T_OpenBrace)	FatalM("Invalid function declaration; Expected open brace '{'.", Line);
	ASTNode* block = ParseBlock();
	ExitScope();
	ASTNode* n = MakeASTNodeEx(A_Function, type, block, NULL, NULL, FlexStr(idStr), FlexPtr(params), cType);
	n->sClass = sc;
	return n;
}

ASTNode* ParseEnumDeclaration(){
	const char* identifier = NULL;
	if(PeekToken()->type == T_Identifier){
		identifier = GetToken()->value.strVal;
		InsertEnumName(identifier);
	}
	if(PeekToken()->type == T_Semicolon){
		// incomplete type -- Never hit due to lookahead behaviour in ParseNode()
		FatalM("Incomplete enum declarations not yet supported!", Line);
	}
	if(GetToken()->type != T_OpenBrace)		FatalM("Expected open brace '{' in enum declaration!", Line);
	ASTNodeList* values = MakeASTNodeList();
	int lastValue = -1;
	while(PeekToken()->type == T_Identifier){
		PrimordialType type = P_Undefined;
		int value = lastValue + 1;
		const char* id = GetToken()->value.strVal;
		if(PeekToken()->type == T_Equal){
			GetToken();
			ASTNode* expr = ParseExpression();
			// TODO: Fold constant expressions
			if(expr->op != A_LitInt)	FatalM("Only integer and character literals are supported at this time! (Internal @ parse.h)", __LINE__);
			value = expr->value.intVal;
		}
		if(NULL == InsertEnumValue(id, value))
			FatalM("Failed to create enum value!", Line);
		AddNodeToASTList(values, MakeASTNodeEx(A_EnumValue, type, NULL, NULL, NULL, FlexStr(id), FlexInt(value), NULL));
		lastValue = value;
		if(PeekToken()->type == T_Comma)
			GetToken();
	}
	if (GetToken()->type != T_CloseBrace)
		FatalM("Expected close brace after Enum declaration!", Line);
	if(GetToken()->type != T_Semicolon)
		FatalM("Expected semicolon after Enum declaration!", Line);
	// Add enum values to symtable
	return MakeASTList(A_EnumDecl, values, FlexStr(identifier));
}

ASTNode* ParseCompositeDeclaration(){
	Token* cTok = GetToken();
	if(cTok->type == T_Enum)	return ParseEnumDeclaration();
	if(cTok->type != T_Struct && cTok->type != T_Union)
		FatalM("Expected composite type!", Line);
	Token* tok = PeekToken();
	// if(tok->type != T_Identifier)			FatalM("Anonymous composites not yet implemented!", Line);
	const char* identifier = NULL;
	if(tok->type == T_Identifier){
		identifier = tok->value.strVal;
		GetToken();
	}
	SymList* incomplete = cTok->type == T_Struct
		? InsertStruct(identifier,	NULL)
		: InsertUnion(identifier,	NULL);
	if(PeekToken()->type == T_Semicolon){
		// incomplete type -- Never hit due to lookahead behaviour in ParseNode()
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
		? UpdateStruct(incomplete,	identifier,	MakeCompMembers(memberNodes))
		: UpdateUnion(incomplete,	identifier,	MakeCompMembers(memberNodes));
	if(list == NULL)						FatalM("Failed to create composite definition! (In parse.h)", __LINE__);
	if(PeekToken()->type != T_Identifier){
		if(GetToken()->type != T_Semicolon)	FatalM("Expected semicolon after composite declaration!", Line);
		return MakeASTList(A_StructDecl, memberNodes, FlexStr(identifier));
	}
	if(identifier != NULL){
		while(!streq(list->item->key, identifier) && list->next != NULL)
			list = list->next;
		if(!streq(list->item->key, identifier))	FatalM("Failed to find composite definition after creation! (In parse.h)", __LINE__);
	}
	SymEntry* entry = list->item;
	FlexibleValue declName = GetToken()->value;
	ASTNode* varDecl = MakeASTLeaf(A_Declare, P_Composite, declName);
	varDecl->cType = entry;
	InsertVar(declName.strVal, NULL, P_Composite, entry, C_Default, scope);
	if(GetToken()->type != T_Semicolon)		FatalM("Expected semicolon after struct declaratioin!", Line);
	ASTNode* ret = MakeASTList(A_StructDecl, memberNodes, FlexStr(identifier));
	ret->lhs = varDecl;
	return ret;
}

ASTNode* ParseTypedef(){
	if(GetToken()->type != T_Typedef)	FatalM("Expected 'typedef' keyword to begin typedef!", Line);
	Token* advCTok = PeekToken();
	Token* advITok = PeekTokenN(1);
	PrimordialType type = ParseType(NULL);
	SymEntry* cType = NULL;
	bool advDecl = false;
	if(type == P_Composite){
		cType = ParseCompRef(&type);
		if(cType == NULL && advCTok->type != T_Enum){
			if(advITok->type != T_Identifier)	FatalM("Expected identifier in advance declaration typedef!", Line);
			advDecl = true;
			SymList* list = NULL;
			switch(advCTok->type){
				case T_Union:	list = InsertUnion(advITok->value.strVal,	NULL);	break;
				case T_Struct:	list = InsertStruct(advITok->value.strVal,	NULL);	break;
				default:		FatalM("Unexpected composite type! (Internal @ parse.h)", __LINE__);
			}
			if(list == NULL || list->item == NULL)
				FatalM("Failed to create shadown composite! (Internal @ parse.h)", __LINE__);
			cType = list->item;
			
		}
	}
	free(advCTok);
	free(advITok);
	Token* tok = GetToken();
	if (tok->type != T_Identifier){
		if (tok->type != T_Semicolon)	FatalM("Expected semicolon after typedef!", Line);
		if(!advDecl)					FatalM("Anonymous typedefs not yet supported!", Line);
		return MakeASTLeaf(A_Undefined, P_Undefined, FlexNULL());
	}
	const char* id = tok->value.strVal;
	if(GetToken()->type != T_Semicolon)	FatalM("Expected semicolon after typedef!", Line);
	if(NULL == InsertTypedef(id, type, cType))	FatalM("Failed to create typedef SymEntry!", Line);
	return MakeASTLeaf(A_Undefined, P_Undefined, FlexNULL());
}

ASTNode* ParseNode(){
	int i = 0;
	Token* tok = PeekTokenN(i);
	if(tok->type == T_Typedef)
		return ParseTypedef();
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