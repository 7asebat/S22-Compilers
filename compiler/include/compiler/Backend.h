#pragma once
#include <stdint.h>

namespace s22
{
	struct IBackend;
	using Backend = IBackend*;

	struct Symbol;
	struct Expr;

	struct Value_Location
	{
		enum KIND
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
		KIND kind;

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
	backend_decl_expr(Backend self, const Symbol *sym, const Expr *expr);

	void
	backend_expr(Backend self, const Expr *expr);
}