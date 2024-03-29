%code top {
	#include <stdio.h>  // printf
}

%require "3.8" // Require Bison v3.8+

// Required for token type definition
%code requires {
	#include "compiler/Parser.h"
	using namespace s22;
}

%define api.value.type		{ s22::YY_Symbol }			// token data type
%parse-param				{ s22::Parser *p }			// parser function parameters
%locations												// generate token location code
%define api.pure full									// modify yylex to receive parameters as shown in %code provides
%define api.location.type	{ s22::Source_Location }	// source location type, aliased by YYLTYPE
%define parse.trace										// allow debugging
%define parse.error custom								// custom error handling function

// Forward declarations for Lexer
%code provides {
	// Lexer entry-point declaration
	#define YY_DECL \
	int \
	yylex(YYSTYPE * yylval_param, YYLTYPE * yylloc_param)

	YY_DECL;

	// Location actions
	#define YYLLOC_DEFAULT location_reduce
	#define YYLOCATION_PRINT location_print
}

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
//// Proc declaration
%token <value> DBL_COLON

/// Others
%token <id> IDENTIFIER
%token <value> LIT_INT LIT_UINT LIT_FLOAT

// Non-terminal types and print routines
%type <unit> program stmt
%type <unit> literal proc_call array_access
%type <unit> expr expr_math expr_logic
%type <type> type type_base type_array decl_proc_return
%type <unit> conditional c_else c_else_ifs
%type <unit> assignment block loop loop_for_init loop_for_condition loop_for_post
%type <unit> decl_var decl_const decl_proc

%printer { semexpr_print($$, yyo); } type
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

%nonassoc U_LOGICAL		// Unary logical operators !,~
%nonassoc U_MINUS		// Unary -

%left '['			// Subscript operator
%left '('			// Proc call operator
%%

program:
					{ p->program_begin(); }
	stmt_list		{ p->program_end(); }
	;

// stmt *
stmt_list:
	stmt_list stmt	{ p->block_add($stmt); }
	| %empty
	;

block:
	'{'				{ p->block_begin(); }
		stmt_list
	'}'				{ $block = p->block_end(); }
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
	| RETURN expr ';'	{ $$ = p->return_value(@RETURN, $expr); }
	| RETURN ';'		{ $$ = p->return_value(@RETURN); }
	;

conditional:
	IF expr
		block
	c_else				{ $$ = p->if_cond($expr, $block, $c_else); }

	|
	SWITCH expr			{ p->switch_begin($expr); }
	'{' switch_list '}'	{ $$ = p->switch_end(@SWITCH); }
	;

c_else:
	%empty				{ $$ = {}; }
	|
	c_else_ifs			{ $$ = $c_else_ifs; }
	|
	c_else_ifs
	ELSE block			{ $$ = p->else_if_cond($1, {}, $block); }
	|
	ELSE block			{ $$ = p->if_cond({}, $block, {}); }
	;

c_else_ifs:
	c_else_ifs
	ELSE IF expr block	{ $$ = p->else_if_cond($1, $expr, $block); }
	|
	ELSE IF expr block	{ $$ = p->if_cond($expr, $block, {}); }
	;

// case *
switch_list:
	switch_list switch_case
	| %empty
	;

switch_case:
	CASE							{ p->switch_case_begin(@CASE); }
	switch_literal_list block		{ p->switch_case_end(@CASE, $block); }
	|
	DEFAULT block					{ p->switch_default(@DEFAULT, $block); }
	;


switch_literal_list:
	switch_literal_list ',' literal	{ p->switch_case_add($literal); }
	| literal						{ p->switch_case_add($literal); }
	;

loop:
	WHILE expr
		block						{ $$ = p->while_loop(@WHILE, $expr, $block); }
	|

	DO
		block
	WHILE expr ';'					{ $$ = p->do_while_loop(@WHILE, $expr, $block); }
	|

	FOR														{ p->for_loop_begin(); }
	loop_for_init ';' loop_for_condition ';' loop_for_post
		block_no_action										{ $$ = p->for_loop(@FOR, $loop_for_init, $loop_for_condition, $loop_for_post); }
	;

loop_for_init:
	decl_var
	| %empty { $$ = {}; }
	;

loop_for_condition:
	expr
	| %empty { $$ = {}; }
	;

loop_for_post:
	assignment | expr
	| %empty { $$ = {}; }
	;

decl_var:
	IDENTIFIER ':' type							{ $$ = p->decl(@IDENTIFIER, $IDENTIFIER, $type); }
	| IDENTIFIER ':' type '=' expr				{ $$ = p->decl_expr(@IDENTIFIER, $IDENTIFIER, $type, $expr); }
	;

