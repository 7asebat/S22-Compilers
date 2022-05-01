#pragma once

#include "compiler/AST.h"
#include "compiler/Backend.h"
#include "compiler/Semantic_Expr.h"
#include "compiler/Symbol.h"

#include <stack>
#include <unordered_set>

namespace s22
{
	struct Parse_Unit
	{
		Semantic_Expr semexpr;
		AST ast;
		Source_Location loc;
		Error err;
	};

	union YY_Symbol
	{
		Str id;
		Literal value;
		Symbol_Type type;
		Parse_Unit unit;
	};

	struct Parser
	{
		void
		program_begin();

		void
		program_end();

		void
		block_begin();

		void
		block_add(const Parse_Unit &stmt);

		Parse_Unit
		block_end();

		Parse_Unit
		return_value(Source_Location loc);

		Parse_Unit
		return_value(Source_Location loc, const Parse_Unit &expr);

		Parse_Unit
		literal(Source_Location loc, Literal lit, Symbol_Type::BASE base);

		Parse_Unit
		id(Source_Location loc, const Str &id);

		Parse_Unit
		array(Source_Location loc, const Str &id, const Parse_Unit &right);

		Parse_Unit
		assign(Source_Location loc, const Str &id, Asn op, const Parse_Unit &right);

		Parse_Unit
		array_assign(Source_Location loc, const Parse_Unit &left, Asn op, const Parse_Unit &right);

		Parse_Unit
		binary(Source_Location loc, const Parse_Unit &left, Bin op, const Parse_Unit &right);

		Parse_Unit
		unary(Source_Location loc, Uny op, const Parse_Unit &right);

		void
		pcall_begin();

		void
		pcall_add(const Parse_Unit &arg);

		Parse_Unit
		pcall(Source_Location loc, const Str &id);

		Parse_Unit
		decl(Source_Location loc, const Str &id, Symbol_Type type);

		Parse_Unit
		decl_expr(Source_Location loc, const Str &id, Symbol_Type type, const Parse_Unit &right);

		Parse_Unit
		decl_array(Source_Location loc, const Str &id, Symbol_Type type, const Parse_Unit &right);

		Parse_Unit
		decl_const(Source_Location loc, const Str &id, Symbol_Type type, const Parse_Unit &right);

		void
		decl_proc_begin();

		void
		decl_proc_params_add(const Parse_Unit &arg);

		void
		decl_proc_params_end(Source_Location loc, const Str &id, const Symbol_Type &ret);

		Parse_Unit
		decl_proc_end(const Str &id);

		Parse_Unit
		if_cond(const Parse_Unit &cond, const Parse_Unit &block, const Parse_Unit &next);

		Parse_Unit
		else_if_cond(Parse_Unit &prev, const Parse_Unit &cond, const Parse_Unit &block);

		void
		switch_begin(const Parse_Unit &expr);

		Parse_Unit
		switch_end(Source_Location loc);

		void
		switch_case_begin(Source_Location loc);

		void
		switch_case_end(Source_Location loc, const Parse_Unit &block);

		void
		switch_case_add(const Parse_Unit &literal);

		void
		switch_default(Source_Location loc, const Parse_Unit &block);

		Parse_Unit
		while_loop(Source_Location loc, const Parse_Unit &cond, const Parse_Unit &block);

		Parse_Unit
		do_while_loop(Source_Location loc, const Parse_Unit &cond, const Parse_Unit &block);

		void
		for_loop_begin();

		Parse_Unit
		for_loop(Source_Location loc, const Parse_Unit &init, const Parse_Unit &cond, const Parse_Unit &post);

		struct Context
		{
			Scope *scope;
			std::vector<Parse_Unit> proc_call_arguments;
			std::vector<Decl *> decl_proc_arguments;
			std::vector<AST> block_stmts;
			size_t stack_offset;

			struct Sw_Case
			{
				std::vector<Literal *> group;
				Switch_Case *ast_sw_case;
			};
			AST switch_expr;
			std::vector<Sw_Case> switch_cases;
			Block *switch_default;
		};
		std::stack<Context> context;
		Scope global;
		Backend backend;
		bool has_errors;
	};

	enum class Log_Level
	{
		INFO,
		WARNING,
		ERROR,
		CRITICAL,
	};

	Parser *
	parser_instance();

	void
	parser_log(const Error &err, Log_Level lvl = Log_Level::INFO);

	void
	parser_log(const Error &err, Source_Location loc, Log_Level lvl = Log_Level::INFO);
}

void
yyerror(const s22::Source_Location *location, s22::Parser *p, const char *message);
