#pragma once
#include <stdint.h>
#include "compiler/AST.h"

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

		I_MOV,
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
		uint64_t id;
	};

	// Temporary register
	// Memory address
	// Immediate value
	// Condition
	enum OPERAND_LOCATION
	{
		// NIL value
		OP_NIL,

		// General purpose registers
		RAX, RBX, RCX, RDX,

		// Reserved registers
		RSP, RBP, RSI, RDI,

		// Immediate value
		OP_IMM,

		// Memory address
		OP_MEM,

		// Label
		OP_LBL,

		// Condition
		OP_COND
	};

	struct Memory_Address
	{
		OPERAND_LOCATION base;
		OPERAND_LOCATION index;
		int offset;
	};

	struct Operand
	{
		OPERAND_LOCATION loc;
		size_t size;
		
		inline Operand() = default;
		inline Operand(OPERAND_LOCATION l) 		{ loc = l; }
        inline Operand(int v)					{ loc = OP_IMM; value = (uint64_t)v; }
        inline Operand(uint64_t v)				{ loc = OP_IMM; value = v; }
		inline Operand(Label lbl)				{ loc = OP_LBL; label = lbl; }
		inline Operand(Memory_Address adr)		{ loc = OP_MEM; address = adr; }

		union // immediate value/memory offset
		{
			uint64_t value;
			Label label;
			Memory_Address address;
		};
	};

	Backend
	backend_instance();

	void
	backend_init(Backend self);

	void
	backend_write(Backend self);

	Operand
	backend_generate(Backend self, AST ast);

	void
	backend_decl(Backend self, const Symbol *sym);

	void
	backend_decl_expr(Backend self, const Symbol *sym, const Parse_Unit &right);

	Operand
	backend_lit(Backend self, Literal *lit);

	Operand
	backend_sym(Backend self, const Symbol *sym);

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