decl_const:
	CONST IDENTIFIER ':' type '=' expr			{ $$ = p->decl_const(@IDENTIFIER, $IDENTIFIER, $type, $expr); }
	;

decl_proc:
	IDENTIFIER DBL_COLON PROC					{ p->decl_proc_begin(); }
	'(' decl_proc_params ')' decl_proc_return	{ p->decl_proc_params_end(@IDENTIFIER, $IDENTIFIER, $decl_proc_return); }
		block_no_action							{ $$ = p->decl_proc_end($IDENTIFIER); }
	;


// param *
// Had to be split to allow left recursive grammar without the (, one_param) case
decl_proc_params:
	decl_proc_params_list
	| %empty
	;

decl_proc_params_list:
	decl_proc_params_list ',' decl_var	{ p->decl_proc_params_add($decl_var); }
	| decl_var							{ p->decl_proc_params_add($decl_var); }
	;

// return ?
decl_proc_return:
	ARROW type	{ $$ = $2; }
	| %empty 	{ $$ = SEMEXPR_VOID; }
	;

type:
	type_base
	| type_array
	;

type_base:
	INT			{ $$ = SEMEXPR_INT; }
	| UINT		{ $$ = SEMEXPR_UINT; }
	| FLOAT		{ $$ = SEMEXPR_FLOAT; }
	| BOOL		{ $$ = SEMEXPR_BOOL; }
	;

type_array:
	'[' literal ']' type_base		{ $$ = p->type_array(@literal, $literal, $type_base); }

assignment:
	IDENTIFIER '=' expr			{ $$ = p->assign(@2, $IDENTIFIER, Asn::MOV, $expr); }
	| IDENTIFIER AS_ADD expr	{ $$ = p->assign(@2, $IDENTIFIER, Asn::ADD, $expr); }
	| IDENTIFIER AS_SUB expr	{ $$ = p->assign(@2, $IDENTIFIER, Asn::SUB, $expr); }
	| IDENTIFIER AS_MUL expr	{ $$ = p->assign(@2, $IDENTIFIER, Asn::MUL, $expr); }
	| IDENTIFIER AS_DIV expr	{ $$ = p->assign(@2, $IDENTIFIER, Asn::DIV, $expr); }
	| IDENTIFIER AS_MOD expr	{ $$ = p->assign(@2, $IDENTIFIER, Asn::MOD, $expr); }
	| IDENTIFIER AS_AND expr	{ $$ = p->assign(@2, $IDENTIFIER, Asn::AND, $expr); }
	| IDENTIFIER AS_OR expr 	{ $$ = p->assign(@2, $IDENTIFIER, Asn::OR,  $expr); }
	| IDENTIFIER AS_XOR expr	{ $$ = p->assign(@2, $IDENTIFIER, Asn::XOR, $expr); }
	| IDENTIFIER AS_SHL expr	{ $$ = p->assign(@2, $IDENTIFIER, Asn::SHL, $expr); }
	| IDENTIFIER AS_SHR expr	{ $$ = p->assign(@2, $IDENTIFIER, Asn::SHR, $expr); }

	| array_access '=' expr		{ $$ = p->array_assign(@2, $array_access, Asn::MOV, $expr); }
	| array_access AS_ADD expr	{ $$ = p->array_assign(@2, $array_access, Asn::ADD, $expr); }
	| array_access AS_SUB expr	{ $$ = p->array_assign(@2, $array_access, Asn::SUB, $expr); }
	| array_access AS_MUL expr	{ $$ = p->array_assign(@2, $array_access, Asn::MUL, $expr); }
	| array_access AS_DIV expr	{ $$ = p->array_assign(@2, $array_access, Asn::DIV, $expr); }
	| array_access AS_MOD expr	{ $$ = p->array_assign(@2, $array_access, Asn::MOD, $expr); }
	| array_access AS_AND expr	{ $$ = p->array_assign(@2, $array_access, Asn::AND, $expr); }
	| array_access AS_OR expr	{ $$ = p->array_assign(@2, $array_access, Asn::OR,  $expr); }
	| array_access AS_XOR expr	{ $$ = p->array_assign(@2, $array_access, Asn::XOR, $expr); }
	| array_access AS_SHL expr	{ $$ = p->array_assign(@2, $array_access, Asn::SHL, $expr); }
	| array_access AS_SHR expr	{ $$ = p->array_assign(@2, $array_access, Asn::SHR, $expr); }
	;

expr:
	'(' expr ')'	{ $$ = $2; $$.loc = @1; }
	| expr_math
	| expr_logic
	| proc_call
	| array_access
	| IDENTIFIER	{ $$ = p->id(@IDENTIFIER, $IDENTIFIER); }
	| literal
	| TRUE			{ $$ = p->literal(@TRUE,  $TRUE,  Semantic_Expr::BOOL); }
	| FALSE			{ $$ = p->literal(@FALSE, $FALSE, Semantic_Expr::BOOL); }
	;

