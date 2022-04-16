%code top {
    #include <stdio.h>	// printf

    extern void yyerror(char *s);
    extern int yylex();	// Lexer
}

%union {
    const char* lexeme;
    unsigned long long value;
}

%define parse.trace

// Text values
%token <lexeme> IDENTIFIER
%token <value> VALUE

// Keywords
%token <value> CONST WHILE DO FOR SWITCH CASE PROC RETURN
%token <value> TRUE FALSE
%token <value> INT UINT FLOAT BOOL

// Operators
%token <value> SHL SHR LEQ EQ NEQ GEQ
%token <value> L_AND L_OR
%token <value> AS_ADD AS_SUB AS_MUL AS_DIV AS_MOD AS_AND AS_OR AS_XOR AS_SHL AS_SHR 

// Spacing
%token <value> NL

%token <value> IF ELSE
%nonassoc ELSE

%type <value> expr expr_math expr_logic 
%type <lexeme> type

%printer { fprintf(yyo, "%s", $$); } IDENTIFIER
%printer { fprintf(yyo, "%d", $$); } VALUE
%printer { fprintf(yyo, "%d", $$); } expr

// Precedence
%left L_OR
%left L_AND

%left '|'
%left '^'
%left '&'

%left '<' LEQ EQ NEQ '>' GEQ

%left SHL SHR
%left '+' '-'
%left '*' '/' '%'

%nonassoc U_LOGICAL
%nonassoc U_MINUS

%%

program:
    stmt_list

stmt_list:
    wrapping_NL stmts wrapping_NL
    | wrapping_NL // EMPTY
    ;

wrapping_NL:
    NL 
    | // EMPTY
    ;

stmts:
    stmts NL stmt
    | stmt
    ;

braced_block:
    '{' stmt_list '}'

stmt:
    decl | assignment | expr | conditional | loop
    | braced_block
    ;

assignment:
    IDENTIFIER '=' expr
    | IDENTIFIER AS_ADD expr
    | IDENTIFIER AS_SUB expr
    | IDENTIFIER AS_MUL expr
    | IDENTIFIER AS_DIV expr
    | IDENTIFIER AS_MOD expr
    | IDENTIFIER AS_AND expr
    | IDENTIFIER AS_OR expr
    | IDENTIFIER AS_XOR expr
    | IDENTIFIER AS_SHL expr
    | IDENTIFIER AS_SHR expr
    ;

conditional:
    IF expr NL 
        braced_block NL
    |

    IF expr NL 
        braced_block NL 
    ELSE NL 
        braced_block
    |

    SWITCH expr NL
        '{' switch_list '}'
    ;

switch_list:
    wrapping_NL switch_cases wrapping_NL
    ;

switch_cases:
    switch_cases NL switch_case
    | switch_case
    ;

switch_case:
    CASE VALUE braced_block;

loop:
    WHILE expr NL 
        braced_block
    |

    DO NL
        braced_block WHILE expr
    |

    FOR decl_var ';' expr_logic ';' assignment NL
        braced_block
    ;

decl:
    decl_var | decl_const;

decl_var:
    IDENTIFIER ':' type 			{}
    | IDENTIFIER ':' type '=' expr 	{}
    ;

decl_const:
    CONST IDENTIFIER ':' type '=' expr 	{}

expr:
    expr_math | expr_logic
    | IDENTIFIER {}
    | VALUE | TRUE | FALSE

expr_math:
    '-' expr %prec U_MINUS {}
    | '!' expr %prec U_LOGICAL {}
    | '~' expr %prec U_LOGICAL {}
    | expr '+' expr
    | expr '-' expr
    | expr '*' expr
    | expr '/' expr
    | expr '&' expr
    | expr '|' expr
    | expr '^' expr
    | expr SHL expr
    | expr SHR expr
    ;

expr_logic:
    expr '<' expr
    | expr LEQ expr
    | expr EQ expr
    | expr NEQ expr
    | expr GEQ expr
    | expr '>' expr
    | expr L_AND expr
    | expr L_OR expr
    ;

type:
    INT 	{ $$ = "int"; }
    | UINT 	{ $$ = "uint"; }
    | FLOAT { $$ = "float"; }
    | BOOL 	{ $$ = "bool"; }
    ;
%%

void
yyerror(char *s)
{
    fprintf(stderr, "%s\n", s);
}