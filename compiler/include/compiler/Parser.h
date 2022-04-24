#pragma once

#include "compiler/Symbol.h"
#include "compiler/Expr.h"

#include <stack>

namespace s22
{
	struct Parser;
}

void
yyerror(const s22::Location *location, s22::Parser *p, const char *message);

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

		Scope global;
		Scope *current_scope;
		std::stack<std::vector<Expr>> list_stack;
		bool caught_error;

		void
		push();

		void
		pop();

		void
		new_stmt();

		void
		return_value(Location loc);

		void
		return_value(Location loc, Expr &expr);

		void
		id(const char *id, Location loc, Expr &dst);

		void
		literal(uint64_t value, Location loc, Symbol_Type::BASE base, Expr &dst);

		void
		assign(const char *id, Location loc, Op_Assign::KIND op, Expr &right);

		void
		binary(Expr &left, Location loc, Op_Binary::KIND op, Expr &right, Expr &dst);

		void
		unary(Op_Unary::KIND op, Expr &right, Location loc, Expr &dst);

		void
		array(const char *id, Location loc, Expr &expr, Expr &dst);

		void
		pcall_begin();

		void
		pcall_add(Expr &expr);

		void
		pcall(const char *id, Location loc, Expr &dst);

		void
		decl(const char *id, Location loc, Symbol_Type type);

		void
		decl_expr(const char *id, Location loc, Symbol_Type type, Expr &expr);

		void
		decl_array(const char *id, Location loc, Symbol_Type type, Expr &expr);

		void
		decl_const(const char *id, Location loc, Symbol_Type type, Expr &expr);

		void
		decl_proc_begin(const char *id, Location loc);

		void
		decl_proc_end(const char *id, Symbol_Type return_type);
	};
}