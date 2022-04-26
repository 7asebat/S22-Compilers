#include "compiler/Backend.h"
#include "compiler/Symbol.h"
#include "compiler/Expr.h"

#include <unordered_map>
#include <fstream>
#include <iostream>
#include <stack>

namespace s22
{
	struct Proc
	{
		uint8_t argument_sizes[64];
		uint8_t return_size;
		uint64_t return_address;
	};

	struct Register
	{
		Operand::LOCATION id;
		bool is_used;
	};

	struct IBackend
	{
		Register reg[Operand::R4 + 1];

		std::unordered_map<const Symbol*, Operand> variables;
		std::unordered_map<std::string, uint64_t> labels;
		std::stack<size_t> stack_offset;

		std::ofstream out_file;

		inline ~IBackend()
		{
			out_file.close();
		}
	};

	template<typename... TArgs>
	void
	add_ins(Backend self, const char *fmt, TArgs &&...args)
	{
		auto str = std::format(fmt, std::forward<TArgs>(args)...);
		std::cout << str << std::endl;
//		self->out_file << str << std::endl;
	}

	Register
	first_available_register(Backend self)
	{
		// TODO: Handle no available registers, overwrite for now
		for (const auto &reg: self->reg)
		{
			if (reg.is_used == false)
				return reg;
		}

		return self->reg[Operand::R4];
	}

	Backend
	backend_instance()
	{
		static IBackend be = {};
		return &be;
	}

	void
	backend_init(Backend self)
	{
		self->out_file.open("out.asm");

		// Begin a new offset/code path
		self->stack_offset.push(0);

		self->reg[Operand::R0] = { .id = Operand::R0 };
		self->reg[Operand::R1] = { .id = Operand::R1 };
		self->reg[Operand::R2] = { .id = Operand::R2 };
		self->reg[Operand::R3] = { .id = Operand::R3 };
		self->reg[Operand::R4] = { .id = Operand::R4 };
	}

	void
	backend_decl(Backend self, const Symbol *sym)
	{
		uint64_t word_count = std::max(1ui64, sym->type.array);

		// SP - word_count
		self->stack_offset.top() += word_count;
		self->variables[sym] = { .loc = Operand::MEM, .offset = self->stack_offset.top() };

		add_ins(self, "SUB SP, {}", word_count);
	}

	void
	backend_assign_array(Backend self, const Expr &left, const Expr *expr)
	{
/*		auto loc = expr->value_loc;

		// Move to register first
		if (expr->value_loc.kind == Value_Location::MEM)
		{
			auto reg = first_available_register(self);
			add_ins(self, "MOV {}, {}", Value_Location{ reg.id }, loc);
			loc = Value_Location{reg.id};
		}

		auto dst_loc = self->variables[left.as_array.sym];
		dst_loc.offset += left.as_array.index->value_loc.value;
		add_ins(self, "MOV {}, {}", dst_loc, loc);*/
	}

	void
	backend_assign(Backend self, const Expr *left, const Expr *expr)
	{
/*		auto loc = expr->value_loc;

		// Move to register first
		if (expr->value_loc.kind == Value_Location::MEM)
		{
			auto reg = first_available_register(self);
			add_ins(self, "MOV {}, {}", Value_Location{ reg.id }, loc);
			loc = Value_Location{reg.id};
		}

		add_ins(self, "MOV {}, {}", self->variables[left->as_id.sym], loc);*/
	}

	void
	backend_decl_expr(Backend self, const Symbol *sym, const Expr *expr)
	{
/*		backend_decl(self, sym);
		Expr left = { .as_id = { .sym = (Symbol*)sym }};
		backend_assign(self, &left, expr);*/
	}
}

template <>
struct std::formatter<s22::Operand> : std::formatter<std::string>
{
	auto
	format(s22::Operand opr, format_context &ctx)
	{
		using namespace s22;
		switch (opr.loc)
		{
		case Operand::R0: return format_to(ctx.out(), "R0");
		case Operand::R1: return format_to(ctx.out(), "R1");
		case Operand::R2: return format_to(ctx.out(), "R2");
		case Operand::R3: return format_to(ctx.out(), "R3");
		case Operand::R4: return format_to(ctx.out(), "R4");
		case Operand::IR: return format_to(ctx.out(), "IR");
		case Operand::SP: return format_to(ctx.out(), "SP");
		case Operand::BP: return format_to(ctx.out(), "BP");

		case Operand::IMM: return format_to(ctx.out(), "{}", opr.value);
		case Operand::MEM: return format_to(ctx.out(), "-{}(BP)", opr.offset);

		default: return ctx.out();
		}
	}
};
