#include "compiler/Backend.h"
#include "compiler/Symbol.h"
#include "compiler/Semantic_Expr.h"
#include "compiler/Parser.h"

#include <unordered_map>
#include <fstream>
#include <iostream>
#include <stack>

namespace s22
{
	constexpr static Operand OPR_SP   = { .loc = Operand::SP };
	constexpr static Operand OPR_BP   = { .loc = Operand::BP };
	constexpr static Operand OPR_ZERO = { .loc = Operand::IMM, .value = 0 };
	constexpr static Operand OPR_ONE  = { .loc = Operand::IMM, .value = 1 };

	struct Instruction
	{
		INSTRUCTION_OP op;
		Operand dst, src1, src2;

		size_t operand_count;
		Label label;
	};

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

		std::vector<Instruction> program;
	};

	template <typename... TArgs>
	inline static void
	instruction(Backend self, INSTRUCTION_OP op, TArgs &&...args)
	{
		Instruction ins = { op, std::forward<TArgs>(args)... };
		ins.operand_count = sizeof...(args);
		self->program.push_back(ins);
	}

	template <typename... TArgs>
	inline static void
	instruction(Backend self, Label label, INSTRUCTION_OP op, TArgs &&...args)
	{
		Instruction ins = { op, std::forward<TArgs>(args)... };
		ins.operand_count = sizeof...(args);
		ins.label = label;
		self->program.push_back(ins);
	}

	inline static void
	instruction(Backend self, Label label)
	{
		Instruction ins = { .label = label };
		self->program.push_back(ins);
	}

	inline static bool
	op_is_logical(INSTRUCTION_OP op)
	{
		switch (op)
		{
		case I_LOG_LT:
		case I_LOG_LEQ:
		case I_LOG_EQ:
		case I_LOG_NEQ:
		case I_LOG_GT:
		case I_LOG_GEQ:
		case I_LOG_NOT:
		case I_LOG_AND:
		case I_LOG_OR:
			return true;
		default:
			return false;
		}
	}

	inline static INSTRUCTION_OP
	invert_op(INSTRUCTION_OP op)
	{
		if (op_is_logical(op) == false)
			return op;

		switch (op)
		{
		case I_LOG_LT: 	return I_LOG_GEQ;
		case I_LOG_LEQ:	return I_LOG_GT;
		case I_LOG_EQ:	return I_LOG_NEQ;
		case I_LOG_NEQ:	return I_LOG_EQ;
		case I_LOG_GT:	return I_LOG_LEQ;
		case I_LOG_GEQ:	return I_LOG_LT;

		default: // unreachable
			parser_log(Error{"UNREACHABLE"}, Log_Level::CRITICAL);
			return I_BR;
		}
	}

	inline static bool
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

	inline static size_t
	make_label_id()
	{
		static size_t label_id = 0;
		return label_id++;
	}

	inline static void
	logical_and(Backend self, Instruction ins)
	{
		Operand br = { .loc = Operand::LBL };

		auto label_id = make_label_id();
		Label end_if  = { .type = Label::END_IF,  .id = label_id };
		Label end_all = { .type = Label::END_ALL, .id = label_id };

		// bz $end_if, op1
		// bz $end_if, op2
		br.label = end_if;
		instruction(self, I_BZ, br, ins.src1);
		instruction(self, I_BZ, br, ins.src2);

		// mov dst, 1
		instruction(self, I_MOV, ins.dst, OPR_ONE);

		// br $end
		br.label = end_all;
		instruction(self, I_BR, br);

		// $end_if: mov dst, 0
		instruction(self, end_if, I_MOV, ins.dst, OPR_ZERO);

		// $end_all
		instruction(self, end_all, I_NOP);
	}

	inline static void
	logical_or(Backend self, Instruction ins)
	{
		// a = b || c
		// a = !(!b && !c)
		Operand br = { .loc = Operand::LBL };

		auto label_id = make_label_id();
		Label end_if  = { .type = Label::END_IF,  .id = label_id };
		Label end_all = { .type = Label::END_ALL, .id = label_id };

		// bnz $end_if, op1
		// bnz $end_if, op2
		br.label = end_if;
		instruction(self, I_BNZ, br, ins.src1);
		instruction(self, I_BNZ, br, ins.src2);

		// mov dst, 0
		instruction(self, I_MOV, ins.dst, OPR_ZERO);

		// br $end
		br.label = end_all;
		instruction(self, I_BR, br);

		// $end_if: mov dst, 1
		instruction(self, end_if, I_MOV, ins.dst, OPR_ONE);

		// $end_all
		instruction(self, end_all, I_NOP);
	}

	inline static void
	logical_not(Backend self, Instruction ins)
	{
		Operand br = { .loc = Operand::LBL };

		auto label_id = make_label_id();
		Label end_if  = { .type = Label::END_IF,  .id = label_id };
		Label end_all = { .type = Label::END_ALL, .id = label_id };

		// bz $end_if, op
		br.label = end_if;
		instruction(self, I_BZ, br, ins.src1);

		// mov dst, 1
		instruction(self, I_MOV, ins.dst, OPR_ONE);

		// br $end
		br.label = end_all;
		instruction(self, I_BR, br);

		// $end_if: mov dst, 0
		instruction(self, end_if, I_MOV, ins.dst, OPR_ZERO);

		// $end_all
		instruction(self, end_all, I_NOP);
	}

	inline static void
	compare(Backend self, Instruction ins)
	{
		Operand br = { .loc = Operand::LBL };

		auto label_id = make_label_id();
		Label end_if  = { .type = Label::END_IF,  .id = label_id };
		Label end_all = { .type = Label::END_ALL, .id = label_id };

		// b_inv $end_if, op1, op2
		br.label = end_if;
		instruction(self, invert_op(ins.op), br, ins.src1, ins.src2);

		// mov dst, 1
		instruction(self, I_MOV, ins.dst, OPR_ONE);

		// br $end
		br.label = end_all;
		instruction(self, I_BR, br);

		// $end_if: mov dst, 0
		instruction(self, end_if, I_MOV, ins.dst, OPR_ZERO);

		// $end_all
		instruction(self, end_all, I_NOP);
	}

	inline static void
	assign(Backend self, INSTRUCTION_OP op, Operand left, Operand right)
	{
		auto src = right;
		if (op == I_MOV)
		{
			if (operand_is_register(right) == false)
			{
				// Value needs to be passed
				src = first_available_register(self);
				instruction(self, I_MOV, src, right);
			}
		}
		else
		{
			// Move into register
			src = first_available_register(self);
			instruction(self, I_LD, src, left);
			instruction(self, op, src, src, right);
		}

		// Write result
		instruction(self, I_ST, left, src);

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
		// Begin a new offset/code path
		self->stack_offset.push(0);

		self->reg[Operand::R0] = { .id = Operand::R0 };
		self->reg[Operand::R1] = { .id = Operand::R1 };
		self->reg[Operand::R2] = { .id = Operand::R2 };
		self->reg[Operand::R3] = { .id = Operand::R3 };
		self->reg[Operand::R4] = { .id = Operand::R4 };
	}


	void
	backend_write(Backend self)
	{
		for (const auto &ins : self->program)
			std::cout << std::format("{}\n", ins);
	}

	void
	backend_decl(Backend self, const Symbol *sym)
	{
		uint64_t word_count = std::max(1ui64, sym->type.array);

		// SP - word_count
		self->stack_offset.top() += word_count;
		self->variables[sym] = { .loc = Operand::MEM, .offset = self->stack_offset.top() };

		Operand stack_offset = { .loc = Operand::MEM, .offset = word_count };
		instruction(self, I_SUB, OPR_SP, stack_offset);
	}

	void
	backend_decl_expr(Backend self, const Symbol *sym, const Parse_Unit &right)
	{
		backend_decl(self, sym);
		backend_assign(self, I_MOV, sym, right);
	}

	Operand
	backend_lit(Backend self, Literal *lit)
	{
		return { .loc = Operand::IMM, .size = 1, .value = lit->value };
	}

	Operand
	backend_sym(Backend self, const Symbol *sym)
	{
		return self->variables[sym];
	}

	Operand
	backend_array_access(Backend self, const Symbol *sym, const Parse_Unit &right)
	{
		auto opr = self->variables[sym];
		opr.offset -= right.opr.value;
		return opr;
	}

	void
	backend_assign(Backend self, INSTRUCTION_OP op, const Symbol *sym, const Parse_Unit &right)
	{
		assign(self, op, self->variables[sym], right.opr);
	}

	void
	backend_array_assign(Backend self, INSTRUCTION_OP op, const Parse_Unit &left, const Parse_Unit &right)
	{
		assign(self, op, left.opr, right.opr);
	}

	Operand
	backend_binary(Backend self, INSTRUCTION_OP op, const Parse_Unit &left, const Parse_Unit &right)
	{
		// Collect operands
		auto opr1 = left.opr;
		if (opr1.loc == Operand::MEM)
		{
			auto reg = first_available_register(self);
			instruction(self, I_LD, reg, left.opr);
			opr1 = reg;

			// Don't use for other op
			self->reg[opr1.loc].is_used = true;
		}

		auto opr2 = right.opr;
		if (opr2.loc == Operand::MEM)
		{
			auto reg = first_available_register(self);
			instruction(self, I_LD, reg, right.opr);
			opr2 = reg;
		}

		auto dst = first_available_register(self);
		self->reg[dst.loc].is_used = true;

		// TODO: Add locations for operands
		if (op_is_logical(op) == false)
		{
			instruction(self, op, dst, opr1, opr2);
		}
		else
		{
			Instruction ins = { .op = op, .dst = dst, .src1 = opr1, .src2 = opr2 };
			if (op == I_LOG_AND)
			{
				logical_and(self, ins);
			}
			else if (op == I_LOG_OR)
			{
				logical_or(self, ins);
			}
			else
			{
				compare(self, ins);
			}
		}

		if (operand_is_register(opr1))
			self->reg[opr1.loc].is_used = false;

		return dst;
	}

	Operand
	backend_unary(Backend self, INSTRUCTION_OP op, const Parse_Unit &right)
	{
		auto opr = right.opr;
		if (opr.loc == Operand::MEM)
		{
			auto reg = first_available_register(self);
			instruction(self, I_LD, reg, right.opr);
			opr = reg;
		}

		auto dst = first_available_register(self);
		self->reg[dst.loc].is_used = true;

		if (op_is_logical(op) == false)
		{
			instruction(self, op, dst, opr);
		}
		else
		{
			Instruction ins = { .op = I_LOG_NOT, .dst = dst, .src1 = opr };
			logical_not(self, ins);
		}

		return dst;
	}

	Label
	backend_gen_condition(Backend self, AST ast, Label end_block)
	{
		Operand br_end = { .loc = Operand::LBL, .label = end_block };

		switch (ast.kind)
		{
		case AST::NIL: // ELSE branch
			break;

		case AST::LITERAL: case AST::SYMBOL: case AST::PROC_CALL: case AST::ARRAY_ACCESS: {
			auto opr = backend_generate(self, ast);
			if (opr.loc == Operand::MEM)
			{
				auto reg = first_available_register(self);
				instruction(self, I_LD, reg, opr);
				opr = reg;
			}

			instruction(self, I_BZ, br_end, opr);
			break;
		}

		case AST::BINARY: {
			auto bin = ast.as_binary;
			auto op = (INSTRUCTION_OP)bin->kind;

			// cond != 0
			if (op_is_logical(op) == false)
			{
				auto opr = backend_generate(self, ast);
				instruction(self, I_BZ, br_end, opr);
			}
			else if (op == I_LOG_AND)
			{
				auto left = backend_generate(self, bin->left);
				if (left.loc == Operand::MEM)
				{
					auto reg = first_available_register(self);
					instruction(self, I_LD, reg, left);
					left = reg;
				}
				instruction(self, I_BZ, br_end, left);

				auto right = backend_generate(self, bin->right);
				if (right.loc == Operand::MEM)
				{
					auto reg = first_available_register(self);
					instruction(self, I_LD, reg, right);
					right = reg;
				}
				instruction(self, I_BZ, br_end, right);
			}
			else if (op == I_LOG_OR)
			{
				Label lbl_true = { .type = Label::LABEL, .id = make_label_id() };
				Operand br_true = { .loc = Operand::LBL, .label = lbl_true };

				auto left = backend_generate(self, bin->left);
				if (left.loc == Operand::MEM)
				{
					auto reg = first_available_register(self);
					instruction(self, I_LD, reg, left);
					left = reg;
				}
				instruction(self, I_BNZ, br_true, left);

				auto right = backend_generate(self, bin->right);
				if (right.loc == Operand::MEM)
				{
					auto reg = first_available_register(self);
					instruction(self, I_LD, reg, right);
					right = reg;
				}
				instruction(self, I_BNZ, br_true, right);

				instruction(self, I_BR, br_end);
				instruction(self, lbl_true);
			}
			else
			{
				auto left = backend_generate(self, bin->left);
				if (left.loc == Operand::MEM)
				{
					auto reg = first_available_register(self);
					instruction(self, I_LD, reg, left);
					left = reg;

					self->reg[left.loc].is_used = true;
				}

				auto right = backend_generate(self, bin->right);
				if (right.loc == Operand::MEM)
				{
					auto reg = first_available_register(self);
					instruction(self, I_LD, reg, right);
					right = reg;
				}

				instruction(self, invert_op(op), br_end, left, right);
			}
			break;
		}

		case AST::UNARY: {
			auto uny = ast.as_unary;
			auto op = (INSTRUCTION_OP)uny->kind;

			// cond != 0
			if (op_is_logical(op) == false)
			{
				auto opr = backend_generate(self, ast);
				if (opr.loc == Operand::MEM)
				{
					auto reg = first_available_register(self);
					instruction(self, I_LD, reg, opr);
					opr = reg;
				}
				instruction(self, I_BZ, br_end, opr);
			}
			else // NOT
			{
				auto opr = backend_generate(self, uny->right);
				if (opr.loc == Operand::MEM)
				{
					auto reg = first_available_register(self);
					instruction(self, I_LD, reg, opr);
					opr = reg;
				}
				instruction(self, I_BNZ, br_end, opr);
			}
			break;
		}

		case AST::SWITCH_CASE: {
			Label lbl_true = { .type = Label::LABEL, .id = make_label_id() };
			Operand br_true = { .loc = Operand::LBL, .label = lbl_true };

			auto swc = ast.as_case;
			// Fetch the expression
			auto expr = backend_generate(self, swc->expr);
			if (expr.loc == Operand::MEM)
			{
				auto reg = first_available_register(self);
				instruction(self, I_LD, reg, expr);
				expr = reg;
			}
			// Go to true if any value matches
			for (auto lit: swc->group)
			{
				instruction(self, I_LOG_EQ, br_true, expr, backend_lit(self, lit));
			}

			instruction(self, I_BR, br_end);
			instruction(self, lbl_true);
			break;
		}

		default:
			break;
		}

		return end_block;
	}

	Operand
	backend_generate(Backend self, AST ast)
	{
		switch (ast.kind)
		{
		case AST::LITERAL:	return backend_lit(self, ast.as_lit);
		case AST::SYMBOL:	return backend_sym(self, ast.as_sym);

		case AST::PROC_CALL: return {}; // TODO: IMPLEMENT

		case AST::ARRAY_ACCESS: {
			auto arr = ast.as_arr_access;
			auto opr = backend_sym(self, arr->sym);
			auto offset = backend_generate(self, arr->index);
			opr.offset -= offset.value;
			return opr;
		}

		case AST::BINARY: {
			auto bin = ast.as_binary;
			auto left = backend_generate(self, bin->left);
			auto right = backend_generate(self, bin->right);
			return backend_binary(self, (INSTRUCTION_OP)bin->kind, Parse_Unit{.opr = left}, Parse_Unit{.opr = right});
		}

		case AST::UNARY: {
			auto uny = ast.as_unary;
			auto right = backend_generate(self, uny->right);
			return backend_unary(self, (INSTRUCTION_OP)uny->kind, Parse_Unit{.opr = right});
		}

		case AST::ASSIGN: {
			auto as = ast.as_assign;
			auto dst = backend_generate(self, as->dst);
			auto expr = backend_generate(self, as->expr);
			assign(self, (INSTRUCTION_OP)as->kind, dst, expr);
			return {};
		}

		case AST::DECL: {
			auto decl = ast.as_decl;

			if (decl->expr.kind == AST::NIL)
			{
				backend_decl(self, decl->sym);
			}
			else
			{
				auto expr = backend_generate(self, decl->expr);
				backend_decl_expr(self, decl->sym, Parse_Unit{.opr = expr});
			}

			return {};
		}

		case AST::DECL_PROC: 	return {}; // TODO: Implement

		case AST::IF_COND: {
			Label end_all = { .type = Label::END_ALL, .id = make_label_id() };

			for (auto ifc = ast.as_if; ifc != nullptr; ifc = ifc->next)
			{
				Label end_if = { .type = Label::END_IF, .id = make_label_id() };
				backend_gen_condition(self, ifc->cond, end_if);
				clear_registers(self);

				for (auto stmt: ifc->block->stmts)
					backend_generate(self, stmt);

				Operand to_end_all = { .loc = Operand::LBL, .label = end_all };
				instruction(self, I_BR, to_end_all);
				instruction(self, end_if);
			}

			instruction(self, end_all);
			return {};
		}

		case AST::SWITCH: {
			Label end_switch = { .type = Label::END_SWITCH, .id = make_label_id() };
			AST case_ast = { .kind = AST::SWITCH_CASE };

			auto sw = ast.as_switch;

			for (auto &swc: sw->cases)
			{
				Label end_case = { .type = Label::END_CASE, .id = make_label_id() };
				case_ast.as_case = swc;
				backend_gen_condition(self, case_ast, end_case);
				clear_registers(self);

				for (auto stmt: swc->block->stmts)
					backend_generate(self, stmt);

				Operand to_end_switch = { .loc = Operand::LBL, .label = end_switch };
				instruction(self, I_BR, to_end_switch);
				instruction(self, end_case);
			}
			if (sw->case_default)
			{
				for (auto stmt: sw->case_default->stmts)
					backend_generate(self, stmt);
			}

			instruction(self, end_switch);
			return {};
		}

		case AST::WHILE: {
			auto wh = ast.as_while;

			Label begin_while = { .type = Label::WHILE,     .id = make_label_id() };
			Label end_while   = { .type = Label::END_WHILE, .id = make_label_id() };

			instruction(self, begin_while);

			backend_gen_condition(self, wh->cond, end_while);
			clear_registers(self);

			for (auto stmt: wh->block->stmts)
				backend_generate(self, stmt);

			Operand to_begin_while = { .loc = Operand::LBL, .label = begin_while };
			instruction(self, I_BR, to_begin_while);

			instruction(self, end_while);
			return {};
		}


		case AST::DO_WHILE: {
			auto do_wh = ast.as_do_while;

			Label begin_do_while = { .type = Label::WHILE,     .id = make_label_id() };
			Label end_do_while   = { .type = Label::END_WHILE, .id = make_label_id() };

			instruction(self, begin_do_while);
			for (auto stmt: do_wh->block->stmts)
				backend_generate(self, stmt);

			backend_gen_condition(self, do_wh->cond, end_do_while);
			clear_registers(self);

			Operand to_begin_do_while = { .loc = Operand::LBL, .label = begin_do_while };
			instruction(self, I_BR, to_begin_do_while);

			instruction(self, end_do_while);
			return {};
		}
		case AST::FOR:  		return {}; // TODO: Implement

		case AST::BLOCK: {
			for (auto stmt: ast.as_block->stmts)
				backend_generate(self, stmt);

			return {};
		}

		default:
			return {};
		}
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
		case Operand::SP: return format_to(ctx.out(), "SP");
		case Operand::BP: return format_to(ctx.out(), "BP");

		case Operand::IMM: return format_to(ctx.out(), "#{}", opr.value);
		case Operand::MEM: return format_to(ctx.out(), "-{}(BP)", opr.offset);
		case Operand::LBL: return format_to(ctx.out(), "{}", opr.label);

		default: return ctx.out();
		}
	}
};

