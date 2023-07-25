#ifndef TYPES_INCLUDED
#define TYPES_INCLUDED

#include "globals.h"

#define TYPES_COMPATIBLE	1
#define TYPES_INCOMPATIBLE	0
#define TYPES_WIDEN_LHS		-1
#define TYPES_WIDEN_RHS		-2
typedef enum ePrimordialType PrimordialType;
typedef enum eTokenCategory TokenType;
typedef enum eNodeType NodeType;
typedef enum eStorageClass StorageClass;
typedef union flexible_value FlexibleValue;
typedef struct doubly_linked_list DbLnkList;
typedef struct param Parameter;
typedef struct token Token;
typedef struct ast_node ASTNode;
typedef struct ast_node_list ASTNodeList;
typedef enum eStructuralType StructuralType;
typedef struct SymEntry SymEntry;
typedef struct SymList SymList;


// Bottom nibble stores level of reference
// EG:
// 0x20 = char
// 0x21 = char*
// 0x22 = char**
enum ePrimordialType {
	P_Undefined	= 0,
	P_Void		= 0x10,
	P_Char		= 0x20,
	P_Int		= 0x30,
	P_Long		= 0x40,
	P_LongLong	= 0x50,
	P_Composite	= 0x60,
};

enum eTokenCategory {
	T_Undefined = 0,
	T_Void,
	T_Char,
	T_Int,
	T_Long,
	T_Identifier,
	T_OpenParen,
	T_CloseParen,
	T_OpenBrace,
	T_CloseBrace,
	T_Return,
	T_Semicolon,
	T_LitInt,
	T_Minus,
	T_Bang,
	T_Tilde,
	T_Plus,
	T_Asterisk,
	T_Divide,
	T_Percent,
	T_Less,
	T_Greater,
	T_LessEqual,
	T_GreaterEqual,
	T_DoubleEqual,
	T_BangEqual,
	T_Ampersand,
	T_Caret,
	T_Pipe,
	T_DoubleAmpersand,
	T_DoublePipe,
	T_DoubleLess,
	T_DoubleGreater,
	T_Equal,
	T_Question,
	T_Colon,
	T_If,
	T_Else,
	T_While,
	T_Do,
	T_For,
	T_Break,
	T_Continue,
	T_PlusEqual,
	T_MinusEqual,
	T_AsteriskEqual,
	T_DivideEqual,
	T_PercentEqual,
	T_DoubleLessEqual,
	T_DoubleGreaterEqual,
	T_AmpersandEqual,
	T_CaretEqual,
	T_PipeEqual,
	T_PlusPlus,
	T_MinusMinus,
	T_Comma,
	T_OpenBracket,
	T_CloseBracket,
	T_LitStr,
	T_Struct,
	T_Arrow,
	T_Period,
	T_Union,
	T_Enum,
	T_Typedef,
	T_Octothorp,
	T_Ellipsis,
	T_Extern,
	T_Switch,
	T_Case,
	T_Default,
	T_Sizeof,
	T_Static,
};

enum eNodeType {
	A_Undefined = 0,
	A_Glue,
	A_Function,
	A_Return,
	A_LitInt,
	A_Negate,
	A_LogicalNot,
	A_BitwiseComplement,
	A_Multiply,
	A_Divide,
	A_Modulo,
	A_Add,
	A_Subtract,
	A_LeftShift,
	A_RightShift,
	A_LessThan,
	A_GreaterThan,
	A_LessOrEqual,
	A_GreaterOrEqual,
	A_EqualTo,
	A_NotEqualTo,
	A_BitwiseAnd,
	A_BitwiseXor,
	A_BitwiseOr,
	A_LogicalAnd,
	A_LogicalOr,
	A_Declare,
	A_VarRef,
	A_Assign,
	A_Block,
	A_Ternary,
	A_If,
	A_While,
	A_Do,
	A_For,
	A_Continue,
	A_Break,
	A_Increment,
	A_Decrement,
	A_AssignSum,
	A_AssignDifference,
	A_AssignProduct,
	A_AssignQuotient,
	A_AssignModulus,
	A_AssignLeftShift,
	A_AssignRightShift,
	A_AssignBitwiseAnd,
	A_AssignBitwiseXor,
	A_AssignBitwiseOr,
	A_FunctionCall,
	A_AddressOf,
	A_Dereference,
	A_LitStr,
	A_StructDecl,
	A_EnumDecl,
	A_EnumValue,
	A_Switch,
	A_Case,
	A_Default,
	A_Cast,
};

enum eStorageClass{
	C_Default	= 0,
	C_Extern,
	C_Static,
};

