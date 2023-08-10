#include "defs.h"

#define CAPACITY 1000

SymList* UpdateStruct(SymList* list, const char* name, SymEntry* members);
SymList* UpdateUnion(SymList* list, const char* name, SymEntry* members);

static SymList* MakeSymList(SymEntry* entry, SymList* next){
	SymList* ret = malloc(sizeof(SymList));
	ret->item = entry;
	ret->next = next;
	return ret;
}

static SymEntry* MakeSymEntry(const char* key, FlexibleValue value, StructuralType sType){
	SymEntry* ret = malloc(sizeof(SymEntry));
	ret->key = key;
	ret->value = value;
	ret->sType = sType;
	return ret;
}

static SymEntry* MakeTypedSymEntry(const char* key, PrimordialType type, SymEntry* cType, StructuralType sType) {
	SymEntry* ret = malloc(sizeof(SymEntry));
	ret->key = key;
	ret->cType = cType;
	ret->type = type;
	ret->sType = sType;
	return ret;
}

static SymEntry* MakeVarEntry(const char* key, const char* val, PrimordialType type, SymEntry* cType, StorageClass sc){
	SymEntry* ret = malloc(sizeof(SymEntry));
	ret->key = key;
	ret->value.strVal = val;
	ret->sValue.intVal = sc;
	ret->type = type;
	ret->sType = S_Variable;
	ret->cType = cType;
	return ret;
}

static SymEntry* MakeFuncEntry(const char* key, FlexibleValue val, PrimordialType type, SymEntry* cType){
	SymEntry* ret = malloc(sizeof(SymEntry));
	ret->key = key;
	ret->value = val;
	ret->type = type;
	ret->sType = S_Function;
	ret->cType = cType;
	return ret;
}

static SymEntry* MakeStructEntry(const char* name, SymEntry* members){
	SymEntry* ret = malloc(1 * sizeof(SymEntry));
	ret->key = name;
	ret->value.ptrVal = members;
	SymEntry* pos = members;
	ret->sValue.intVal = 0;
	while(pos != NULL){
		pos->value.intVal = ret->sValue.intVal;
		ret->sValue.intVal += GetTypeSize(pos->type, pos->cType);
		pos = pos->sValue.ptrVal;
	}
	// int totalSize = 0;
	// while(pos != NULL){
	// 	int size = GetTypeSize(pos->type, pos->cType);
	// 	if(totalSize % size)
	// 		totalSize = align(totalSize, size);
	// 	pos->value.intVal = totalSize;
	// 	totalSize += size;
	// 	pos = pos->sValue.ptrVal;
	// }
	// ret->sValue.intVal = totalSize;
	ret->type = P_Composite;
	ret->sType = S_Composite;
	ret->cType = NULL;
	if(members == NULL)
		ret->sValue.intVal = -1;
	return ret;
}

static SymEntry* MakeUnionEntry(const char* name, SymEntry* members){
	SymEntry* ret = malloc(1 * sizeof(SymEntry));
	ret->key = name;
	ret->value.ptrVal = members;
	SymEntry* pos = members;
	ret->sValue.intVal = 0;
	while(pos != NULL){
		pos->value.intVal = 0;
		int size = GetTypeSize(pos->type, pos->cType);
		if(ret->sValue.intVal < size)
			ret->sValue.intVal = size;
		pos = pos->sValue.ptrVal;
	}
	ret->type = P_Composite;
	ret->sType = S_Composite;
	ret->cType = NULL;
	if(members == NULL)
		ret->sValue.intVal = -1;
	return ret;
}

SymEntry* MakeCompMember(const char* name, SymEntry* next, PrimordialType type, SymEntry* cType){
	SymEntry* ret = malloc(sizeof(SymEntry));
	ret->key = name;
	ret->sValue.ptrVal = (void*)next;
	ret->type = type;
	ret->sType = S_Member;
	ret->cType = cType;
	return ret;
}

