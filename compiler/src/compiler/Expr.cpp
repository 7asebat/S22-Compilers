#include "compiler/Expr.h"

namespace s22
{
	Expr
	expr_error(const Error &err, Source_Location loc)
	{
		Expr self = { .loc = loc, .err = err };
		return self;
	}

	Expr
	expr_literal(Scope *scope, Literal lit, Source_Location loc, Symbol_Type::BASE base)
	{
		Expr self = { .loc = loc };
		self.type = { .base = base };

		self.kind = Expr::LITERAL;
		self.lit = lit;

		return self;
	}

	Expr
	expr_identifier(Scope *scope, const char *id, Source_Location loc)
	{
		auto sym = scope_get_id(scope, id);
		if (sym == nullptr)
			return expr_error(Error{ loc, "undeclared identifier" }, loc);
		sym->is_used = true;

		Expr self = { .loc = loc };
		self.type = sym->type;
		return self;
	}

	Expr
	expr_assign(Scope *scope, const char *id, Source_Location loc, const Expr &right, Op_Assign op)
	{
		auto sym = scope_get_id(scope, id);
		if (sym == nullptr)
			return expr_error(Error{ loc, "undeclared identifier" }, loc);
		sym->is_used = true;

		if (sym->type != right.type)
			return expr_error(Error{ right.loc, "type mismatch" }, loc);

		if (sym->is_constant || sym->type.procedure)
			return expr_error(Error{ loc, "assignment to constant" }, loc);

		sym->is_set = true;

		Expr self = { .loc = loc };
		self.type = sym->type;
		return self;
	}

	Expr
	expr_array_assign(Scope *scope, const Expr &left, Source_Location loc, const Expr &right, Op_Assign op)
	{
		if (left.type != right.type)
			return expr_error(Error{ right.loc, "type mismatch" }, loc);

		Expr self = { .loc = loc };
		self.type = left.type;
		return self;
	}

	Expr
	expr_binary(Scope *scope, const Expr &left, Source_Location loc, const Expr &right, Op_Binary op)
	{
		if (left.type != right.type)
			return expr_error(Error{ right.loc, "type mismatch" }, loc);

		if (symtype_allows_arithmetic(left.type) == false)
			return expr_error(Error{ right.loc, "invalid operand" }, loc);

		Expr self = { .loc = loc };

		if (op < Op_Binary::LOGICAL)
			self.type = left.type;
		else
			self.type = SYMTYPE_BOOL;

		return self;
	}

	Expr
	expr_unary(Scope *scope, const Expr &right, Source_Location loc, Op_Unary op)
	{
		if (symtype_allows_arithmetic(right.type) == false)
			return expr_error(Error{ right.loc, "invalid operand" }, loc);

		Expr self = { .loc = loc };

		if (op != Op_Unary::NOT)
			self.type = right.type;
		else
			self.type = SYMTYPE_BOOL;

		return self;
	}

	Expr
	expr_array_access(Scope *scope, const char *id, Source_Location loc, const Expr &expr)
	{
		auto sym = scope_get_id(scope, id);
		if (sym == nullptr)
			return expr_error(Error{ loc, "undeclared identifier" }, loc);
		sym->is_used = true;

		if (sym->type.array == false)
			return expr_error(Error{ loc, "type cannot be indexed" }, loc);

		if (symtype_is_integral(expr.type) == false)
			return expr_error(Error{ expr.loc, "invalid index" }, loc);

		Expr self = { .loc = loc };
		self.type = sym->type;
		self.type.array = 0;

		return self;
	}

	Expr
	expr_proc_call(Scope *scope, const char *id, Source_Location loc, Buf<Expr> params)
	{
		auto sym = scope_get_id(scope, id);
		if (sym == nullptr)
			return expr_error(Error{ loc, "undeclared identifier" }, loc);
		sym->is_used = true;

		if (sym->type.procedure == false)
			return expr_error(Error{ loc, "type is not callable" }, loc);

		if (sym->type.procedure->parameters.count != params.count)
			return expr_error(Error{ loc, "invalid argument count" }, loc);

		for (size_t i = 0; i < params.count; i++)
		{
			if (params[i].type != sym->type.procedure->parameters[i])
				return expr_error(Error{ params[i].loc, "invalid argument" }, loc);
		}

		Expr self { .loc = loc };
		self.type = sym->type.procedure->return_type | SYMTYPE_VOID;

		return self;
	}
}