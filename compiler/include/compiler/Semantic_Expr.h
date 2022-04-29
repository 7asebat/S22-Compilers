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

		LT  = I_LOG_LT,   	// A < B
		LEQ = I_LOG_LEQ, 	// A <= B

		EQ  = I_LOG_EQ,   	// A == B
		NEQ = I_LOG_NEQ, 	// A != B

		GT  = I_LOG_GT,   	// A > B
		GEQ = I_LOG_GEQ, 	// A >= B

		L_AND = I_LOG_AND, 	// A && B
		L_OR  = I_LOG_OR,   // A || B
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