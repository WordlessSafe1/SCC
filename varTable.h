#include "defs.h"

#define CAPACITY 1000

struct VarEntry {
	const char* key;
	const char* value;
};
typedef struct VarEntry VarEntry;

struct VarList {
	VarEntry* item;
	struct VarList* next;
};
typedef struct VarList VarList;

static VarList* MakeVarList(VarEntry* entry, VarList* next){
	VarList* ret = malloc(sizeof(VarList));
	ret->item = entry;
	ret->next = next;
	return ret;
}

static VarEntry* MakeVarEntry(const char* key, const char* val){
	VarEntry* ret = malloc(sizeof(VarEntry));
	ret->key = key;
	ret->value = val;
	return ret;
}

static VarList*** hashArray;
static int* varCount;
static int maxScope = 5;

static void CreateScope(int scope){
	while(maxScope <= scope){
		hashArray	= realloc(hashArray,	(maxScope + 5) * sizeof(VarList**));
		varCount	= realloc(varCount,		(maxScope + 5) * sizeof(int));
		stackIndex	= realloc(stackIndex,	(maxScope + 5) * sizeof(int));
		for(int i = 0; i < 5; i++){
			hashArray[maxScope+i]	= NULL;
			varCount[maxScope+i]	= 0;
			stackIndex[maxScope+i]	= 0;
		}
		maxScope += 5;
	}
	hashArray[scope] = malloc(sizeof(VarList*) * CAPACITY);
	for(int i = 0; i < CAPACITY; i++)
		hashArray[scope][i] = NULL;
}

void InitVarTable(){
	// Allocate with the assumption of a maximum scope depth of 5
	hashArray	= malloc(sizeof(VarList**) * 5);
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
VarEntry* FindVar(const char* key, int scope);
VarEntry* FindLocalVar(const char* key, int scope);

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
int trail = 0;

static VarEntry* FindVarPosition(const char* key, int scope, bool strict){
	if(hashArray[scope] == NULL)
		return NULL;
	int hash = hash_oaat(key, strlen(key)) % CAPACITY;
	VarList* list = hashArray[scope][hash];
	if (list == NULL)
		if (scope < 1 || strict)	return NULL;
		else		return FindVar(key, scope - 1);
	while(list->item->key != key && list->next != NULL)
		list = list->next;
	if(!streq(list->item->key, key))
		if(scope < 1 || strict)	return NULL;
		else		return FindVar(key, scope - 1);
	return list->item;
}

VarEntry* FindVar(const char* key, int scope){
	return FindVarPosition(key, scope, false);
}

VarEntry* FindLocalVar(const char* key, int scope){
	return FindVarPosition(key, scope, true);
}

VarList* InsertVar(const char* key, const char* value, int scope){
	int hash = hash_oaat(key, strlen(key)) % CAPACITY;
	if(hashArray[scope] == NULL)
		return NULL;
	VarList* list = hashArray[scope][hash];
	if(list == NULL){
		varCount[scope]++;
		return hashArray[scope][hash] = MakeVarList(MakeVarEntry(key, value), NULL);
	}
	while(list->item->key != key && list->next != NULL){
		trail++;
		list = list->next;
	}
	if(list->item->key != key){
		varCount[scope]++;
		return list->next = MakeVarList(MakeVarEntry(key, value), NULL);
	}
	list->item->value = value;
	return list;
}

int GetLocalVarCount(int scope){
	return varCount[scope];
}

int EnterScope(){
	CreateScope(++scope);
	return scope;
}

int ExitScope(){
	DestroyVarTable(--scope);
	stackIndex[scope] = 0;
	return scope;
}

#undef CAPACITY