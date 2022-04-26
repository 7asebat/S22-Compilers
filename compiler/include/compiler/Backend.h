#pragma once
#include <stdint.h>

namespace s22
{
	struct IBackend;
	using Backend = IBackend*;

	struct Symbol;
	struct Expr;

	enum class Op_Assign;
	enum class Op_Binary;
	enum class Op_Unary;

	// Temporary register
	// Memory address
	// Immediate value
	struct Operand
	{
		enum LOCATION
		{
			// General purpose registers
			R0, R1, R2, R3, R4,

			// Reserved registers
			SP, BP, IR,

			// Immediate value
			IMM,

			// Memory address
			MEM,
		};
		LOCATION loc;
		size_t size;

		union // immediate value/memory offset
		{
			uint64_t value;
			uint64_t offset;
		};
	};

	Backend
	backend_instance();

	void
	backend_init(Backend self);

	void
	backend_decl(Backend self, const Symbol *sym);

	void
	backend_decl_expr(Backend self, const Symbol *sym, Operand right);

	Operand
	backend_id(Backend self, const Symbol *sym);

	Operand
	backend_array_access(Backend self, const Symbol *sym, Operand right);

	void
	backend_assign(Backend self, Op_Assign op, const Symbol *sym, Operand right);

	void
	backend_array_assign(Backend self, Op_Assign op, Operand left, Operand right);

	Operand
	backend_binary(Backend self, Op_Binary op, Operand left, Operand right);

	Operand
	backend_unary(Backend self, Op_Unary op, Operand right);
}