template <>
struct std::formatter<s22::Label> : std::formatter<std::string>
{
	auto
	format(s22::Label label, format_context &ctx)
	{
		using namespace s22;
		if (label.type == Label::NONE)
			return ctx.out();

		const char *lbl = nullptr;
		switch (label.type)
		{
		case Label::NONE: 		return ctx.out();
		case Label::LABEL: 		lbl = "LABEL"; 		break;

		case Label::END_IF: 	lbl = "END_IF";		break;
		case Label::END_ELSEIF: lbl = "END_ELSEIF";	break;
		case Label::END_ALL: 	lbl = "END_ALL";	break;
		case Label::END_CASE: 	lbl = "END_CASE";	break;
		case Label::END_SWITCH: lbl = "END_SWITCH";	break;

		case Label::FOR: 		lbl = "FOR";		break;
		case Label::END_FOR: 	lbl = "END_FOR";	break;

		case Label::WHILE: 		lbl = "WHILE";		break;
		case Label::END_WHILE: 	lbl = "END_WHILE";	break;

		case Label::PROC: 		lbl = "PROC";		break;
		}

		return format_to(ctx.out(), "{}#{}", lbl, label.id);
	}
};

template <>
struct std::formatter<s22::Instruction> : std::formatter<std::string>
{
	auto
	format(s22::Instruction ins, format_context &ctx)
	{
		if (ins.label.type != s22::Label::NONE)
			format_to(ctx.out(), "{}: ", ins.label);

		using namespace s22;
		switch (ins.op)
		{
		case I_NOP: 	return format_to(ctx.out(), "NOP");

		case I_LD: 		format_to(ctx.out(), "LD"); 	break;
		case I_ST: 		format_to(ctx.out(), "ST"); 	break;
		case I_MOV:		format_to(ctx.out(), "MOV"); 	break;

		// Arithmetic
		case I_ADD:		format_to(ctx.out(), "ADD");	break;
		case I_SUB:		format_to(ctx.out(), "SUB");	break;
		case I_MUL:		format_to(ctx.out(), "MUL");	break;
		case I_DIV:		format_to(ctx.out(), "DIV");	break;
		case I_MOD:		format_to(ctx.out(), "MOD");	break;
		case I_AND:		format_to(ctx.out(), "AND");	break;
		case I_OR:		format_to(ctx.out(), "OR");		break;
		case I_XOR:		format_to(ctx.out(), "XOR");	break;
		case I_SHL:		format_to(ctx.out(), "SHL");	break;
		case I_SHR:		format_to(ctx.out(), "SHR");	break;
		case I_NEG:		format_to(ctx.out(), "NEG");	break;
		case I_INV:		format_to(ctx.out(), "INV");	break;

		// Logical
		case I_LOG_LT:	format_to(ctx.out(), "BLT");	break;
		case I_LOG_LEQ:	format_to(ctx.out(), "BLE");	break;
		case I_LOG_EQ:	format_to(ctx.out(), "BEQ");	break;
		case I_LOG_NEQ:	format_to(ctx.out(), "BNE");	break;
		case I_LOG_GEQ:	format_to(ctx.out(), "BGE");	break;
		case I_LOG_GT:	format_to(ctx.out(), "BGT");	break;

		// Branch
		case I_BR:		format_to(ctx.out(), "BR"); 	break;
		case I_BZ:		format_to(ctx.out(), "BZ"); 	break;
		case I_BNZ:		format_to(ctx.out(), "BNZ"); 	break;
		default: 		return ctx.out();
		}

		if (ins.operand_count >= 1)
			format_to(ctx.out(), " {}", ins.dst);

		if (ins.operand_count >= 2)
			format_to(ctx.out(), ", {}", ins.src1);

		if (ins.operand_count == 3)
			format_to(ctx.out(), ", {}", ins.src2);

		return ctx.out();
	}
};
