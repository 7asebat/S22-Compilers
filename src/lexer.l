%{
	/* Forward declarations used for lexing */
	#include <stdlib.h>  // atoi
	#include "parser.h"  // INTEGER

	void yyerror(char*);
%}

id_prefix 	[_a-zA-Z]
id_postfix	{id_prefix}|[0-9]
identifier	{id_prefix}{id_postfix}*

v_int		[-+]?[0-9]+
v_float		{v_int}\.[0-9]+

operator	[<>+\-*/&|=^!~]

line_comment "//"[^\n]*
line		 {line_comment}|\n

%%
const 			return CONST;
if 				return IF;
else 			return ELSE;
while 			return WHILE;
do 				return DO;
for 			return FOR;
switch 			return SWITCH;
case 			return CASE;
proc 			return PROC;
"return" 		return RETURN;

true 			yylval.value = 1; return TRUE;
false 			yylval.value = 0; return FALSE;

int				return INT;
uint			return UINT;
float			return FLOAT;
bool			return BOOL;

{identifier}	yylval.lexeme = strdup(yytext); return IDENTIFIER;
{v_int}			yylval.value  = atoi(yytext); return VALUE;
{v_float} {
	float v = atof(yytext);
	yylval.value = *(unsigned long long*)(&v);
	return VALUE;
}

"+="	return AS_ADD;
"-="	return AS_SUB;
"*="	return AS_MUL;
"/="	return AS_DIV;
"%="	return AS_MOD;
"&="	return AS_AND;
"|="	return AS_OR;
"^="	return AS_XOR;
"<<="	return AS_SHL;
">>="	return AS_SHR;

"<<"	return SHL;
">>"	return SHR;
"<="	return LEQ;
"=="	return EQ;
"!="	return NEQ;
">="	return GEQ;
"&&"	return L_AND;
"||"	return L_OR;


[;:{}]|{operator}	return *yytext;

[ \t]				; /* skip whitespace */
{line_comment}*		; /* skip line comments */
{line}+				return NL;

. 						yyerror(yytext);

%%
int
yywrap()
{
	return 1;
}

int
main(int argc, char *argv[])
{
	yyin = fopen(argv[1], "r");
	yydebug = 1;
	yyparse();
	fclose(yyin);
	return 0;
}