#pragma once

#include "compiler/Symbol.h"
#include "compiler/Expr.h"

namespace s22
{
	struct Parser;
}

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

		Symbol_Table global;
		Symbol_Table *current_scope;
		bool caught_error;

		void
		push();

		void
		pop();

		void
		new_stmt();

		void
		id(const char *id, Location loc, Expr &dst);

		void
		constant(uint64_t value, Expr &dst);

		void
		binary(Expr &left, Location loc, Op_Binary::KIND op, Expr &right, Expr &dst);

		void
		unary(Op_Unary::KIND op, Expr &right, Location loc, Expr &dst);

		void
		array(const char *id, Location loc, Expr &expr, Expr &dst);

		void
		decl(const char *id, Location loc, Symbol_Type type);

		void
		decl_expr(const char *id, Location loc, Symbol_Type type, Expr expr);

		void
		decl_array(const char *id, Location loc, Symbol_Type type, uint64_t size);

		void
		decl_const(const char *id, Location loc, Symbol_Type type, Expr expr);

		void
		decl_proc_begin(const char *id, Location loc);

		void
		decl_proc_end(const char *id, Symbol_Type return_type);
	};
}

void
yyerror(const s22::Location *location, s22::Parser *p, const char *message);