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
		Source_Location loc;

		// Optional literal value
		Literal lit;

		// Optional error
		Error err;
	};

	enum class Op_Assign
	{
		MOV, // A = B

		ADD, // A += B
		SUB, // A -= B
		MUL, // A *= B
		DIV, // A /= B
		MOD, // A %= B

		AND, // A &= B
		OR,  // A |= B
		XOR, // A ^= B
		SHL, // A <<= B
		SHR, // A >>= B
	};

	enum class Op_Binary
	{
		ADD, // A + B
		SUB, // A - B
		MUL, // A * B
		DIV, // A / B
		MOD, // A % B

		AND, // A & B
		OR,  // A | B
		XOR, // A ^ B
		SHL, // A << B
		SHR, // A >> B

		LOGICAL,
		LT,  // A < B
		LEQ, // A <= B

		EQ,  // A == B
		NEQ, // A != B

		GT,  // A > B
		GEQ, // A >= B

		L_AND,  // A && B
		L_OR,    // A || B
	};

	enum class Op_Unary
	{
		// Mathematical
		NEG, // -A

		// Binary
		INV, // ~A

		// Logical
		NOT, // !A
	};

	Expr
	expr_literal(Scope *scope, Literal lit, Source_Location loc, Symbol_Type::BASE base);

	Expr
	expr_identifier(Scope *scope, const char *id, Source_Location loc);

	Expr
	expr_assign(Scope *scope, const char *id, Source_Location loc, const Expr &right, Op_Assign op);

	Expr
	expr_array_assign(Scope *scope, const Expr &left, Source_Location loc, const Expr &right, Op_Assign op);

	Expr
	expr_binary(Scope *scope, const Expr &left, Source_Location loc, const Expr &right, Op_Binary op);

	Expr
	expr_unary(Scope *scope, const Expr &right, Source_Location loc, Op_Unary op);

	Expr
	expr_array_access(Scope *scope, const char *id, Source_Location loc, const Expr &expr);

	Expr
	expr_proc_call(Scope *scope, const char *id, Source_Location loc, Buf<Expr> params);
}