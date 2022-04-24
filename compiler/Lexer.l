/* DEFINITIONS SECTION */
%{
    #include <stdlib.h>    // atoi
    #include <stdio.h>     // getline
    #include "Parser.hpp"  // Token definitions

    // Call location_update before each token is processed
    #define YY_USER_ACTION location_update(yylloc);
%}

/* Use yylex(symbol_pointer, location_pointer) */
%option bison-locations
/* Keep track of line number */
%option yylineno
/* Do not wrap once EOF is hit */
%option noyywrap
/* Do not depend on platform specific headers */
%option nounistd
/* Only read input files */
%option never-interactive

/* Unique identifiers */
id_prefix  [_a-zA-Z]
id_postfix {id_prefix}|[0-9]
identifier {id_prefix}{id_postfix}*

/* Constants */
v_int   [-+]?[0-9]+
v_float {v_int}\.[0-9]+

/* Operators */
operator [<>+\-*/&|=^!~]

/* A line can be preceeded by a line comment */
line_comment "//"[^\n]*
line         {line_comment}?\n

/* RULES SECTION */
%%
const    return CONST;
if       return IF;
else     return ELSE;
while    return WHILE;
do       return DO;
for      return FOR;
switch   return SWITCH;
case     return CASE;

proc     return PROC;
return   return RETURN;
"->"     return ARROW;

true     yylval->value = 1; return TRUE;
false    yylval->value = 0; return FALSE;

int      return INT;
uint     return UINT;
float    return FLOAT;
bool     return BOOL;

{identifier}    yylval->lexeme = strdup(yytext); return IDENTIFIER;
{v_int}         yylval->value  = atoi(yytext);   return CONSTANT;
{v_float} {
    float v = atof(yytext);
    yylval->value = *(unsigned long long*)(&v);
    return CONSTANT;
}

"+="    return AS_ADD;
"-="    return AS_SUB;
"*="    return AS_MUL;
"/="    return AS_DIV;
"%="    return AS_MOD;
"&="    return AS_AND;
"|="    return AS_OR;
"^="    return AS_XOR;
"<<="   return AS_SHL;
">>="   return AS_SHR;

"<<"    return SHL;
">>"    return SHR;
"<="    return LEQ;
"=="    return EQ;
"!="    return NEQ;
">="    return GEQ;
"&&"    return L_AND;
"||"    return L_OR;


[;:{},]     return *yytext; /* Return these characters as is */
"("|")"     return *yytext;  
"["|"]"     return *yytext;  
{operator}  return *yytext;

[ \t]+       ; /* skip whitespace */
{line}       ; /* skip lines */

.            yyerror(yylloc, nullptr, "Unexpected character"); /* Any other character */

%%
/* OTHERS SECTION */
// Entry point
int
main(int argc, char *argv[])
{
    // Input stream, open first argument as a file
    yyin = fopen(argv[1], "r");
    if (yyin == nullptr)
        return -1;

	// Disable debugging by default, set debugging if second arg is passed
    yydebug = 0;
    if (argc > 2)
        yydebug = 1;

    // Run parser
    s22::Parser parser = {};
    parser.current_scope = &parser.global;
    yyparse(&parser);

    fclose(yyin);
    return 0;
}