#include <stdint.h>
#include <unordered_map>

#include "compiler/Expr.h"
namespace s22
{
	using word = uint64_t;

	struct Reference
	{
		word location;
	};

	struct Value
	{
		enum SOURCE
		{
			// General purpose registers
			R0, R1, R2, R3, R4,

			// Special registers
			SP, BP, IR,

			// Immediate value
			Imm,
		};
		SOURCE src;
		word imm_val; // Only valid in the case of immediate values
	};

	struct Proc
	{
		uint8_t argument_sizes[64];
		uint8_t return_size;
		word return_address;
	};

	struct Register
	{
		enum ID { R0, R1, R2, R3, R4 };
		ID id;

		bool is_used;
	};

	struct Backend
	{
		Register reg[Value::R4 + 1];
		std::unordered_map<Symbol*, Reference> variables;
		std::unordered_map<std::string, Reference> labels;
	};

	void
	backend_init(Backend *self);

	void
	backend_decl(Backend *self, const Symbol *sym);

	void
	backend_decl_expr(Backend *self, const Symbol *sym, Expr &expr);

	void
	backend_decl_array(Backend *self, const Symbol *sym, word size);

	void
	backend_expr(Backend *self, Expr &expr);

	void
	backend_op_assign(Backend *self, Op_Assign &binary);

	void
	backend_op_array_assign(Backend *self, Op_Assign &binary);

	void
	backend_op_binary(Backend *self, Op_Binary &binary);

	void
	backend_op_unary(Backend *self, Op_Unary &unary);
}