#pragma once

#include "compiler/Parser.h"
#include <unordered_map>

namespace s22
{
	inline static Parser::Context&
	ctx_push(std::stack<Parser::Context> &context)
	{
		if (context.empty())
			return context.emplace();

		auto &parent_scope = context.top().scope;
		auto &ctx = context.emplace();

		ctx.scope = parent_scope;
		scope_push(ctx.scope);

		return ctx;
	}

	inline static Parser::Context
	ctx_pop(std::stack<Parser::Context> &context)
	{
		auto ctx = std::move(context.top());
		context.pop();
		scope_pop(ctx.scope);
		return ctx;
	}

	void
	Parser::init()
	{
		this->backend = backend_instance();
		backend_init(this->backend);

		// Begin global context
		auto &ctx = ctx_push(this->context);
		ctx.scope = &this->global;
	}

	void
	Parser::block_begin()
	{
		// Begin new context
		auto &ctx = ctx_push(this->context);
	}

	void
	Parser::block_add(const Parse_Unit &unit)
	{
		auto &ctx = this->context.top();
		ctx.block_stmts.push_back(unit.ast);
	}

	Parse_Unit
	Parser::block_end()
	{
		Parse_Unit self = {};

		// End context
		// TODO: Assert returns
		auto ctx = ctx_pop(this->context);

		auto stmts = Buf<AST>::clone(ctx.block_stmts);
		self.ast = ast_block(stmts);

		if (this->context.empty())
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
		auto ctx = this->context.top();
		auto err = scope_return_is_valid(ctx.scope, SYMTYPE_VOID);
		if (err)
		{
			parser_log(err, loc);
		}
	}


	void
	Parser::return_value(Source_Location loc, const Parse_Unit &unit)
	{
		auto ctx = this->context.top();
		auto &expr = unit.smxp;
		auto err = scope_return_is_valid(ctx.scope, expr.type);

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
		self.smxp = expr;
		self.ast = ast_literal(&lit);

		return self;
	}

	Parse_Unit
	Parser::switch_stmt(const Parse_Unit &expr)
	{
		Parse_Unit self = {};
		return self;
	}

	Parse_Unit
	Parser::id(Source_Location loc, const Str &id)
	{
		auto ctx = this->context.top();

		Parse_Unit self = { .loc = loc };
		self.loc = loc;

		if (auto [expr, err] = semexpr_id(ctx.scope, id.data); err)
		{
			self.err = Error{loc, err};

			parser_log(err, loc);
		}
		else
		{
			self.smxp = expr;
		}

		if (auto sym = scope_get_id(ctx.scope, id.data))
		{
			self.ast = ast_symbol(sym);
		}

		return self;
	}

	Parse_Unit
	Parser::array(Source_Location loc, const Str &id, const Parse_Unit &right)
	{
		auto ctx = this->context.top();

		Parse_Unit self = { .loc = loc };
		if (auto [expr, err] = semexpr_array_access(ctx.scope, id.data, right); err)
		{
			self.err = Error{loc, err};

			if (right.err == false)
				parser_log(err, loc);
		}
		else
		{
			self.smxp = expr;
		}

		if (auto sym = scope_get_id(ctx.scope, id.data))
		{
			self.ast = ast_array_access(sym, right.ast);
		}

		return self;
	}

	Parse_Unit
	Parser::assign(Source_Location loc, const Str &id, Op_Assign op, const Parse_Unit &right)
	{
		auto ctx = this->context.top();

		Parse_Unit self = {};
		if (auto [expr, err] = semexpr_assign(ctx.scope, id.data, right, op); err)
		{
			if (right.err == false)
				parser_log(err, loc);
		}

		if (auto sym = scope_get_id(ctx.scope, id.data))
		{
			self.ast = ast_assign((Assignment::KIND)op, ast_symbol(sym), right.ast);
		}

		return self;
	}

	Parse_Unit
	Parser::array_assign(Source_Location loc, const Parse_Unit &left, Op_Assign op, const Parse_Unit &right)
	{
		auto ctx = this->context.top();

		Parse_Unit self = {};
		if (auto [expr, err] = semexpr_array_assign(ctx.scope, left, right, op); err)
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
		auto ctx = this->context.top();

		Parse_Unit self = { .loc = loc };
		if (auto [expr, err] = semexpr_binary(ctx.scope, left, right, op); err)
		{
			self.err = Error{loc, err};

			if (left.err == false && right.err == false)
				parser_log(err, loc);
		}
		else
		{
			self.smxp = expr;
			self.ast = ast_binary((Binary_Op::KIND)op, left.ast, right.ast);
		}

		return self;
	}

	Parse_Unit
	Parser::unary(Source_Location loc, Uny op, const Parse_Unit &right)
	{
		auto ctx = this->context.top();

		Parse_Unit self = { .loc = loc };
		if (auto [expr, err] = semexpr_unary(ctx.scope, right, op); err)
		{
			self.err = Error{loc, err};

			if (right.err == false)
				parser_log(err, loc);
		}
		else
		{
			self.smxp = expr;
			self.ast = ast_unary((Unary_Op::KIND)op, right.ast);
		}

		return self;
	}

	void
	Parser::pcall_begin()
	{
		auto &ctx = ctx_push(this->context);
	}

	void
	Parser::pcall_add(const Parse_Unit &arg)
	{
		auto &ctx = this->context.top();
		ctx.proc_call_arguments.push_back(arg);
	}

