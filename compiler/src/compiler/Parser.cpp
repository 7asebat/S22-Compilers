#pragma once

#include "compiler/Parser.h"

extern int yylineno;
extern int yyleng;
extern char *yytext;
extern FILE *yyin;

namespace s22
{
	constexpr const char *
	errlvl_to_str(Error_Level lvl)
	{
		switch (lvl)
		{
		case Error_Level::INFO:    return "INFO";
		case Error_Level::ERROR:   return "ERROR";
		case Error_Level::WARNING: return "WARNING";
		default:                   return "UNREACHABLE";
		}
	}

	void
	parse_error(const Error &err, Error_Level lvl)
	{
		auto msg = std::format("{}: {}", lvl, err);
		yyerror(&err.loc, nullptr, msg.c_str());
	}

	void
	Parser::init()
	{
		this->current_scope = &this->global;
	}

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

		if (this->current_scope == nullptr)
			parse_error(Error{"Complete!"}, Error_Level::INFO);
	}

	void
	Parser::new_stmt()
	{
	}

	void
	Parser::return_value(Source_Location loc)
	{
		auto err = scope_return_is_valid(this->current_scope, SYMTYPE_VOID);
		if (err)
		{
			parse_error(Error{ loc, err });
		}
	}

	void
	Parser::return_value(Source_Location loc, Expr &expr)
	{
		auto err = scope_return_is_valid(this->current_scope, expr.type);
		if (err && expr.kind != Expr::ERROR) // Limit error propagation
		{
			parse_error(Error{ expr.loc, err });
		}
	}

	void
	Parser::literal(uint64_t value, Source_Location loc, Symbol_Type::BASE base, Expr &dst)
	{
		dst = make_literal(nullptr, value, loc, base);
	}

	void
	Parser::id(const char *id, Source_Location loc, Expr &dst)
	{
		dst = make_identifier(this->current_scope, id, loc);

		if (dst.kind == Expr::ERROR)
			parse_error(dst.as_error);
	}

	void
	Parser::assign(const char *id, Source_Location loc, Op_Assign::KIND op, Expr &right)
	{
		auto dst = make_op_assign(this->current_scope, id, loc, right, op);

		if (dst.kind == Expr::ERROR && right.kind != Expr::ERROR)
			parse_error(dst.as_error);
	}

	void
	Parser::array_assign(Expr &left, Source_Location loc, Op_Assign::KIND op, Expr &right)
	{
		auto dst = make_op_array_assign(this->current_scope, left, loc, right, op);

		if (dst.kind == Expr::ERROR && right.kind != Expr::ERROR)
			parse_error(dst.as_error);
	}

	void
	Parser::binary(Expr &left, Source_Location loc, Op_Binary::KIND op, Expr &right, Expr &dst)
	{
		dst = make_op_binary(this->current_scope, left, loc, right, op);

		if (dst.kind == Expr::ERROR && left.kind != Expr::ERROR && right.kind != Expr::ERROR)
			parse_error(dst.as_error);
	}

	void
	Parser::unary(Op_Unary::KIND op, Expr &right, Source_Location loc, Expr &dst)
	{
		dst = make_op_unary(this->current_scope, right, loc, op);

		if (dst.kind == Expr::ERROR && right.kind != Expr::ERROR)
			parse_error(dst.as_error);
	}

	void
	Parser::array(const char *id, Source_Location loc, Expr &expr, Expr &dst)
	{
		dst = make_array_access(this->current_scope, id, loc, expr);

		if (dst.kind == Expr::ERROR && expr.kind != Expr::ERROR)
			parse_error(dst.as_error);
	}

	void
	Parser::pcall_begin()
	{
		this->proc_call_arguments_stack.emplace();
	}

	void
	Parser::pcall_add(Expr &expr)
	{
		auto &list = this->proc_call_arguments_stack.top();
		list.push_back(expr);
	}

	void
	Parser::pcall(const char *id, Source_Location loc, Expr &dst)
	{
		auto &list = this->proc_call_arguments_stack.top();
		auto params = Buf<Expr>{list.data(), list.size()};

		dst = make_proc_call(this->current_scope, id, loc, params);

		bool unique_error = true;
		for (const auto &expr : params)
			unique_error &= expr.kind != Expr::ERROR;

		if (dst.kind == Expr::ERROR && unique_error)
			parse_error(dst.as_error);

		this->proc_call_arguments_stack.pop();
	}

	void
	Parser::decl(const char *id, Source_Location loc, Symbol_Type type)
	{
		Symbol sym = { .id = id, .type = type, .defined_at = loc };

		auto [_, err] = scope_add_decl(this->current_scope, sym);
		if (err)
		{
			parse_error(err);
		}
	}

	void
	Parser::decl_expr(const char *id, Source_Location loc, Symbol_Type type, Expr &expr)
	{
		Symbol sym = { .id = id, .type = type, .defined_at = loc };

		auto [_, err] = scope_add_decl(this->current_scope, sym, expr);
		if (err && expr.kind != Expr::ERROR)
		{
			parse_error(err);
		}
	}

	void
	Parser::decl_array(const char *id, Source_Location loc, Symbol_Type type, Expr &index)
	{
		Symbol sym = { .id = id, .type = type, .defined_at = loc };
		sym.type.array = index.as_literal.value;

		auto [_, err] = scope_add_decl(this->current_scope, sym);
		if (err && index.kind != Expr::ERROR)
		{
			parse_error(err);
		}
	}

	void
	Parser::decl_const(const char *id, Source_Location loc, Symbol_Type type, Expr &expr)
	{
		Symbol sym = { .id = id, .type = type, .defined_at = loc, .is_constant = true };

		auto [_, err] = scope_add_decl(this->current_scope, sym, expr);
		if (err && expr.kind != Expr::ERROR)
		{
			parse_error(err);
		}
	}

	void
	Parser::decl_proc_begin(const char *id, Source_Location loc)
	{
		Symbol sym = {
			.id = id,
			.type = { .base = Symbol_Type::PROC },
			.defined_at = loc
		};

		auto [_, err] = scope_add_decl(this->current_scope, sym);
		if (err)
		{
			parse_error(err);
		}
	}

	void
	Parser::decl_proc_end(const char *id, Symbol_Type return_type)
	{
		auto sym = scope_get_id(this->current_scope->parent_scope, id);
		if (sym == nullptr)
			return parse_error(Error{ "UNREACHABLE" });

		sym->type.procedure = scope_make_proc(this->current_scope, return_type);
		this->current_scope->procedure = return_type;
	}

	void
	Parser::switch_begin(const Expr &expr)
	{
		if (symtype_is_valid_index(expr.type) == false)
			return parse_error(Error{ expr.loc, "invalid type" });

		// Start building the partitioned set
		this->switch_current_partition = 0;
	}

	void
	Parser::switch_end()
	{
		// Stop building the partitioned set
		this->switch_cases.clear();
	}

	void
	Parser::switch_case_begin()
	{
		// Start building a partition
	}

	void
	Parser::switch_case_end()
	{
		// End the partition
		this->switch_current_partition++;
	}

	void
	Parser::switch_case_add(Expr &expr)
	{
		if (expr.kind != Expr::LITERAL || symtype_is_valid_index(expr.type) == false)
		{
			if (expr.kind != Expr::ERROR)
				parse_error(Error{ expr.loc, "invalid case" });

			return;
		}

		// Insert into the partition
		Switch_Case swc = { .value = expr.as_literal.value, .partition = this->switch_current_partition };
		if (this->switch_cases.contains(swc))
			return parse_error(Error{ expr.loc, "duplicate case" });

		this->switch_cases.insert(swc);
	}

	void
	Parser::switch_default(Source_Location loc)
	{
		if (this->switch_cases.contains(SWC_DEFAULT))
			return parse_error(Error{ loc, "duplicate default" });

		this->switch_cases.insert(SWC_DEFAULT);
	}
}

void
yyerror(const s22::Source_Location *location, s22::Parser *p, const char *message)
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

template <>
struct std::formatter<s22::Error_Level> : std::formatter<std::string>
{
	auto
	format(s22::Error_Level lvl, format_context &ctx)
	{
		return format_to(ctx.out(), "{}", s22::errlvl_to_str(lvl));
	}
};
