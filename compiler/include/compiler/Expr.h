#pragma once

#include "compiler/Backend.h"
#include "compiler/Symbol.h"

namespace s22
{
	union Literal
	{
		uint64_t value;

		// Value interpretations
		uint64_t u64;
		int64_t s64;
		double f64;
		bool b;
	};

	struct Expr
	{
		enum KIND { VARIABLE, LITERAL } kind;

		Symbol_Type type;

		// Optional literal value
		Literal lit;
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

	enum class Op_Binary
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

	enum class Op_Unary
	{
		// Mathematical
		NEG = I_NEG, // -A

		// Binary
		INV = I_INV, // ~A

		// Logical
		NOT = I_LOG_NOT, // !A
	};

	struct Parse_Unit;

	Result<Expr>
	expr_literal(Scope *scope, Literal lit, Symbol_Type::BASE base);

	Result<Expr>
	expr_identifier(Scope *scope, const char *id);

	Result<Expr>
	expr_assign(Scope *scope, const char *id, const Parse_Unit &right, Op_Assign op);

	Result<Expr>
	expr_array_assign(Scope *scope, const Parse_Unit &left, const Parse_Unit &right, Op_Assign op);

	Result<Expr>
	expr_binary(Scope *scope, const Parse_Unit &left, const Parse_Unit &right, Op_Binary op);

	Result<Expr>
	expr_unary(Scope *scope, const Parse_Unit &right, Op_Unary op);

	Result<Expr>
	expr_array_access(Scope *scope, const char *id, const Parse_Unit &expr);

	Result<Expr>
	expr_proc_call(Scope *scope, const char *id, Buf<Parse_Unit> params);
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
struct std::formatter<s22::Op_Binary> : std::formatter<std::string>
{
	auto
	format(s22::Op_Binary op, format_context &ctx)
	{
		using s22::Op_Binary;

		switch (op)
		{
		case Op_Binary::ADD: return format_to(ctx.out(), "ADD");
		case Op_Binary::SUB: return format_to(ctx.out(), "SUB");
		case Op_Binary::MUL: return format_to(ctx.out(), "MUL");
		case Op_Binary::DIV: return format_to(ctx.out(), "DIV");
		case Op_Binary::MOD: return format_to(ctx.out(), "MOD");

		case Op_Binary::AND: return format_to(ctx.out(), "AND");
		case Op_Binary::OR: return format_to(ctx.out(), "OR");
		case Op_Binary::XOR: return format_to(ctx.out(), "XOR");
		case Op_Binary::SHL: return format_to(ctx.out(), "SHL");
		case Op_Binary::SHR: return format_to(ctx.out(), "SHR");

		// Branch conditions
		case Op_Binary::LT: return format_to(ctx.out(), "LT");
		case Op_Binary::LEQ: return format_to(ctx.out(), "LEQ");
		case Op_Binary::EQ: return format_to(ctx.out(), "EQ");
		case Op_Binary::NEQ: return format_to(ctx.out(), "NEQ");
		case Op_Binary::GT: return format_to(ctx.out(), "GT");
		case Op_Binary::GEQ: return format_to(ctx.out(), "GEQ");

		// L_AND, L_OR do not correspond to instructions
		}

		return ctx.out();
	}
};

template <>
struct std::formatter<s22::Op_Unary> : std::formatter<std::string>
{
	auto
	format(s22::Op_Unary op, format_context &ctx)
	{
		using s22::Op_Unary;

		switch (op)
		{
		case Op_Unary::NOT: return format_to(ctx.out(), "NOT");
		case Op_Unary::NEG: return format_to(ctx.out(), "NEG");
		case Op_Unary::INV: return format_to(ctx.out(), "INV");
		}

		return ctx.out();
	}
};