SymEntry* MakeCompMembers(ASTNodeList* list){
	SymEntry* members = NULL;
	for(int i = list->count - 1; i >= 0; i--){
		ASTNode* node = list->nodes[i];
		members = MakeCompMember(node->value.strVal, members, node->type, node->cType);
	}
	return members;
}

static SymList*** hashArray;
static int* varCount;
static int* stackSize;
static int maxScope = 5;

static void CreateScope(int scope){
	while(maxScope <= scope){
		hashArray	= realloc(hashArray,	(maxScope + 5) * sizeof(SymList**));
		varCount	= realloc(varCount,		(maxScope + 5) * sizeof(int));
		stackSize	= realloc(stackSize,	(maxScope + 5) * sizeof(int));
		stackIndex	= realloc(stackIndex,	(maxScope + 5) * sizeof(int));
		for(int i = 0; i < 5; i++){
			hashArray[maxScope+i]	= NULL;
			varCount[maxScope+i]	= 0;
			stackSize[maxScope+i]	= 0;
			stackIndex[maxScope+i]	= 0;
		}
		maxScope += 5;
	}
	hashArray[scope] = malloc(sizeof(SymList*) * CAPACITY);
	stackIndex[scope] = scope ? stackIndex[scope - 1] : 0;
	for(int i = 0; i < CAPACITY; i++)
		hashArray[scope][i] = NULL;
}

void InitVarTable(){
	// Allocate with the assumption of a maximum scope depth of 5
	hashArray	= malloc(sizeof(SymList**) * 5);
	varCount	= malloc(sizeof(int) * 5);
	stackSize	= malloc(sizeof(int) * 5);
	stackIndex	= malloc(sizeof(int) * 5);
	for (int i = 0; i < 5; i++){
		hashArray[i] = NULL;
		varCount[i] = 0;
		stackSize[i] = 0;
		stackIndex[i] = 0;
	}
	CreateScope(0);
	for(int i = 0; i < CAPACITY; i++)
		hashArray[0][i] = NULL;
}

void DestroyVarTable(int scope){
	for(int i = 0; i < CAPACITY; i++)
		free(hashArray[scope][i]);
	free(hashArray[scope]);
	hashArray[scope] = NULL;
	varCount[scope] = 0;
	stackSize[scope] = 0;
}

void ResetVarTable(int scope) {
	DestroyVarTable(scope);
	CreateScope(scope);
	for (int i = 0; i < CAPACITY; i++)
		hashArray[scope][i] = NULL;
}

SymEntry* FindVar(const char* key, int scope);
SymEntry* FindLocalVar(const char* key, int scope);

static unsigned int hash_oaat(const char* key, int length){
	int i = 0;
	int hash = 0;
	while(i != length){
		hash += key[i++];
		hash += hash << 10;
		hash ^= hash >> 6;
	}
	hash += hash << 3;
	hash ^= hash >> 11;
	hash += hash << 15;
	return hash;
}
int collisions = 0;

static SymEntry* FindVarPosition(const char* key, int scope, bool strict){
	if(hashArray[scope] == NULL)
		return NULL;
	unsigned int hash = hash_oaat(key, strlen(key)) % CAPACITY;
	SymList* list = hashArray[scope][hash];
	if (list == NULL)
		if (scope < 1 || strict)	return NULL;
		else		return FindVar(key, scope - 1);
	while((list->item->sType != S_Variable || !streq(list->item->key, key)) && list->next != NULL)
		list = list->next;
	if(list->item->sType != S_Variable || !streq(list->item->key, key))
		if(scope < 1 || strict)	return NULL;
		else		return FindVar(key, scope - 1);
	return list->item;
}

SymEntry* FindVar(const char* key, int scope){
	return FindVarPosition(key, scope, false);
}

SymEntry* FindLocalVar(const char* key, int scope){
	return FindVarPosition(key, scope, true);
}

