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

%parse-param    { Parser *p }

%locations              // Generate token location code
%define api.pure full   // Modify yylex to receive parameters as shown in %code provides
%define api.location.type {Source_Location}

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
%define parse.error verbose

// TOKENS
/// Keywords
%token <value> CONST WHILE DO FOR SWITCH CASE DEFAULT PROC RETURN
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
%token <id> IDENTIFIER
%token <value> LIT_INT LIT_UINT LIT_FLOAT

// Non-terminal types and print routines
%type <unit> program stmt
%type <unit> literal proc_call array_access
%type <unit> expr expr_math expr_logic
%type <type> type decl_proc_return
%type <unit> condition
%type <unit> assignment block loop
%type <unit> decl_var decl_const decl_proc

%printer { symtype_print(yyo, $$); } type
%printer { fprintf(yyo, "%s", $$.data); } IDENTIFIER

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
    { p->init(); } stmt_list { $program = p->block_end(); }
    ;

// stmt *
stmt_list:
    stmt_list
    stmt { p->block_add($stmt); }
    | %empty
    ;

block:
    '{' { p->block_begin(); }
        stmt_list
    '}' { $block = p->block_end(); }
    ;

block_no_action:
    '{' stmt_list '}'
    ;

stmt:
    block

    | conditional
    | loop

    | decl_var ';'
    | decl_const ';'
    | decl_proc

    | assignment ';'

    // Expressions (mainly procedure calls)
    | expr ';'

	// Return values
    | RETURN expr ';'   { p->return_value(@RETURN, $expr); }
    | RETURN ';'        { p->return_value(@RETURN); }
    ;

conditional:
    IF condition        { p->if_begin(@IF, $condition); }
        block
    conditional_else_if
    conditional_else    { p->if_end(@IF); }

    |
    SWITCH expr         { p->switch_begin(@SWITCH, $expr); }
    '{' switch_list '}' { p->switch_end(@SWITCH); }
    ;

// else_if *
conditional_else_if:
    conditional_else_if

    ELSE IF condition   { p->else_if_begin(@ELSE, $condition); }
        block

    | %empty
    ;

// else ?
conditional_else:
    ELSE                { p->else_begin(@ELSE); }
        block
    | %empty
    ;

// case *
switch_list:
    switch_list switch_case
    | %empty
    ;

switch_case:
    CASE                { p->switch_case_begin(@CASE); }
    switch_literal_list { p->switch_case_end(@CASE); }
        block
    |
    DEFAULT             { p->switch_default(@DEFAULT); }
        block
    ;


switch_literal_list:
    switch_literal_list ',' literal { p->switch_case_add($literal); }
    | literal                       { p->switch_case_add($literal); }
    ;

loop:
    WHILE condition         { p->while_begin(@WHILE, $condition); }
        block               { p->while_end(@WHILE); }
    |

    DO                      { p->do_while_begin(@DO); }
        block
    WHILE condition ';'     { p->do_while_end(@DO, $condition); }
    |

    FOR                                                      { p->block_begin(); }
    loop_for_init ';' loop_for_condition ';' loop_for_post
        block_no_action                                      { $$ = p->block_end(); }
    ;

loop_for_init:
    decl_var | %empty
    ;

loop_for_condition:
    condition | %empty
    ;

loop_for_post:
    assignment | expr | %empty
    ;

condition:
    expr { $$ = p->condition($expr); };

decl_var:
    IDENTIFIER ':' type                     { $$ = p->decl(@IDENTIFIER, $IDENTIFIER, $type); }
    | IDENTIFIER ':' type '=' expr          { $$ = p->decl_expr(@IDENTIFIER, $IDENTIFIER, $type, $expr); }
    | IDENTIFIER ':' '[' literal ']' type   { $$ = p->decl_array(@IDENTIFIER, $IDENTIFIER, $type, $literal); }
    ;

