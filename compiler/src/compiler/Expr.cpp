#include "compiler/Expr.h"
#include "compiler/Parser.h"

namespace s22
{
	Result<Expr>
	expr_literal(Scope *scope, Literal lit, Symbol_Type::BASE base)
	{
		Expr self = {};
		self.type = { .base = base };

		self.kind = Expr::LITERAL;
		self.lit = lit;

		return self;
	}

	Result<Expr>
	expr_identifier(Scope *scope, const char *id)
	{
		auto sym = scope_get_id(scope, id);
		if (sym == nullptr)
			return Error{ "undeclared identifier" };
		sym->is_used = true;

		Expr self = {};
		self.type = sym->type;
		return self;
	}

	Result<Expr>
	expr_assign(Scope *scope, const char *id, const Parse_Unit &right, Op_Assign op)
	{
		auto sym = scope_get_id(scope, id);
		if (sym == nullptr)
			return Error{ "undeclared identifier" };
		sym->is_used = true;

		if (sym->type != right.expr.type)
			return Error{ "type mismatch" };

		if (sym->is_constant || sym->type.procedure)
			return Error{ "assignment to constant" };

		sym->is_set = true;

		Expr self = {};
		self.type = sym->type;
		return self;
	}

	Result<Expr>
	expr_array_assign(Scope *scope, const Parse_Unit &left, const Parse_Unit &right, Op_Assign op)
	{
		if (left.expr.type != right.expr.type)
			return Error{ "type mismatch" };

		Expr self = {};
		self.type = left.expr.type;
		return self;
	}

	Result<Expr>
	expr_binary(Scope *scope, const Parse_Unit &left, const Parse_Unit &right, Op_Binary op)
	{
		if (left.expr.type != right.expr.type)
			return Error{ "type mismatch" };

		if (symtype_allows_arithmetic(left.expr.type) == false)
			return Error{ left.loc, "invalid operand" };

		Expr self = {};

		switch (op)
		{
		case Op_Binary::ADD:
		case Op_Binary::SUB:
		case Op_Binary::MUL:
		case Op_Binary::DIV:
		case Op_Binary::MOD:
		case Op_Binary::AND:
		case Op_Binary::OR:
		case Op_Binary::XOR:
		case Op_Binary::SHL:
		case Op_Binary::SHR:
			self.type = left.expr.type;
			break;

		case Op_Binary::LT:
		case Op_Binary::LEQ:
		case Op_Binary::EQ:
		case Op_Binary::NEQ:
		case Op_Binary::GT:
		case Op_Binary::GEQ:
		case Op_Binary::L_AND:
		case Op_Binary::L_OR:
			self.type = SYMTYPE_BOOL;
			break;
		}

		return self;
	}

	Result<Expr>
	expr_unary(Scope *scope, const Parse_Unit &right, Op_Unary op)
	{
		if (symtype_allows_arithmetic(right.expr.type) == false)
			return Error{ right.loc, "invalid operand" };

		Expr self = {};

		if (op != Op_Unary::NOT)
			self.type = right.expr.type;
		else
			self.type = SYMTYPE_BOOL;

		return self;
	}

	Result<Expr>
	expr_array_access(Scope *scope, const char *id, const Parse_Unit &expr)
	{
		auto sym = scope_get_id(scope, id);
		if (sym == nullptr)
			return Error{ "undeclared identifier" };
		sym->is_used = true;

		if (sym->type.array == false)
			return Error{ "type cannot be indexed" };

		if (symtype_is_integral(expr.expr.type) == false)
			return Error{ expr.loc, "invalid index" };

		Expr self = {};
		self.type = sym->type;
		self.type.array = 0;

		return self;
	}

	Result<Expr>
	expr_proc_call(Scope *scope, const char *id, Buf<Parse_Unit> params)
	{
		auto sym = scope_get_id(scope, id);
		if (sym == nullptr)
			return Error{ "undeclared identifier" };
		sym->is_used = true;

		if (sym->type.procedure == false)
			return Error{ "type is not callable" };

		if (sym->type.procedure->parameters.count != params.count)
			return Error{ "invalid argument count" };

		for (size_t i = 0; i < params.count; i++)
		{
			if (params[i].expr.type != sym->type.procedure->parameters[i])
				return Error{ params[i].loc, "invalid argument" };
		}

		Expr self {};
		self.type = sym->type.procedure->return_type | SYMTYPE_VOID;

		return self;
	}
}