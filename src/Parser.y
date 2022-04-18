// Top level code
%code top {
    #include <stdio.h>	// printf
}

// Different types of tokens
%union {
    const char* lexeme;
    unsigned long long value;
}

%locations              // Generate token location code
%define api.pure full   // Modify yylex to receive parameters as shown in %code provides

// Forward declarations for Lexer
%code provides {
    int
    yylex(YYSTYPE *symbol, YYLTYPE *location);

    void
    yyerror(YYLTYPE *location, const char *message);
}

// Enable debugging
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
//// Return value separator
%token <value> ARROW

/// Others
%token <lexeme> IDENTIFIER
%token <value> CONSTANT

// Non-terminal types and print routines
%type <value> expr expr_math expr_logic 
%type <lexeme> type

%printer { fprintf(yyo, "%s", $$); } type
%printer { fprintf(yyo, "%s", $$); } IDENTIFIER
%printer { fprintf(yyo, "%d", $$); } CONSTANT
%printer { fprintf(yyo, "%d", $$); } expr

// OPERATOR PRECEDENCE (LOWEST TO HIGHEST)
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

// stmt *
stmt_list:
    stmt_list stmt
    | %empty
    ;

scope:
    '{' stmt_list '}'

stmt:
    scope

    | conditional
    | loop

    | decl_var ';'
    | decl_const ';'
    | decl_proc

    | assignment ';'

    // Expressions (mainly procedure calls)
    | expr ';'

	// Return values
    | RETURN expr ';'
    | RETURN ';'
    ;

conditional:
    IF expr
        scope
	// Optional else if
    conditional_else_if
    // Optional else
    conditional_else
    |

    SWITCH expr
        '{' switch_list '}';
    ;

// else_if *
conditional_else_if:
    conditional_else_if

    ELSE IF expr
        scope

    | %empty
    ;

// else ?
conditional_else:
    ELSE 
        scope
    | %empty
    ;

// case *
switch_list:
    switch_list CASE CONSTANT
        scope
    | %empty
    ;

loop:
    WHILE expr
        scope
    |

    DO
        scope 
    WHILE expr ';'
    |

    FOR decl_var ';' expr_logic ';' assignment
        scope
    ;

decl_var:
    IDENTIFIER ':' type 			{} // No action
    | IDENTIFIER ':' type '=' expr 	{} // No action
    ;

decl_const:
    CONST IDENTIFIER ':' type '=' expr 	{} // No action

decl_proc:
    // Optional parameter list
    // Optional return type
    IDENTIFIER ':' PROC '(' decl_proc_params ')' decl_proc_return
        scope
    ;


// param *
// Had to be split to allow left recursive grammar without the (, one_param) case
decl_proc_params:
    decl_proc_params_list
    | %empty
    ;

decl_proc_params_list:
    decl_proc_params_list ',' decl_var
    | decl_var
    ;

// return ?
decl_proc_return:
    ARROW type
    | %empty
    ;

type:
    INT 	{ $$ = "int"; }
    | UINT 	{ $$ = "uint"; }
    | FLOAT { $$ = "float"; }
    | BOOL 	{ $$ = "bool"; }
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

expr:
    expr_math | expr_logic
    | proc_call {}
    | IDENTIFIER {}
    | CONSTANT | TRUE | FALSE

expr_math:
    '-' expr %prec U_MINUS {} // No action
    | '!' expr %prec U_LOGICAL {} // No action
    | '~' expr %prec U_LOGICAL {} // No action
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

expr_logic: expr '<' expr
    | expr LEQ expr
    | expr EQ expr
    | expr NEQ expr
    | expr GEQ expr
    | expr '>' expr
    | expr L_AND expr
    | expr L_OR expr
    ;

proc_call:
    // Optional parameters
    IDENTIFIER '(' proc_call_params ')'

// param *
// Had to be split to allow left recursive grammar without the (, one_param) case
proc_call_params:
    proc_call_params_list
    | %empty
    ;

proc_call_params_list:
    proc_call_params_list ',' expr
    | expr
    ;
%%