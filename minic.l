%{
#include <stdio.h>
#include "ast.h"
#include "y.tab.h"
%}

%%
"int" { return INT; }
"read" { return READ; }
"print" { return PRINT; }
"return" { return RETURN; }
"==" { return EQ; }
"!=" { return NEQ; }
">=" { return GE; }
"<=" { return LE; }
"while" { return WHILE; }
"if" { return IF; }
"else" { return ELSE; }
"extern" { return EXTERN; }
"void" { return VOID; }
"main" { return MAIN; }
[0-9]+ { yylval.ival = atoi(yytext); return NUM; }
[A-Za-z][_|A-Za-z0-9]* { yylval.sval = strdup(yytext); return NAME; }
[ \n\t]
[//].*[\n]
. { return yytext[0]; }
%%

int yywrap() { return 1; }
