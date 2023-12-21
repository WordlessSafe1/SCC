#ifndef SYMTABLE_H
#define SYMTABLE_H

#include "defs.h"

// static SymList* MakeSymList(SymEntry* entry, SymList* next);
// static SymEntry* MakeSymEntry(const char* key, FlexibleValue value, StructuralType sType);
// static SymEntry* MakeTypedSymEntry(const char* key, PrimordialType type, SymEntry* cType, StructuralType sType);
// static SymEntry* MakeVarEntry(const char* key, const char* val, PrimordialType type, SymEntry* cType, StorageClass sc);
// static SymEntry* MakeFuncEntry(const char* key, FlexibleValue val, PrimordialType type, SymEntry* cType);
// static SymEntry* MakeStructEntry(const char* name, SymEntry* members);
// static SymEntry* MakeUnionEntry(const char* name, SymEntry* members);
SymEntry* MakeCompMember(const char* name, SymEntry* next, PrimordialType type, SymEntry* cType);
SymEntry* MakeCompMembers(ASTNodeList* list);
// static SymList*** hashArray;
// static int* varCount;
// static int* stackSize;
// static int maxScope = 5;

// static void CreateScope(int scope);

void InitVarTable();
void DestroyVarTable(int scope);
void ResetVarTable(int scope);
SymEntry* FindVar(const char* key, int scope);
SymEntry* FindLocalVar(const char* key, int scope);
// static unsigned int hash_oaat(const char* key, int length);
extern int collisions;
// static SymEntry* FindVarPosition(const char* key, int scope, bool strict);
SymEntry* FindVar(const char* key, int scope);
SymEntry* FindLocalVar(const char* key, int scope);
SymList* InsertEnumName(const char* name);
SymList* InsertEnumValue(const char* name, int value);
SymList* InsertVar(const char* key, const char* value, PrimordialType type, SymEntry* cType, StorageClass sc, int scope);
SymList* InsertFunc(const char* key, FlexibleValue params, PrimordialType type, SymEntry* cType);
SymList* InsertStruct(const char* name, SymEntry* members);
SymList* InsertUnion(const char* name, SymEntry* members);
SymList* InsertTypedef(const char* alias, PrimordialType type, SymEntry* cType);
SymList* UpdateStruct(SymList* list, const char* name, SymEntry* members);
SymList* UpdateUnion(SymList* list, const char* name, SymEntry* members);
SymEntry* FindGlobal(const char* key, StructuralType type);
SymEntry* FindFunc(const char* key);
SymEntry* FindStruct(const char* key);
SymEntry* FindEnumValue(const char* key);
int GetLocalVarCount(int scope);
int GetLocalStackSize(int scope);
int EnterScope();
int ExitScope();

#endif