literal:
	LIT_INT			{ $$ = p->literal(@LIT_INT,   $LIT_INT,   Semantic_Expr::INT); }
	| LIT_UINT		{ $$ = p->literal(@LIT_UINT,  $LIT_UINT,  Semantic_Expr::UINT); }
	| LIT_FLOAT		{ $$ = p->literal(@LIT_FLOAT, $LIT_FLOAT, Semantic_Expr::FLOAT); }
	;

expr_math:
	'-' expr %prec U_MINUS		{ $$ = p->unary(@1, Uny::NEG, $expr); }
	| '~' expr %prec U_LOGICAL	{ $$ = p->unary(@1, Uny::INV, $expr); }

	| expr '+' expr				{ $$ = p->binary(@2, $1, Bin::ADD, $3); }
	| expr '-' expr				{ $$ = p->binary(@2, $1, Bin::SUB, $3); }
	| expr '*' expr				{ $$ = p->binary(@2, $1, Bin::MUL, $3); }
	| expr '/' expr				{ $$ = p->binary(@2, $1, Bin::DIV, $3); }
	| expr '%' expr				{ $$ = p->binary(@2, $1, Bin::MOD, $3); }
	| expr '&' expr				{ $$ = p->binary(@2, $1, Bin::AND, $3); }
	| expr '|' expr				{ $$ = p->binary(@2, $1, Bin::OR,  $3); }
	| expr '^' expr				{ $$ = p->binary(@2, $1, Bin::XOR, $3); }
	| expr SHL expr				{ $$ = p->binary(@2, $1, Bin::SHL, $3); }
	| expr SHR expr				{ $$ = p->binary(@2, $1, Bin::SHR, $3); }
	;

expr_logic:
	'!' expr %prec U_LOGICAL	{ $$ = p->unary(@1, Uny::NOT, $expr); }

	| expr '<' expr				{ $$ = p->binary(@2, $1, Bin::LT,    $3); }
	| expr LEQ expr				{ $$ = p->binary(@2, $1, Bin::LEQ,   $3); }
	| expr EQ expr				{ $$ = p->binary(@2, $1, Bin::EQ,    $3); }
	| expr NEQ expr				{ $$ = p->binary(@2, $1, Bin::NEQ,   $3); }
	| expr GEQ expr				{ $$ = p->binary(@2, $1, Bin::GEQ,   $3); }
	| expr '>' expr				{ $$ = p->binary(@2, $1, Bin::GT,    $3); }
	| expr L_AND expr			{ $$ = p->binary(@2, $1, Bin::L_AND, $3); }
	| expr L_OR expr			{ $$ = p->binary(@2, $1, Bin::L_OR,  $3); }
	;

proc_call:
	// Optional parameters
	IDENTIFIER					{ p->pcall_begin(); }
	'(' proc_call_params ')'	{ $$ = p->pcall(@1, $IDENTIFIER); }
	;

// param *
// Had to be split to allow left recursive grammar without the (, one_param) case
proc_call_params:
	comma_separated_exprs
	| %empty
	;

comma_separated_exprs:
	comma_separated_exprs ',' expr	{ p->pcall_add($expr); }
	| expr							{ p->pcall_add($expr); }
	;

array_access:
	IDENTIFIER '[' expr ']'			{ $$ = p->array_access(@2, $IDENTIFIER, $expr); }
	;

%%

inline static int
yyreport_syntax_error(const yypcontext_t *ctx, s22::Parser *parser)
{
	int res = 0;
	auto loc = yypcontext_location(ctx);

	if (auto lookahead = yypcontext_token(ctx); lookahead != YYSYMBOL_YYEMPTY)
	{
		auto msg = std::format("unexpected {}", yysymbol_name(lookahead));

		constexpr auto TOKEN_MAX = 5;
		yysymbol_kind_t expected[TOKEN_MAX];

		int num_expected = yypcontext_expected_tokens(ctx, expected, TOKEN_MAX);
		if (num_expected < 0)
		{
			res = num_expected;
		}
		else
		{
			if (num_expected == 0)
				num_expected = TOKEN_MAX;

			for (int i = 0; i < num_expected; i++)
			{
				if (i == 0)
				{
					msg += std::format(", expected {}", yysymbol_name(expected[i]));
				}
				else
				{
					msg += std::format(" or {}", yysymbol_name(expected[i]));
				}
			}


			s22::parser_log(Error{*loc, "{}", msg}, s22::Log_Level::ERROR);
		}
	}

	return res;
}