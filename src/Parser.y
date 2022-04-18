%code top {
    #include <stdio.h>	// printf
}

%union {
    const char* lexeme;
    unsigned long long value;
}

%locations
%define api.pure full

%code provides {
    int
    yylex(YYSTYPE*, YYLTYPE*);

    void
    yyerror(YYLTYPE *loc, const char* msg);
}

%define parse.trace

// TOKENS
/// Keywords
%token <value> CONST WHILE DO FOR SWITCH CASE PROC RETURN
%token <value> TRUE FALSE
%token <value> INT UINT FLOAT BOOL
%token <value> IF ELSE

/// Operators
//// Logical operators
%token <value> L_AND L_OR LEQ EQ NEQ GEQ
//// Bitwise operators
%token <value> SHL SHR
//// Assignment operators
%token <value> AS_ADD AS_SUB AS_MUL AS_DIV AS_MOD AS_AND AS_OR AS_XOR AS_SHL AS_SHR 
/// Other
%token <lexeme> IDENTIFIER
%token <value> VALUE

// Non-terminal types and print routines
%type <value> expr expr_math expr_logic 
%type <lexeme> type

%printer { fprintf(yyo, "%s", $$); } type
%printer { fprintf(yyo, "%s", $$); } IDENTIFIER
%printer { fprintf(yyo, "%d", $$); } VALUE
%printer { fprintf(yyo, "%d", $$); } expr

// PRECEDENCE (LOWEST TO HIGHEST)
%left L_OR
%left L_AND

%left '|'
%left '^'
%left '&'

%left '<' LEQ EQ NEQ '>' GEQ

%left SHL SHR
%left '+' '-'
%left '*' '/' '%'

%nonassoc U_LOGICAL // Unary logical operators !,~
%nonassoc U_MINUS   // Unary -

%%

program:
    stmt_list { yyerror(&@$, "Complete!"); }

stmt_list:
    stmt_list stmt
    | // EMPTY
    ;

scope:
    '{' stmt_list '}'

stmt:
    decl ';' | assignment ';' | expr ';' | conditional | loop
    | scope
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
    IF expr
        scope
    |

    IF expr 
        scope 
    ELSE 
        scope
    |

    SWITCH expr
        '{' switch_list '}';
    ;


switch_list:
    switch_list switch_case
    | // EMPTY
    ;

switch_case:
    CASE VALUE
        scope;

loop:
    WHILE expr
        scope
    |

    DO
        scope WHILE expr ';'
    |

    FOR decl_var ';' expr_logic ';' assignment
        scope
    ;

decl:
    decl_var | decl_const
    ;

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