union flexible_value {
	long long intVal;
	const char* strVal;
	void* ptrVal;
};

struct doubly_linked_list {
	void* val;
	DbLnkList* prev;
	DbLnkList* next;
};

struct param {
	const char* id;
	PrimordialType type;
	Parameter* next;
	Parameter* prev;
};

struct token {
	TokenType type;
	FlexibleValue value;
};

struct ast_node {
	NodeType op;
	PrimordialType type;
	struct ast_node *lhs;
	struct ast_node *mid;
	struct ast_node *rhs;
	struct ast_node_list *list;
	FlexibleValue value;
	FlexibleValue secondaryValue;
	SymEntry* cType;
	StorageClass sClass;
	bool lvalue;
};

struct ast_node_list {
	ASTNode** nodes;
	int count;
	int size;
};

enum eStructuralType {
	S_Undefined = 0,
	S_Variable,
	S_Function,
	S_Composite,
	S_Member,
	S_EnumName,
	S_EnumValue,
	S_Typedef,
};

struct SymEntry {
	const char* key;
	FlexibleValue value;
	FlexibleValue sValue; // Secondary Value
	PrimordialType type;
	StructuralType sType;
	SymEntry* cType; // Composite Type
};

struct SymList {
	SymEntry* item;
	SymList* next;
};

FlexibleValue FlexNULL();

ASTNodeList* MakeASTNodeList(){
	ASTNodeList* list = malloc(sizeof(ASTNodeList));
	list->size = 10;
	list->nodes = malloc(list->size * sizeof(ASTNode*));
	list->count = 0;
}

ASTNodeList* AddNodeToASTList(ASTNodeList* list, ASTNode* node){
	while(list->count >= list->size){
		list->size += 10;
		list->nodes = realloc(list->nodes, list->size * sizeof(ASTNode*));
	}
	list->nodes[list->count++] = node;
	return list;
}

ASTNode* MakeASTNodeEx(int op, PrimordialType type, ASTNode* lhs, ASTNode* mid, ASTNode* rhs, FlexibleValue value, FlexibleValue secondValue, SymEntry* cType){
	ASTNode* node = malloc(sizeof(ASTNode));
	if(node == NULL)
		FatalM("Failed to malloc in MakeASTNode()", Line);
	node->op = op;
	node->type = type;
	node->lhs = lhs;
	node->mid = mid;
	node->rhs = rhs;
	node->value = value;
	node->secondaryValue = secondValue;
	node->list = NULL;
	node->cType = cType;
	switch(op){
		case A_VarRef:
		case A_Dereference:		node->lvalue = true;	break;
		default:				node->lvalue = false;	break;
	}
	return node;
}

ASTNode* MakeASTNode(int op, PrimordialType type, ASTNode* lhs, ASTNode* mid, ASTNode* rhs, FlexibleValue value, SymEntry* cType){
	return MakeASTNodeEx(op, type, lhs, mid, rhs, value, FlexNULL(), cType);
}

ASTNode* MakeASTBinary(int op, PrimordialType type, ASTNode* lhs, ASTNode* rhs, FlexibleValue value){
	return MakeASTNode(op, type, lhs, NULL, rhs, value, NULL);
}

ASTNode* MakeASTLeaf(int op, PrimordialType type, FlexibleValue value){
	return MakeASTNode(op, type, NULL, NULL, NULL, value, NULL);
}

ASTNode* MakeASTUnary(int op, ASTNode* lhs, FlexibleValue value, SymEntry* cType){
	return MakeASTNode(op, lhs->type, lhs, NULL, NULL, value, cType);
}

ASTNode* MakeASTList(int op, ASTNodeList* list, FlexibleValue value){
	ASTNode* node = MakeASTLeaf(op, P_Undefined, value);
	node->list = list;
}

FlexibleValue FlexStr(const char* str){
	FlexibleValue f;
	f.strVal = str;
	return f;
}

FlexibleValue FlexInt(long long num){
	FlexibleValue f;
	f.intVal = num;
	return f;
}

FlexibleValue FlexPtr(void* ptr){
	FlexibleValue f;
	f.ptrVal = ptr;
	return f;
}

FlexibleValue FlexNULL(){
	return FlexPtr(NULL);
}

int GetPrimSize(PrimordialType prim){
	if(prim & 0xF)			return 8; // Pointer
	switch(prim){
		case P_Undefined:	return 0;
		case P_Void:		return 0;
		case P_Char:		return 1;
		case P_Int:			return 4;
		case P_Long:		return 4;
		case P_LongLong:	return 8;
		default:			FatalM("Unhandled primordial in GetPrimSize()! (In types.h)", __LINE__);
	}
}

