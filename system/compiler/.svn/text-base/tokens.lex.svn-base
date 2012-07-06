%{
#include <limits.h>
#include <string.h>
#include "parser.tab.h"
%}

%option noyywrap
%option yylineno

%%

"+" {return PLUS;}
"-" {return MINUS;}
"*" {return STAR;}
"/" {return SLASH;}
"^" {return XOR;}
"&" {return AND;}
"." {return DOT;}
"[" {return OB;}
"]" {return CB;}
"{" {return OCURL;}
"}" {return CCURL;}
";" {return SEM;}
"(" {return '(';}
")" {return ')';}
"!" {return '!';}
":" {return ':';}
"," {return COMMA;}
== {return EQ;}
">" {return G;}
"<" {return L;}
">>" {return SHIFTR;}
"<<" {return SHIFTL;}
===[A-Za-z0-9 ]* {fprintf(stderr, "%s\n",yytext);}
:= {return EQUALS;}
defvar {return DEFVAR;}
defun {return DEFUN;}
input {return INPUT;}
output {return OUTPUT;}
for {return FOR;}
from {return FROM;}
to {return TO;}
begin {return LOOP;}
end {return END;}
if {return IF;}
then {return THEN;}
else {return ELSE;}
return {return RET;}
step {return STEP;}
\/\/.*\n {}
[ \t\n] {}
[0-9]+ {yylval.val = atoi(yytext); return NUMBER;}
[A-Za-z_]+[A-Za-z0-9_]*  {strcpy(yylval.sym,yytext); return VAR;}
. {fprintf(stderr, "Unknown token %s\n", yytext);}

%%
