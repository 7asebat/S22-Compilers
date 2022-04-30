#include "compiler/Semantic_Expr.h"
#include "compiler/Parser.h"

namespace s22
{
	Result<Semantic_Expr>
	semexpr_literal(Scope *scope, Literal lit, Symbol_Type::BASE base)
	{
		Semantic_Expr self = {};
		self.type = { .base = base };
		self.is_literal = true;
		return self;
	}

	Result<Semantic_Expr>
	semexpr_id(Scope *scope, const char *id)
	{
		auto sym = scope_get_id(scope, id);
		if (sym == nullptr)
			return Error{ "undeclared identifier" };
		sym->is_used = true;

		Semantic_Expr self = {};
		self.type = sym->type;
		return self;
	}

	Result<Semantic_Expr>
	semexpr_assign(Scope *scope, const char *id, const Parse_Unit &right, Asn op)
	{
		auto sym = scope_get_id(scope, id);
		if (sym == nullptr)
			return Error{ "undeclared identifier" };
		sym->is_used = true;

		if (sym->type != right.semexpr.type)
			return Error{ "type mismatch" };

		if (sym->is_constant || sym->type.procedure)
			return Error{ "assignment to constant" };

		sym->is_set = true;

		Semantic_Expr self = {};
		self.type = sym->type;
		return self;
	}

	Result<Semantic_Expr>
	semexpr_array_assign(Scope *scope, const Parse_Unit &left, const Parse_Unit &right, Asn op)
	{
		if (left.semexpr.type != right.semexpr.type)
			return Error{ "type mismatch" };

		Semantic_Expr self = {};
		self.type = left.semexpr.type;
		return self;
	}

	Result<Semantic_Expr>
	semexpr_binary(Scope *scope, const Parse_Unit &left, const Parse_Unit &right, Bin op)
	{
		// TODO: Cast to boolean
		if (left.semexpr.type != right.semexpr.type)
			return Error{ "type mismatch" };

		if (symtype_allows_arithmetic(left.semexpr.type) == false)
			return Error{ left.loc, "invalid operand" };

		Semantic_Expr self = {};
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
		case Bin::SHR:
			self.type = left.semexpr.type;
			break;

		case Bin::LT:
		case Bin::LEQ:
		case Bin::EQ:
		case Bin::NEQ:
		case Bin::GT:
		case Bin::GEQ:
		case Bin::L_AND:
		case Bin::L_OR:
			self.type = SYMTYPE_BOOL;
			break;
		}

		return self;
	}

	Result<Semantic_Expr>
	semexpr_unary(Scope *scope, const Parse_Unit &right, Uny op)
	{
		if (symtype_allows_arithmetic(right.semexpr.type) == false)
			return Error{ right.loc, "invalid operand" };

		Semantic_Expr self = {};

		if (op != Uny::NOT)
			self.type = right.semexpr.type;
		else
			self.type = SYMTYPE_BOOL;

		return self;
	}

	Result<Semantic_Expr>
	semexpr_array_access(Scope *scope, const char *id, const Parse_Unit &expr)
	{
		auto sym = scope_get_id(scope, id);
		if (sym == nullptr)
			return Error{ "undeclared identifier" };
		sym->is_used = true;

		if (sym->type.array == false)
			return Error{ "type cannot be indexed" };

		if (symtype_is_integral(expr.semexpr.type) == false)
			return Error{ expr.loc, "invalid index" };

		Semantic_Expr self = {};
		self.type = sym->type;
		self.type.array = 0;

		return self;
	}

	Result<Semantic_Expr>
	semexpr_proc_call(Scope *scope, const char *id, Buf<Parse_Unit> params)
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
			if (params[i].semexpr.type != sym->type.procedure->parameters[i])
				return Error{ params[i].loc, "invalid argument" };
		}

		Semantic_Expr self {};
		self.type = sym->type.procedure->return_type;

		return self;
	}
}