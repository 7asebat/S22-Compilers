#pragma once

#include "compiler/Backend.h"
#include "compiler/Symbol.h"

namespace s22
{
	struct Semantic_Expr
	{
		bool is_literal;
		Symbol_Type type;
		// TODO: UNINITIALIZED VARIABLES
	};

	enum class Op_Assign
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

		LT,  // A < B
		LEQ, // A <= B

		EQ,  // A == B
		NEQ, // A != B

		GT,  // A > B
		GEQ, // A >= B

		L_AND,  // A && B
		L_OR,   // A || B
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

	struct Parse_Unit;

	Result<Semantic_Expr>
	semexpr_literal(Scope *scope, Literal lit, Symbol_Type::BASE base);

	Result<Semantic_Expr>
	semexpr_id(Scope *scope, const char *id);

	Result<Semantic_Expr>
	semexpr_assign(Scope *scope, const char *id, const Parse_Unit &right, Op_Assign op);

	Result<Semantic_Expr>
	semexpr_array_assign(Scope *scope, const Parse_Unit &left, const Parse_Unit &right, Op_Assign op);

	Result<Semantic_Expr>
	semexpr_binary(Scope *scope, const Parse_Unit &left, const Parse_Unit &right, Bin op);

	Result<Semantic_Expr>
	semexpr_unary(Scope *scope, const Parse_Unit &right, Uny op);

	Result<Semantic_Expr>
	semexpr_array_access(Scope *scope, const char *id, const Parse_Unit &expr);

	Result<Semantic_Expr>
	semexpr_proc_call(Scope *scope, const char *id, Buf<Parse_Unit> params);
}

template <>
struct std::formatter<s22::Op_Assign> : std::formatter<std::string>
{
	auto
	format(s22::Op_Assign op, format_context &ctx)
	{
		using s22::Op_Assign;

		switch (op)
		{
		case Op_Assign::MOV: return format_to(ctx.out(), "MOV");
		case Op_Assign::ADD: return format_to(ctx.out(), "ADD");
		case Op_Assign::SUB: return format_to(ctx.out(), "SUB");
		case Op_Assign::MUL: return format_to(ctx.out(), "MUL");
		case Op_Assign::DIV: return format_to(ctx.out(), "DIV");
		case Op_Assign::MOD: return format_to(ctx.out(), "MOD");

		case Op_Assign::AND: return format_to(ctx.out(), "AND");
		case Op_Assign::OR:  return format_to(ctx.out(), "OR");
		case Op_Assign::XOR: return format_to(ctx.out(), "XOR");
		case Op_Assign::SHL: return format_to(ctx.out(), "SHL");
		case Op_Assign::SHR: return format_to(ctx.out(), "SHR");
		}

		return ctx.out();
	}
};

template <>
struct std::formatter<s22::Bin> : std::formatter<std::string>
{
	auto
	format(s22::Bin op, format_context &ctx)
	{
		using s22::Bin;

		switch (op)
		{
		case Bin::ADD: return format_to(ctx.out(), "ADD");
		case Bin::SUB: return format_to(ctx.out(), "SUB");
		case Bin::MUL: return format_to(ctx.out(), "MUL");
		case Bin::DIV: return format_to(ctx.out(), "DIV");
		case Bin::MOD: return format_to(ctx.out(), "MOD");

		case Bin::AND: return format_to(ctx.out(), "AND");
		case Bin::OR: return format_to(ctx.out(), "OR");
		case Bin::XOR: return format_to(ctx.out(), "XOR");
		case Bin::SHL: return format_to(ctx.out(), "SHL");
		case Bin::SHR: return format_to(ctx.out(), "SHR");

		// Branch conditions
		case Bin::LT: return format_to(ctx.out(), "LT");
		case Bin::LEQ: return format_to(ctx.out(), "LEQ");
		case Bin::EQ: return format_to(ctx.out(), "EQ");
		case Bin::NEQ: return format_to(ctx.out(), "NEQ");
		case Bin::GT: return format_to(ctx.out(), "GT");
		case Bin::GEQ: return format_to(ctx.out(), "GEQ");

		// L_AND, L_OR do not correspond to instructions
		}

		return ctx.out();
	}
};

template <>
struct std::formatter<s22::Uny> : std::formatter<std::string>
{
	auto
	format(s22::Uny op, format_context &ctx)
	{
		using s22::Uny;

		switch (op)
		{
		case Uny::NOT: return format_to(ctx.out(), "NOT");
		case Uny::NEG: return format_to(ctx.out(), "NEG");
		case Uny::INV: return format_to(ctx.out(), "INV");
		}

		return ctx.out();
	}
};
