#pragma once

#include "compiler/Parser.h"

namespace s22
{
	constexpr const char *
	errlvl_to_str(Log_Level lvl)
	{
		switch (lvl)
		{
		case Log_Level::INFO:    return "INFO";
		case Log_Level::ERROR:   return "ERROR";
		case Log_Level::WARNING: return "WARNING";
		default:                 return "UNREACHABLE";
		}
	}

	void
	parser_log(const Error &err, Log_Level lvl)
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
		{
			parser_log(Error{ "Complete!" }, Log_Level::INFO);
			backend_generate(this->backend);
		}
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
			parser_log(Error{ loc, err });
		}
	}


	void
	Parser::return_value(Source_Location loc, const Parse_Unit &unit)
	{
		auto &expr = unit.expr;
		auto err = scope_return_is_valid(this->current_scope, expr.type);

		if (err)
		{
			if (expr.err == false) // Limit error propagation
				parser_log(Error{ expr.loc, err });
			return;
		}
	}

	Parse_Unit
	Parser::condition(const Parse_Unit &unit)
	{
		// Turn last expression into a condition
		Parse_Unit self = {};
		self.expr = unit.expr;
		self.comp = backend_condition(this->backend);
		return self;
	}

	void
	Parser::if_begin(const Parse_Unit &cond, Source_Location loc)
	{
	}

	void
	Parser::else_if_begin(const Parse_Unit &cond, Source_Location loc)
	{
	}

	void
	Parser::else_begin(Source_Location loc)
	{
	}

	void
	Parser::if_end(Source_Location loc)
	{
	}

	Parse_Unit
	Parser::literal(Literal lit, Source_Location loc, Symbol_Type::BASE base)
	{
		Parse_Unit self = {};
		self.expr = expr_literal(nullptr, lit, loc, base);

		self.opr = {
			.loc = Operand::IMM,
			.size = 1,
			.value = lit.value
		};

		return self;
	}

	Parse_Unit
	Parser::id(const Str &id, Source_Location loc)
	{
		Parse_Unit self = {};
		self.expr = expr_identifier(this->current_scope, id.data, loc);

		if (self.expr.err)
			parser_log(self.expr.err);

		if (auto sym = scope_get_id(this->current_scope, id.data))
			self.opr = backend_id(this->backend, sym);

		return self;
	}

	Parse_Unit
	Parser::array(const Str &id, Source_Location loc, const Parse_Unit &right)
	{
		Parse_Unit self = {};
		self.expr = expr_array_access(this->current_scope, id.data, loc, right.expr);

		if (self.expr.err && right.expr.err == false)
			parser_log(self.expr.err);

		if (auto sym = scope_get_id(this->current_scope, id.data))
			self.opr = backend_array_access(this->backend, sym, right.opr);

		return self;
	}

	void
	Parser::assign(const Str &id, Source_Location loc, Op_Assign op, const Parse_Unit &right)
	{
		auto expr = expr_assign(this->current_scope, id.data, loc, right.expr, op);

		if (expr.err && right.expr.err == false)
			parser_log(expr.err);

		if (auto sym = scope_get_id(this->current_scope, id.data))
			backend_assign(this->backend, (INSTRUCTION_OP)op, sym, right.opr);
	}

	void
	Parser::array_assign(const Parse_Unit &left, Source_Location loc, Op_Assign op, const Parse_Unit &right)
	{
		auto expr = expr_array_assign(this->current_scope, left.expr, loc, right.expr, op);

		if (expr.err && right.expr.err == false)
			parser_log(expr.err);

		if (expr.err == false)
			backend_array_assign(this->backend, (INSTRUCTION_OP)op, left.opr, right.opr);
	}

	Parse_Unit
	Parser::binary(const Parse_Unit &left, Source_Location loc, Op_Binary op, const Parse_Unit &right)
	{
		Parse_Unit self = {};
		self.expr = expr_binary(this->current_scope, left.expr, loc, right.expr, op);

		if (self.expr.err && left.expr.err == false && right.expr.err == false)
			parser_log(self.expr.err);

		if (self.expr.err == false)
			self.opr = backend_binary(this->backend, (INSTRUCTION_OP)op, left.opr, right.opr);

		return self;
	}

	Parse_Unit
	Parser::unary(Op_Unary op, const Parse_Unit &right, Source_Location loc)
	{
		Parse_Unit self = {};
		self.expr = expr_unary(this->current_scope, right.expr, loc, op);

		if (self.expr.err && right.expr.err == false)
			parser_log(self.expr.err);

		if (self.expr.err == false)
			self.opr = backend_unary(this->backend, (INSTRUCTION_OP)op, right.opr);

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
			parser_log(self.expr.err);

		this->proc_call_arguments_stack.pop();

		return self;
	}

	void
	Parser::decl(const Str &id, Source_Location loc, Symbol_Type type)
	{
		Symbol symbol = { .id = id, .type = type, .defined_at = loc };

		auto [sym, err] = scope_add_decl(this->current_scope, symbol);
		if (err)
		{
			parser_log(err);
		}

		if (err == false)
			backend_decl(this->backend, sym);
	}

	void
	Parser::decl_expr(const Str &id, Source_Location loc, Symbol_Type type, const Parse_Unit &right)
	{
		Symbol symbol = { .id = id, .type = type, .defined_at = loc };

		auto [sym, err] = scope_add_decl(this->current_scope, symbol, right.expr);
		if (err && right.expr.err == false)
		{
			parser_log(err);
		}

		if (err == false)
			backend_decl_expr(this->backend, sym, right.opr);
	}

	void
	Parser::decl_array(const Str &id, Source_Location loc, Symbol_Type type, const Parse_Unit &right)
	{
		Symbol symbol = { .id = id, .type = type, .defined_at = loc };
		symbol.type.array = right.expr.lit.value;

		auto [sym, err] = scope_add_decl(this->current_scope, symbol);
		if (err && right.expr.err == false)
		{
			parser_log(err);
		}

		if (err == false)
			backend_decl(this->backend, sym);
	}

	void
	Parser::decl_const(const Str &id, Source_Location loc, Symbol_Type type, const Parse_Unit &right)
	{
		Symbol symbol = { .id = id, .type = type, .defined_at = loc, .is_constant = true };

		auto [sym, err] = scope_add_decl(this->current_scope, symbol, right.expr);
		if (err && right.expr.err == false)
		{
			parser_log(err);
		}

		if (err == false)
			backend_decl_expr(this->backend, sym, right.opr);
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
			parser_log(err);
		}
	}

	void
	Parser::decl_proc_end(const Str &id, Symbol_Type return_type)
	{
		auto sym = scope_get_id(this->current_scope->parent_scope, id.data);
		if (sym == nullptr)
			return parser_log(Error{ "UNREACHABLE" });

		sym->type.procedure = scope_make_proc(this->current_scope, return_type);
		this->current_scope->procedure = return_type;
	}

	void
	Parser::switch_begin(const Parse_Unit &unit, Source_Location loc)
	{
		if (symtype_is_integral(unit.expr.type) == false)
			return parser_log(Error{ unit.expr.loc, "invalid type" });

		// Start building the partitioned set
		this->switch_current_partition = 0;
	}

	void
	Parser::switch_end(Source_Location loc)
	{
		// Stop building the partitioned set
		this->switch_cases.clear();
	}

	void
	Parser::switch_case_begin(Source_Location loc)
	{
		// Start building a partition
	}

	void
	Parser::switch_case_end(Source_Location loc)
	{
		// End the partition
		this->switch_current_partition++;
	}

	void
	Parser::switch_case_add(const Parse_Unit &unit)
	{
		auto &expr = unit.expr;
		auto &opr = unit.opr;

		if (expr.kind != Expr::LITERAL || symtype_is_integral(expr.type) == false)
		{
			if (expr.err == false)
				parser_log(Error{ expr.loc, "invalid case" });

			return;
		}

		// Insert into the partition
		Switch_Case swc = { .value = expr.lit.value, .partition = this->switch_current_partition };
		if (this->switch_cases.contains(swc))
			return parser_log(Error{ expr.loc, "duplicate case" });

		this->switch_cases.insert(swc);
	}

	void
	Parser::switch_default(Source_Location loc)
	{
		if (this->switch_cases.contains(SWC_DEFAULT))
			return parser_log(Error{ loc, "duplicate default" });

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
struct std::formatter<s22::Log_Level> : std::formatter<std::string>
{
	auto
	format(s22::Log_Level lvl, format_context &ctx)
	{
		return format_to(ctx.out(), "{}", s22::errlvl_to_str(lvl));
	}
};
