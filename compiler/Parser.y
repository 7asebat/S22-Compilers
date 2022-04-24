// Top level code
%code top {
    #include <stdio.h>	// printf
}

// Required for token union definition
%code requires {
    #include "compiler/Parser.h"
    using namespace s22;
}
%define api.value.type {Parser::YY_Symbol}

%parse-param {Parser *p}

%locations              // Generate token location code
%define api.pure full   // Modify yylex to receive parameters as shown in %code provides
%define api.location.type {Location}

// Forward declarations for Lexer
%code provides {
    #define YY_DECL \
    int \
    yylex(YYSTYPE * yylval_param, YYLTYPE * yylloc_param)

    YY_DECL;

    #define YYLLOC_DEFAULT location_reduce
    #define YYLOCATION_PRINT location_print
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
%token <value> LIT_INT LIT_UINT LIT_FLOAT
%nterm <expr> literal

// Non-terminal types and print routines
%type <expr> expr expr_math expr_logic proc_call array_access
%type <type> type type_standard
%type <type> decl_proc_return

%printer { symtype_print(yyo, $$); } type
%printer { fprintf(yyo, "%s", $$); } IDENTIFIER

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

%nonassoc U_LOGICAL    // Unary logical operators !,~
%nonassoc U_MINUS      // Unary -

%left '['           // Subscript operator
%left '('           // Proc call operator
%%

program:
    { p->push(); }
        stmt_list
    { p->pop(); }

// stmt *
stmt_list:
    stmt_list
    { p->new_stmt(); } stmt
    | %empty
    ;

scope:
    '{' { p->push(); }
        stmt_list
    '}' { p->pop(); }
    ;

scope_no_action:
    '{' stmt_list '}'
    ;

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
    | RETURN expr ';' { p->return_value(@RETURN, $expr); }
    | RETURN ';'      { p->return_value(@RETURN); }
    ;

conditional:
    IF expr
        scope
    conditional_else_if /* Optional else if */
    conditional_else    /* Optional else */
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
    switch_list CASE switch_literal_list
        scope
    | %empty
    ;

switch_literal_list:
    switch_literal_list ',' literal
    | literal
    ;

loop:
    WHILE expr
        scope
    |

    DO
        scope 
    WHILE expr ';'
    |

    FOR decl_var ';' expr ';' assignment
        scope
    ;

decl_var:
    IDENTIFIER ':' type                     { p->decl($IDENTIFIER, @IDENTIFIER, $type); }
    | IDENTIFIER ':' type '=' expr          { p->decl_expr($IDENTIFIER, @IDENTIFIER, $type, $expr); }
    | IDENTIFIER ':' '[' literal ']' type   { p->decl_array($IDENTIFIER, @IDENTIFIER, $type, $literal); }
    ;

decl_const:
    CONST IDENTIFIER ':' type '=' expr      { p->decl_const($IDENTIFIER, @IDENTIFIER, $type, $expr); }
    ;

decl_proc:
    IDENTIFIER ':' PROC                         { p->decl_proc_begin($IDENTIFIER, @IDENTIFIER); p->push(); }
    '(' decl_proc_params ')' decl_proc_return   { p->decl_proc_end($IDENTIFIER, $decl_proc_return); }
        scope_no_action                         { p->pop(); }
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
    ARROW type { $$ = $2; }
    | %empty { $$.base = Symbol_Type::VOID; }
    ;

type:
    type_standard
    | '*' type { $$ = $2; (*$$.pointer)++; } // Pointer to type
    ;

type_standard:
    INT 	{ $$.base = Symbol_Type::INT; }
    | UINT 	{ $$.base = Symbol_Type::UINT; }
    | FLOAT { $$.base = Symbol_Type::FLOAT; }
    | BOOL 	{ $$.base = Symbol_Type::BOOL; }
    ;

assignment:
    IDENTIFIER '=' expr       { p->assign($1, @1, Op_Assign::MOV, $expr); }
    | IDENTIFIER AS_ADD expr  { p->assign($1, @1, Op_Assign::ADD, $expr); }
    | IDENTIFIER AS_SUB expr  { p->assign($1, @1, Op_Assign::SUB, $expr); }
    | IDENTIFIER AS_MUL expr  { p->assign($1, @1, Op_Assign::MUL, $expr); }
    | IDENTIFIER AS_DIV expr  { p->assign($1, @1, Op_Assign::DIV, $expr); }
    | IDENTIFIER AS_MOD expr  { p->assign($1, @1, Op_Assign::MOD, $expr); }
    | IDENTIFIER AS_AND expr  { p->assign($1, @1, Op_Assign::AND, $expr); }
    | IDENTIFIER AS_OR expr   { p->assign($1, @1, Op_Assign::OR,  $expr); }
    | IDENTIFIER AS_XOR expr  { p->assign($1, @1, Op_Assign::XOR, $expr); }
    | IDENTIFIER AS_SHL expr  { p->assign($1, @1, Op_Assign::SHL, $expr); }
    | IDENTIFIER AS_SHR expr  { p->assign($1, @1, Op_Assign::SHR, $expr); }
    ;

expr:
	'(' expr ')' { $$ = $2; }
    | expr_math
    | expr_logic
    | proc_call
    | array_access
    | IDENTIFIER { p->id($IDENTIFIER, @IDENTIFIER, $$); }
    | literal
    | TRUE       { p->literal($TRUE,  @TRUE,  Symbol_Type::BOOL, $$); }
    | FALSE      { p->literal($FALSE, @FALSE, Symbol_Type::BOOL, $$); }

literal:
    LIT_INT      { p->literal($LIT_INT,   @LIT_INT,   Symbol_Type::INT,   $$); }
    | LIT_UINT   { p->literal($LIT_UINT,  @LIT_UINT,  Symbol_Type::UINT,  $$); }
    | LIT_FLOAT  { p->literal($LIT_FLOAT, @LIT_FLOAT, Symbol_Type::FLOAT, $$); }

expr_math:
    '-' expr %prec U_MINUS      { p->unary(Op_Unary::NEG, $expr, @expr, $$); }
    | '!' expr %prec U_LOGICAL  { p->unary(Op_Unary::NOT, $expr, @expr, $$); }
    | '~' expr %prec U_LOGICAL  { p->unary(Op_Unary::INV, $expr, @expr, $$); }

    | expr '+' expr { p->binary($1, @1, Op_Binary::ADD, $3, $$); }
    | expr '-' expr { p->binary($1, @1, Op_Binary::SUB, $3, $$); }
    | expr '*' expr { p->binary($1, @1, Op_Binary::MUL, $3, $$); }
    | expr '/' expr { p->binary($1, @1, Op_Binary::DIV, $3, $$); }
    | expr '%' expr { p->binary($1, @1, Op_Binary::MOD, $3, $$); }
    | expr '&' expr { p->binary($1, @1, Op_Binary::AND, $3, $$); }
    | expr '|' expr { p->binary($1, @1, Op_Binary::OR,  $3, $$); }
    | expr '^' expr { p->binary($1, @1, Op_Binary::XOR, $3, $$); }
    | expr SHL expr { p->binary($1, @1, Op_Binary::SHL, $3, $$); }
    | expr SHR expr { p->binary($1, @1, Op_Binary::SHR, $3, $$); }
    ;

expr_logic:
    expr '<' expr     { p->binary($1, @1, Op_Binary::SHR,   $3, $$); }
    | expr LEQ expr   { p->binary($1, @1, Op_Binary::LEQ,   $3, $$); }
    | expr EQ expr    { p->binary($1, @1, Op_Binary::EQ,    $3, $$); }
    | expr NEQ expr   { p->binary($1, @1, Op_Binary::NEQ,   $3, $$); }
    | expr GEQ expr   { p->binary($1, @1, Op_Binary::GEQ,   $3, $$); }
    | expr '>' expr   { p->binary($1, @1, Op_Binary::GT,    $3, $$); }
    | expr L_AND expr { p->binary($1, @1, Op_Binary::L_AND, $3, $$); }
    | expr L_OR expr  { p->binary($1, @1, Op_Binary::L_OR,  $3, $$); }
    ;

proc_call:
    // Optional parameters
    IDENTIFIER { p->pcall_begin(); } '(' proc_call_params ')' { p->pcall($IDENTIFIER, @IDENTIFIER, $$); }

// param *
// Had to be split to allow left recursive grammar without the (, one_param) case
proc_call_params:
    comma_separated_exprs
    | %empty
    ;

comma_separated_exprs:
    comma_separated_exprs ',' expr { p->pcall_add($expr); }
    | expr { p->pcall_add($expr); }
    ;

array_access:
    IDENTIFIER '[' expr ']' { p->array($IDENTIFIER, @IDENTIFIER, $expr, $$); }
    ;

%%