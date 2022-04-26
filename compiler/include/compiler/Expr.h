#pragma once

#include "compiler/Backend.h"
#include "compiler/Symbol.h"

namespace s22
{
	struct Expr;

	struct Literal
	{
		Symbol_Type::BASE base;
	};

	struct Identifier
	{
		Symbol *sym;
	};

	struct Array_Access
	{
		Symbol *sym;
		Expr *index;
	};

	struct Op_Assign
	{
		enum KIND
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

		KIND kind;
		Expr *left;
		Expr *right;
	};

	struct Op_Binary
	{
		enum KIND
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

			// Binary
			INV, // ~A

			// Logical
			NOT, // !A
		};

		KIND kind;
		Expr *right;
	};

	struct Proc_Call
	{
		Symbol *sym;
		Buf<Expr> params;
	};

	struct Expr
	{
		enum KIND
		{
			LITERAL,
			IDENTIFIER,
			ARRAY_ACCESS,
			OP_ASSIGN,
			OP_BINARY,
			OP_UNARY,
			PROC_CALL,
		};

		KIND kind;
		Symbol_Type type;
		Source_Location loc;
		Value_Location value_loc;
		Error err;

		union
		{
			Literal as_literal;
			Identifier as_id;
			Array_Access as_array;
			Op_Assign as_assign;
			Op_Binary as_binary;
			Op_Unary as_unary;
			Proc_Call as_proc_call;
		};
	};

	Expr
	make_literal(Scope *scope, uint64_t value, Source_Location loc, Symbol_Type::BASE base);

	Expr
	make_identifier(Scope *scope, const char *id, Source_Location loc);

	Expr
	make_op_assign(Scope *scope, const char *id, Source_Location loc, Expr &right, Op_Assign::KIND kind);

	Expr
	make_op_array_assign(Scope *scope, Expr &left, Source_Location loc, Expr &right, Op_Assign::KIND kind);

	Expr
	make_op_binary(Scope *scope, Expr &left, Source_Location loc, Expr &right, Op_Binary::KIND kind);

	Expr
	make_op_unary(Scope *scope, Expr &right, Source_Location loc, Op_Unary::KIND kind);

	Expr
	make_array_access(Scope *scope, const char *id, Source_Location loc, Expr &expr);

	Expr
	make_proc_call(Scope *scope, const char *id, Source_Location loc, Buf<Expr> params);
}