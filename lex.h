#ifndef LEX_INCLUDED
#define LEX_INCLUDED

#include "defs.h"
#include "types.h"
#include "globals.h"

static Token* Tokenize(const char* str){
	Token* token = malloc(sizeof(Token));
	if(streq(str, "\0"))			token->type = T_Undefined;
	else if(streq(str, "("))		token->type = T_OpenParen;
	else if(streq(str, ")"))		token->type = T_CloseParen;
	else if(streq(str, "{"))		token->type = T_OpenBrace;
	else if(streq(str, "}"))		token->type = T_CloseBrace;
	else if(streq(str, "return"))	token->type = T_Return;
	else if(streq(str, ";"))		token->type = T_Semicolon;
	else if(streq(str, "int"))		token->type = T_Int;
	else if(streq(str, "char"))		token->type = T_Char;
	else if(streq(str, "void"))		token->type = T_Void;
	else if(streq(str, "-"))		token->type = T_Minus;
	else if(streq(str, "!"))		token->type = T_Bang;
	else if(streq(str, "~"))		token->type = T_Tilde;
	else if(streq(str, "+"))		token->type = T_Plus;
	else if(streq(str, "*"))		token->type = T_Asterisk;
	else if(streq(str, "/"))		token->type = T_Divide;
	else if(streq(str, "%"))		token->type = T_Percent;
	else if(streq(str, "<"))		token->type = T_Less;
	else if(streq(str, ">"))		token->type = T_Greater;
	else if(streq(str, "<="))		token->type = T_LessEqual;
	else if(streq(str, ">="))		token->type = T_GreaterEqual;
	else if(streq(str, "=="))		token->type = T_DoubleEqual;
	else if(streq(str, "!="))		token->type = T_BangEqual;
	else if(streq(str, "&"))		token->type = T_Ampersand;
	else if(streq(str, "^"))		token->type = T_Caret;
	else if(streq(str, "|"))		token->type = T_Pipe;
	else if(streq(str, "&&"))		token->type = T_DoubleAmpersand;
	else if(streq(str, "||"))		token->type = T_DoublePipe;
	else if(streq(str, "<<"))		token->type = T_DoubleLess;
	else if(streq(str, ">>"))		token->type = T_DoubleGreater;
	else if(streq(str, "="))		token->type = T_Equal;
	else if(streq(str, "?"))		token->type = T_Question;
	else if(streq(str, ":"))		token->type = T_Colon;
	else if(streq(str, "if"))		token->type = T_If;
	else if(streq(str, "else"))		token->type = T_Else;
	else if(streq(str, "while"))	token->type = T_While;
	else if(streq(str, "do"))		token->type = T_Do;
	else if(streq(str, "for"))		token->type = T_For;
	else if(streq(str, "break"))	token->type = T_Break;
	else if(streq(str, "continue"))	token->type = T_Continue;
	else if(streq(str, "+="))		token->type = T_PlusEqual;
	else if(streq(str, "-="))		token->type = T_MinusEqual;
	else if(streq(str, "*="))		token->type = T_AsteriskEqual;
	else if(streq(str, "/="))		token->type = T_DivideEqual;
	else if(streq(str, "%="))		token->type = T_PercentEqual;
	else if(streq(str, "<<="))		token->type = T_DoubleLessEqual;
	else if(streq(str, ">>="))		token->type = T_DoubleGreaterEqual;
	else if(streq(str, "&="))		token->type = T_AmpersandEqual;
	else if(streq(str, "^="))		token->type = T_CaretEqual;
	else if(streq(str, "|="))		token->type = T_PipeEqual;
	else if(streq(str, "++"))		token->type = T_PlusPlus;
	else if(streq(str, "--"))		token->type = T_MinusMinus;
	else if(streq(str, ","))		token->type = T_Comma;
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
		if(strchr("(){};-~!+*/%%<>=&^|?:,", c)){
			if(i){
				fseek(fptr, -1, SEEK_CUR);
				break;
			}
			// If double symbol is valid operator
			if(strchr("=|&<>/+-", c)){
				char nextChar = fgetc(fptr);
				if(c == nextChar){
					if(c == '/'){
						while(nextChar != '\n' && nextChar != EOF)
							nextChar = fgetc(fptr);
						fseek(fptr, -1, SEEK_CUR);
						continue;
					}
					token[i++] = c;
					token[i++] = nextChar;
					// If double symbol followed by equals is valid operator
					if(strchr("<>", c)){
						nextChar = fgetc(fptr);
						if(nextChar == '='){
							token[i++] = nextChar;
							break;
						}
						fseek(fptr, -1, SEEK_CUR);
					}
					break;
				}
				fseek(fptr, -1, SEEK_CUR);
			}
			// If symbol followed by equal is valid operator
			if(strchr("=<>!+-*/%&^|", c)){
				char nextChar = fgetc(fptr);
				if(nextChar == '='){
					token[i++] = c;
					token[i++] = nextChar;
					break;
				}
				fseek(fptr, -1, SEEK_CUR);
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

Token* PeekToken(){
	fpos_t* fpos = malloc(sizeof(fpos_t));
	fgetpos(fptr, fpos);
	int ln = Line;
	const char* str = ShiftToken();
	fsetpos(fptr, fpos);
	free(fpos);
	Line = ln;
	if(str == NULL)	return NULL;
	return Tokenize(str);
}

/// Peek the next token + n.
/// PeekTokenN(0) == PeekToken()
Token* PeekTokenN(int n){
	fpos_t* fpos = malloc(sizeof(fpos_t));
	int ln = Line;
	bool failed = false;
	fgetpos(fptr, fpos);
	for(int i = 0; i < n; i++){
		if(ShiftToken() == NULL){
			failed = true;
			break;
		}
	}
	const char* str = ShiftToken();
	if (str == NULL)
		failed = true;
	Token* t = failed ? NULL : Tokenize(str);
	fsetpos(fptr, fpos);
	free(fpos);
	Line = ln;
	return t;
}

Token* GetToken(){
	char* str = ShiftToken();
	return str ? Tokenize(str) : NULL;
}


#endif