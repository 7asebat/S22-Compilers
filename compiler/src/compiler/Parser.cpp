#pragma once

#include "compiler/Parser.h"

namespace s22
{
	void
	Parser::init()
	{
		this->current_scope = &this->global;

		this->backend = backend_instance();
		backend_init(this->backend);

		// Start a new list of statements
		this->block_stmts_stack.emplace();
	}

	void
	Parser::block_begin()
	{
		scope_push(this->current_scope);
		this->block_stmts_stack.emplace();
	}

	void
	Parser::block_add(const Parse_Unit &unit)
	{
		auto &l = this->block_stmts_stack.top();
		l.push_back(unit.ast);
	}

	Parse_Unit
	Parser::block_end()
	{
		Parse_Unit self = {};

		// Stop current list of statements
		{
			auto l = std::move(this->block_stmts_stack.top());
			this->block_stmts_stack.pop();

			auto stmts = Buf<AST>::clone(l);
			self.ast = ast_block(stmts);
		}

		// TODO: Assert returns
		scope_pop(this->current_scope);

		if (this->current_scope == nullptr)
		{
			parser_log(Error{ "Complete!" }, Log_Level::INFO);
			backend_generate(this->backend, self.ast);
			backend_write(this->backend);
		}

		return self;
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
			parser_log(err, loc);
		}
	}


	void
	Parser::return_value(Source_Location loc, const Parse_Unit &unit)
	{
		auto &expr = unit.expr;
		auto err = scope_return_is_valid(this->current_scope, expr.type);

		if (err && unit.err == false) // Limit error propagation
		{
			parser_log(err, unit.loc);
		}
	}

	Parse_Unit
	Parser::if_cond(const Parse_Unit &cond, const Parse_Unit &block, const Parse_Unit &next)
	{
		Parse_Unit self = {};

		If_Condition *next_if = {};
		if (next.ast.kind != AST::NIL)
		{
			// Find the earliest point to join
			for (auto n = next.ast.as_if; n != nullptr; n = n->prev)
				next_if = n;
		}

		self.ast = ast_if({}, cond.ast, block.ast.as_block, next_if);
		return self;
	}

	Parse_Unit
	Parser::else_if_cond(Parse_Unit &prev, const Parse_Unit &cond, const Parse_Unit &block)
	{
		auto else_if = ast_if(prev.ast.as_if, cond.ast, block.ast.as_block, {});
		prev.ast.as_if->next = else_if.as_if;

		Parse_Unit self = {};
		self.ast = else_if;
		return self;
	}

	Parse_Unit
	Parser::literal(Source_Location loc, Literal lit, Symbol_Type::BASE base)
	{
		Parse_Unit self = { .loc = loc };

		auto [expr, _] = semexpr_literal(nullptr, lit, base);
		self.expr = expr;
		self.ast = ast_literal(lit);

		return self;
	}

	Parse_Unit
	Parser::id(Source_Location loc, const Str &id)
	{
		Parse_Unit self = { .loc = loc };
		self.loc = loc;

		if (auto [expr, err] = semexpr_id(this->current_scope, id.data); err)
		{
			self.err = Error{loc, err};

			parser_log(err, loc);
		}
		else
		{
			self.expr = expr;
		}

		if (auto sym = scope_get_id(this->current_scope, id.data))
		{
			self.ast = ast_symbol(sym);
		}

		return self;
	}

	Parse_Unit
	Parser::array(Source_Location loc, const Str &id, const Parse_Unit &right)
	{
		Parse_Unit self = { .loc = loc };
		if (auto [expr, err] = semexpr_array_access(this->current_scope, id.data, right); err)
		{
			self.err = Error{loc, err};

			if (right.err == false)
				parser_log(err, loc);
		}
		else
		{
			self.expr = expr;
		}

		if (auto sym = scope_get_id(this->current_scope, id.data))
		{
			self.ast = ast_array_access(sym, right.ast);
		}

		return self;
	}

	Parse_Unit
	Parser::assign(Source_Location loc, const Str &id, Op_Assign op, const Parse_Unit &right)
	{
		Parse_Unit self = {};
		if (auto [expr, err] = semexpr_assign(this->current_scope, id.data, right, op); err)
		{
			if (right.err == false)
				parser_log(err, loc);
		}

		if (auto sym = scope_get_id(this->current_scope, id.data))
		{
			self.ast = ast_assign((Assignment::KIND)op, ast_symbol(sym), right.ast);
		}

		return self;
	}

	Parse_Unit
	Parser::array_assign(Source_Location loc, const Parse_Unit &left, Op_Assign op, const Parse_Unit &right)
	{
		Parse_Unit self = {};
		if (auto [expr, err] = semexpr_array_assign(this->current_scope, left, right, op); err)
		{
			if (right.err == false)
				parser_log(err, loc);
		}

		else
		{
			self.ast = ast_assign((Assignment::KIND)op, left.ast, right.ast);
		}

		return self;
	}

	Parse_Unit
	Parser::binary(Source_Location loc, const Parse_Unit &left, Bin op, const Parse_Unit &right)
	{
		Parse_Unit self = { .loc = loc };
		if (auto [expr, err] = semexpr_binary(this->current_scope, left, right, op); err)
		{
			self.err = Error{loc, err};

			if (left.err == false && right.err == false)
				parser_log(err, loc);
		}
		else
		{
			self.expr = expr;
			self.ast = ast_binary((Binary_Op::KIND)op, left.ast, right.ast);
		}

		return self;
	}

	Parse_Unit
	Parser::unary(Source_Location loc, Uny op, const Parse_Unit &right)
	{
		Parse_Unit self = { .loc = loc };
		if (auto [expr, err] = semexpr_unary(this->current_scope, right, op); err)
		{
			self.err = Error{loc, err};

			if (right.err == false)
				parser_log(err, loc);
		}
		else
		{
			self.expr = expr;
			self.ast = ast_unary((Unary_Op::KIND)op, right.ast);
		}

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
		list.push_back(unit);
	}

	Parse_Unit
	Parser::pcall(Source_Location loc, const Str &id)
	{
		auto &list = this->proc_call_arguments_stack.top();
		Buf<Parse_Unit> params = { .data = list.data(), .count = list.size() };

		Parse_Unit self = { .loc = loc };
		if (auto [expr, err] = semexpr_proc_call(this->current_scope, id.data, params); err)
		{
			self.err = Error{loc, err};

			bool unique_error = true;
			for (const auto &unit : params)
			{
				if (unit.err)
				{
					unique_error = false;
					break;
				}
			}

			if (unique_error)
				parser_log(err, loc);
		}
		else if (auto sym = scope_get_id(this->current_scope, id.data))
		{
			self.expr = expr;

			// Build ast arguments
			auto ast_args = Buf<AST>::make(params.count);
			for (size_t i = 0; i < params.count; i++)
				ast_args[i] = params[i].ast;

			self.ast = ast_pcall(sym, ast_args);
		}

		this->proc_call_arguments_stack.pop();

		return self;
	}

	Parse_Unit
	Parser::decl(Source_Location loc, const Str &id, Symbol_Type type)
	{
		Parse_Unit self = {};
		Symbol symbol = { .id = id, .type = type, .defined_at = loc };

		auto [sym, err] = scope_add_decl(this->current_scope, symbol);
		if (err)
		{
			parser_log(err, loc);
		}

		if (err == false)
		{
			self.ast = ast_decl(sym, AST{});
		}
		return self;
	}

	Parse_Unit
	Parser::decl_expr(Source_Location loc, const Str &id, Symbol_Type type, const Parse_Unit &right)
	{
		Parse_Unit self = {};
		Symbol symbol = { .id = id, .type = type, .defined_at = loc };

		auto [sym, err] = scope_add_decl(this->current_scope, symbol, right);
		if (err && right.err == false)
		{
			parser_log(err, loc);
		}

		if (err == false)
		{
			self.ast = ast_decl(sym, right.ast);
		}

		return self;
	}

	Parse_Unit
	Parser::decl_array(Source_Location loc, const Str &id, Symbol_Type type, const Parse_Unit &right)
	{
		Parse_Unit self = {};
		Symbol symbol = { .id = id, .type = type, .defined_at = loc };
		symbol.type.array = right.expr.lit.value;

		auto [sym, err] = scope_add_decl(this->current_scope, symbol);
		if (err && right.err == false)
		{
			parser_log(err, loc);
		}

		if (err == false)
		{
			self.ast = ast_decl(sym, AST{});
		}
		return self;
	}

	Parse_Unit
	Parser::decl_const(Source_Location loc, const Str &id, Symbol_Type type, const Parse_Unit &right)
	{
		Parse_Unit self = {};
		Symbol symbol = { .id = id, .type = type, .defined_at = loc, .is_constant = true };

		auto [sym, err] = scope_add_decl(this->current_scope, symbol, right);
		if (err && right.err == false)
		{
			parser_log(err, loc);
		}

		if (err == false)
		{
			self.ast = ast_decl(sym, right.ast);
		}

		return self;
	}

	void
	Parser::decl_proc_begin(Source_Location loc, const Str &id)
	{
		Symbol sym = {
			.id = id,
			.type = { .base = Symbol_Type::PROC },
			.defined_at = loc
		};

		auto [_, err] = scope_add_decl(this->current_scope, sym);
		if (err)
		{
			parser_log(err, loc);
		}
	}

	void
	Parser::decl_proc_end(const Str &id, Symbol_Type return_type)
	{
		auto sym = scope_get_id(this->current_scope->parent_scope, id.data);
		if (sym == nullptr)
			parser_log(Error{ "UNREACHABLE" }, Log_Level::CRITICAL);

		sym->type.procedure = scope_make_proc(this->current_scope, return_type);
		this->current_scope->procedure = return_type;
	}

	void
	Parser::switch_begin(Source_Location loc, const Parse_Unit &unit)
	{
		if (symtype_is_integral(unit.expr.type) == false)
			return parser_log(Error{ unit.loc, "invalid type" });

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

		if (expr.kind != Semantic_Expr::LITERAL || symtype_is_integral(expr.type) == false)
		{
			if (unit.err == false)
				parser_log(Error{ unit.loc, "invalid case" });

			return;
		}

		// Insert into the partition
		Switch_Case swc = { .value = expr.lit.value, .partition = this->switch_current_partition };
		if (this->switch_cases.contains(swc))
			return parser_log(Error{ unit.loc, "duplicate case" });

		this->switch_cases.insert(swc);
	}

	void
	Parser::switch_default(Source_Location loc)
	{
		if (this->switch_cases.contains(SWC_DEFAULT))
			return parser_log(Error{ loc, "duplicate default" });

		this->switch_cases.insert(SWC_DEFAULT);
	}

	void
	Parser::while_begin(Source_Location loc, const Parse_Unit &cond)
	{
		// TODO: Implement
	}

	void
	Parser::while_end(Source_Location loc)
	{
		// TODO: Implement
	}

	void
	Parser::do_while_begin(Source_Location loc)
	{
		// TODO: Implement
	}

	void
	Parser::do_while_end(Source_Location loc, const Parse_Unit &cond)
	{
		// TODO: Implement
	}

	void
	parser_log(const Error &err, Log_Level lvl)
	{
		parser_log(err, err.loc, lvl);
	}

	void
	parser_log(const Error &err, Source_Location loc, Log_Level lvl)
	{
		// Fill with loc if error has none
		auto msg = std::format("{}: {}", lvl, err);

		if (err.loc != Source_Location{})
			loc = err.loc;

		yyerror(&loc, nullptr, msg.c_str());

		if (lvl == Log_Level::CRITICAL)
			exit(-1);
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
		const char *str = "UNREACHABLE";
		switch (lvl)
		{
		case s22::Log_Level::INFO:    str = "INFO"; break;
		case s22::Log_Level::ERROR:   str = "ERROR"; break;
		case s22::Log_Level::WARNING: str = "WARNING"; break;

		default:
			break;
		}

		return format_to(ctx.out(), "{}", str);
	}
};
