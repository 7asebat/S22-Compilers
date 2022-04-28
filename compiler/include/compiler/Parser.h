#pragma once

#include "compiler/Symbol.h"
#include "compiler/Expr.h"
#include "compiler/Backend.h"

#include <stack>
#include <unordered_set>

namespace s22
{
	struct Parse_Unit
	{
		Expr expr;
		Operand opr;
		Source_Location loc;
		Error err;
	};

	struct Parser
	{
		union YY_Symbol
		{
			Str id;
			Literal value;
			Symbol_Type type;
			Parse_Unit unit;
		};

		void
		init();

		void
		push();

		void
		pop();

		void
		new_stmt();

		void
		return_value(Source_Location loc);

		void
		return_value(Source_Location loc, const Parse_Unit &unit);

		Parse_Unit
		literal(Source_Location loc, Literal lit, Symbol_Type::BASE base);

		Parse_Unit
		id(Source_Location loc, const Str &id);

		Parse_Unit
		array(Source_Location loc, const Str &id, const Parse_Unit &right);

		void
		assign(Source_Location loc, const Str &id, Op_Assign op, const Parse_Unit &right);

		void
		array_assign(Source_Location loc, const Parse_Unit &left, Op_Assign op, const Parse_Unit &right);

		Parse_Unit
		binary(Source_Location loc, const Parse_Unit &left, Op_Binary op, const Parse_Unit &right);

		Parse_Unit
		unary(Source_Location loc, Op_Unary op, const Parse_Unit &right);

		void
		pcall_begin();

		void
		pcall_add(const Parse_Unit &unit);

		Parse_Unit
		pcall(Source_Location loc, const Str &id);

		void
		decl(Source_Location loc, const Str &id, Symbol_Type type);

		void
		decl_expr(Source_Location loc, const Str &id, Symbol_Type type, const Parse_Unit &right);

		void
		decl_array(Source_Location loc, const Str &id, Symbol_Type type, const Parse_Unit &right);

		void
		decl_const(Source_Location loc, const Str &id, Symbol_Type type, const Parse_Unit &right);

		void
		decl_proc_begin(Source_Location loc, const Str &id);

		void
		decl_proc_end(const Str &id, Symbol_Type return_type);

		Parse_Unit
		condition(const Parse_Unit &unit);

		void
		if_begin(Source_Location loc, const Parse_Unit &cond);

		void
		else_if_begin(Source_Location loc, const Parse_Unit &cond);

		void
		else_begin(Source_Location loc);

		void
		if_end(Source_Location loc);

		void
		switch_begin(Source_Location loc, const Parse_Unit &unit);

		void
		switch_end(Source_Location loc);

		void
		switch_case_begin(Source_Location loc);

		void
		switch_case_end(Source_Location loc);

		void
		switch_case_add(const Parse_Unit &unit);

		void
		switch_default(Source_Location loc);

		void
		while_begin(Source_Location loc, const Parse_Unit &cond);

		void
		while_end(Source_Location loc);

		void
		do_while_begin(Source_Location loc);

		void
		do_while_end(Source_Location loc, const Parse_Unit &cond);

		Scope global;
		Scope *current_scope;

		std::stack<std::vector<Parse_Unit>> proc_call_arguments_stack;

		struct Switch_Case
		{
			uint64_t value;
			size_t partition;

			struct Hash
			{
				using SWC = Switch_Case;
				inline size_t
				operator()(const SWC &self) const { return std::hash<uint64_t>{}(self.value); }
			};

			struct Pred
			{
				using SWC = Switch_Case;
				inline bool
				operator()(const SWC &lhs, const SWC &rhs) const
				{
					bool equal = lhs.value == rhs.value;

					if (lhs.partition == size_t(-1) || rhs.partition == size_t(-1))
						equal &= lhs.partition == rhs.partition;

					return equal;
				}
			};
		};
		constexpr static Switch_Case SWC_DEFAULT = { .partition = size_t(-1) };

		std::unordered_set<Switch_Case, Switch_Case::Hash, Switch_Case::Pred> switch_cases;
		size_t switch_current_partition;

		Backend backend;
	};

	enum class Log_Level
	{
		INFO,
		WARNING,
		ERROR,
		CRITICAL,
	};

	void
	parser_log(const Error &err, Log_Level lvl = Log_Level::INFO);

	void
	parser_log(const Error &err, Source_Location loc, Log_Level lvl = Log_Level::INFO);
}

void
yyerror(const s22::Source_Location *location, s22::Parser *p, const char *message);

