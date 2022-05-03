#pragma once
#include <vector>
#include <array>
#include <string>

namespace s22
{
	struct AST;

	struct IBackend;
	using Backend = IBackend*;

	// Instructions are defined in the header to directly map different operation kinds to instructions
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

		// Procedures and stack
		I_PUSH, I_POP,
		I_CALL, I_RET,
	};

	// Backend singleton instance
	Backend
	backend_instance();

	// Cleanup resources
	void
	backend_dispose(Backend self);

	// Output quadruples (label (1) + instruction (4))
	using Program = std::vector<std::array<std::string, 5>>;

	Program
	backend_write(Backend self);

	// Compile AST into instructions, follow up with backend_write to retrieve the program as a list of quadruples
	void
	backend_compile(Backend self, AST ast);
}