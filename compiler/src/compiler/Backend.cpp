#include "compiler/Backend.h"
#include "compiler/Symbol.h"
#include "compiler/Parser.h"

#include <unordered_map>
#include <iostream>
#include <stack>

namespace s22
{
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
		inline Operand(OPERAND_LOCATION l) 		{ *this = {}; loc = l; }
		inline Operand(int v)					{ *this = {}; loc = OP_IMM; value = (uint64_t)v; }
		inline Operand(uint64_t v)				{ *this = {}; loc = OP_IMM; value = v; }
		inline Operand(Label lbl)				{ *this = {}; loc = OP_LBL; label = lbl; }
		inline Operand(Memory_Address adr)		{ *this = {}; loc = OP_MEM; address = adr; }

		union // immediate value/memory offset
		{
			uint64_t value;
			Label label;
			Memory_Address address;
		};
	};

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

	struct IBackend
	{
		bool is_used[RDX + 1];

		std::unordered_map<const Symbol*, Operand> variables;
		std::stack<size_t> stack_offset;

		std::vector<Instruction> program;
	};

	Operand
	backend_generate(Backend self, AST ast);

	template <typename... TArgs>
	inline static void
	instruction(Backend self, INSTRUCTION_OP op, TArgs &&...args)
	{
		Instruction ins = { op, Operand{std::forward<TArgs>(args)}... };
		ins.operand_count = sizeof...(args);
		self->program.push_back(ins);
	}

	template <typename... TArgs>
	inline static void
	instruction(Backend self, Label label, INSTRUCTION_OP op, TArgs &&...args)
	{
		Instruction ins = { op, {std::forward<TArgs>(args)}... };
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
		case RAX: case RBX: case RCX: case RDX: case RSP: case RBP:
			return true;
		default:
			return false;
		}
	}

	inline static bool
	is_mem_to_mem(Operand src, Operand dst)
	{
		return src.loc == OP_MEM && dst.loc == OP_MEM;
	}

	inline static Operand
	first_available_register(Backend self)
	{
		for (auto reg: { RAX, RBX, RCX, RDX })
		{
			if (self->is_used[reg] == false)
				return Operand{reg};
		}

		// TODO: Handle no available registers, overwrite for now
		return Operand{RAX};
	}

	inline static std::pair<Operand, Operand>
	prepare_operands(Backend self, Operand op1, Operand op2)
	{
		if (is_mem_to_mem(op1, op2) == false)
			return { op1, op2 };
		
		auto reg = first_available_register(self);
		instruction(self, I_MOV, reg, op2);

		return { op1, reg };
	}

	inline static void
	clear_registers(Backend self)
	{
		self->is_used[RAX] = false;
		self->is_used[RBX] = false;
		self->is_used[RCX] = false;
		self->is_used[RDX] = false;
	}

	inline static void
	use_reg(Backend self, Operand reg)
	{
		self->is_used[reg.loc] = true;
	}

	inline static void
	clear_reg(Backend self, Operand reg)
	{
		self->is_used[reg.loc] = false;
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
		auto label_id = make_label_id();
		Label end_if  = { .type = Label::END_IF,  .id = label_id };
		Label end_all = { .type = Label::END_ALL, .id = label_id };

		// bz $end_if, op1
		// bz $end_if, op2
		instruction(self, I_BZ, end_if, ins.src1);
		instruction(self, I_BZ, end_if, ins.src2);

		// mov dst, 1
		instruction(self, I_MOV, ins.dst, 1);

		// br $end
		instruction(self, I_BR, end_all);

		// $end_if: mov dst, 0
		instruction(self, end_if, I_MOV, ins.dst, 0);

		// $end_all:
		instruction(self, end_all);
	}

	inline static void
	logical_or(Backend self, Instruction ins)
	{
		// a = b || c
		// a = !(!b && !c)

		auto label_id = make_label_id();
		Label end_if  = { .type = Label::END_IF,  .id = label_id };
		Label end_all = { .type = Label::END_ALL, .id = label_id };

		// bnz $end_if, op1
		// bnz $end_if, op2
		instruction(self, I_BNZ, end_if, ins.src1);
		instruction(self, I_BNZ, end_if, ins.src2);

		// mov dst, 0
		instruction(self, I_MOV, ins.dst, 0);

		// br $end
		instruction(self, I_BR, end_all);

		// $end_if: mov dst, 1
		instruction(self, end_if, I_MOV, ins.dst, 1);

		// $end_all:
		instruction(self, end_all);
	}

	inline static void
	logical_not(Backend self, Instruction ins)
	{
		auto label_id = make_label_id();
		Label end_if  = { .type = Label::END_IF,  .id = label_id };
		Label end_all = { .type = Label::END_ALL, .id = label_id };

		// bz $end_if, op
		instruction(self, I_BZ, end_if, ins.src1);

		// mov dst, 1
		instruction(self, I_MOV, ins.dst, 1);

		// br $end
		instruction(self, I_BR, end_all);

		// $end_if: mov dst, 0
		instruction(self, end_if, I_MOV, ins.dst, 0);

		// $end_all:
		instruction(self, end_all);
	}

	inline static void
	compare(Backend self, Instruction ins)
	{
		auto label_id = make_label_id();
		Label end_if  = { .type = Label::END_IF,  .id = label_id };
		Label end_all = { .type = Label::END_ALL, .id = label_id };

		// b_inv $end_if, op1, op2
		instruction(self, invert_op(ins.op), end_if, ins.src1, ins.src2);

		// mov dst, 1
		instruction(self, I_MOV, ins.dst, 1);

		// br $end
		instruction(self, I_BR, end_all);

		// $end_if: mov dst, 0
		instruction(self, end_if, I_MOV, ins.dst, 0);

		// $end_all:
		instruction(self, end_all);
	}

	inline static void
	assign(Backend self, INSTRUCTION_OP op, Operand left, Operand right)
	{
		auto [dst, src] = prepare_operands(self, left, right);
		instruction(self, op, dst, src);
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
	}


	void
	backend_write(Backend self)
	{
		for (const auto &ins : self->program)
			std::cout << std::format("{}\n", ins);
	}

	void
	backend_compile(Backend self, AST ast)
	{
		backend_generate(self, ast);
	}

	void
	backend_decl(Backend self, const Symbol *sym)
	{
		uint64_t word_count = std::max(1ui64, sym->type.array);

		// SP - word_count
		self->stack_offset.top() += word_count;

		Memory_Address adr = { .base = RBP, .offset = -(int)self->stack_offset.top() };
		self->variables[sym] = { adr };

		// TODO: One shot stack pointer subtraction
	}

	void
	backend_decl_expr(Backend self, const Symbol *sym, Operand right)
	{
		backend_decl(self, sym);
		assign(self, I_MOV, self->variables[sym], right);
	}

	Operand
	backend_lit(Backend self, Literal *lit)
	{
		Operand opr = { lit->value };
		opr.size = 1;
		return opr;
	}

	Operand
	backend_sym(Backend self, const Symbol *sym)
	{
		return self->variables[sym];
	}

	Operand
	backend_array_access(Backend self, const Array_Access *arr)
	{
		auto opr = backend_sym(self, arr->sym);

		auto idx = backend_generate(self, arr->index);
		if (idx.loc == OP_IMM)
		{
			opr.address.offset += idx.value;
		}
		else if (operand_is_register(idx))
		{
			opr.address.index = idx.loc;
		}
		else
		{
			instruction(self, I_MOV, RSI, idx);
			opr.address.index = RSI;
		}

		return opr;
	}

	Operand
	backend_binary(Backend self, INSTRUCTION_OP op, Operand left, Operand right)
	{
		// Collect operands
		auto [opr1, opr2] = prepare_operands(self, left, right);

		auto dst = first_available_register(self);
		use_reg(self, dst);

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
			clear_reg(self, opr1);

		return dst;
	}

	Operand
	backend_unary(Backend self, INSTRUCTION_OP op, Operand right)
	{
		auto dst = first_available_register(self);
		use_reg(self, dst);

		if (op_is_logical(op) == false)
		{
			instruction(self, op, dst, right);
		}
		else
		{
			Instruction ins = { .op = I_LOG_NOT, .dst = dst, .src1 = right };
			logical_not(self, ins);
		}

		return dst;
	}

	Label
	backend_gen_condition(Backend self, AST ast, Label end_block)
	{
		switch (ast.kind)
		{
		case AST::NIL: // ELSE branch
			break;

		case AST::LITERAL: case AST::SYMBOL: case AST::PROC_CALL: case AST::ARRAY_ACCESS: {
			auto opr = backend_generate(self, ast);
			instruction(self, I_BZ, end_block, opr);
			break;
		}

		case AST::BINARY: {
			auto bin = ast.as_binary;
			auto op = (INSTRUCTION_OP)bin->kind;

			// cond != 0
			if (op_is_logical(op) == false)
			{
				auto opr = backend_generate(self, ast);
				instruction(self, I_BZ, end_block, opr);
			}
			else if (op == I_LOG_AND)
			{
				auto left = backend_generate(self, bin->left);
				instruction(self, I_BZ, end_block, left);

				auto right = backend_generate(self, bin->right);
				instruction(self, I_BZ, end_block, right);
			}
			else if (op == I_LOG_OR)
			{
				Label lbl_true = { .type = Label::LABEL, .id = make_label_id() };

				auto left = backend_generate(self, bin->left);
				instruction(self, I_BNZ, lbl_true, left);

				auto right = backend_generate(self, bin->right);
				instruction(self, I_BNZ, lbl_true, right);

				instruction(self, I_BR, end_block);
				instruction(self, lbl_true);
			}
			else
			{
				auto left = backend_generate(self, bin->left);
				auto right = backend_generate(self, bin->right);

				instruction(self, invert_op(op), end_block, left, right);
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
				instruction(self, I_BZ, end_block, opr);
			}
			else // NOT
			{
				auto opr = backend_generate(self, uny->right);
				instruction(self, I_BNZ, end_block, opr);
			}
			break;
		}

		case AST::SWITCH_CASE: {
			auto swc = ast.as_case;

			// Fetch the expression
			auto expr = backend_generate(self, swc->expr);

			// Go to true if any value matches
			Label lbl_true = { .type = Label::LABEL, .id = make_label_id() };
			for (auto lit: swc->group)
			{
				instruction(self, I_LOG_EQ, lbl_true, expr, backend_lit(self, lit));
			}

			instruction(self, I_BR, end_block);
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

		case AST::PROC_CALL: 		return {}; // TODO: IMPLEMENT
		case AST::ARRAY_ACCESS: 	return backend_array_access(self, ast.as_arr_access);
		case AST::BINARY: {
			auto bin = ast.as_binary;
			auto left = backend_generate(self, bin->left);
			auto right = backend_generate(self, bin->right);
			return backend_binary(self, (INSTRUCTION_OP)bin->kind, left, right);
		}

		case AST::UNARY: {
			auto uny = ast.as_unary;
			auto right = backend_generate(self, uny->right);
			return backend_unary(self, (INSTRUCTION_OP)uny->kind, right);
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
				backend_decl_expr(self, decl->sym, expr);
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

				instruction(self, I_BR, end_all);
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

				instruction(self, I_BR, end_switch);
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
			Label end_while   = { .type = Label::END_WHILE, .id = begin_while.id };

			instruction(self, begin_while);

			backend_gen_condition(self, wh->cond, end_while);
			clear_registers(self);

			for (auto stmt: wh->block->stmts)
				backend_generate(self, stmt);

			instruction(self, I_BR, begin_while);

			instruction(self, end_while);
			return {};
		}

		case AST::DO_WHILE: {
			auto do_wh = ast.as_do_while;

			Label begin_do_while = { .type = Label::WHILE,     .id = make_label_id() };
			Label end_do_while   = { .type = Label::END_WHILE, .id = begin_do_while.id };

			instruction(self, begin_do_while);
			for (auto stmt: do_wh->block->stmts)
				backend_generate(self, stmt);

			backend_gen_condition(self, do_wh->cond, end_do_while);
			clear_registers(self);

			instruction(self, I_BR, begin_do_while);

			instruction(self, end_do_while);
			return {};
		}

		case AST::FOR: {
			auto loop = ast.as_for;

			Label begin_for = { .type = Label::FOR,     .id = make_label_id() };
			Label end_for   = { .type = Label::END_FOR, .id = begin_for.id };

			// init
			backend_generate(self, loop->init);

			instruction(self, begin_for);
			backend_gen_condition(self, loop->cond, end_for);
			clear_registers(self);

			for (auto stmt: loop->block->stmts)
				backend_generate(self, stmt);

			// post
			backend_generate(self, loop->post);

			instruction(self, I_BR, begin_for);

			instruction(self, end_for);
			return {};
		}

		case AST::BLOCK: {
			auto blk = ast.as_block;
			if (blk->stack_offset != 0)
			{
				instruction(self, I_SUB, RSP, blk->stack_offset);

				for (auto stmt: blk->stmts)
					backend_generate(self, stmt);

				instruction(self, I_ADD, RSP, blk->stack_offset);
			}
			else
			{
				for (auto stmt : blk->stmts)
					backend_generate(self, stmt);
			}

			return {};
		}

		default:
			return {};
		}
	}

}

