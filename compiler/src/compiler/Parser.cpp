#pragma once

#include "compiler/Parser.h"

extern int yylineno;
extern int yyleng;
extern char *yytext;
extern FILE *yyin;

namespace s22
{
	void
	Parser::push()
	{
		symtab_push_scope(this->current_scope);
	}

	void
	Parser::pop()
	{
		symtab_pop_scope(this->current_scope);
		if (this->current_scope == &this->global)
		{
			Location loc = {};
			yyerror(&loc, nullptr, "Complete!");
		}
	}

	void
	Parser::new_stmt()
	{
		this->caught_error = false;
	}

	inline static void
	parser_handle_error(Parser *self, const Error& err, Location loc)
	{
		if (self->caught_error == false)
		{
			auto msg = std::format("ERROR: {}", err);
			yyerror(&loc, self, msg.c_str());
			self->caught_error = true;
		}
	}

	inline static void
	parser_handle_error(Parser *self, const Error& err, Location loc, Expr &dst)
	{
		dst = EXPR_ERROR;
		parser_handle_error(self, err, loc);
	}

	void
	Parser::id(const char *id, Location loc, Expr &dst)
	{
		auto [expr, err] = identifier_new(this->current_scope, id);
		if (err)
		{
			parser_handle_error(this, err, loc, dst);
		}
		else
		{
			dst = expr;
		}
	}

	void
	Parser::constant(uint64_t value, Expr &dst)
	{
		dst = constant_new(value);
	}

	void
	Parser::binary(Expr &left, Location loc, Op_Binary::KIND op, Expr &right, Expr &dst)
	{
		auto [expr, err] = op_binary_new(this->current_scope, left, op, right);
		if (err)
		{
			parser_handle_error(this, err, loc, dst);
		}
		else
		{
			dst = expr;
		}
	}

	void
	Parser::unary(Op_Unary::KIND op, Expr &right, Location loc, Expr &dst)
	{
		auto [expr, err] = op_unary_new(this->current_scope, op, right);
		if (err)
		{
			parser_handle_error(this, err, loc, dst);
		}
		else
		{
			dst = expr;
		}
	}

	void
	Parser::array(const char *id, Location loc, Expr &expr, Expr &dst)
	{
		auto sym = symtab_get_id(this->current_scope, id);

		// undefined
		if (sym == nullptr)
		{
			parser_handle_error(this, Error{"undeclared identifier"}, loc, dst);
			return;
		}

		// error index
		if (auto [type, err] = expr_evaluate(expr, this->current_scope); err)
		{
			parser_handle_error(this, err, loc, dst);
			return;
		}

		// invalid index
		else if (type != SYMTYPE_INT && type != SYMTYPE_UINT)
		{
			parser_handle_error(this, Error{"invalid index"}, loc, dst);
			return;
		}

		// not array
		if (sym->type.array == false)
		{
			parser_handle_error(this, Error{"type cannot be indexed"}, loc, dst);
			return;
		}

		sym->is_used = true;
		auto type = sym->type;

		// TODO: VERIFY THIS
		type.array = nullptr;
		dst = { .kind = Expr::CONSTANT, .type = type, };
	}

	void
	Parser::decl(const char *id, Location loc, Symbol_Type type)
	{
		Symbol sym = {
			.id = id, .type = type, .defined_at = loc
		};

		auto [_, err] = symtab_add_decl(this->current_scope, sym);
		if (err)
		{
			parser_handle_error(this, err, loc);
		}
	}

	void
	Parser::decl_expr(const char *id, Location loc, Symbol_Type type, Expr expr)
	{
		Symbol sym = { .id = id, .type = type, .defined_at = loc };

		auto [_, err] = symtab_add_decl(this->current_scope, sym, expr);
		if (err)
		{
			parser_handle_error(this, err, loc);
		}
	}

	void
	Parser::decl_array(const char *id, Location loc, Symbol_Type type, uint64_t size)
	{
		Symbol sym = { .id = id, .type = type, .defined_at = loc };
		sym.type.array = size;

		auto [_, err] = symtab_add_decl(this->current_scope, sym);
		if (err)
		{
			parser_handle_error(this, err, loc);
		}
	}

	void
	Parser::decl_const(const char *id, Location loc, Symbol_Type type, Expr expr)
	{
		Symbol sym = { .id = id, .type = type, .defined_at = loc, .is_constant = true };

		auto [_, err] = symtab_add_decl(this->current_scope, sym, expr);
		if (err)
		{
			parser_handle_error(this, err, loc);
		}
	}

	void
	Parser::decl_proc_begin(const char *id, Location loc)
	{
		Symbol sym = {
			.id = id,
			.type = { .base = Symbol_Type::PROC },
			.defined_at = loc
		};

		auto [_, err] = symtab_add_decl(this->current_scope, sym);
		if (err)
		{
			parser_handle_error(this, err, loc);
		}
	}

	void
	Parser::decl_proc_end(const char *id, Symbol_Type return_type)
	{
		auto sym = symtab_get_id(this->current_scope->parent_scope, id);
		if (sym == nullptr)
		{
			Location loc = {};
			yyerror(&loc, nullptr, "UNREACHABLE");
			return;
		}

		sym->type.procedure = symtab_make_procedure(this->current_scope, return_type);
	}
}

void
yyerror(const s22::Location *location, s22::Parser *p, const char *message)
{
	// Save offset to continue read
	auto offset_before = ftell(yyin);
	rewind(yyin);

	// Read file line by line into buffer
	char buf[1024] = {};
	size_t lines_to_read = location->first_line;
	while (lines_to_read > 0)
	{
		fgets(buf, 1024, yyin);
		buf[strlen(buf) - 1] = '\0';

		lines_to_read--;
	}
	// Continue read
	fseek(yyin, offset_before, SEEK_SET);

	// Print error
	fprintf(stderr, "(%d) %s\n", location->last_line, message);
	fprintf(stderr, "%s\n", buf);

	// Print indicator
	for (size_t i = 0; i < strlen(buf); i++)
	{
		if (i == location->first_column - 1)
			buf[i] = '^';

		else if (isspace(buf[i]) == false)
			buf[i] = ' ';
	}
	fprintf(stderr, "%s\n", buf);
}
