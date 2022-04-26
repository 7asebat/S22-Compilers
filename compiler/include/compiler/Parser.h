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
		literal(Literal lit, Source_Location loc, Symbol_Type::BASE base);

		Parse_Unit
		id(const Str &id, Source_Location loc);

		Parse_Unit
		array(const Str &id, Source_Location loc, const Parse_Unit &right);

		void
		assign(const Str &id, Source_Location loc, Op_Assign op, const Parse_Unit &right);

		void
		array_assign(const Parse_Unit &left, Source_Location loc, Op_Assign op, const Parse_Unit &right);

		Parse_Unit
		binary(const Parse_Unit &left, Source_Location loc, Op_Binary op, const Parse_Unit &right);

		Parse_Unit
		unary(Op_Unary op, const Parse_Unit &right, Source_Location loc);

		void
		pcall_begin();

		void
		pcall_add(const Parse_Unit &unit);

		Parse_Unit
		pcall(const Str &id, Source_Location loc);

		void
		decl(const Str &id, Source_Location loc, Symbol_Type type);

		void
		decl_expr(const Str &id, Source_Location loc, Symbol_Type type, const Parse_Unit &right);

		void
		decl_array(const Str &id, Source_Location loc, Symbol_Type type, const Parse_Unit &right);

		void
		decl_const(const Str &id, Source_Location loc, Symbol_Type type, const Parse_Unit &right);

		void
		decl_proc_begin(const Str &id, Source_Location loc);

		void
		decl_proc_end(const Str &id, Symbol_Type return_type);

		void
		switch_begin(const Parse_Unit &unit);

		void
		switch_end();

		void
		switch_case_begin();

		void
		switch_case_end();

		void
		switch_case_add(const Parse_Unit &unit);

		void
		switch_default(Source_Location loc);

		Scope global;
		Scope *current_scope;

		std::stack<std::vector<Expr>> proc_call_arguments_stack;

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

	enum class Error_Level
	{
		INFO,
		WARNING,
		ERROR,
	};

	void
	parse_error(const Error &err, Error_Level lvl = Error_Level::ERROR);
}

void
yyerror(const s22::Source_Location *location, s22::Parser *p, const char *message);

