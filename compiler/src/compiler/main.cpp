#include "compiler/Util.h"

int yylineno;
int yyleng;
char *yytext;
FILE *yyin;

int
main(int argc, char *argv[])
{
	using namespace s22;

	auto buf = Buf<int>::make(4);
	printf("%s\n", std::format("{}", buf).c_str());

	#if 0
	Symbol_Table table = {};
	Symbol_Table *self = &table;
	auto parent = symtab_push_scope(self);
	{
		symtab_add_decl(
			self,
			Symbol{ .id = "me", .type = { s22::Symbol_Type::INT }, .is_constant = true },
			Expression{ .type = { s22::Symbol_Type::INT }}
		);

		symtab_add_decl(
			parent,
			Symbol{ .id = "you", .type = { s22::Symbol_Type::INT }}
		);

		auto sym = symtab_has_id(self, "you");

		symtab_add_decl(
			self,
			Symbol{ .id = "me2", .type = { s22::Symbol_Type::FLOAT }, .is_constant = true }
		);

		auto proc = symtab_make_procedure(self, Symbol_Type{ s22::Symbol_Type::UINT });
	}

	symtab_pop_scope(self);
	#endif


	return 0;
}