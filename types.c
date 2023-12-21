#include "types.h"

ASTNodeList* MakeASTNodeList(){
	ASTNodeList* list = malloc(sizeof(ASTNodeList));
	list->size = 10;
	list->nodes = malloc(list->size * sizeof(ASTNode*));
	list->count = 0;
	return list;
}

ASTNodeList* AddNodeToASTList(ASTNodeList* list, ASTNode* node){
	while(list->count >= list->size){
		list->size += 10;
		list->nodes = realloc(list->nodes, list->size * sizeof(ASTNode*));
	}
	list->nodes[list->count++] = node;
	return list;
}

ASTNode* MakeASTNodeEx(NodeType op, PrimordialType type, ASTNode* lhs, ASTNode* mid, ASTNode* rhs, FlexibleValue value, FlexibleValue secondValue, SymEntry* cType){
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
		case A_FunctionCall:
			node->lvalue = (type & 0xF) && ((type & 0xF0) == P_Composite);
			break;
		case A_VarRef:
		case A_Dereference:		node->lvalue = true;	break;
		default:				node->lvalue = false;	break;
	}
	return node;
}

ASTNode* MakeASTNode(NodeType op, PrimordialType type, ASTNode* lhs, ASTNode* mid, ASTNode* rhs, FlexibleValue value, SymEntry* cType){
	return MakeASTNodeEx(op, type, lhs, mid, rhs, value, FlexNULL(), cType);
}

ASTNode* MakeASTBinary(NodeType op, PrimordialType type, ASTNode* lhs, ASTNode* rhs, FlexibleValue value){
	return MakeASTNode(op, type, lhs, NULL, rhs, value, NULL);
}

ASTNode* MakeASTLeaf(NodeType op, PrimordialType type, FlexibleValue value){
	return MakeASTNode(op, type, NULL, NULL, NULL, value, NULL);
}

ASTNode* MakeASTUnary(NodeType op, ASTNode* lhs, FlexibleValue value, SymEntry* cType){
	return MakeASTNode(op, lhs->type, lhs, NULL, NULL, value, cType);
}

ASTNode* MakeASTList(NodeType op, ASTNodeList* list, FlexibleValue value){
	ASTNode* node = MakeASTLeaf(op, P_Undefined, value);
	node->list = list;
	return node;
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
		case P_Void:		return 1;
		case P_UChar:
		case P_Char:		return 1;
		case P_UInt:
		case P_Int:			return 4;
		case P_ULong:
		case P_Long:		return 4;
		case P_ULongLong:
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

bool IsUnsigned(PrimordialType prim){
	switch(prim){
		case P_UChar:
		case P_UInt:
		case P_ULong:
		case P_ULongLong:	return true;
		default:			return false;
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

Parameter* MakeParam(const char* id, PrimordialType type, SymEntry* cType, Parameter* prev){
	Parameter* p = malloc(sizeof(Parameter));
	p->id = id;
	p->type = type;
	p->cType = cType;
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
	return l;
}

SymEntry* GetMember(SymEntry* structDef, const char* member){
	SymEntry* members = structDef->value.ptrVal;
	if(members == NULL)		FatalM("Struct definition contained no members! (Internal @ types.h)", __LINE__);
	while(!streq(members->key, member) && members->sValue.ptrVal != NULL)
		members = members->sValue.ptrVal;
	if(!streq(members->key, member))	FatalM("Undefined composite member!", Line);
	return members;
}
