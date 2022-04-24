#pragma once

#include "compiler/Symbol.h"

#include <string_view>

namespace s22
{
	struct Expr;

	Error
	expr_evaluate(Expr &expr, Scope *scope);

	struct Literal
	{
		// 64 bits for all constants
		Symbol_Type::BASE base;
		uint64_t value;
	};

	struct Identifier
	{
		// Identifier in symbol table
		char id[128];
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

			// Logical
			NOT, // !A
			INV, // ~A
		};

		KIND kind;
		Expr *right;
	};

	struct Proc_Call
	{
		char id[128];
		Buf<Expr> params;
	};

	struct Expr
	{
		enum KIND
		{
			LITERAL,
			IDENTIFIER,
			OP_ASSIGN,
			OP_BINARY,
			OP_UNARY,
			PROC_CALL,
		};

		KIND kind;
		union
		{
			Literal as_literal;
			Identifier as_id;
			Op_Assign as_assign;
			Op_Binary as_binary;
			Op_Unary as_unary;
			Proc_Call as_proc_call;
		};

		Symbol_Type type;
		Location loc;
	};

	Result <Expr>
	literal_new(Scope *scope, uint64_t value, Location loc, Symbol_Type::BASE base);

	Result<Expr>
	identifier_new(Scope *scope, const char *id, Location loc);

	Result<Expr>
	op_assign_new(Scope *scope, const char *id, Location loc, Expr &right, Op_Assign::KIND kind);

	Result<Expr>
	op_binary_new(Scope *scope, Expr &left, Location loc, Expr &right, Op_Binary::KIND kind);

	Result<Expr>
	op_unary_new(Scope *scope, Expr &right, Location loc, Op_Unary::KIND kind);

	Result<Expr>
	proc_call_new(Scope *scope, const char *id, Location loc, Buf<Expr> params);
}