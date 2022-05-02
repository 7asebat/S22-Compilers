#pragma once

#include "compiler/Parser.h"

// Bison variables
extern int yylineno;
extern int yycolno;
extern int yyleng;
extern char *yytext;

namespace s22
{
	inline static Parser::Context&
	ctx_push(std::stack<Parser::Context> &ctx)
	{
		if (ctx.empty())
			return ctx.emplace();

		auto &parent = ctx.top();
		auto &inner_ctx = ctx.emplace();

		inner_ctx.scope = parent.scope;
		scope_push(inner_ctx.scope);

		// One stack offset for any nested scope
		inner_ctx.stack_offset = parent.stack_offset;

		return inner_ctx;
	}

	inline static Parser::Context
	ctx_pop(std::stack<Parser::Context> &ctx)
	{
		auto inner_ctx = std::move(ctx.top());
		ctx.pop();
		scope_pop(inner_ctx.scope);

		// One stack offset for any nested scope
		if (ctx.empty() == false)
		{
			ctx.top().stack_offset = inner_ctx.stack_offset;
			inner_ctx.stack_offset = 0;
		}

		return inner_ctx;
	}

	void
	Parser::program_begin()
	{
		// Start instance
		this->backend = backend_instance();

		// Begin global context
		auto &ctx = ctx_push(this->context);
		ctx.scope = &this->global;
	}

	void
	Parser::program_end()
	{
		// End context
		auto ctx = ctx_pop(this->context);

		// Program blocks
		auto ast = ast_block(Buf<AST>::view(ctx.block_stmts));
		ast.as_block->used_stack_size = ctx.stack_offset;

		// Set using parser_log
		if (this->has_errors == false)
		{
			parser_log(Error{ "Complete!" }, Log_Level::INFO);
			backend_compile(this->backend, ast);
		}
		else
		{
			parser_log(Error{ "Complete with errors!" }, Log_Level::INFO);
		}
	}

	Program
	Parser::program_write()
	{
		if (this->has_errors)
			return {};

		return backend_write(this->backend);
	}

	void
	Parser::dispose()
	{
		this->has_errors = false;
		backend_dispose(this->backend);
		this->global = {};
		this->context = {};

		auto mem = Memory_Log::instance();
		mem->free_all();
	}

	void
	Parser::block_begin()
	{
		// Begin new context
		auto &ctx = ctx_push(this->context);
	}

	void
	Parser::block_add(const Parse_Unit &stmt)
	{
		auto &ctx = this->context.top();
		ctx.block_stmts.push_back(stmt.ast);
	}

	Parse_Unit
	Parser::block_end()
	{
		Parse_Unit self = {};

		// End context
		auto ctx = ctx_pop(this->context);

		// Build block from statements found in the context
		auto stmts = Buf<AST>::clone(ctx.block_stmts);
		self.ast = ast_block(stmts);
		self.ast.as_block->used_stack_size = ctx.stack_offset;

		return self;
	}

	Parse_Unit
	Parser::return_value(Source_Location loc)
	{
		Parse_Unit self = {};

		auto &ctx = this->context.top();
		if (auto [proc_sym, err] = scope_return_matches_proc_sym(ctx.scope, SYMTYPE_VOID); err)
		{
			parser_log(err, loc);
		}
		else
		{
			self.ast = ast_return(AST{}, proc_sym);
		}

		return self;
	}

	Parse_Unit
	Parser::return_value(Source_Location loc, const Parse_Unit &expr)
	{
		Parse_Unit self = {};

		auto &ctx = this->context.top();
		if (auto [proc_sym, err] = scope_return_matches_proc_sym(ctx.scope, expr.semexpr.type); err)
		{
			if (expr.err == false)  // Limit error propagation
				parser_log(err, expr.loc);
		}
		else
		{
			self.ast = ast_return(expr.ast, proc_sym);
		}

		return self;
	}