SymList* InsertEnumName(const char* name){
	if(name == NULL)	FatalM("No name supplied to InsertEnumName!", Line);
	unsigned int hash = hash_oaat(name, strlen(name)) % CAPACITY;
	if(hashArray[0] == NULL)
		return NULL;
	SymList* list = hashArray[0][hash];
	if(list == NULL)
		return hashArray[0][hash] = MakeSymList(MakeSymEntry(name, FlexNULL(), S_EnumName), NULL);
	while((list->item->sType != S_EnumName || !streq(list->item->key, name)) && list->next != NULL)
		list = list->next;
	if(list->item->sType != S_EnumName || !streq(list->item->key, name))
		return list->next = MakeSymList(MakeSymEntry(name, FlexNULL(), S_EnumName), NULL);
	FatalM("Redeclaration of enums is strictly forbidden!", Line);
}

SymList* InsertEnumValue(const char* name, int value){
	if(name == NULL)	FatalM("No name supplied to InsertEnumValue!", Line);
	unsigned int hash = hash_oaat(name, strlen(name)) % CAPACITY;
	if(hashArray[0] == NULL)
		return NULL;
	SymList* list = hashArray[0][hash];
	if(list == NULL)
		return hashArray[0][hash] = MakeSymList(MakeSymEntry(name, FlexInt(value), S_EnumValue), NULL);
	while((list->item->sType != S_EnumValue || !streq(list->item->key, name)) && list->next != NULL)
		list = list->next;
	if(list->item->sType != S_EnumValue || !streq(list->item->key, name))
		return list->next = MakeSymList(MakeSymEntry(name, FlexInt(value), S_EnumValue), NULL);
	FatalM("Redeclaration of enum values is strictly forbidden!", Line);
}

SymList* InsertVar(const char* key, const char* value, PrimordialType type, SymEntry* cType, StorageClass sc, int scope){
	unsigned int hash = hash_oaat(key, strlen(key)) % CAPACITY;
	if(hashArray[scope] == NULL)
		return NULL;
	SymList* list = hashArray[scope][hash];
	if(list == NULL){
		varCount[scope]++;
		stackSize[scope] += align(GetTypeSize(type, cType), 16);
		return hashArray[scope][hash] = MakeSymList(MakeVarEntry(key, value, type, cType, sc), NULL);
	}
	while((list->item->sType != S_Variable || !streq(list->item->key, key))&& list->next != NULL)
		list = list->next;
	if(list->item->sType != S_Variable || !streq(list->item->key, key)){
		varCount[scope]++;
		stackSize[scope] += align(GetTypeSize(type, cType), 16);
		return list->next = MakeSymList(MakeVarEntry(key, value, type, cType, sc), NULL);
	}
	list->item->value = FlexStr(value);
	return list;
}

SymList* InsertFunc(const char* key, FlexibleValue params, PrimordialType type, SymEntry* cType){
	unsigned int hash = hash_oaat(key, strlen(key)) % CAPACITY;
	if(hashArray[0] == NULL)
		return NULL;
	SymList* list = hashArray[0][hash];
	if(list == NULL)
		return hashArray[0][hash] = MakeSymList(MakeFuncEntry(key, params, type, cType), NULL);
	while((list->item->sType != S_Function || !streq(list->item->key, key)) && list->next != NULL)
		list = list->next;
	if(list->item->sType != S_Function || !streq(list->item->key, key))
		return list->next = MakeSymList(MakeFuncEntry(key, params, type, cType), NULL);
	return list;
}

SymList* InsertStruct(const char* name, SymEntry* members){
	if(name == NULL)
		return MakeSymList(MakeStructEntry(NULL, members), NULL);
	unsigned int hash = hash_oaat(name, strlen(name)) % CAPACITY;
	if(hashArray[0] == NULL)
		return NULL;
	SymList* list = hashArray[0][hash];
	if(list == NULL)
		return hashArray[0][hash] = MakeSymList(MakeStructEntry(name, members), NULL);
	while((list->item->sType != S_Composite || !streq(list->item->key, name)) && list->next != NULL)
		list = list->next;
	if(list->item->sType != S_Composite || !streq(list->item->key, name))
		return list->next = MakeSymList(MakeStructEntry(name, members), NULL);
	if(list->item->value.ptrVal != NULL)	WarnM("Overriding previous composite declaration!", Line);
	return UpdateStruct(list, name, members);
}

