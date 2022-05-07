%code top {
	#include <stdio.h>  // fprintf
	#include <format>	// std::format
}

%require "3.8" // Require Bison v3.8+

// Required for token type definition
%code requires {
	#include <stdint.h>
	struct Literal
	{
		union
		{
			uint64_t value;

			// Value interpretations
			uint64_t u64;
			int64_t s64;
			double f64;
			bool b;
		};

		inline bool
		operator==(const Literal &other) { return this->value == other.value; }
	};

	struct String
	{
		constexpr static auto CAP = 256;

		char data[CAP + 1];
		size_t count;

		inline String() = default;
		inline String(const char *str)
		{
			*this = {};
			if (str != nullptr)
			{
				strcpy_s(this->data, str);
				this->count = strlen(str);
			}
		}

		inline bool operator==(const char *other) const		{ return strcmp(data, other) == 0; }
		inline bool operator!=(const char *other) const		{ return !operator==(other); }
		inline bool operator==(const String &other) const	{ return operator==(other.data); }
		inline bool operator!=(const String &other) const	{ return !operator==(other); }
	};

	struct Source_Location
	{
		int first_line, first_column;
		int last_line, last_column;

		inline bool
		operator==(const Source_Location &other) const
		{
			return first_line   == other.first_line &&
					first_column == other.first_column &&
					last_line    == other.last_line &&
					last_column  == other.last_column;
		}

		inline bool
		operator !=(const Source_Location &other) const { return !operator==(other); }
	};

	// Reduces locations of the rhs of a production to the current
	void
	location_reduce(Source_Location &current, Source_Location *rhs, size_t N);

	// Used by the lexer, sets the given location to the current token
	void
	location_update(Source_Location *loc);

	void
	location_print(FILE *out, const Source_Location *loc);
}

%union {
	String id;
	Literal value;
	void* type;
	void* unit;
}

%locations									// generate token location code
%define api.pure full						// modify yylex to receive parameters as shown in %code provides
%define api.location.type {Source_Location}	// source location type, aliased by YYLTYPE
%define parse.trace							// allow debugging
%define parse.error custom					// custom error handling function

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

	typedef struct yy_buffer_state *YY_BUFFER_STATE;
	
	// Scan a null-terminated in-memory buffer
	YY_BUFFER_STATE
	lexer_scan_buffer(char *buf, size_t buf_size);

	// Cleanup allocated buffer
	void
	lexer_delete_buffer(YY_BUFFER_STATE buf);
	
	void
	yyerror(const Source_Location*, const char*);
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
	stmt_list;

// stmt *
stmt_list:
	stmt_list stmt	
	| %empty
	;

block:
	'{'				
		stmt_list
	'}'				
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
	| RETURN expr ';'	
	| RETURN ';'		
	;

conditional:
	IF expr
		block
	c_else				

	|
	SWITCH expr			
	'{' switch_list '}'	
	;

c_else:
	%empty				
	|
	c_else_ifs
	|
	c_else_ifs
	ELSE block			
	|
	ELSE block			
	;

c_else_ifs:
	c_else_ifs
	ELSE IF expr block	
	|
	ELSE IF expr block	
	;

// case *
switch_list:
	switch_list switch_case
	| %empty
	;

switch_case:
	CASE							
	switch_literal_list block		
	|
	DEFAULT block					
	;


switch_literal_list:
	switch_literal_list ',' literal	
	| literal						
	;

loop:
	WHILE expr
		block						
	|

	DO
		block
	WHILE expr ';'					
	|

	FOR														
	loop_for_init ';' loop_for_condition ';' loop_for_post
		block_no_action										
	;

loop_for_init:
	decl_var
	| %empty 
	;

loop_for_condition:
	expr
	| %empty 
	;

loop_for_post:
	assignment | expr
	| %empty 
	;

decl_var:
	IDENTIFIER ':' type							
	| IDENTIFIER ':' type '=' expr				
	;

decl_const:
	CONST IDENTIFIER ':' type '=' expr			
	;

decl_proc:
	IDENTIFIER DBL_COLON PROC							
	'(' decl_proc_params ')' decl_proc_return	
		block_no_action							
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
	type_base
	| type_array
	;

type_base:
	INT			
	| UINT		
	| FLOAT		
	| BOOL		
	;

type_array:
	'[' literal ']' type_base		

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

	| array_access '=' expr		
	| array_access AS_ADD expr	
	| array_access AS_SUB expr	
	| array_access AS_MUL expr	
	| array_access AS_DIV expr	
	| array_access AS_MOD expr	
	| array_access AS_AND expr	
	| array_access AS_OR expr	
	| array_access AS_XOR expr	
	| array_access AS_SHL expr	
	| array_access AS_SHR expr	
	;

expr:
	'(' expr ')'	
	| expr_math
	| expr_logic
	| proc_call
	| array_access
	| IDENTIFIER	
	| literal
	| TRUE			
	| FALSE			
	;

literal:
	LIT_INT			
	| LIT_UINT		
	| LIT_FLOAT		
	;

expr_math:
	'-' expr %prec U_MINUS		
	| '~' expr %prec U_LOGICAL	

	| expr '+' expr				
	| expr '-' expr				
	| expr '*' expr				
	| expr '/' expr				
	| expr '%' expr				
	| expr '&' expr				
	| expr '|' expr				
	| expr '^' expr				
	| expr SHL expr				
	| expr SHR expr				
	;

expr_logic:
	'!' expr %prec U_LOGICAL	

	| expr '<' expr				
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
	IDENTIFIER					
	'(' proc_call_params ')'	
	;

// param *
// Had to be split to allow left recursive grammar without the (, one_param) case
proc_call_params:
	comma_separated_exprs
	| %empty
	;

comma_separated_exprs:
	comma_separated_exprs ',' expr	
	| expr							
	;

array_access:
	IDENTIFIER '[' expr ']'			
	;

%%
inline static int
yyreport_syntax_error(const yypcontext_t *ctx)
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

			yyerror(loc, msg.c_str());
		}
	}

	return res;
}