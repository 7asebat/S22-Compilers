#include "compiler/Semantic_Expr.h"
#include "compiler/Parser.h"

#include <format>
#include <stdio.h>

namespace s22
{
	bool
	semexpr_allows_arithmetic(const Semantic_Expr &semexpr)
	{
		// Arrays are different from array access Expr{arr} != Expr{arr[i]}
		return (semexpr.array || semexpr.procedure) == false;
	}

	bool
	semexpr_is_integral(const Semantic_Expr &semexpr)
	{
		return semexpr == SEMEXPR_INT || semexpr == SEMEXPR_UINT || semexpr == SEMEXPR_BOOL;
	}

	void
	semexpr_print(const Semantic_Expr &semexpr, FILE *out)
	{
		auto fmt = std::format("{}", semexpr);
		fprintf(out, "%s", fmt.data());
	}

	Result<Semantic_Expr>
	semexpr_literal(Scope *scope, Literal lit, Semantic_Expr::BASE base)
	{
		return Semantic_Expr{ .base = base, .is_literal = true };
	}

	Result<Semantic_Expr>
	semexpr_id(Scope *scope, const char *id)
	{
		auto sym = scope_get_sym(scope, id);
		if (sym == nullptr)
			return Error{ "undeclared identifier" };
		sym->is_used = true;

		return sym->type;
	}

	Result<Semantic_Expr>
	semexpr_assign(Scope *scope, const char *id, const Parse_Unit &right, Asn op)
	{
		auto sym = scope_get_sym(scope, id);
		if (sym == nullptr)
			return Error{ "undeclared identifier" };
		sym->is_used = true;

		if (sym->type != right.semexpr)
			return Error{ "type mismatch" };

		if (sym->is_constant || sym->type.procedure)
			return Error{ "assignment to constant" };

		sym->is_set = true;

		return sym->type;
	}

	Result<Semantic_Expr>
	semexpr_array_assign(Scope *scope, const Parse_Unit &left, const Parse_Unit &right, Asn op)
	{
		if (left.semexpr != right.semexpr)
			return Error{ "type mismatch" };

		return left.semexpr;
	}

	Result<Semantic_Expr>
	semexpr_binary(Scope *scope, const Parse_Unit &left, const Parse_Unit &right, Bin op)
	{
		if (semexpr_allows_arithmetic(left.semexpr) == false)
			return Error{ left.loc, "invalid operand" };

		// Operand specific rules
		switch (op)
		{
		case Bin::SHL: case Bin::SHR: case Bin::MOD:
			if (semexpr_is_integral(right.semexpr) == false)
				return Error{ right.loc, "invalid operand" };

		case Bin::L_AND: case Bin::L_OR:
			break; // both types are cast to boolean

		default:
			if (left.semexpr != right.semexpr)
				return Error{ "type mismatch" };
		}

		switch (op)
		{
		case Bin::ADD:
		case Bin::SUB:
		case Bin::MUL:
		case Bin::DIV:
		case Bin::MOD:
		case Bin::AND:
		case Bin::OR:
		case Bin::XOR:
		case Bin::SHL:
		case Bin::SHR: return left.semexpr;

		case Bin::LT:
		case Bin::LEQ:
		case Bin::EQ:
		case Bin::NEQ:
		case Bin::GT:
		case Bin::GEQ:
		case Bin::L_AND:
		case Bin::L_OR: return SEMEXPR_BOOL;

		default: return Semantic_Expr{};
		}
	}

	Result<Semantic_Expr>
	semexpr_unary(Scope *scope, const Parse_Unit &right, Uny op)
	{
		if (semexpr_allows_arithmetic(right.semexpr) == false)
			return Error{ right.loc, "invalid operand" };

		if (op != Uny::NOT)
		{
			return right.semexpr;
		}
		else
		{
			return SEMEXPR_BOOL;
		}
	}

	Result<Semantic_Expr>
	semexpr_array_access(Scope *scope, const char *id, const Parse_Unit &expr)
	{
		auto sym = scope_get_sym(scope, id);
		if (sym == nullptr)
			return Error{ "undeclared identifier" };
		sym->is_used = true;

		if (sym->type.array == false)
			return Error{ "type cannot be indexed" };

		if (semexpr_is_integral(expr.semexpr) == false)
			return Error{ expr.loc, "invalid index" };

		Semantic_Expr self = sym->type;
		self.array = 0;

		return self;
	}

	Result<Semantic_Expr>
	semexpr_proc_call(Scope *scope, const char *id, const Buf<Parse_Unit> &params)
	{
		auto sym = scope_get_sym(scope, id);
		if (sym == nullptr)
			return Error{ "undeclared identifier" };
		sym->is_used = true;

		if (sym->type.procedure == false)
			return Error{ "type is not callable" };

		if (sym->type.procedure->parameters.count != params.count)
			return Error{ "invalid argument count" };

		for (size_t i = 0; i < params.count; i++)
		{
			if (params[i].semexpr != sym->type.procedure->parameters[i])
				return Error{ params[i].loc, "invalid argument" };
		}

		return sym->type.procedure->return_type;
	}
}