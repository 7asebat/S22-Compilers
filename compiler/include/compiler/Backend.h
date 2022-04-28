#pragma once
#include <stdint.h>

namespace s22
{
	struct IBackend;
	using Backend = IBackend*;

	struct Symbol;
	struct Parse_Unit;

	enum INSTRUCTION_OP
	{
		// Used for labels
		I_NOP,

		I_LD, I_ST, I_MOV,
		I_TEST, I_BR, I_BZ, I_BNZ,

		// Binary
		I_ADD, I_SUB, I_MUL, I_DIV, I_MOD, I_AND, I_OR, I_XOR, I_SHL, I_SHR,

		// Logical binary
		I_LOG_LT, I_LOG_LEQ, I_LOG_EQ, I_LOG_NEQ, I_LOG_GT, I_LOG_GEQ,
		I_LOG_AND, I_LOG_OR,

		// Unary
		I_NEG, I_INV,

		// Logical unary
		I_LOG_NOT,
	};

	struct Label
	{
		enum TYPE
		{
			NONE,
			LABEL,

			END_IF, END_ELSEIF, END_ALL,
			END_CASE, END_SWITCH,

			FOR, END_FOR,
			WHILE, END_WHILE,

			PROC,
		};
		TYPE type;
		int line, col;
	};

	// Temporary register
	// Memory address
	// Immediate value
	// Condition
	struct Operand
	{
		enum LOCATION
		{
			// General purpose registers
			R0, R1, R2, R3, R4,

			// Reserved registers
			SP, BP,

			// Immediate value
			IMM,

			// Memory address
			MEM,

			// Label
			LBL,

			// Condition
			COND
		};
		LOCATION loc;
		size_t size;

		union // immediate value/memory offset
		{
			uint64_t value;
			uint64_t offset;
			Label label;
		};
	};

	Backend
	backend_instance();

	void
	backend_init(Backend self);

	void
	backend_generate(Backend self);

	void
	backend_decl(Backend self, const Symbol *sym);

	void
	backend_decl_expr(Backend self, const Symbol *sym, const Parse_Unit &right);

	Operand
	backend_id(Backend self, const Symbol *sym);

	Operand
	backend_array_access(Backend self, const Symbol *sym, const Parse_Unit &right);

	void
	backend_assign(Backend self, INSTRUCTION_OP op, const Symbol *sym, const Parse_Unit &right);

	void
	backend_array_assign(Backend self, INSTRUCTION_OP op, const Parse_Unit &left, const Parse_Unit &right);

	Operand
	backend_binary(Backend self, INSTRUCTION_OP op, const Parse_Unit &left, const Parse_Unit &right);

	Operand
	backend_unary(Backend self, INSTRUCTION_OP op, const Parse_Unit &right);
}