#ifndef TYPES_INCLUDED
#define TYPES_INCLUDED

#include <stdlib.h>
#include <string.h>

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
	P_UChar		= 0x70,
	P_UInt		= 0x80,
	P_ULong		= 0x90,
	P_ULongLong	= 0xA0,
};
#define P_UNSIGNED_DIFF (P_UChar - P_Char)
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
	T_Unsigned,
	T_EqualDoublePipe,
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
	A_ExpressionList,
	A_RepeatLogicalOr,
	A_BuiltinCall,
	A_RawASM,
	A_Logicize,
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
	SymEntry* cType;
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

ASTNodeList* MakeASTNodeList();
ASTNodeList* AddNodeToASTList(ASTNodeList* list, ASTNode* node);
ASTNode* MakeASTNodeEx(NodeType op, PrimordialType type, ASTNode* lhs, ASTNode* mid, ASTNode* rhs, FlexibleValue value, FlexibleValue secondValue, SymEntry* cType);
ASTNode* MakeASTNode(NodeType op, PrimordialType type, ASTNode* lhs, ASTNode* mid, ASTNode* rhs, FlexibleValue value, SymEntry* cType);
ASTNode* MakeASTBinary(NodeType op, PrimordialType type, ASTNode* lhs, ASTNode* rhs, FlexibleValue value);
ASTNode* MakeASTLeaf(NodeType op, PrimordialType type, FlexibleValue value);
ASTNode* MakeASTUnary(NodeType op, ASTNode* lhs, FlexibleValue value, SymEntry* cType);
ASTNode* MakeASTList(NodeType op, ASTNodeList* list, FlexibleValue value);
FlexibleValue FlexStr(const char* str);
FlexibleValue FlexInt(long long num);
FlexibleValue FlexPtr(void* ptr);
FlexibleValue FlexNULL();
int GetPrimSize(PrimordialType prim);
int GetTypeSize(PrimordialType type, SymEntry* compositeType);
bool IsUnsigned(PrimordialType prim);
bool IsPointer(PrimordialType prim);
bool IsIntegral(PrimordialType type);
int CheckTypeCompatibility(PrimordialType lhs, PrimordialType rhs);
PrimordialType GetWidestType(PrimordialType lhs, PrimordialType rhs);
ASTNode* ScaleNode(ASTNode* node, PrimordialType complement);
ASTNode* WidenNode(ASTNode* node, PrimordialType complement, NodeType op);
Parameter* MakeParam(const char* id, PrimordialType type, SymEntry* cType, Parameter* prev);
DbLnkList* MakeDbLnkList(void* val, DbLnkList* prev, DbLnkList* next);
SymEntry* GetMember(SymEntry* structDef, const char* member);


#define NodeTypesCompatible(lhs, rhs) CheckTypeCompatibility(lhs->type, rhs->type)
#define NodeWidestType(lhs, rhs) GetWidestType(lhs->type, rhs->type)

#endif