int GetTypeSize(PrimordialType type, SymEntry* compositeType){
	if(type & 0x0F)			return 8; // Pointer
	switch(type & 0xF0){
		case P_Composite:
			if(compositeType == NULL)	FatalM("Expected composite type! (In types.h)", __LINE__);
			if(compositeType->sValue.intVal == -1)
				FatalM("Invalid use of incomplete type!", Line);
			return compositeType->sValue.intVal;
		default:	return GetPrimSize(type);
	}
}

bool IsPointer(PrimordialType prim){
	return (prim & 0x0F) != 0;
}

bool IsIntegral(PrimordialType type){
	switch(type){
		case P_Char:	return true;
		case P_Int:		return true;
		case P_Long:	return true;
		default:		return false;
	}
}

int CheckTypeCompatibility(PrimordialType lhs, PrimordialType rhs){
	if(lhs == P_Void	|| rhs == P_Void)			return TYPES_INCOMPATIBLE;
	if(lhs == rhs)									return TYPES_COMPATIBLE;
	if (lhs == P_Composite || rhs == P_Composite)	return TYPES_INCOMPATIBLE;
	char lptr = lhs & 0x0F; // The level of indirection of lhs
	char rptr = rhs & 0x0F; // The level of indirection of rhs
	int lbase = lhs & 0xF0; // The base type of lhs
	int rbase = rhs & 0xF0; // The base type of rhs
	if((lptr) && (rptr) && (lbase == P_Void || rbase == P_Void))	return TYPES_COMPATIBLE;
	if(lptr && rptr && (lptr != rptr))								return TYPES_INCOMPATIBLE; // Different level pointers

	int sizeL = GetPrimSize(lhs);
	int sizeR = GetPrimSize(rhs);
	if(sizeL ==	sizeR)						return TYPES_COMPATIBLE;
	if(sizeL >	sizeR)						return TYPES_WIDEN_RHS;
	if(sizeL <	sizeR)						return TYPES_WIDEN_LHS;
	return TYPES_INCOMPATIBLE;
}

PrimordialType GetWidestType(PrimordialType lhs, PrimordialType rhs){
	switch(CheckTypeCompatibility(lhs, rhs)){
		case TYPES_INCOMPATIBLE:	return P_Undefined;
		case TYPES_WIDEN_LHS:		return rhs;
		default:					return lhs;
	}
}

ASTNode* ScaleNode(ASTNode* node, PrimordialType complement){
	if(!IsPointer(complement))	return node;
	if(IsPointer(node->type))	FatalM("Cannot scale pointers! (In types.h)", __LINE__);
	complement -= 0x01; // Decrement level of indirection
	return MakeASTBinary(A_Multiply, node->type, node, MakeASTLeaf(A_LitInt, P_Int, FlexInt(GetPrimSize(complement))), FlexNULL());
}

ASTNode* WidenNode(ASTNode* node, PrimordialType complement, NodeType op){
	if(IsPointer(complement))	return ScaleNode(node, complement);
	PrimordialType widest = GetWidestType(node->type, complement);
	if(widest == P_Undefined)	FatalM("Types incompatible!", Line);
	if(widest == node->type)	return node;
	// return MakeASTNode(A_WIDEN, complement, node, NULL, NULL, FlexNULL());
	return node;
}

Parameter* MakeParam(const char* id, PrimordialType type, Parameter* prev){
	Parameter* p = malloc(sizeof(Parameter));
	p->id = id;
	p->type = type;
	p->prev = prev;
	if (prev != NULL) {
		p->next = prev->next;
		prev->next = p;
	}
	else
		p->next = NULL;
	return p;
}

DbLnkList* MakeDbLnkList(void* val, DbLnkList* prev, DbLnkList* next){
	DbLnkList* l = malloc(sizeof(DbLnkList));
	l->val	= val;
	l->prev	= prev;
	l->next	= next;
}

SymEntry* GetMember(SymEntry* structDef, const char* member){
	SymEntry* members = structDef->value.ptrVal;
	if(members == NULL)		FatalM("Struct definition contained no members! (Internal @ types.h)", __LINE__);
	while(!streq(members->key, member) && members->sValue.ptrVal != NULL)
		members = members->sValue.ptrVal;
	if(!streq(members->key, member))	FatalM("Invalid member!", Line);
	return members;
}


#define NodeTypesCompatible(lhs, rhs) CheckTypeCompatibility(lhs->type, rhs->type)
#define NodeWidestType(lhs, rhs) GetWidestType(lhs->type, rhs->type)

#endif