template <>
struct std::formatter<s22::OPERAND_LOCATION> : std::formatter<std::string>
{
	auto
	format(s22::OPERAND_LOCATION loc, format_context &ctx)
	{
		using namespace s22;

		switch (loc)
		{
		case RAX: return format_to(ctx.out(), "RAX");
		case RBX: return format_to(ctx.out(), "RBX");
		case RCX: return format_to(ctx.out(), "RCX");
		case RDX: return format_to(ctx.out(), "RDX");
		case RSP: return format_to(ctx.out(), "RSP");
		case RBP: return format_to(ctx.out(), "RBP");
		case RSI: return format_to(ctx.out(), "RSI");
		case RDI: return format_to(ctx.out(), "RDI");

		default: return ctx.out();
		};
	}
};

template <>
struct std::formatter<s22::Operand> : std::formatter<std::string>
{
	auto
	format(s22::Operand opr, format_context &ctx)
	{
		using namespace s22;
		switch (opr.loc)
		{
		case OP_IMM: return format_to(ctx.out(), "{}", opr.value);

		case OP_MEM: {
			format_to(ctx.out(), "[{}", opr.address.base);
			if (opr.address.index != OP_NIL)
			{
				format_to(ctx.out(), "+{}", opr.address.index);
			}
			if (opr.address.offset != 0)
			{
				format_to(ctx.out(), "{:+}", opr.address.offset);
			}
			return format_to(ctx.out(), "]");
		}

		case OP_LBL: return format_to(ctx.out(), "{}", opr.label);

		default: return format_to(ctx.out(), "{}", opr.loc);
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

		const char *lbl = nullptr;
		switch (label.type)
		{
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

		default: return ctx.out();
		}

		return format_to(ctx.out(), "{}${}", lbl, label.id);
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
		case I_NOP: 	return ctx.out();
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
