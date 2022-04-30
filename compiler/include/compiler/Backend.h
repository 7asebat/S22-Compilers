#pragma once

namespace s22
{
	struct AST;

	struct IBackend;
	using Backend = IBackend*;

	enum INSTRUCTION_OP
	{
		// Used for labels
		I_NOP,

		I_MOV,
		I_BR, I_BZ, I_BNZ,

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

	Backend
	backend_instance();

	void
	backend_init(Backend self);

	void
	backend_write(Backend self);

	void
	backend_compile(Backend self, AST ast);
}