	Parse_Unit
	Parser::if_cond(const Parse_Unit &cond, const Parse_Unit &block, const Parse_Unit &next)
	{
		Parse_Unit self = {};

		If_Condition *next_if = nullptr;
		if (next.ast.kind != AST::NIL)
		{
			// Find the first if condition to join to
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
		self.semexpr = expr;
		self.ast = ast_literal(&lit);

		return self;
	}

	Parse_Unit
	Parser::id(Source_Location loc, const Str &id)
	{
		Parse_Unit self = { .loc = loc };
		self.loc = loc;

		auto &ctx = this->context.top();
		if (auto [expr, err] = semexpr_id(ctx.scope, id.data); err)	// Verify semantics
		{
			self.err = Error{loc, err};

			parser_log(err, loc);
		}
		else if (auto sym = scope_get_sym(ctx.scope, id.data))		// Build AST
		{
			// exclude arrays from initialization check
			if (sym->is_set == false && sym->type.array == 0)
				parser_log(Error{ loc, "uninitialized identifier" }, Log_Level::WARNING);

			self.semexpr = expr;
			self.ast = ast_symbol(sym);
		}

		return self;
	}

	Parse_Unit
	Parser::array(Source_Location loc, const Str &id, const Parse_Unit &right)
	{
		Parse_Unit self = {.loc = loc};
		
		auto &ctx = this->context.top();
		if (auto [expr, err] = semexpr_array_access(ctx.scope, id.data, right); err)
		{
			self.err = Error{loc, err};

			if (right.err == false)
				parser_log(err, loc);
		}
		else if (auto sym = scope_get_sym(ctx.scope, id.data))
		{
			self.semexpr = expr;
			self.ast = ast_array_access(sym, right.ast);
		}

		return self;
	}

	Parse_Unit
	Parser::assign(Source_Location loc, const Str &id, Asn op, const Parse_Unit &right)
	{
		Parse_Unit self = {.loc = loc};
		
		auto &ctx = this->context.top();
		if (auto [expr, err] = semexpr_assign(ctx.scope, id.data, right, op); err)
		{
			self.err = Error{loc, err};

			if (right.err == false)
				parser_log(err, loc);
		}
		else if (auto sym = scope_get_sym(ctx.scope, id.data))
		{
			self.ast = ast_assign((Assignment::KIND)op, ast_symbol(sym), right.ast);
		}

		return self;
	}

	Parse_Unit
	Parser::array_assign(Source_Location loc, const Parse_Unit &left, Asn op, const Parse_Unit &right)
	{
		Parse_Unit self = {.loc = loc};
		
		auto &ctx = this->context.top();
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
		Parse_Unit self = {.loc = loc};

		auto &ctx = this->context.top();
		if (auto [expr, err] = semexpr_binary(ctx.scope, left, right, op); err)
		{
			self.err = Error{loc, err};

			if (left.err == false && right.err == false)
				parser_log(err, loc);
		}
		else
		{
			self.semexpr = expr;
			self.ast = ast_binary((Binary_Op::KIND)op, left.ast, right.ast);
		}

		return self;
	}

	Parse_Unit
	Parser::unary(Source_Location loc, Uny op, const Parse_Unit &right)
	{
		Parse_Unit self = {.loc = loc};

		auto &ctx = this->context.top();
		if (auto [expr, err] = semexpr_unary(ctx.scope, right, op); err)
		{
			self.err = Error{loc, err};

			if (right.err == false)
				parser_log(err, loc);
		}
		else
		{
			self.semexpr = expr;
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
		Parse_Unit self = {.loc = loc};
		
		auto ctx = ctx_pop(this->context);
		auto params = Buf<Parse_Unit>::view(ctx.proc_call_arguments);

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
		else if (auto sym = scope_get_sym(ctx.scope, id.data))
		{
			self.semexpr = expr;

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
		Parse_Unit self = { .loc = loc };
		Symbol symbol = { .id = id, .type = type, .defined_at = loc };
		
		auto &ctx = this->context.top();
		if (auto [sym, err] = scope_add_decl(ctx.scope, symbol); err)
		{
			parser_log(err, loc);
		}
		else
		{
			ctx.stack_offset += 1;
			self.ast = ast_decl(sym, AST{});
		}
		return self;
	}

	Parse_Unit
	Parser::decl_expr(Source_Location loc, const Str &id, Symbol_Type type, const Parse_Unit &right)
	{
		Parse_Unit self = { .loc = loc };
		Symbol symbol = { .id = id, .type = type, .defined_at = loc };

		auto &ctx = this->context.top();
		if (auto [sym, err] = scope_add_decl(ctx.scope, symbol, right); err)
		{
			self.err = Error{loc, err};

			if (right.err == false)
				parser_log(err, loc);
		}
		else
		{
			ctx.stack_offset += 1;
			self.ast = ast_decl(sym, right.ast);
		}
		
		return self;
	}

	Parse_Unit
	Parser::decl_array(Source_Location loc, const Str &id, Symbol_Type type, const Parse_Unit &right)
	{
		Parse_Unit self = { .loc = loc };
		
		Symbol symbol = { .id = id, .type = type, .defined_at = loc };
		symbol.type.array = right.ast.as_lit->value;

		auto &ctx = this->context.top();
		if (auto [sym, err] = scope_add_decl(ctx.scope, symbol); err)
		{
			self.err = Error{loc, err};

			if (right.err == false)
				parser_log(err, loc);
		}
		else
		{
			ctx.stack_offset += symbol.type.array;
			self.ast = ast_decl(sym, AST{});
		}
		return self;
	}

	Parse_Unit
	Parser::decl_const(Source_Location loc, const Str &id, Symbol_Type type, const Parse_Unit &right)
	{
		Parse_Unit self = {.loc = loc};
		Symbol symbol = {.id = id, .type = type, .defined_at = loc, .is_constant = true};

		auto &ctx = this->context.top();
		if (auto [sym, err] = scope_add_decl(ctx.scope, symbol, right); err)
		{
			self.err = Error{loc, err};

			if (right.err == false)
				parser_log(err, loc);
		}
		else
		{
			ctx.stack_offset += 1;
			self.ast = ast_decl(sym, right.ast);
		}

		return self;
	}

	void
	Parser::decl_proc_begin()
	{
		/**
		 * Add declarations within the semantic block
		 * without generating the statements themselves
		 * 
		 * Also, do not modify the external stack offset
		 */
		auto &parent = this->context.top();
		auto &inner_ctx = this->context.emplace();

		inner_ctx.scope = parent.scope;
		scope_push(inner_ctx.scope);
	}

	void
	Parser::decl_proc_params_add(const Parse_Unit &arg)
	{
		auto &ctx = this->context.top();
		ctx.decl_proc_arguments.push_back(arg.ast.as_decl);
	}

	void
	Parser::decl_proc_params_end(Source_Location loc, const Str &id, const Symbol_Type &ret)
	{
		auto &ctx = this->context.top();

		Symbol_Type proc_type = {.base = Symbol_Type::PROC};
		proc_type.procedure = scope_make_proc(ctx.scope, ret);

		// remove size from context stack offset
		for (const auto &arg: proc_type.procedure->parameters)
		{
			uint64_t size = std::max(1ui64, arg.array); // in words
			ctx.stack_offset -= size;
		}

		Symbol symbol = { .id = id, .type = proc_type, .defined_at = loc };
		if (auto [sym, err] = scope_add_decl(ctx.scope->parent_scope, symbol); err)
		{
			parser_log(err, loc);
		}
		else
		{
			// Add return type and symbol
			ctx.scope->proc_sym = sym;
		}
	}

	Parse_Unit
	Parser::decl_proc_end(const Str &id)
	{
		Parse_Unit self = {};
		
		// Create block statements
		// Keep old stack offset
		// Pop context
		auto proc_ctx = std::move(this->context.top());
		auto block = ast_block(Buf<AST>::clone(proc_ctx.block_stmts), proc_ctx.stack_offset);
		auto args = Buf<Decl *>::clone(proc_ctx.decl_proc_arguments);

		this->context.pop();
		scope_pop(proc_ctx.scope);

		auto &ctx = this->context.top();
		if (auto sym = scope_get_sym(ctx.scope, id.data))
		{
			self.loc = sym->defined_at;
			self.ast = ast_decl_proc(sym, args, block.as_block);
		}

		return self;
	}

	void
	Parser::switch_begin(const Parse_Unit &expr)
	{
		if (symtype_is_integral(expr.semexpr.type) == false)
			return parser_log(Error{ expr.loc, "invalid type" });

		auto &ctx = ctx_push(this->context);
		ctx.switch_expr = expr.ast;
	}

	Parse_Unit
	Parser::switch_end(Source_Location loc)
	{
		Parse_Unit self = { .loc = loc };
		
		auto ctx = ctx_pop(this->context);

		// Collect switch cases from context
		auto switch_cases = Buf<Switch_Case*>::make(ctx.switch_cases.size());
		for (size_t i = 0; i < switch_cases.count; i++)
			switch_cases[i] = ctx.switch_cases[i].ast_sw_case;

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
		auto &expr = literal.semexpr;

		if (expr.is_literal == false || symtype_is_integral(expr.type) == false)
		{
			if (literal.err == false)
				parser_log(Error{ literal.loc, "invalid case" });

			return;
		}

		// Look for duplicates in all cases
		auto &ctx = this->context.top();
		for (auto &sw_case : ctx.switch_cases)
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

		// Add literals in last case
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
		Parse_Unit self = { .loc = loc };
		self.ast = ast_while(cond.ast, block.ast.as_block);
		return self;
	}

	Parse_Unit
	Parser::do_while_loop(Source_Location loc, const Parse_Unit &cond, const Parse_Unit &block)
	{
		Parse_Unit self = { .loc = loc };
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
		Parse_Unit self = { .loc = loc };
		auto block = this->block_end();
		self.ast = ast_for(init.ast, cond.ast, post.ast, block.ast.as_block);
		return self;
	}

	Parser *
	parser_instance()
	{
		static Parser self = {};
		return &self;
	}

	void
	parser_log(const Error &err, Log_Level lvl)
	{
		parser_log(err, err.loc, lvl);
	}

	void
	parser_log(const Error &err, Source_Location loc, Log_Level lvl)
	{
		auto parser = parser_instance();
		if (lvl != Log_Level::INFO)
			parser->has_errors = true;

		auto msg = std::format("{}: {}", lvl, err);
		
		// Fill with loc if error has none
		if (err.loc != Source_Location{})
			loc = err.loc;

		yyerror(&loc, nullptr, msg.c_str());

		if (lvl == Log_Level::CRITICAL)
			exit(-1);
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

		// Current line and yycolno
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
		auto str = std::format("{}", *loc);
		fprintf(out, "%s", str.data());
	}
}


void
yyerror(const s22::Source_Location *location, s22::Parser *p, const char *message)
{
	auto parser = s22::parser_instance();
	auto &source_code = parser->source_code;

	if (*location == s22::Source_Location{})
	{
		parser->logs.push_back(std::string{message});
		return;
	}
	else
	{
		parser->logs.push_back(std::format("({}) {}", location->last_line, message));
	}

	// Read file line by line into buffer
	char buf[1024 + 1] = {};
	size_t lines_to_read = location->first_line;
	size_t position_in_source_code = 0;
	
	while (lines_to_read > 0)
	{
		size_t i = 0;
		for (; i + 1 < sizeof(buf) && position_in_source_code < source_code.count; i++)
		{
			char c = source_code.buf[position_in_source_code++];
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
	parser->logs.push_back(std::string{buf});

	// Print indicator
	for (size_t i = 0; i < strlen(buf); i++)
	{
		if (i == location->first_column - 1)
			buf[i] = '^';

		else if (isspace(buf[i]) == false)
			buf[i] = ' ';
	}
	parser->logs.push_back(std::string{buf});
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