#pragma once

#include "compiler/Symbol.h"

#include <string_view>

namespace s22
{
	struct Expr;

	Result<Symbol_Type>
	expr_evaluate(Expr &expr, Symbol_Table *scope);

	struct Constant
	{
		// 64 bits for all constants
		uint64_t value;
	};

	struct Identifier
	{
		// Identifier in symbol table
		const char *id;
	};

	struct Op_Assign
	{
		enum KIND
		{
			NOP, // A = B

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

		KIND kind;
		Symbol *left;
		Expr *right;
	};

	struct Op_Binary
	{
		enum KIND
		{
			// Mathematical
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

			// Logical
			LT,  // A < B
			LEQ, // A <= B

			EQ,  // A == B
			NEQ, // A != B

			GT,  // A > B
			GEQ, // A >= B

			L_AND,  // A && B
			L_OR, 	// A || B
		};

		KIND kind;
		Expr *left;
		Expr *right;
	};

	struct Op_Unary
	{
		enum KIND
		{
			// Mathematical
			NEG, // -A

			// Logical
			NOT, // !A
			INV, // ~A
		};

		KIND kind;
		Expr *right;
	};

	struct Expr
	{
		enum KIND
		{
			CONSTANT,
			IDENTIFIER,
			OP_ASSIGN,
			OP_BINARY,
			OP_UNARY,
			// TODO: PROCEDURE CALLS
			// TODO: ARRAY ACCESS
		};

		KIND kind;
		union
		{
			Constant as_const;
			Identifier as_id;
			Op_Assign as_assign;
			Op_Binary as_binary;
			Op_Unary as_unary;

			void* as_error;
		};

		Symbol_Type type;
	};

	constexpr inline Expr EXPR_ERROR = { .type = SYMTYPE_ERROR };

	Expr
	constant_new(uint64_t value);

	Result<Expr>
	identifier_new(Symbol_Table *scope, const char *id);

	Result<Expr>
	op_assign_new(Symbol_Table *scope, const char *id, Op_Assign::KIND kind, Expr &right);

	Result<Expr>
	op_binary_new(Symbol_Table *scope, Expr &left, Op_Binary::KIND kind, Expr &right);

	Result<Expr>
	op_unary_new(Symbol_Table *scope, Op_Unary::KIND kind, Expr &right);
}