	Parse_Unit
	Parser::pcall(Source_Location loc, const Str &id)
	{
		auto ctx = ctx_pop(this->context);
		auto &list = ctx.proc_call_arguments;
		Buf<Parse_Unit> params = { .data = list.data(), .count = list.size() };

		Parse_Unit self = { .loc = loc };
		if (auto [expr, err] = semexpr_proc_call(ctx.scope, id.data, params); err)
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
		else if (auto sym = scope_get_id(ctx.scope, id.data))
		{
			self.smxp = expr;

			// Build ast arguments
			auto ast_args = Buf<AST>::make(params.count);
			for (size_t i = 0; i < params.count; i++)
				ast_args[i] = params[i].ast;

			self.ast = ast_pcall(sym, ast_args);
		}
		return self;
	}

	Parse_Unit
	Parser::decl(Source_Location loc, const Str &id, Symbol_Type type)
	{
		auto ctx = this->context.top();

		Parse_Unit self = {};
		Symbol symbol = { .id = id, .type = type, .defined_at = loc };

		auto [sym, err] = scope_add_decl(ctx.scope, symbol);
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
		auto ctx = this->context.top();

		Parse_Unit self = {};
		Symbol symbol = { .id = id, .type = type, .defined_at = loc };

		auto [sym, err] = scope_add_decl(ctx.scope, symbol, right);
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
		auto ctx = this->context.top();

		Parse_Unit self = {};
		Symbol symbol = { .id = id, .type = type, .defined_at = loc };
		symbol.type.array = right.ast.as_lit->value;

		auto [sym, err] = scope_add_decl(ctx.scope, symbol);
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
		auto ctx = this->context.top();

		Parse_Unit self = {};
		Symbol symbol = { .id = id, .type = type, .defined_at = loc, .is_constant = true };

		auto [sym, err] = scope_add_decl(ctx.scope, symbol, right);
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
		auto ctx = this->context.top();

		Symbol sym = {
			.id = id,
			.type = { .base = Symbol_Type::PROC },
			.defined_at = loc
		};

		auto [_, err] = scope_add_decl(ctx.scope, sym);
		if (err)
		{
			parser_log(err, loc);
		}
	}

	void
	Parser::decl_proc_end(const Str &id, Symbol_Type return_type)
	{
		auto ctx = this->context.top();

		auto sym = scope_get_id(ctx.scope->parent_scope, id.data);
		if (sym == nullptr)
			parser_log(Error{ "UNREACHABLE" }, Log_Level::CRITICAL);

		sym->type.procedure = scope_make_proc(ctx.scope, return_type);
		ctx.scope->procedure = return_type;
	}

	void
	Parser::switch_begin(Source_Location loc, const Parse_Unit &expr)
	{
		if (symtype_is_integral(expr.smxp.type) == false)
			return parser_log(Error{ expr.loc, "invalid type" });

		auto &ctx = ctx_push(this->context);
		ctx.switch_expr = expr.ast;
	}

	Parse_Unit
	Parser::switch_end()
	{
		auto ctx = ctx_pop(this->context);

		auto switch_cases = Buf<Switch_Case*>::make(ctx.switch_cases.size());
		for (size_t i = 0; i < switch_cases.count; i++)
			switch_cases[i] = ctx.switch_cases[i].ast_sw_case;

		Parse_Unit self = {};
		self.ast = ast_switch(switch_cases, ctx.switch_default);
		return self;
	}

	void
	Parser::switch_case_begin(Source_Location loc)
	{
		// Start building a group
		auto &ctx = this->context.top();
		ctx.switch_cases.push_back({});
	}

	void
	Parser::switch_case_add(const Parse_Unit &literal)
	{
		auto &expr = literal.smxp;
		auto &opr = literal.opr;
		auto &ctx = this->context.top();

		if (expr.is_literal == false || symtype_is_integral(expr.type) == false)
		{
			if (literal.err == false)
				parser_log(Error{ literal.loc, "invalid case" });

			return;
		}

		// Look for duplicates in all cases
		for (auto &sw_case: ctx.switch_cases)
		{
			for (auto &lit: sw_case.group)
			{
				if (*lit == *literal.ast.as_lit)
					return parser_log(Error{ literal.loc, "duplicate case" });
			}
		}

		// Add lit to last case
		auto &last_case = ctx.switch_cases.back();
		last_case.group.push_back(literal.ast.as_lit);
	}

	void
	Parser::switch_case_end(Source_Location loc, const Parse_Unit &block)
	{
		auto &ctx = this->context.top();

		// Add lit to last case
		auto &last_case = ctx.switch_cases.back();

		auto group = Buf<Literal*>::make(last_case.group.size());
		for (size_t i = 0; i < group.count; i++)
			group[i] = last_case.group[i];

		// Add block
		last_case.ast_sw_case = ast_switch_case(ctx.switch_expr, group, block.ast.as_block).as_case;
	}

	void
	Parser::switch_default(Source_Location loc, const Parse_Unit &block)
	{
		auto &ctx = this->context.top();
		if (ctx.switch_default != nullptr)
			return parser_log(Error{ loc, "duplicate default" });

		ctx.switch_default = block.ast.as_block;
	}

	Parse_Unit
	Parser::while_loop(Source_Location loc, const Parse_Unit &cond, const Parse_Unit &block)
	{
		Parse_Unit self = {};
		self.ast = ast_while(cond.ast, block.ast.as_block);
		return self;
	}

	Parse_Unit
	Parser::do_while_loop(Source_Location loc, const Parse_Unit &cond, const Parse_Unit &block)
	{
		Parse_Unit self = {};
		self.ast = ast_do_while(cond.ast, block.ast.as_block);
		return self;
	}

	void
	Parser::for_loop_begin()
	{
		this->block_begin();
	}

	Parse_Unit
	Parser::for_loop(Source_Location loc, const Parse_Unit &init, const Parse_Unit &cond, const Parse_Unit &post)
	{
		Parse_Unit self = {};
		auto block = this->block_end();
		self.ast = ast_for(init.ast, cond.ast, post.ast, block.ast.as_block);
		return self;
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
