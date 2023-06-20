#ifndef TYPES_INCLUDED
#define TYPES_INCLUDED

#include "globals.h"

enum eTokenCategory {
	T_Undefined = 0,
	T_Type,
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
};
typedef enum eTokenCategory TokenType;

enum eNodeType {
	A_Undefined = 0,
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
};
typedef enum eNodeType NodeType;

union flexible_value {
	int intVal;
	const char* strVal;
};
typedef union flexible_value FlexibleValue;

struct token {
	TokenType type;
	FlexibleValue value;
};
typedef struct token Token;

struct ast_node {
	NodeType op;
	struct ast_node *lhs;
	struct ast_node *mid;
	struct ast_node *rhs;
	struct ast_node_list *list;
	FlexibleValue value;
	FlexibleValue secondValue;
};
typedef struct ast_node ASTNode;

struct ast_node_list {
	ASTNode** nodes;
	int count;
	int size;
};
typedef struct ast_node_list ASTNodeList;

ASTNodeList* MakeASTNodeList(){
	ASTNodeList* list = malloc(sizeof(ASTNodeList));
	list->size = 10;
	list->nodes = malloc(list->size * sizeof(ASTNode*));
	list->count = 0;
}

ASTNodeList* AddNodeToASTList(ASTNodeList* list, ASTNode* node){
	while(list->count >= list->size){
		list->size += 10;
		list->nodes = realloc(list->nodes, list->size);
	}
	list->nodes[list->count++] = node;
	return list;
}

ASTNode* MakeASTNodeExtended(int op, ASTNode* lhs, ASTNode* mid, ASTNode* rhs, FlexibleValue value, FlexibleValue secondValue){
	ASTNode* node = malloc(sizeof(ASTNode));
	if(node == NULL)
		FatalM("Failed to malloc in MakeASTNode()", Line);
	node->op = op;
	node->lhs = lhs;
	node->mid = mid;
	node->rhs = rhs;
	node->value = value;
	node->secondValue = secondValue;
	return node;
}

ASTNode* MakeASTNode (int op, ASTNode* lhs, ASTNode* mid, ASTNode* rhs, FlexibleValue value){
	ASTNode* node = malloc(sizeof(ASTNode));
	if(node == NULL)
		FatalM("Failed to malloc in MakeASTNode()", Line);
	node->op = op;
	node->lhs = lhs;
	node->mid = mid;
	node->rhs = rhs;
	node->value = value;
	node->secondValue.strVal = NULL;
	return node;
}

ASTNode* MakeASTBinary(int op, ASTNode* lhs, ASTNode* rhs, FlexibleValue value){
	return MakeASTNode(op, lhs, NULL, rhs, value);
}

ASTNode* MakeASTLeaf(int op, FlexibleValue value){
	return MakeASTBinary(op, NULL, NULL, value);
}

ASTNode* MakeASTUnary(int op, ASTNode* lhs, FlexibleValue value){
	return MakeASTBinary(op, lhs, NULL, value);
}

ASTNode* MakeASTList(int op, ASTNodeList* list, FlexibleValue value){
	ASTNode* node = MakeASTLeaf(op, value);
	node->list = list;
}

FlexibleValue FlexStr(const char* str){
	FlexibleValue f = { .strVal = str };
	return f;
}

FlexibleValue FlexInt(int num){
	FlexibleValue f = { .intVal = num };
	return f;
}

FlexibleValue FlexNULL(){
	return FlexStr(NULL);
}

#endif