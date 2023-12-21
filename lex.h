#ifndef LEX_INCLUDED
#define LEX_INCLUDED

#include <ctype.h>
#include <errno.h>

#include "defs.h"
#include "types.h"
#include "globals.h"

extern Token* transientToken;
// static Token* Tokenize(const char* str);
char* ShiftToken();
Token* PeekToken();
/// Peek the next token + n.
/// PeekTokenN(0) == PeekToken()
Token* PeekTokenN(int n);
Token* GetToken();
Token* GetTransientToken();
void SkipToken();


#endif