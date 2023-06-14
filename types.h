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
	int op;
	struct ast_node *lhs;
	struct ast_node *rhs;
	union {
		int intVal;
		char* strVal;
	} value;
};
typedef struct ast_node ASTNode;

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

#endif