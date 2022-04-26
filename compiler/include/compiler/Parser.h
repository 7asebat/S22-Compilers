#pragma once

#include "compiler/Symbol.h"
#include "compiler/Expr.h"

#include <stack>
#include <unordered_set>

namespace s22
{
	struct Parser
	{
		union YY_Symbol
		{
			const char* lexeme;
			uint64_t value;
			Symbol_Type type;
			Expr expr;
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
		return_value(Source_Location loc, Expr &expr);

		void
		literal(uint64_t value, Source_Location loc, Symbol_Type::BASE base, Expr &dst);

		void
		id(const char *id, Source_Location loc, Expr &dst);

		void
		assign(const char *id, Source_Location loc, Op_Assign::KIND op, Expr &right);

		void
		array_assign(Expr &left, Source_Location loc, Op_Assign::KIND op, Expr &right);

		void
		binary(Expr &left, Source_Location loc, Op_Binary::KIND op, Expr &right, Expr &dst);

		void
		unary(Op_Unary::KIND op, Expr &right, Source_Location loc, Expr &dst);

		void
		array(const char *id, Source_Location loc, Expr &expr, Expr &dst);

		void
		pcall_begin();

		void
		pcall_add(Expr &expr);

		void
		pcall(const char *id, Source_Location loc, Expr &dst);

		void
		decl(const char *id, Source_Location loc, Symbol_Type type);

		void
		decl_expr(const char *id, Source_Location loc, Symbol_Type type, Expr &expr);

		void
		decl_array(const char *id, Source_Location loc, Symbol_Type type, Expr &index);

		void
		decl_const(const char *id, Source_Location loc, Symbol_Type type, Expr &expr);

		void
		decl_proc_begin(const char *id, Source_Location loc);

		void
		decl_proc_end(const char *id, Symbol_Type return_type);

		void
		switch_begin(const Expr &expr);

		void
		switch_end();

		void
		switch_case_begin();

		void
		switch_case_end();

		void
		switch_case_add(Expr &expr);

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

