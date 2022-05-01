#include "compiler/Parser.h"

extern FILE *yyin;
extern int yydebug;

int
yyparse(s22::Parser *);

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
	auto parser = s22::parser_instance();
	yyparse(parser);

	fclose(yyin);
	return 0;
}