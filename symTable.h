#include "defs.h"

#define CAPACITY 1000

enum eStructuralType {
	S_Undefined = 0,
	S_Variable,
	S_Function,
};
typedef enum eStructuralType StructuralType;

struct SymEntry {
	const char* key;
	const char* value;
	PrimordialType type;
	StructuralType sType;
};
typedef struct SymEntry SymEntry;

typedef struct SymList SymList;
struct SymList {
	SymEntry* item;
	SymList* next;
};

static SymList* MakeSymList(SymEntry* entry, SymList* next){
	SymList* ret = malloc(sizeof(SymList));
	ret->item = entry;
	ret->next = next;
	return ret;
}

static SymEntry* MakeVarEntry(const char* key, const char* val, PrimordialType type){
	SymEntry* ret = malloc(sizeof(SymEntry));
	ret->key = key;
	ret->value = val;
	ret->type = type;
	ret->sType = S_Variable;
	return ret;
}

static SymEntry* MakeFuncEntry(const char* key, const char* val, PrimordialType type){
	SymEntry* ret = malloc(sizeof(SymEntry));
	ret->key = key;
	ret->value = val;
	ret->type = type;
	ret->sType = S_Function;
	return ret;
}

static SymList*** hashArray;
static int* varCount;
static int maxScope = 5;

static void CreateScope(int scope){
	while(maxScope <= scope){
		hashArray	= realloc(hashArray,	(maxScope + 5) * sizeof(SymList**));
		varCount	= realloc(varCount,		(maxScope + 5) * sizeof(int));
		stackIndex	= realloc(stackIndex,	(maxScope + 5) * sizeof(int));
		for(int i = 0; i < 5; i++){
			hashArray[maxScope+i]	= NULL;
			varCount[maxScope+i]	= 0;
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
	stackIndex	= malloc(sizeof(int) * 5);
	for (int i = 0; i < 5; i++){
		hashArray[i] = NULL;
		varCount[i] = 0;
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
	int hash = hash_oaat(key, strlen(key)) % CAPACITY;
	SymList* list = hashArray[scope][hash];
	if (list == NULL)
		if (scope < 1 || strict)	return NULL;
		else		return FindVar(key, scope - 1);
	while((!streq(list->item->key, key) || list->item->sType != S_Variable) && list->next != NULL)
		list = list->next;
	if(!streq(list->item->key, key) || list->item->sType != S_Variable)
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

SymList* InsertVar(const char* key, const char* value, PrimordialType type, int scope){
	int hash = hash_oaat(key, strlen(key)) % CAPACITY;
	if(hashArray[scope] == NULL)
		return NULL;
	SymList* list = hashArray[scope][hash];
	if(list == NULL){
		varCount[scope]++;
		return hashArray[scope][hash] = MakeSymList(MakeVarEntry(key, value, type), NULL);
	}
	while((!streq(list->item->key, key) || list->item->sType != S_Variable)&& list->next != NULL)
		list = list->next;
	if(!streq(list->item->key, key)){
		varCount[scope]++;
		return list->next = MakeSymList(MakeVarEntry(key, value, type), NULL);
	}
	list->item->value = value;
	return list;
}

SymList* InsertFunc(const char* key, PrimordialType type){
	int hash = hash_oaat(key, strlen(key)) % CAPACITY;
	if(hashArray[0] == NULL)
		return NULL;
	SymList* list = hashArray[0][hash];
	if(list == NULL)
		return hashArray[0][hash] = MakeSymList(MakeFuncEntry(key, NULL, type), NULL);
	while((!streq(list->item->key, key) || list->item->sType != S_Function) && list->next != NULL)
		list = list->next;
	if(!streq(list->item->key, key))
		return list->next = MakeSymList(MakeFuncEntry(key, NULL, type), NULL);
	return list;
}

SymEntry* FindFunc(const char* key){
	if(hashArray[0] == NULL)
		return NULL;
	int hash = hash_oaat(key, strlen(key)) % CAPACITY;
	SymList* list = hashArray[0][hash];
	if(list == NULL)
		return NULL;
	while((!streq(list->item->key, key) || list->item->sType != S_Function) && list->next != NULL)
		list = list->next;
	if(!streq(list->item->key, key) || list->item->sType != S_Function)
		return NULL;
	return list->item;
}

int GetLocalVarCount(int scope){
	return varCount[scope];
}

int EnterScope(){
	CreateScope(++scope);
	return scope;
}

int ExitScope(){
	DestroyVarTable(scope--);
	stackIndex[scope] = 0;
	return scope;
}

#undef CAPACITY