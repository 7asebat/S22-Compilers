#pragma once

#include "compiler/Util.h"
#include "compiler/Backend.h"	// INSTRUCTION_OP

#include <format>

namespace s22
{
	struct Parse_Unit;
	struct Scope;
	struct Literal;
	struct Procedure;

	// Semantic expression types
	struct Semantic_Expr
	{
		// Base type
		enum BASE
		{
			VOID,
			PROC,

			INT,
			UINT,
			FLOAT,
			BOOL,
		};
		BASE base;

		size_t array;					// array size, 0 if not an array
		bool is_literal;				// true if a literal
		Optional<Procedure> procedure;	// optional value if symbol represents a procedure

		inline bool
		operator==(const Semantic_Expr &other) const
		{
			if (base != other.base)
				return false;

			if (array != other.array)
				return false;

			if (procedure != other.procedure)
				return false;

			return true;
		}

		inline bool
		operator!=(const Semantic_Expr &other) const { return !operator==(other); }
	};

	constexpr Semantic_Expr SEMEXPR_VOID  = { .base = Semantic_Expr::VOID  };
	constexpr Semantic_Expr SEMEXPR_INT   = { .base = Semantic_Expr::INT   };
	constexpr Semantic_Expr SEMEXPR_UINT  = { .base = Semantic_Expr::UINT  };
	constexpr Semantic_Expr SEMEXPR_FLOAT = { .base = Semantic_Expr::FLOAT };
	constexpr Semantic_Expr SEMEXPR_BOOL  = { .base = Semantic_Expr::BOOL  };

	struct Procedure
	{
		Buf<Semantic_Expr> parameters;
		Semantic_Expr return_type;

		inline bool
		operator==(const Procedure &other) const
		{
			if (parameters != other.parameters)
				return false;

			if (return_type != other.return_type)
				return false;

			return true;
		}
	};

	enum class Asn
	{
		MOV = I_MOV, // A = B

		ADD = I_ADD, // A += B
		SUB = I_SUB, // A -= B
		MUL = I_MUL, // A *= B
		DIV = I_DIV, // A /= B
		MOD = I_MOD, // A %= B

		AND = I_AND, // A &= B
		OR  = I_OR,  // A |= B
		XOR = I_XOR, // A ^= B
		SHL = I_SHL, // A <<= B
		SHR = I_SHR, // A >>= B
	};

	enum class Bin
	{
		ADD = I_ADD, // A + B
		SUB = I_SUB, // A - B
		MUL = I_MUL, // A * B
		DIV = I_DIV, // A / B
		MOD = I_MOD, // A % B

		AND = I_AND, // A & B
		OR  = I_OR,  // A | B
		XOR = I_XOR, // A ^ B
		SHL = I_SHL, // A << B
		SHR = I_SHR, // A >> B

		LT  = I_LOG_LT,		// A < B
		LEQ = I_LOG_LEQ,	// A <= B

		EQ  = I_LOG_EQ,		// A == B
		NEQ = I_LOG_NEQ,	// A != B

		GT  = I_LOG_GT,		// A > B
		GEQ = I_LOG_GEQ,	// A >= B

		L_AND = I_LOG_AND,	// A && B
		L_OR  = I_LOG_OR,	// A || B
	};

	enum class Uny
	{
		// Mathematical
		NEG = I_NEG, // -A

		// Binary
		INV = I_INV, // ~A

		// Logical
		NOT = I_LOG_NOT, // !A
	};

	bool
	semexpr_allows_arithmetic(const Semantic_Expr &semexpr);

	bool
	semexpr_is_integral(const Semantic_Expr &semexpr);

	void
	semexpr_print(const Semantic_Expr &semexpr, FILE *out);

	// Constructors for different types
	Result<Semantic_Expr>
	semexpr_literal(Scope *scope, Literal lit, Semantic_Expr::BASE base);

	Result<Semantic_Expr>
	semexpr_id(Scope *scope, const char *id);

	Result<Semantic_Expr>
	semexpr_assign(Scope *scope, const char *id, const Parse_Unit &right, Asn op);

	Result<Semantic_Expr>
	semexpr_array_assign(Scope *scope, const Parse_Unit &left, const Parse_Unit &right, Asn op);

	Result<Semantic_Expr>
	semexpr_binary(Scope *scope, const Parse_Unit &left, const Parse_Unit &right, Bin op);

	Result<Semantic_Expr>
	semexpr_unary(Scope *scope, const Parse_Unit &right, Uny op);

	Result<Semantic_Expr>
	semexpr_array_access(Scope *scope, const char *id, const Parse_Unit &expr);

	Result<Semantic_Expr>
	semexpr_proc_call(Scope *scope, const char *id, const Buf<Parse_Unit> &params);
}

template <>
struct std::formatter<s22::Semantic_Expr> : std::formatter<std::string>
{
	auto
	format(const s22::Semantic_Expr &type, format_context &ctx)
	{
		using namespace s22;
		if (type.procedure)
		{
			format_to(ctx.out(), "proc(");
			{
				format_to(ctx.out(), "{}", type.procedure->parameters);
			}
			format_to(ctx.out(), ")");

			if (type.procedure->return_type != SEMEXPR_VOID)
			{
				format_to(ctx.out(), " -> ");
				format_to(ctx.out(), "{}", type.procedure->return_type);
			}
		}
		else
		{
			if (type.array)
			{
				format_to(ctx.out(), "[{}]", type.array);
			}

			switch (type.base)
			{
			case s22::Semantic_Expr::INT: return format_to(ctx.out(), "int");
			case s22::Semantic_Expr::UINT: return format_to(ctx.out(), "uint");
			case s22::Semantic_Expr::FLOAT: return format_to(ctx.out(), "float");
			case s22::Semantic_Expr::BOOL: return format_to(ctx.out(), "bool");
			default: break;
			}
		}
		return ctx.out();
	}
};
