#include "compiler/Backend.h"
#include "compiler/Symbol.h"
#include "compiler/Expr.h"

#include <unordered_map>
#include <fstream>
#include <iostream>
#include <stack>

namespace s22
{
	bool
	operand_is_register(Operand opr)
	{
		switch (opr.loc)
		{
		case Operand::R0:
		case Operand::R1:
		case Operand::R2:
		case Operand::R3:
		case Operand::R4:
			return true;

		default:
			return false;
		}
	}

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
	inline static void
	instruction(Backend self, const char *fmt, TArgs &&...args)
	{
		auto str = std::format(fmt, std::forward<TArgs>(args)...);
		std::cout << str << std::endl;
		// self->out_file << str << std::endl;
	}

	inline static Operand
	first_available_register(Backend self)
	{
		for (auto &reg: self->reg)
		{
			if (reg.is_used == false)
				return { .loc = reg.id };
		}

		// TODO: Handle no available registers, overwrite for now
		return { .loc = Operand::R0 };
	}

	inline static void
	clear_registers(Backend self)
	{
		for (auto &reg: self->reg)
			reg.is_used = false;
	}

	inline static void
	assign(Backend self, Op_Assign op, Operand left, Operand right)
	{
		auto src = right;
		if (op == Op_Assign::MOV)
		{
			if (operand_is_register(right) == false)
			{
				// Value needs to be passed
				src = first_available_register(self);
				instruction(self, "MOV {}, {}", src, right);
			}
		}
		else
		{
			// Move into register
			src = first_available_register(self);
			instruction(self, "LD {}, {}", src, left);
			instruction(self, "{} {}, {}, {}", op, src, src, right);
		}

		// Write result
		instruction(self, "ST {}, {}", left, src);

		clear_registers(self);
	}

	Backend
	backend_instance()
	{
		static IBackend self = {};
		return &self;
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

		instruction(self, "SUB SP, #{}", word_count);
	}

	void
	backend_decl_expr(Backend self, const Symbol *sym, Operand right)
	{
		backend_decl(self, sym);
		backend_assign(self, Op_Assign::MOV, sym, right);
	}

	Operand
	backend_id(Backend self, const Symbol *sym)
	{
		return self->variables[sym];
	}

	Operand
	backend_array_access(Backend self, const Symbol *sym, Operand right)
	{
		auto opr = self->variables[sym];
		opr.offset -= right.value;
		return opr;
	}

	void
	backend_assign(Backend self, Op_Assign op, const Symbol *sym, Operand right)
	{
		assign(self, op, self->variables[sym], right);
	}

	void
	backend_array_assign(Backend self, Op_Assign op, Operand left, Operand right)
	{
		assign(self, op, left, right);
	}

	Operand
	backend_binary(Backend self, Op_Binary op, Operand left, Operand right)
	{
		// Collect operands
		auto opr1 = left;
		if (opr1.loc == Operand::MEM)
		{
			auto reg = first_available_register(self);
			instruction(self, "LD {}, {}", reg, left);
			opr1 = reg;
		}

		auto opr2 = right;
		if (opr2.loc == Operand::MEM)
		{
			auto reg = first_available_register(self);
			instruction(self, "LD {}, {}", reg, right);
			opr2 = reg;
		}

		auto dst = first_available_register(self);
		self->reg[dst.loc].is_used = true;

		instruction(self, "{} {}, {}, {}", op, dst, opr1, opr2);

		return dst;
	}

	Operand
	backend_unary(Backend self, Op_Unary op, Operand right)
	{
		auto opr = right;
		if (opr.loc == Operand::MEM)
		{
			auto reg = first_available_register(self);
			instruction(self, "LD {}, {}", reg, right);
			opr = reg;
		}

		auto dst = first_available_register(self);
		self->reg[dst.loc].is_used = true;

		instruction(self, "{} {}, {}", op, dst, opr);

		return dst;
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

		case Operand::IMM: return format_to(ctx.out(), "#{}", opr.value);
		case Operand::MEM: return format_to(ctx.out(), "-{}(BP)", opr.offset);

		default: return ctx.out();
		}
	}
};