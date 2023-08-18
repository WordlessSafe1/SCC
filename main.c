#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>
#ifdef __GNUC__
	#include <unistd.h>
	#ifndef __SCC__
		#include <io.h>
	#endif
#endif

#include <errno.h>

#include "defs.h"
#include "types.h"
#include "lex.h"
#include "symTable.h"
#include "parse.h"
#include "gen.h"

#ifdef extern_main
	#undef extern_main
#endif
#define extern_main
#define INIT_VARS

#include "globals.h"

char* AlterFileExtension(const char* filename, const char* extension);
char* DumpASTTree(ASTNode* tree, int depth);
char* charStr(char c, int count);
bool noWarn = false;
char* target;

void Usage(char* file){
	const char* format =
		"Usage: %s [-pqStc] [-o outFile] [-isystem includes] file [file ...]\n"
		"	-q Disable warnings\n"
		"	-p Print the output to the console\n"
		"	-S Generate assembly files, but don't assemble or link them\n"
		"	-t Dump the Abstract Syntax Trees for each file\n"
		"	-c Assemble the files, but do not link them\n"
		"	-o outfile, produce the outfile executable file\n"
		"	-isystem includes, specify an alternate locaton for the standard headers\n"
	;
	printf(format, file);
	exit(0);
}

int main(int argc, char** argv){
	InitVarTable();
	if(argc < 2)	Usage(argv[0]);
	const char* outputTarget = NULL; 
	const char** inputTargets = calloc(MAXFILES, sizeof(char*));
	int inputs = 0;
	bool dump		= false;
	bool print		= false;
	bool supIntl	= false;
	bool asASM		= false;
	bool link		= true;
	const char* incDir = "./include";
	for(int i = 1; i < argc; i++){
		if(argv[i][0] == '-'){
			if(strlen(argv[i]) == 2){
				switch(argv[i][1]){
					case 'o':
						if(i+1 >= argc)	FatalM("Trailing argument '-o'!", NOLINE);
						else			outputTarget = argv[++i];
						break;
					case 't':	dump	= true;	FOLD_INLINE = false;	break;
					case 'c':	link	= false;	break;
					case 'p':	print	= true;		break;
					case 'q':	noWarn	= true;		break;
					case 'S':	asASM	= true;		break;
					case 'h':
					case 'H':
					case '?':	Usage(argv[0]);
					default:	FatalM("Unknown flag(s) supplied!", NOLINE);
				}
			}
			else if(streq(argv[i], "-sI"))	supIntl	= true;
			else if(streq(argv[i], "-isystem")){
				if(i+1 >= argc)	FatalM("Trailing argument '-isystem'!", NOLINE);
				incDir = argv[++i];
			}
			else							FatalM("Unknown flag(s) supplied!", NOLINE);
		}
		else if(inputs == MAXFILES - 2)	FatalM("Too many object files!", NOLINE);
		else							inputTargets[inputs++] = argv[i];
	}
	if (inputs == 0)	FatalM("No input files specified!", NOLINE);
	if(outputTarget == NULL && !dump)	outputTarget = "a.out";
	for(int i = 0; i < inputs; i++){
		curFile = inputTargets[i];
		const char* format = "cpp.exe -D __SCC__ -nostdinc -isystem %s %s -o %s";
		target = AlterFileExtension(inputTargets[i], "tmp_ppc");
		int charCount = strlen(format) + strlen(incDir) + strlen(inputTargets[i]) + strlen(target) + 1;
		char* cmd = malloc(charCount * sizeof(char));
		snprintf(cmd, charCount, format, incDir, inputTargets[i], target);
		if(system(cmd))		exit(1);
		free(cmd);
		const char* output = outputTarget;
		if(!dump && (inputs != 1 || !asASM))
			output = NULL;
		fptr = fopen(target, "r");
		Line = 1;
		ASTNodeList* ast = MakeASTNodeList();
		while(PeekToken() != NULL)
			AddNodeToASTList(ast, ParseNode());
		if(GetToken() != NULL)	FatalM("Expected EOF!", Line);
		fclose(fptr);
		unlink(target);
		free(target);
		target = NULL;
		Line = NOLINE;
		if(dump){
			if(output == NULL || print){
				if(supIntl)
					printf("%s", "#ifdef __INTELLISENSE__\n" "	#pragma diag_suppress 29\n" "	#pragma diag_suppress 169\n" "	#pragma diag_suppress 130\n" "	#pragma diag_suppress 109\n" "	#pragma diag_suppress 20\n" "	#pragma diag_suppress 65\n" "	#pragma diag_suppress 91\n" "	#pragma diag_suppress 7\n" "#endif\n");
				for(int i = 0; i < ast->count; i++){
					char* tree = DumpASTTree(ast->nodes[i], 0);
					printf("%s", tree);
					free(tree);
				}
				putchar('\n');
				continue;
			}
			fptr = fopen(output, "w");
			if(supIntl)
				fprintf(fptr, "%s", "#ifdef __INTELLISENSE__\n" "	#pragma diag_suppress 29\n" "	#pragma diag_suppress 169\n" "	#pragma diag_suppress 130\n" "	#pragma diag_suppress 109\n" "	#pragma diag_suppress 20\n" "	#pragma diag_suppress 65\n" "	#pragma diag_suppress 91\n" "	#pragma diag_suppress 7\n" "#endif\n");
			for(int i = 0; i < ast->count; i++){
				char* tree = DumpASTTree(ast->nodes[i], 0);
				fprintf(fptr, "%s", tree);
				free(tree);
			}
			putc('\n', fptr);
			fclose(fptr);
			continue;
		}
		ResetVarTable(0);
		char* Asm = GenerateAsm(ast);
		if(print){
			printf("%s", Asm);
			break;
		}
		if(output == NULL){
			output = AlterFileExtension(inputTargets[i], "s");
			if(!access(output, 0))
				output = inputTargets[i] = AlterFileExtension(inputTargets[i], "tmp_s");
		}
		fptr = fopen(output, "w");
		fprintf(fptr, "%s", Asm);
		fclose(fptr);
		if(asASM)	return 0;
		{
			const char* format = "as -o %s %s";
			char* outfilename = AlterFileExtension(inputTargets[i], "o");
			int charCount = strlen(outfilename) + strlen(output) + strlen(format) + 1;
			char* cmd = malloc(charCount * sizeof(char));
			snprintf(cmd, charCount * sizeof(char), format, outfilename, output);
			system(cmd);
			unlink(output);
			free(outfilename);
			free(cmd);
		}
		free(Asm);
	}
	if(dump || !link)	return 0;
	{
		char* cmd = _strdup("cc -o ");
		int charCount = strlen(cmd) + strlen(outputTarget) + 1;
		cmd = realloc(cmd, charCount * sizeof(char));
		strcat(cmd, outputTarget);
		for(int i = 0; i < inputs; i++){
			char* targ = AlterFileExtension(inputTargets[i], "o");
			charCount += strlen(targ) + 1;
			char* buffer = calloc(charCount, sizeof(char));
			snprintf(buffer, charCount, "%s %s", cmd, targ);
			free(cmd);
			free(targ);
			cmd = buffer;
		}
		system(cmd);
		for(int i = 0; i < inputs; i++){
			char* targ = AlterFileExtension(inputTargets[i], "o");
			unlink(targ);
			free(targ);
		}
	}
	return 0;
}


