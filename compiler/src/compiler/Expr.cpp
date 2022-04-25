#include "compiler/Expr.h"

namespace s22
{
	Expr
	make_error(const Error &err, Source_Location loc)
	{
		Expr self = { .kind = Expr::ERROR, .loc = loc };
		self.as_error = err;
		return self;
	}

	Expr
	make_literal(Scope *scope, uint64_t value, Source_Location loc, Symbol_Type::BASE base)
	{
		Expr self = { .loc = loc };
		self.kind = Expr::LITERAL;
		self.type = { .base = base };

		self.as_literal = { .base = base, .value = value };
		return self;
	}

	Expr
	make_identifier(Scope *scope, const char *id, Source_Location loc)
	{
		auto sym = scope_get_id(scope, id);
		if (sym == nullptr)
			return make_error(Error{ loc, "undeclared identifier" }, loc);
		sym->is_used = true;

		Expr self = { .loc = loc };
		self.kind = Expr::IDENTIFIER;
		self.type = sym->type;

		self.as_id = { .sym = sym };
		return self;
	}

	Expr
	make_op_assign(Scope *scope, const char *id, Source_Location loc, Expr &right, Op_Assign::KIND kind)
	{
		auto sym = scope_get_id(scope, id);
		if (sym == nullptr)
			return make_error(Error{ loc, "undeclared identifier" }, loc);
		sym->is_used = true;

		if (sym->type != right.type)
			return make_error(Error{ right.loc, "type mismatch" }, loc);

		if (sym->is_constant || sym->type.procedure)
			return make_error(Error{ loc, "assignment to constant" }, loc);

		sym->is_set = true;

		Expr self = { .loc = loc };
		self.kind = Expr::OP_ASSIGN;
		self.type = sym->type;

		self.as_assign = { .kind = kind, .left = copy(make_identifier(scope, id, loc)), .right = copy(right) };
		return self;
	}

	Expr
	make_op_array_assign(Scope *scope, Expr &left, Source_Location loc, Expr &right, Op_Assign::KIND kind)
	{
		if (left.type != right.type)
			return make_error(Error{ right.loc, "type mismatch" }, loc);

		Expr self = { .loc = loc };
		self.kind = Expr::OP_ASSIGN;
		self.type = left.type;

		self.as_assign = { .kind = kind, .left = copy(left), .right = copy(right) };
		return self;
	}

	Expr
	make_op_binary(Scope *scope, Expr &left, Source_Location loc, Expr &right, Op_Binary::KIND kind)
	{
		if (left.type != right.type)
			return make_error(Error{ right.loc, "type mismatch" }, loc);

		if (symtype_allows_arithmetic(left.type) == false)
			return make_error(Error{ right.loc, "invalid operand" }, loc);

		Expr self = { .loc = loc };
		self.kind = Expr::OP_BINARY;

		if (kind < Op_Binary::LOGICAL)
			self.type = left.type;
		else
			self.type = SYMTYPE_BOOL;

		self.as_binary = { .kind = kind, .left = copy(left), .right = copy(right) };
		return self;
	}

	Expr
	make_op_unary(Scope *scope, Expr &right, Source_Location loc, Op_Unary::KIND kind)
	{
		if (symtype_allows_arithmetic(right.type) == false)
			return make_error(Error{ right.loc, "invalid operand" }, loc);

		Expr self = { .loc = loc };
		self.kind = Expr::OP_UNARY;

		if (kind != Op_Unary::NOT)
			self.type = right.type;
		else
			self.type = SYMTYPE_BOOL;

		self.as_unary = { .kind = kind, .right = copy(right) };
		return self;
	}

	Expr
	make_array_access(Scope *scope, const char *id, Source_Location loc, Expr &expr)
	{
		auto sym = scope_get_id(scope, id);
		if (sym == nullptr)
			return make_error(Error{ loc, "undeclared identifier" }, loc);
		sym->is_used = true;

		if (sym->type.array == false)
			return make_error(Error{ loc, "type cannot be indexed" }, loc);

		if (symtype_is_valid_index(expr.type) == false)
			return make_error(Error{ expr.loc, "invalid index" }, loc);

		Expr self = { .loc = loc };
		self.kind = Expr::ARRAY_ACCESS;
		self.type = sym->type;
		self.type.array = 0;

		self.as_array = { .sym = sym, .index = copy(expr) };
		return self;
	}

	Expr
	make_proc_call(Scope *scope, const char *id, Source_Location loc, Buf<Expr> params)
	{
		auto sym = scope_get_id(scope, id);
		if (sym == nullptr)
			return make_error(Error{ loc, "undeclared identifier" }, loc);
		sym->is_used = true;

		if (sym->type.procedure == false)
			return make_error(Error{ loc, "type is not callable" }, loc);

		if (sym->type.procedure->parameters.count != params.count)
			return make_error(Error{ loc, "wrong parameter count" }, loc);

		for (size_t i = 0; i < params.count; i++)
		{
			if (params[i].type != sym->type.procedure->parameters[i])
				return make_error(Error{ params[i].loc, "invalid argument" }, loc);
		}

		Expr self { .loc = loc };
		self.kind = Expr::PROC_CALL;
		self.type = sym->type.procedure->return_type | SYMTYPE_VOID;

		self.as_proc_call = { .sym = sym, .params = params };
		return self;
	}
}