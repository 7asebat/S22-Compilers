/* DEFINITIONS SECTION */
%{
    #include <stdlib.h>			// atoi
    #include <stdint.h>			// size_t
    #include <ctype.h>			// isspace
    #include "p1_Parser.hpp"	// Token definitions

    // Call location_update before each token is processed
    inline static int yycolno = 1; // Used to track current column in the line
    #define YY_USER_ACTION location_update(yylloc);

    inline static char source_code[(1<<13) + 2] = {};
    inline static size_t source_code_count = 0;
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

[ \t]+       ; /* skip whitespace */
{line}       ; /* skip lines */

.            yyerror(yylloc, "unexpected character"); /* Any other character */
%%

YY_BUFFER_STATE
lexer_scan_buffer(char *buf, size_t buf_size)
{
    // Reset global line and column number
	yylineno = 1;
    yycolno = 1;

    // Start new buffer
    return yy_scan_string(buf);
    // return yy_scan_buffer(buf, buf_size); // This apparently causes issues and changes the underlying buffer
}

void
lexer_delete_buffer(YY_BUFFER_STATE buf)
{
    yy_delete_buffer(buf);
}


void
location_reduce(Source_Location &current, Source_Location *rhs, size_t N)
{
	if (N != 0)
	{
		current.first_line = rhs[1].first_line;
		current.first_column = rhs[1].first_column;
		current.last_line = rhs[N].last_line;
		current.last_column = rhs[N].last_column;
	}
	else
	{
		current.first_line = current.last_line = rhs[0].last_line;
		current.first_column = current.last_column = rhs[0].last_column;
	}
}

void
location_update(Source_Location *loc)
{
	// New line
	if (yytext[yyleng - 1] == '\n')
	{
		yycolno = 1;
		return;
	}

	// Current line and column
	loc->first_line = yylineno;
	loc->first_column = yycolno;

	// Next line and column
	loc->last_line = yylineno;
	loc->last_column = loc->first_column + yyleng - 1;

	// Update current column
	yycolno = loc->last_column + 1;
}

void
location_print(FILE *out, const Source_Location *const loc)
{
	fprintf(out, "%d,%d", loc->first_line, loc->first_column);
}


void
yyerror(const Source_Location *location, const char *message)
{
	location_print(stderr, location);
	fprintf(stderr, " %s\n", message);
	
	// Read file line by line into buffer
	char buf[1024 + 1] = {};
	size_t position_in_source_code = 0;

	size_t lines_to_read = location->first_line;
	while (lines_to_read > 0)
	{
		size_t i = 0;
		for (; i + 1 < sizeof(buf) && position_in_source_code < source_code_count; i++)
		{
			char c = source_code[position_in_source_code++];
			if (c == '\n')
			{
				break;
			}
			else
			{
				buf[i] = c;
			}
		}
		
		buf[i] = '\0';
		lines_to_read--;
	}

	// Log error
	fprintf(stderr, " %s\n", buf);

	// Print indicator
	for (size_t i = 0; i < strlen(buf); i++)
	{
		if (i == location->first_column - 1)
			buf[i] = '^';

		else if (isspace(buf[i]) == false)
			buf[i] = ' ';
	}
	fprintf(stderr, " %s\n", buf);
}

constexpr auto HELP_MESSAGE = R"""(
	usage: phase1 "path/to/input"
)""";

int
main(int argc, char* argv[])
{
	if (argc != 2)
	{
		fprintf(stderr, "%s", HELP_MESSAGE);
		return 1;
	}

	if (auto f = fopen(argv[1], "r"); f == nullptr)
	{
		fprintf(stderr, "failed to open %s", argv[1]);
		return 1;
	}
	else
	{
		// Get file size
		fseek(f, 0, SEEK_END);
		source_code_count = ftell(f);
	
		rewind(f);
		fread(source_code, source_code_count, 1, f);

		fprintf(stdout, "%s\n---\n", source_code);
		fclose(f);
	}

	auto lexer_buf = lexer_scan_buffer(source_code, source_code_count+2);
	if (lexer_buf == nullptr)
	{
		fprintf(stderr, "lexer buffer is nullptr\n");
		return 1;
	}

	yyparse();
	lexer_delete_buffer(lexer_buf); 
	return 0;
}