#ifndef LEX_INCLUDED
#define LEX_INCLUDED

#include "types.h"
#include "globals.h"

bool streq(const char*, const char*);

static Token* Tokenize(const char* str){
	Token* token = malloc(sizeof(Token));
	if(streq(str, "\0"))			token->type = T_Undefined;
	else if(streq(str, "("))		token->type = T_OpenParen;
	else if(streq(str, ")"))		token->type = T_CloseParen;
	else if(streq(str, "{"))		token->type = T_OpenBrace;
	else if(streq(str, "}"))		token->type = T_CloseBrace;
	else if(streq(str, "return"))	token->type = T_Return;
	else if(streq(str, ";"))		token->type = T_Semicolon;
	else if(streq(str, "int"))		token->type = T_Type;
	else if(streq(str, "-"))		token->type = T_Minus;
	else if(streq(str, "!"))		token->type = T_Bang;
	else if(streq(str, "~"))		token->type = T_Tilde;
	else if(streq(str, "+"))		token->type = T_Plus;
	else if(streq(str, "*"))		token->type = T_Asterisk;
	else if(streq(str, "/"))		token->type = T_Divide;
	else if(streq(str, "%"))		token->type = T_Percent;
	else if(isdigit(str[0])){
		token->type = T_LitInt;
		token->value.intVal = atoi(str);
	}
	else{
		token->type = T_Identifier;
		token->value.strVal = str;
	}
	return token;
}

char* ShiftToken(){
	char* token = malloc(32 * sizeof(char));
	int len = 32;
	int i = 0;
	while(true){
		if(i == len)
			token = realloc(token, len += 32);
		char c = fgetc(fptr);
		// For some reason, fgetc() keeps pulling a phantom null byte. This counteracts it.
		while(c == '\0')	c = fgetc(fptr);
		if(c == EOF)
			break;
		if(c == '\n' || c == '\t' || c == ' '){
			if(c == '\n')
				Line++;
			if(i)	break;
			else	continue;
		}
		if(strchr("(){};-~!+*/%%", c)){
			if(i){
				ungetc(c, fptr);
				break;
			}
			token[i++] = c;
			break;
		}
		token[i++] = c;
	}
	token = realloc(token, i + 1);
	token[i] = '\0';
	return i ? token : NULL;
}

static void UnshiftToken(const char* token){
	int i = 0;
	while(token[i++]);
	while(i--)
		ungetc(token[i], fptr);
}

Token* PeekToken(){
	const char* str = ShiftToken();
	UnshiftToken(str);
	return Tokenize(str);
}

Token* GetToken(){
	char* str = ShiftToken();
	return str ? Tokenize(str) : NULL;
}

bool streq(const char* lhs, const char* rhs){
	int i = 0;
	while(lhs[i] && rhs[i])
		if(lhs[i] != rhs[i])	return false;
		else					i++;
	return lhs[i] == rhs[i];
}

#endif