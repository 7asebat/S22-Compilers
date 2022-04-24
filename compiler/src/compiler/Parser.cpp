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
		scope_push(this->current_scope);
	}

	void
	Parser::pop()
	{
		// TODO: Assert returns
		scope_pop(this->current_scope);

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
	parser_handle_error(Parser *self, const Error& err)
	{
		auto msg = std::format("ERROR: {}", err);
		yyerror(&err.loc, self, msg.c_str());

		if (self->caught_error == false)
		{
			// NOTE: This logs a single error per statement
			self->caught_error = true;
		}
	}

	inline static void
	parser_handle_error(Parser *self, const Error& err, Expr &dst)
	{
		dst.type = SYMTYPE_ERROR;
		parser_handle_error(self, err);
	}

	void
	Parser::return_value(Location loc)
	{
		auto err = scope_return_is_valid(this->current_scope, SYMTYPE_VOID);
		if (err)
		{
			parser_handle_error(this, Error{loc, err});
		}
	}

	void
	Parser::return_value(Location loc, Expr &expr)
	{
		auto err = scope_return_is_valid(this->current_scope, expr.type);
		if (err)
		{
			parser_handle_error(this, Error{loc, err});
		}
	}

	void
	Parser::id(const char *id, Location loc, Expr &dst)
	{
		auto [expr, err] = identifier_new(this->current_scope, id, loc);
		if (err)
		{
			parser_handle_error(this, err, dst);
		}
		else
		{
			dst = expr;
		}
	}

	void
	Parser::literal(uint64_t value, Location loc, Symbol_Type::BASE base, Expr &dst)
	{
		auto [expr, _] = literal_new(nullptr, value, loc, base);
		dst = expr;
	}

	void
	Parser::assign(const char *id, Location loc, Op_Assign::KIND op, Expr &right)
	{
		auto [expr, err] = op_assign_new(this->current_scope, id, loc, right, op);
		if (err)
		{
			parser_handle_error(this, err);
		}
	}

	void
	Parser::binary(Expr &left, Location loc, Op_Binary::KIND op, Expr &right, Expr &dst)
	{
		auto [expr, err] = op_binary_new(this->current_scope, left, loc, right, op);
		if (err)
		{
			parser_handle_error(this, err, dst);
		}
		else
		{
			dst = expr;
		}
	}

	void
	Parser::unary(Op_Unary::KIND op, Expr &right, Location loc, Expr &dst)
	{
		auto [expr, err] = op_unary_new(this->current_scope, right, loc, op);
		if (err)
		{
			parser_handle_error(this, err, dst);
		}
		else
		{
			dst = expr;
		}
	}

	void
	Parser::array(const char *id, Location loc, Expr &expr, Expr &dst)
	{
		auto sym = scope_get_id(this->current_scope, id);

		// undefined
		if (sym == nullptr)
		{
			parser_handle_error(this, Error{loc, "undeclared identifier"}, dst);
			return;
		}
		sym->is_used = true;

		// error index
		if (auto err = expr_evaluate(expr, this->current_scope))
		{
			parser_handle_error(this, err, dst);
			return;
		}

		// invalid index
		else if (expr.type != SYMTYPE_INT && expr.type != SYMTYPE_UINT)
		{
			parser_handle_error(this, Error{expr.loc, "invalid index"}, dst);
			return;
		}

		// not array
		if (sym->type.array == false)
		{
			parser_handle_error(this, Error{loc, "type cannot be indexed"}, dst);
			return;
		}

		auto type = sym->type;

		// TODO: VERIFY THIS
		type.array = nullptr;
		dst = { .kind = Expr::LITERAL, .type = type, .loc = loc };
	}

	void
	Parser::pcall_begin()
	{
		this->list_stack.emplace();
	}

	void
	Parser::pcall_add(Expr &expr)
	{
		auto &list = this->list_stack.top();
		list.push_back(expr);
	}

	void
	Parser::pcall(const char *id, Location loc, Expr &dst)
	{
		auto &list = this->list_stack.top();
		auto data = list.data();
		auto count = list.size();

		auto [expr, err] = proc_call_new(this->current_scope, id, loc, Buf<Expr>{data, count});
		if (err)
		{
			parser_handle_error(this, err, dst);
		}
		else
		{
			dst = expr;
		}

		this->list_stack.pop();
	}

	void
	Parser::decl(const char *id, Location loc, Symbol_Type type)
	{
		Symbol sym = { .id = id, .type = type, .defined_at = loc };

		auto [_, err] = scope_add_decl(this->current_scope, sym);
		if (err)
		{
			parser_handle_error(this, err);
		}
	}

	void
	Parser::decl_expr(const char *id, Location loc, Symbol_Type type, Expr &expr)
	{
		Symbol sym = { .id = id, .type = type, .defined_at = loc };

		auto [_, err] = scope_add_decl(this->current_scope, sym, expr);
		if (err)
		{
			parser_handle_error(this, err);
		}
	}

	void
	Parser::decl_array(const char *id, Location loc, Symbol_Type type, Expr &expr)
	{
		Symbol sym = { .id = id, .type = type, .defined_at = loc };
		sym.type.array = expr.as_literal.value;

		auto [_, err] = scope_add_decl(this->current_scope, sym);
		if (err)
		{
			parser_handle_error(this, err);
		}
	}

	void
	Parser::decl_const(const char *id, Location loc, Symbol_Type type, Expr &expr)
	{
		Symbol sym = { .id = id, .type = type, .defined_at = loc, .is_constant = true };

		auto [_, err] = scope_add_decl(this->current_scope, sym, expr);
		if (err)
		{
			parser_handle_error(this, err);
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

		auto [_, err] = scope_add_decl(this->current_scope, sym);
		if (err)
		{
			parser_handle_error(this, err);
		}
	}

	void
	Parser::decl_proc_end(const char *id, Symbol_Type return_type)
	{
		auto sym = scope_get_id(this->current_scope->parent_scope, id);
		if (sym == nullptr)
		{
			Location loc = {};
			yyerror(&loc, nullptr, "UNREACHABLE");
			return;
		}

		sym->type.procedure = scope_make_proc(this->current_scope, return_type);
		this->current_scope->procedure = return_type;
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
