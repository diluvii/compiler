%{
#include <stdio.h>
#include <vector>
#include "ast.h"
extern int yylex();
int yyerror(const char*);
astNode* root;
%}

%union {
	int ival;
	char* sval;
	astNode* node;
	vector<astNode*> *vec;
}

%token <ival> NUM
%token <sval> NAME
%token INT READ PRINT RETURN EQ NEQ LE GE WHILE IF ELSE MAIN EXTERN VOID

%type <node> s ext1 ext2 func param block decl stmt asgn expr read val var call ret cond while if else
%type <vec> decls stmts
%start s

%%
s	: ext1 ext2 func { root = createProg($1, $2, $3); }
	| ext1 func { root = createProg($1, NULL, $2); }
	| ext2 func { root = createProg(NULL, $1, $2); }
	| func { root = createProg(NULL, NULL, $1); }
ext1	: EXTERN VOID PRINT '(' INT ')' ';' { $$ = createExtern("print"); }
ext2	: EXTERN INT READ '(' ')' ';' { $$ = createExtern("read"); }
func	: INT NAME '(' ')' block { $$ = createFunc($2, NULL, $5); }
	| INT NAME '(' param ')' block { $$ = createFunc($2, $4, $6); }
param	: INT var { $$ = $2; }
block	: '{' decls stmts '}' { $2->insert($2->end(), $3->begin(), $3->end()); $$ = createBlock($2); }
decls	: { $$ = new vector<astNode*>(); }
	| decls decl { $$ = $1; $$->push_back($2); }
decl	: INT NAME ';' { $$ = createDecl($2); }
stmts	: { $$ = new vector<astNode*>(); }
	| stmts stmt { $$ = $1; $$->push_back($2); }
stmt	: asgn { $$ = $1; }
	| call { $$ = $1; }
	| ret { $$ = $1; }
	| block { $$ = $1; }
	| while { $$ = $1; }
	| if { $$ = $1; }
asgn	: var '=' expr ';' { $$ = createAsgn($1, $3); }
	| var '=' read ';' { $$ = createAsgn($1, $3); }
expr	: val '+' val { $$ = createBExpr($1, $3, add); }
	| val '-' val { $$ = createBExpr($1, $3, sub); }
	| val '*' val { $$ = createBExpr($1, $3, mul); }
	| val '/' val { $$ = createBExpr($1, $3, divide); }
	| '-' val { $$ = createUExpr($2, uminus); }
	| val { $$ = $1; }
read	: READ '(' ')' { $$ = createCall("read", NULL); }
val	: NUM { $$ = createCnst($1); }
	| var { $$ = $1; }
var	: NAME { $$ = createVar($1); }
call	: PRINT '(' val ')' ';' { $$ = createCall("print", $3); }
ret	: RETURN expr ';' { $$ = createRet($2); }
	| RETURN '(' expr ')' ';' { $$ = createRet($3); }
cond	: val '<' val { $$ = createRExpr($1, $3, lt); }
	| val '>' val { $$ = createRExpr($1, $3, gt); }
	| val EQ val { $$ = createRExpr($1, $3, eq); }
	| val NEQ val { $$ = createRExpr($1, $3, neq); }
	| val LE val { $$ = createRExpr($1, $3, le); }
	| val GE val { $$ = createRExpr($1, $3, ge); }
while	: WHILE '(' cond ')' block { $$ = createWhile($3, $5); }
	| WHILE '(' cond ')' stmt { $$ = createWhile($3, $5); }
if	: IF '(' cond ')' block { $$ = createIf($3, $5, NULL); }
	| IF '(' cond ')' block else { $$ = createIf($3, $5, $6); }
	| IF '(' cond ')' stmt { $$ = createIf($3, $5, NULL); }
	| IF '(' cond ')' stmt else { $$ = createIf($3, $5, $6); }
else	: ELSE block { $$ = $2; }
	| ELSE stmt { $$ = $2; }

%%

int yyerror(const char* s) {
	fprintf(stderr, "%s\n", s);
	return 0;
}
