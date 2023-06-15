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
};
typedef enum eNodeType NodeType;

struct token {
	TokenType type;
	union{
		int intVal;
		const char* strVal;
	} value;
};
typedef struct token Token;

struct ast_node {
	NodeType op;
	struct ast_node *lhs;
	struct ast_node *rhs;
	struct ast_node_list *list;
	union {
		int intVal;
		char* strVal;
	} value;
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

ASTNode* MakeASTNode(int op, ASTNode* lhs, ASTNode* rhs, int intValue, const char* strValue){
	ASTNode* node = malloc(sizeof(ASTNode));
	if(node == NULL)
		FatalM("Failed to malloc in MakeASTNode()", Line);
	node->op = op;
	node->lhs = lhs;
	node->rhs = rhs;
	if(strValue != NULL){
		node->value.strVal = malloc((1 + strlen(strValue)) * sizeof(char));
		strcpy(node->value.strVal, strValue);
	}
	else
		node->value.intVal = intValue;
	return node;
}

ASTNode* MakeASTLeaf(int op, int intValue, const char* strValue){
	return MakeASTNode(op, NULL, NULL, intValue, strValue);
}

ASTNode* MakeASTUnary(int op, ASTNode* lhs, int intValue, const char* strValue){
	return MakeASTNode(op, lhs, NULL, intValue, strValue);
}

ASTNode* MakeASTList(int op, ASTNodeList* list, int intValue, const char* strValue){
	ASTNode* node = MakeASTLeaf(op, intValue, strValue);
	node->list = list;
}

#endif