char* AlterFileExtension(const char* filename, const char* extension){
	int charCount = strlen(filename) + strlen(extension) + 1;
	char* dpos;
	char* newfile = calloc(charCount, sizeof(char));
	strncpy(newfile, filename, charCount);
	if((dpos = strrchr(newfile, '.')) == NULL)	return NULL;
	if(*++dpos == '\0')	return NULL;
	*dpos = '\0';
	strncat(newfile, extension, strlen(extension));
	return newfile;
}

void FatalM(const char* msg, int line){
	if(line == NOLINE)	printf("Fatal error encountered:\n\t%s\n", msg);
	else				printf("Fatal error encountered on ln %d in %s:\n\t%s\n", line, curFile, msg);
	if (fptr != NULL)
		fclose(fptr);
	if(target != NULL){
		unlink(target);
		free(target);
	}
	exit(-1);
}

void WarnM(const char* msg, int line){
	if(noWarn)	return;
	if(line == NOLINE)	printf("Warning:\n\t%s\n", msg);
	else				printf("Warning - on ln %d in %s:\n\t%s\n", line, curFile, msg);
}

char* DumpASTTree(ASTNode* tree, int depth){
	char* lhs = tree->lhs == NULL ? calloc(1, sizeof(char)) : DumpASTTree(tree->lhs, depth + 1);
	char* mid = tree->mid == NULL ? calloc(1, sizeof(char)) : DumpASTTree(tree->mid, depth + 1);
	char* rhs = tree->rhs == NULL ? calloc(1, sizeof(char)) : DumpASTTree(tree->rhs, depth + 1);
	char* list = calloc(1, sizeof(char));
	if(tree->op == A_FunctionCall)
		tree->list = tree->secondaryValue.ptrVal;
	if (tree->list != NULL) {
		list = calloc(3, sizeof(char));
		list[0] = ' ';
		list[1] = '{';
		for(int i = 0; i < tree->list->count; i++){
			char* buffer = DumpASTTree(tree->list->nodes[i], depth + 1);
			list = realloc(list, strlen(list) + strlen(buffer) + 1);
			strncat(list, buffer, strlen(list) + strlen(buffer) + 1);
			free(buffer);
		}
		list = realloc(list, strlen(list) + (depth * 2) + 3);
		char* tabs = charStr(' ', depth * 2);
		strcat(list, "\n");
		strcat(list, tabs);
		free(tabs);
		strcat(list, "}");
	}
	const char* name = calloc(1, sizeof(char));
	const char* val = calloc(1, sizeof(char));
	switch(tree->op){
		case A_Function:			val = tree->value.strVal;	break;
		case A_Declare:				val = tree->value.strVal;	break;
		case A_VarRef:				val = tree->value.strVal;	break;
		case A_Assign:				val = tree->value.strVal == NULL ? "expr" : tree->value.strVal;	break;
		case A_FunctionCall:		val = tree->value.strVal;	break;
		case A_LitStr:
			val = tree->value.strVal;
			{
				char* buffer = calloc(2, sizeof(char));
				buffer[0] = '"';
				strapp(&buffer, val);
				strapp(&buffer, "\"");
				val = buffer;
			}
			break;
		case A_EnumValue: {
			const int charCount = strlen(tree->value.strVal) + intlen(tree->secondaryValue.intVal) + (tree->value.intVal < 0) + 2 + 1;
			char* buffer = malloc(charCount * sizeof(char));
			snprintf(buffer, charCount, "%s)(%lld", tree->value.strVal, tree->secondaryValue.intVal);
			val = buffer;
			break;
		}
		case A_Case:
		case A_LitInt:{
			const int charCount = intlen(tree->value.intVal) + (tree->value.intVal < 0) + 1;
			char* buffer = malloc(charCount * sizeof(char));
			snprintf(buffer, charCount, "%lld", tree->value.intVal);
			val = buffer;
			break;
		}
		default:					break;
	}
	switch(tree->op){
		case A_Undefined:			name = "Undefined";				break;
		case A_Glue:				name = "Glue";					break;
		case A_Function:			name = "Function";				break;
		case A_Return:				name = "Return";				break;
		case A_LitInt:				name = "LitInt";				break;
		case A_Negate:				name = "Negate";				break;
		case A_LogicalNot:			name = "LogicalNot";			break;
		case A_BitwiseComplement:	name = "BitwiseComplement";		break;
		case A_Multiply:			name = "Multiply";				break;
		case A_Divide:				name = "Divide";				break;
		case A_Modulo:				name = "Modulo";				break;
		case A_Add:					name = "Add";					break;
		case A_Subtract:			name = "Subtract";				break;
		case A_LeftShift:			name = "LeftShift";				break;
		case A_RightShift:			name = "RightShift";			break;
		case A_LessThan:			name = "LessThan";				break;
		case A_GreaterThan:			name = "GreaterThan";			break;
		case A_LessOrEqual:			name = "LessOrEqual";			break;
		case A_GreaterOrEqual:		name = "GreaterOrEqual";		break;
		case A_EqualTo:				name = "EqualTo";				break;
		case A_NotEqualTo:			name = "NotEqualTo";			break;
		case A_BitwiseAnd:			name = "BitwiseAnd";			break;
		case A_BitwiseXor:			name = "BitwiseXor";			break;
		case A_BitwiseOr:			name = "BitwiseOr";				break;
		case A_LogicalAnd:			name = "LogicalAnd";			break;
		case A_LogicalOr:			name = "LogicalOr";				break;
		case A_Declare:				name = "Declare";				break;
		case A_VarRef:				name = "VarRef";				break;
		case A_Assign:				name = "Assign";				break;
		case A_Block:				name = "Block";					break;
		case A_Ternary:				name = "Ternary";				break;
		case A_If:					name = "If";					break;
		case A_While:				name = "While";					break;
		case A_Do:					name = "Do";					break;
		case A_For:					name = "For";					break;
		case A_Continue:			name = "Continue";				break;
		case A_Break:				name = "Break";					break;
		case A_Increment:			name = "Increment";				break;
		case A_Decrement:			name = "Decrement";				break;
		case A_AssignSum:			name = "AssignSum";				break;
		case A_AssignDifference:	name = "AssignDifference";		break;
		case A_AssignProduct:		name = "AssignProduct";			break;
		case A_AssignQuotient:		name = "AssignQuotient";		break;
		case A_AssignModulus:		name = "AssignModulus";			break;
		case A_AssignLeftShift:		name = "AssignLeftShift";		break;
		case A_AssignRightShift:	name = "AssignRightShift";		break;
		case A_AssignBitwiseAnd:	name = "AssignBitwiseAnd";		break;
		case A_AssignBitwiseXor:	name = "AssignBitwiseXor";		break;
		case A_AssignBitwiseOr:		name = "AssignBitwiseOr";		break;
		case A_FunctionCall:		name = "FunctionCall";			break;
		case A_BuiltinCall:			name = "BuiltinCall";			break;
		case A_AddressOf:			name = "AddressOf";				break;
		case A_Dereference:			name = "Dereference";			break;
		case A_LitStr:				name = "LitString";				break;
		case A_StructDecl:			name = "StructDecl";			break;
		case A_EnumDecl:			name = "EnumDecl";				break;
		case A_EnumValue:			name = "EnumValue";				break;
		case A_Switch:				name = "Switch";				break;
		case A_Case:				name = "Case";					break;
		case A_Default:				name = "Default";				break;
		case A_Cast:				name = "Cast";					break;
		case A_ExpressionList:		name = "ExpressionList";		break;
		case A_RepeatLogicalOr:		name = "RepeatingLogicalOR";	break;
	}
	const char* type = calloc(1, sizeof(char));
	switch(tree->type & 0xF0){
		case P_Undefined:	break;
		case P_Void:		type = "void";					break;
		case P_Char:		type = "char";					break;
		case P_Int:			type = "int";					break;
		case P_Long:		type = "long";					break;
		case P_LongLong:	type = "long long";				break;
		case P_UChar:		type = "unsigned char";			break;
		case P_UInt:		type = "unsigned int";			break;
		case P_ULong:		type = "unsigned long";			break;
		case P_ULongLong:	type = "unsigned long long";	break;
		case P_Composite:	type = "struct";				break;
	}
	if(tree->type & 0x0F){
		int deref = tree->type & 0x0F;
		int tlen = strlen(type);
		char* buffer = calloc(tlen + deref + 1, sizeof(char));
		strncpy(buffer, type, tlen);
		buffer[tlen + deref] = '\0';
		for(int i = deref + tlen - 1; i >= tlen; i--)
			buffer[i] = '*';
		type = buffer;
	}
	const char* format =
		"\n%s%s(%s)[%s]" // Tabs, Name, val, type
		"%s" // lhs
		"%s" // mid
		"%s" // rhs
		"%s" // list
	;
	char* tabs = charStr(' ', depth * 2);
	const int charCount = strlen(name) + strlen(lhs) + strlen(mid) + strlen(rhs) + strlen(val) + strlen(type) + strlen(format) + strlen(list) + (2 * strlen(tabs)) + 1;
	char* buffer = malloc(charCount * sizeof(char));
	snprintf(buffer, charCount, format, tabs, name, val, type, lhs, mid, rhs, list, tabs);
	free(lhs);
	free(mid);
	free(rhs);
	free(list);
	free(tabs);
	return buffer;
}

char* charStr(char c, int count){
	char* buffer = calloc(count+1, sizeof(char));
	for(int i = 0; i < count; i++)
		buffer[i] = c;
	return buffer;
}

char* strjoin(const char* lhs, const char* rhs){
	int charCount = strlen(lhs) + strlen(rhs) + 1;
	char* buffer = malloc(charCount * sizeof(char));
	snprintf(buffer, charCount, "%s%s", lhs, rhs);
	return buffer;
}

char* strapp(char** lhs, const char* rhs){
	char* buffer = strjoin(*lhs, rhs);
	free(*lhs);
	return *lhs = buffer;
}

char* sngenf(int bufferSize, const char* format, ...){
	char* buffer = malloc(bufferSize);
	va_list args;
	va_start(args, format);
	vsnprintf(buffer, bufferSize, format, args);
	va_end(args);
	return buffer;
}

int intlen(long long value){
	int l = value < 1;
	if(value < 0)	l++;
	while(value){ l++; value/=10; }
	return l;
}