decl_const:
    CONST IDENTIFIER ':' type '=' expr      { $$ = p->decl_const(@IDENTIFIER, $IDENTIFIER, $type, $expr); }
    ;

decl_proc:
    IDENTIFIER ':' PROC                         { p->decl_proc_begin(@IDENTIFIER, $IDENTIFIER); p->block_begin(); }
    '(' decl_proc_params ')' decl_proc_return   { p->decl_proc_end($IDENTIFIER, $decl_proc_return); }
        block_no_action                         { $$ = p->block_end(); }
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
    | %empty { $$ = SYMTYPE_VOID; }
    ;

type:
    INT 	{ $$ = { .base = Symbol_Type::INT }; }
    | UINT 	{ $$ = { .base = Symbol_Type::UINT }; }
    | FLOAT { $$ = { .base = Symbol_Type::FLOAT }; }
    | BOOL 	{ $$ = { .base = Symbol_Type::BOOL }; }
    ;

assignment:
    IDENTIFIER '=' expr         { $$ = p->assign(@2, $IDENTIFIER, Op_Assign::MOV, $expr); }
    | IDENTIFIER AS_ADD expr    { $$ = p->assign(@2, $IDENTIFIER, Op_Assign::ADD, $expr); }
    | IDENTIFIER AS_SUB expr    { $$ = p->assign(@2, $IDENTIFIER, Op_Assign::SUB, $expr); }
    | IDENTIFIER AS_MUL expr    { $$ = p->assign(@2, $IDENTIFIER, Op_Assign::MUL, $expr); }
    | IDENTIFIER AS_DIV expr    { $$ = p->assign(@2, $IDENTIFIER, Op_Assign::DIV, $expr); }
    | IDENTIFIER AS_MOD expr    { $$ = p->assign(@2, $IDENTIFIER, Op_Assign::MOD, $expr); }
    | IDENTIFIER AS_AND expr    { $$ = p->assign(@2, $IDENTIFIER, Op_Assign::AND, $expr); }
    | IDENTIFIER AS_OR expr     { $$ = p->assign(@2, $IDENTIFIER, Op_Assign::OR,  $expr); }
    | IDENTIFIER AS_XOR expr    { $$ = p->assign(@2, $IDENTIFIER, Op_Assign::XOR, $expr); }
    | IDENTIFIER AS_SHL expr    { $$ = p->assign(@2, $IDENTIFIER, Op_Assign::SHL, $expr); }
    | IDENTIFIER AS_SHR expr    { $$ = p->assign(@2, $IDENTIFIER, Op_Assign::SHR, $expr); }

    | array_access '=' expr     { $$ = p->array_assign(@2, $array_access, Op_Assign::MOV, $expr); }
    | array_access AS_ADD expr  { $$ = p->array_assign(@2, $array_access, Op_Assign::ADD, $expr); }
    | array_access AS_SUB expr  { $$ = p->array_assign(@2, $array_access, Op_Assign::SUB, $expr); }
    | array_access AS_MUL expr  { $$ = p->array_assign(@2, $array_access, Op_Assign::MUL, $expr); }
    | array_access AS_DIV expr  { $$ = p->array_assign(@2, $array_access, Op_Assign::DIV, $expr); }
    | array_access AS_MOD expr  { $$ = p->array_assign(@2, $array_access, Op_Assign::MOD, $expr); }
    | array_access AS_AND expr  { $$ = p->array_assign(@2, $array_access, Op_Assign::AND, $expr); }
    | array_access AS_OR expr   { $$ = p->array_assign(@2, $array_access, Op_Assign::OR,  $expr); }
    | array_access AS_XOR expr  { $$ = p->array_assign(@2, $array_access, Op_Assign::XOR, $expr); }
    | array_access AS_SHL expr  { $$ = p->array_assign(@2, $array_access, Op_Assign::SHL, $expr); }
    | array_access AS_SHR expr  { $$ = p->array_assign(@2, $array_access, Op_Assign::SHR, $expr); }
    ;

