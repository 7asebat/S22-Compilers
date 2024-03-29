/* DEFINITIONS SECTION */
%{
    #include <stdlib.h>    // atoi
    #include "Parser.hpp"  // Token definitions

    // Call location_update before each token is processed
    int yycolno = 1; // Used to track current column in the line
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
lit_int   [0-9]+
lit_float {lit_int}\.{lit_int}

/* Operators */
operator [<>+\-*/%&|=^!~]

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
default  return DEFAULT;

proc     return PROC;
"::"     return DBL_COLON;
return   return RETURN;
"->"     return ARROW;

true     yylval->value.value = 1; return TRUE;
false    yylval->value.value = 0; return FALSE;

int      return INT;
uint     return UINT;
float    return FLOAT;
bool     return BOOL;

{identifier}    yylval->id         = yytext;        return IDENTIFIER;
{lit_int}       yylval->value.s64  = atoi(yytext);  return LIT_INT;

{lit_int}u { /* Unsigned int literal */
    yytext[yyleng-1]  = '\0';    // Remove 'u'
    yylval->value.u64 = atoi(yytext);
    return LIT_UINT;
}

{lit_float} {
    yylval->value.f64 = strtod(yytext, nullptr);
    return LIT_FLOAT;
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

[ \t]+          ; /* skip whitespace */
{line_comment}  ; /* skip line comments */
{line}          ; /* skip lines */

.            yyerror(yylloc, nullptr, "unexpected character"); /* Any other character */
%%

YY_BUFFER_STATE
s22::lexer_scan_buffer(char *buf, size_t buf_size)
{
    // Reset global line and column number
	yylineno = 1;
    yycolno = 1;

    // Start new buffer
    return yy_scan_string(buf);
    // return yy_scan_buffer(buf, buf_size); // This apparently causes issues and changes the underlying buffer
}

void
s22::lexer_delete_buffer(YY_BUFFER_STATE buf)
{
    yy_delete_buffer(buf);
}