SymList* InsertUnion(const char* name, SymEntry* members){
	if(name == NULL)
		return MakeSymList(MakeUnionEntry(NULL, members), NULL);
	unsigned int hash = hash_oaat(name, strlen(name)) % CAPACITY;
	if(hashArray[0] == NULL)
		return NULL;
	SymList* list = hashArray[0][hash];
	if(list == NULL)
		return hashArray[0][hash] = MakeSymList(MakeUnionEntry(name, members), NULL);
	while((list->item->sType != S_Composite || !streq(list->item->key, name)) && list->next != NULL)
		list = list->next;
	if(list->item->sType != S_Composite || !streq(list->item->key, name))
		return list->next = MakeSymList(MakeUnionEntry(name, members), NULL);
	if(list->item->value.ptrVal != NULL)	WarnM("Overriding previous composite declaration!", Line);
	return UpdateUnion(list, name, members);
}

SymList* InsertTypedef(const char* alias, PrimordialType type, SymEntry* cType){
	if(alias == NULL)	FatalM("No alias supplied to InsertTypeDef! (Internal @ symTable.h)", __LINE__);
	SymEntry* entry = MakeTypedSymEntry(alias, type, cType, S_Typedef);
	unsigned int hash = hash_oaat(alias, strlen(alias)) % CAPACITY;
	if(hashArray[0] == NULL)
		return NULL;
	SymList* list = hashArray[0][hash];
	if(list == NULL)
		return hashArray[0][hash] = MakeSymList(entry, NULL);
	while((list->item->sType != S_Typedef || !streq(list->item->key, alias)) && list->next != NULL)
		list = list->next;
	if(list->item->sType != S_Typedef || !streq(list->item->key, alias))
		return list->next = MakeSymList(entry, NULL);
	list->item = entry;
	return list;
}

SymList* UpdateStruct(SymList* list, const char* name, SymEntry* members){
	if(name != NULL){
		while((!streq(list->item->key, name) || list->item->sType != S_Composite) && list->next != NULL)
			list = list->next;
		if(!streq(list->item->key, name))
			FatalM("Failed to find struct definition! (Internal @ symTable.h)", __LINE__);
	}
	SymEntry* proto = MakeStructEntry(name, members);
	list->item->value	= proto->value;
	list->item->sValue	= proto->sValue;
	free(proto);
	return list;
}

SymList* UpdateUnion(SymList* list, const char* name, SymEntry* members){
	if(name != NULL){
		while((!streq(list->item->key, name) || list->item->sType != S_Composite) && list->next != NULL)
			list = list->next;
		if(!streq(list->item->key, name))
			FatalM("Failed to find union definition! (Internal @ symTable.h)", __LINE__);
	}
	SymEntry* proto = MakeUnionEntry(name, members);
	list->item->value	= proto->value;
	list->item->sValue	= proto->sValue;
	free(proto);
	return list;
}

static SymEntry* FindGlobal(const char* key, StructuralType type){
	if(hashArray[0] == NULL)
		return NULL;
	unsigned int hash = hash_oaat(key, strlen(key)) % CAPACITY;
	SymList* list = hashArray[0][hash];
	if(list == NULL)
		return NULL;
	while((!streq(list->item->key, key) || list->item->sType != type) && list->next != NULL)
		list = list->next;
	if(!streq(list->item->key, key) || list->item->sType != type)
		return NULL;
	return list->item;
}

SymEntry* FindFunc(const char* key){
	return FindGlobal(key, S_Function);
}

SymEntry* FindStruct(const char* key){
	return FindGlobal(key, S_Composite);
}

SymEntry* FindEnumValue(const char* key){
	return FindGlobal(key, S_EnumValue);
}

int GetLocalVarCount(int scope){
	return varCount[scope];
}

int GetLocalStackSize(int scope){
	return stackSize[scope];
}

int EnterScope(){
	CreateScope(++scope);
	return scope;
}

int ExitScope(){
	stackIndex[scope] = 0;
	DestroyVarTable(scope--);
	return scope;
}

#undef CAPACITY