expr:
	'(' expr ')' { $$ = $2; $$.loc = @1; }
    | expr_math
    | expr_logic
    | proc_call
    | array_access
    | IDENTIFIER { $$ = p->id(@IDENTIFIER, $IDENTIFIER); }
    | literal
    | TRUE       { $$ = p->literal(@TRUE,  $TRUE,  Symbol_Type::BOOL); }
    | FALSE      { $$ = p->literal(@FALSE, $FALSE, Symbol_Type::BOOL); }
    ;

literal:
    LIT_INT      { $$ = p->literal(@LIT_INT,   $LIT_INT,   Symbol_Type::INT); }
    | LIT_UINT   { $$ = p->literal(@LIT_UINT,  $LIT_UINT,  Symbol_Type::UINT); }
    | LIT_FLOAT  { $$ = p->literal(@LIT_FLOAT, $LIT_FLOAT, Symbol_Type::FLOAT); }
    ;

expr_math:
    '-' expr %prec U_MINUS      { $$ = p->unary(@1, Uny::NEG, $expr); }
    | '~' expr %prec U_LOGICAL  { $$ = p->unary(@1, Uny::INV, $expr); }

    | expr '+' expr             { $$ = p->binary(@2, $1, Bin::ADD, $3); }
    | expr '-' expr             { $$ = p->binary(@2, $1, Bin::SUB, $3); }
    | expr '*' expr             { $$ = p->binary(@2, $1, Bin::MUL, $3); }
    | expr '/' expr             { $$ = p->binary(@2, $1, Bin::DIV, $3); }
    | expr '%' expr             { $$ = p->binary(@2, $1, Bin::MOD, $3); }
    | expr '&' expr             { $$ = p->binary(@2, $1, Bin::AND, $3); }
    | expr '|' expr             { $$ = p->binary(@2, $1, Bin::OR,  $3); }
    | expr '^' expr             { $$ = p->binary(@2, $1, Bin::XOR, $3); }
    | expr SHL expr             { $$ = p->binary(@2, $1, Bin::SHL, $3); }
    | expr SHR expr             { $$ = p->binary(@2, $1, Bin::SHR, $3); }
    ;

expr_logic:
    '!' expr %prec U_LOGICAL    { $$ = p->unary(@1, Uny::NOT, $expr); }

    | expr '<' expr             { $$ = p->binary(@2, $1, Bin::SHR,   $3); }
    | expr LEQ expr             { $$ = p->binary(@2, $1, Bin::LEQ,   $3); }
    | expr EQ expr              { $$ = p->binary(@2, $1, Bin::EQ,    $3); }
    | expr NEQ expr             { $$ = p->binary(@2, $1, Bin::NEQ,   $3); }
    | expr GEQ expr             { $$ = p->binary(@2, $1, Bin::GEQ,   $3); }
    | expr '>' expr             { $$ = p->binary(@2, $1, Bin::GT,    $3); }
    | expr L_AND expr           { $$ = p->binary(@2, $1, Bin::L_AND, $3); }
    | expr L_OR expr            { $$ = p->binary(@2, $1, Bin::L_OR,  $3); }
    ;

proc_call:
    // Optional parameters
    IDENTIFIER               { p->pcall_begin(); }
    '(' proc_call_params ')' { $$ = p->pcall(@1, $IDENTIFIER); }
    ;

// param *
// Had to be split to allow left recursive grammar without the (, one_param) case
proc_call_params:
    comma_separated_exprs
    | %empty
    ;

comma_separated_exprs:
    comma_separated_exprs ',' expr  { p->pcall_add($expr); }
    | expr                          { p->pcall_add($expr); }
    ;

array_access:
    IDENTIFIER '[' expr ']' { $$ = p->array(@2, $IDENTIFIER, $expr); }
    ;

%%