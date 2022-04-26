#pragma once

#include "compiler/Parser.h"

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

		this->backend = backend_instance();
		backend_init(this->backend);
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
	Parser::return_value(Source_Location loc, const Parse_Unit &unit)
	{
		auto &[expr, opr] = unit;
		auto err = scope_return_is_valid(this->current_scope, expr.type);

		if (err)
		{
			if (expr.err == false) // Limit error propagation
				parse_error(Error{ expr.loc, err });
			return;
		}
	}

	Parse_Unit
	Parser::literal(Literal lit, Source_Location loc, Symbol_Type::BASE base)
	{
		Parse_Unit self = {};
		self.expr = expr_literal(nullptr, lit, loc, base);
		return self;
	}

	Parse_Unit
	Parser::id(const Str &id, Source_Location loc)
	{
		Parse_Unit self = {};
		self.expr = expr_identifier(this->current_scope, id.data, loc);

		if (self.expr.err)
			parse_error(self.expr.err);

		return self;
	}

	Parse_Unit
	Parser::assign(const Str &id, Source_Location loc, Op_Assign op, const Parse_Unit &unit)
	{
		Parse_Unit self = {};
		self.expr = expr_assign(this->current_scope, id.data, loc, unit.expr, op);

		if (self.expr.err && unit.expr.err == false)
			parse_error(self.expr.err);

		return self;
	}

	Parse_Unit
	Parser::array_assign(const Parse_Unit &left, Source_Location loc, Op_Assign op, const Parse_Unit &right)
	{
		Parse_Unit self = {};
		self.expr = expr_array_assign(this->current_scope, left.expr, loc, right.expr, op);

		if (self.expr.err && right.expr.err == false)
			parse_error(self.expr.err);

		return self;
	}

	Parse_Unit
	Parser::binary(const Parse_Unit &left, Source_Location loc, Op_Binary op, const Parse_Unit &right)
	{
		Parse_Unit self = {};
		self.expr = expr_binary(this->current_scope, left.expr, loc, right.expr, op);

		if (self.expr.err && left.expr.err == false && right.expr.err == false)
			parse_error(self.expr.err);

		return self;
	}

	Parse_Unit
	Parser::unary(Op_Unary op, const Parse_Unit &unit, Source_Location loc)
	{
		Parse_Unit self = {};
		self.expr = expr_unary(this->current_scope, unit.expr, loc, op);

		if (self.expr.err && unit.expr.err == false)
			parse_error(self.expr.err);

		return self;
	}

	Parse_Unit
	Parser::array(const Str &id, Source_Location loc, const Parse_Unit &unit)
	{
		Parse_Unit self = {};
		self.expr = expr_array_access(this->current_scope, id.data, loc, unit.expr);

		if (self.expr.err && unit.expr.err == false)
			parse_error(self.expr.err);

		return self;
	}

	void
	Parser::pcall_begin()
	{
		this->proc_call_arguments_stack.emplace();
	}

	void
	Parser::pcall_add(const Parse_Unit &unit)
	{
		auto &list = this->proc_call_arguments_stack.top();
		list.push_back(unit.expr);
	}

	Parse_Unit
	Parser::pcall(const Str &id, Source_Location loc)
	{
		auto &list = this->proc_call_arguments_stack.top();
		auto params = Buf<Expr>{list.data(), list.size()};

		Parse_Unit self = {};
		self.expr = expr_proc_call(this->current_scope, id.data, loc, params);

		bool unique_error = true;
		for (const auto &expr : params)
			unique_error &= expr.err == false;

		if (self.expr.err && unique_error)
			parse_error(self.expr.err);

		this->proc_call_arguments_stack.pop();

		return self;
	}

	void
	Parser::decl(const Str &id, Source_Location loc, Symbol_Type type)
	{
		Symbol sym = { .id = id, .type = type, .defined_at = loc };

		auto [_, err] = scope_add_decl(this->current_scope, sym);
		if (err)
		{
			parse_error(err);
		}

		if (err == false)
			backend_decl(this->backend, &sym);
	}

	void
	Parser::decl_expr(const Str &id, Source_Location loc, Symbol_Type type, const Parse_Unit &unit)
	{
		Symbol sym = { .id = id, .type = type, .defined_at = loc };

		auto [_, err] = scope_add_decl(this->current_scope, sym, unit.expr);
		if (err && unit.expr.err == false)
		{
			parse_error(err);
		}

		if (err == false)
			backend_decl_expr(this->backend, &sym, &unit.expr);
	}

	void
	Parser::decl_array(const Str &id, Source_Location loc, Symbol_Type type, const Parse_Unit &unit)
	{
		Symbol sym = { .id = id, .type = type, .defined_at = loc };
		sym.type.array = unit.expr.lit.value;

		auto [_, err] = scope_add_decl(this->current_scope, sym);
		if (err && unit.expr.err == false)
		{
			parse_error(err);
		}

		if (err == false)
			backend_decl(this->backend, &sym);
	}

	void
	Parser::decl_const(const Str &id, Source_Location loc, Symbol_Type type, const Parse_Unit &unit)
	{
		Symbol sym = { .id = id, .type = type, .defined_at = loc, .is_constant = true };

		auto [_, err] = scope_add_decl(this->current_scope, sym, unit.expr);
		if (err && unit.expr.err == false)
		{
			parse_error(err);
		}
	}

	void
	Parser::decl_proc_begin(const Str &id, Source_Location loc)
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
	Parser::decl_proc_end(const Str &id, Symbol_Type return_type)
	{
		auto sym = scope_get_id(this->current_scope->parent_scope, id.data);
		if (sym == nullptr)
			return parse_error(Error{ "UNREACHABLE" });

		sym->type.procedure = scope_make_proc(this->current_scope, return_type);
		this->current_scope->procedure = return_type;
	}

	void
	Parser::switch_begin(const Parse_Unit &unit)
	{
		if (symtype_is_integral(unit.expr.type) == false)
			return parse_error(Error{ unit.expr.loc, "invalid type" });

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
	Parser::switch_case_add(const Parse_Unit &unit)
	{
		auto &[expr, opr] = unit;
		if (expr.kind != Expr::LITERAL || symtype_is_integral(expr.type) == false)
		{
			if (expr.err == false)
				parse_error(Error{ expr.loc, "invalid case" });

			return;
		}

		// Insert into the partition
		Switch_Case swc = { .value = expr.lit.value, .partition = this->switch_current_partition };
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

extern int yylineno;
extern int yyleng;
extern char *yytext;
extern FILE *yyin;

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
