#include "compiler/Backend.h"
#include "compiler/Symbol.h"
#include "compiler/Parser.h"

#include <unordered_map>
#include <stack>

namespace s22
{
	struct Label
	{
		enum TYPE
		{
			NONE,

			LABEL,

			OR_TRUE, END_OR,
			AND_FALSE, END_AND,
			NOT_TRUE, END_NOT,
			COND_FALSE, END_COND,

			END_IF, END_ELSEIF, END_ALL,
			CASE, END_CASE, END_SWITCH,

			FOR, END_FOR,
			WHILE, END_WHILE,

			PROC, END_PROC,
		};
		TYPE type;
		uint64_t id; // used for standard labels
		String text; // used for proc labels
	};

	// Temporary register
	// Memory address
	// Immediate value
	// Condition
	enum OPERAND_LOCATION
	{
		OP_NIL,				// NIL value
		RAX, RBX, RCX, RDX, // general purpose registers
		RSP, RBP, RSI, RDI, // reserved registers
		OP_IMM,				// immediate value
		OP_MEM,				// memory address
		OP_MEM_RANGE,		// memory range
		OP_LBL,				// label
		OP_COND,			// condition
	};

	struct Memory_Address
	{
		OPERAND_LOCATION base;	// [base_register + index_register + offset]
		OPERAND_LOCATION index;
		int offset;
	};

	struct Memory_Range
	{
		Memory_Address start;
		size_t size;
	};

	struct Operand
	{
		OPERAND_LOCATION loc;
		size_t size;

		inline Operand() = default;
		inline Operand(OPERAND_LOCATION l)		{ *this = {}; size = 1; loc = l; }
		inline Operand(int v)					{ *this = {}; size = 1; loc = OP_IMM; value = (uint64_t)v; }
		inline Operand(uint64_t v)				{ *this = {}; size = 1; loc = OP_IMM; value = v; }
		inline Operand(Label lbl)				{ *this = {}; size = 1; loc = OP_LBL; label = lbl; }
		inline Operand(Memory_Address adr)		{ *this = {}; size = 1; loc = OP_MEM; address = adr; }
		inline Operand(Memory_Range r)			{ *this = {}; size = r.size; loc = OP_MEM_RANGE, range = r; }

		union // immediate value/memory offset
		{
			uint64_t value;
			Memory_Address address;
			Memory_Range range;			// Used for arrays
		};
		Label label;	// set when loc is a label
						// TODO: procedures carry both their label and their address here, change this
	};

	struct Instruction
	{
		INSTRUCTION_OP op;
		Operand dst, src1, src2;

		size_t operand_count;
		Label label; // adds a label to the instruction
	};

	struct IBackend
	{
		bool is_used[RDX + 1];
		
		std::unordered_map<const Symbol*, Operand> variables;	// maps symbol to memory locations
																// also maps procs to their labels/locations
		
		std::stack<size_t> stack_frame;		// carries how much space variables have used up to this point, within the current procedure
		std::vector<Instruction> program;	// compiled program
		size_t label_counter;				// used for label generation
	};

	Operand
	be_generate(Backend self, AST ast);

	template <typename... TArgs>
	inline static void
	be_instruction(Backend self, INSTRUCTION_OP op, TArgs &&...args) // Handles operand counts [0, 3]
	{
		Instruction ins = { op, Operand{std::forward<TArgs>(args)}... };
		ins.operand_count = sizeof...(args);
		self->program.push_back(ins);
	}

	inline static void
	be_label(Backend self, Label label)
	{
		Instruction ins = { .label = label };
		self->program.push_back(ins);
	}

	template <typename... TArgs>
	inline static void
	be_label_with_instruction(Backend self, Label label, INSTRUCTION_OP op, TArgs &&...args) // Handles operand counts [0, 3]
	{
		Instruction ins = { op, {std::forward<TArgs>(args)}... };
		ins.operand_count = sizeof...(args);
		ins.label = label;
		self->program.push_back(ins);
	}

	inline static bool
	op_is_logical(INSTRUCTION_OP op)
	{
		switch (op)
		{
		case I_LOG_LT: case I_LOG_LEQ: case I_LOG_EQ: case I_LOG_NEQ: case I_LOG_GT: case I_LOG_GEQ: case I_LOG_NOT:
		case I_LOG_AND: case I_LOG_OR:
			return true;
		default:
			return false;
		}
	}

	inline static INSTRUCTION_OP
	op_invert(INSTRUCTION_OP op)
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

		default:
			s22_unreachable_msg("unrecognized op");
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

	inline static Operand
	be_get_reg(Backend self)
	{
		// First available register
		for (auto reg: { RAX, RBX, RCX, RDX })
		{
			if (self->is_used[reg] == false)
				return Operand{reg};
		}

		// TODO: Handle no available registers, overwrite for now
		return Operand{RAX};
	}

	inline static std::pair<Operand, Operand>
	be_prepare_operands(Backend self, Operand op1, Operand op2)
	{
		// not memory-to-memory
		if (op1.loc != OP_MEM || op2.loc != OP_MEM)
			return { op1, op2 };

		// prepare op2
		auto reg = be_get_reg(self);
		be_instruction(self, I_MOV, reg, op2);

		return { op1, reg };
	}

	inline static void
	be_clear_reg_all(Backend self)
	{
		self->is_used[RAX] = false;
		self->is_used[RBX] = false;
		self->is_used[RCX] = false;
		self->is_used[RDX] = false;
	}

	inline static void
	be_use_reg(Backend self, Operand reg)
	{
		self->is_used[reg.loc] = true;
	}

	inline static void
	be_clear_reg(Backend self, Operand reg)
	{
		self->is_used[reg.loc] = false;
	}

	inline static size_t
	be_new_label_id(Backend self)
	{
		return self->label_counter++;
	}

	inline static void
	be_logical_and(Backend self, Instruction ins)
	{
		auto label_id = be_new_label_id(self);
		Label end_if  = { .type = Label::AND_FALSE, .id = label_id };
		Label end_all = { .type = Label::END_AND,   .id = label_id };

		// bz $end_if, op1
		// bz $end_if, op2
		be_instruction(self, I_BZ, end_if, ins.src1);
		be_instruction(self, I_BZ, end_if, ins.src2);

		// mov dst, 1
		be_instruction(self, I_MOV, ins.dst, 1);

		// br $end
		be_instruction(self, I_BR, end_all);

		// $end_if: mov dst, 0
		be_label_with_instruction(self, end_if, I_MOV, ins.dst, 0);

		// $end_all:
		be_label(self, end_all);
	}

	inline static void
	be_logical_or(Backend self, Instruction ins)
	{
		// a = b || c
		// a = !(!b && !c)
		auto label_id = be_new_label_id(self);
		Label end_if  = { .type = Label::OR_TRUE, .id = label_id };
		Label end_all = { .type = Label::END_OR,  .id = label_id };

		// bnz $end_if, op1
		// bnz $end_if, op2
		be_instruction(self, I_BNZ, end_if, ins.src1);
		be_instruction(self, I_BNZ, end_if, ins.src2);

		// mov dst, 0
		be_instruction(self, I_MOV, ins.dst, 0);

		// br $end
		be_instruction(self, I_BR, end_all);

		// $end_if: mov dst, 1
		be_label_with_instruction(self, end_if, I_MOV, ins.dst, 1);

		// $end_all:
		be_label(self, end_all);
	}

	inline static void
	be_logical_not(Backend self, Instruction ins)
	{
		auto label_id = be_new_label_id(self);
		Label end_if  = { .type = Label::NOT_TRUE, .id = label_id };
		Label end_all = { .type = Label::END_NOT,  .id = label_id };

		// bz $end_if, op
		be_instruction(self, I_BZ, end_if, ins.src1);

		// mov dst, 1
		be_instruction(self, I_MOV, ins.dst, 1);

		// br $end
		be_instruction(self, I_BR, end_all);

		// $end_if: mov dst, 0
		be_label_with_instruction(self, end_if, I_MOV, ins.dst, 0);

		// $end_all:
		be_label(self, end_all);
	}

	inline static void
	be_compare(Backend self, Instruction ins)
	{
		auto label_id = be_new_label_id(self);
		Label end_if  = { .type = Label::COND_FALSE, .id = label_id };
		Label end_all = { .type = Label::END_COND, 	 .id = label_id };

		// b_inv $end_if, op1, op2
		be_instruction(self, op_invert(ins.op), end_if, ins.src1, ins.src2);

		// mov dst, 1
		be_instruction(self, I_MOV, ins.dst, 1);

		// br $end
		be_instruction(self, I_BR, end_all);

		// $end_if: mov dst, 0
		be_label_with_instruction(self, end_if, I_MOV, ins.dst, 0);

		// $end_all:
		be_label(self, end_all);
	}

	inline static void
	be_assign(Backend self, INSTRUCTION_OP op, Operand left, Operand right)
	{
		if (left.loc == OP_MEM_RANGE) // compound assignment
		{
			s22_assert_msg(op == I_MOV, "compound assignment with an operation");
			s22_assert_msg(right.loc == OP_MEM_RANGE && right.range.size == left.range.size, "compound assignment with invalid rhs");

			// mov op byte by byte
			Memory_Address dst_adr = left.range.start;
			Memory_Address src_adr = right.range.start;
			auto reg = be_get_reg(self);

			for (size_t i = 0; i < left.range.size; i++)
			{
				be_instruction(self, I_MOV, reg, src_adr);
				be_instruction(self, I_MOV, dst_adr, reg);
				src_adr.offset += 1;
				dst_adr.offset += 1;
			}
		}
		else
		{
			auto [dst, src] = be_prepare_operands(self, left, right);
			be_instruction(self, op, dst, src);
			be_clear_reg_all(self);
		}
	}

	inline static void
	be_decl(Backend self, const Symbol *sym)
	{
		uint64_t size = std::max(1ui64, sym->type.array); // single or array, in words

		// SP - size
		self->stack_frame.top() += size;

		if (size == 1)
		{
			Memory_Address adr = {.base = RBP, .offset = -(int)self->stack_frame.top()};
			self->variables[sym] = {adr};	
		}
		else
		{
			Memory_Address start = {.base = RBP, .offset = -(int)self->stack_frame.top()};
			Memory_Range r = {.start = start, .size = size};
			self->variables[sym] = {r};
		}
	}

	inline static void
	be_decl_expr(Backend self, const Symbol *sym, Operand right)
	{
		be_decl(self, sym);
		be_assign(self, I_MOV, self->variables[sym], right);
	}

	inline static void
	be_decl_proc(Backend self, Decl_Proc *proc, Label proc_lbl)
	{
		self->variables[proc->sym] = {proc_lbl};

						// [RBP]:   old RBP
						// [RBP+1]: return address
		int offset = 2;	// [RBP+2]: return value if any, or first argument
						// [RBP+3]: remaining arguments

		// Return value
		if (proc->sym->type.procedure->return_type != SEMEXPR_VOID)
		{
			uint64_t size = std::max(1ui64, proc->sym->type.procedure->return_type.array); // single or array

			if (size == 1)
			{
				Memory_Address adr = { .base = RBP, .offset = offset }; // First spot below RBP+1
				self->variables[proc->sym].address = adr;	// where the return address symbol is
															// address is used alongsize the label
			}
			else
			{
				Memory_Address start = {.base = RBP, .offset = offset};
				Memory_Range r = {.start = start, .size = size};
				self->variables[proc->sym].range = r;
			}
	
			offset += size;
		}

		// Add arguments
		for (const auto &arg: proc->args)
		{
			auto sym = arg->sym;
			uint64_t size = std::max(1ui64, sym->type.array); // in words

			if (size == 1)
			{
				Memory_Address adr = { .base = RBP, .offset = offset }; // Consecutive spots below RBP+1
				self->variables[sym] = { adr };
			}
			else
			{
				Memory_Address start = {.base = RBP, .offset = offset};
				Memory_Range r = {.start = start, .size = size};
				self->variables[sym] = {r};
			}
			
			offset += size;
		}
	}

	inline static Operand
	be_literal(Backend self, Literal *lit)
	{
		Operand opr = { lit->value };
		opr.size = 1;
		return opr;
	}

	inline static Operand
	be_sym(Backend self, const Symbol *sym)
	{
		return self->variables[sym];
	}

	inline static Operand
	be_array_access(Backend self, const Array_Access *arr)
	{
		auto opr = be_sym(self, arr->sym);
		opr.loc = OP_MEM; // replace range with address

		auto idx = be_generate(self, arr->index);
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
			be_instruction(self, I_MOV, RSI, idx);
			opr.address.index = RSI;
		}

		return opr;
	}

	inline static Operand
	be_binary(Backend self, INSTRUCTION_OP op, Operand left, Operand right)
	{
		// Collect operands
		auto [opr1, opr2] = be_prepare_operands(self, left, right);

		auto dst = be_get_reg(self);
		be_use_reg(self, dst);

		if (op_is_logical(op) == false)
		{
			be_instruction(self, op, dst, opr1, opr2);
		}
		else
		{
			// Logical operations as intermediate boolean result, dst is set to either 0 or 1
			Instruction ins = { .op = op, .dst = dst, .src1 = opr1, .src2 = opr2 };
			if (op == I_LOG_AND)
			{
				be_logical_and(self, ins);
			}
			else if (op == I_LOG_OR)
			{
				be_logical_or(self, ins);
			}
			else
			{
				be_compare(self, ins);
			}
		}

		if (operand_is_register(opr1))
			be_clear_reg(self, opr1);

		return dst;
	}

	inline static Operand
	be_unary(Backend self, INSTRUCTION_OP op, Operand right)
	{
		auto dst = be_get_reg(self);
		be_use_reg(self, dst);

		if (op_is_logical(op) == false)
		{
			be_instruction(self, op, dst, right);
		}
		else
		{
			// Logical operations as intermediate boolean result, dst is set to either 0 or 1
			Instruction ins = { .op = I_LOG_NOT, .dst = dst, .src1 = right };
			be_logical_not(self, ins);
		}

		return dst;
	}

	inline static Operand
	be_proc_call(Backend self, Proc_Call *pcall)
	{
		int offset = 0;
		auto &proc = *pcall->sym->type.procedure;

		// calculate stack offset needed before the return address
		// [args] [+ return value]
		for (const auto &arg : proc.parameters)
			offset += std::max(1ui64, arg.array);

		// leave room for return type on stack
		int offset_before_return_value = offset;
		if (proc.return_type != SEMEXPR_VOID)
		{
			offset += std::max(1ui64, proc.return_type.array);
		}

		if (offset > 0)
		{
			be_instruction(self, I_SUB, RSP, offset);

			// [RSP+<return type size>] return type
			// arguments below
			{
				int arg_offset = offset - offset_before_return_value;
				for (auto arg : pcall->args)
				{
					auto src = be_generate(self, arg);
					Memory_Address dst = {.base = RSP, .offset = arg_offset};

					if (src.loc == OP_MEM_RANGE) // compound assignment
					{
						Memory_Range dst_range = {.start = dst, .size = src.range.size};
						be_assign(self, I_MOV, dst_range, src);
					}
					else
					{
						be_assign(self, I_MOV, dst, src);
					}

					arg_offset += src.size;
				}
			}

			// call function
			be_instruction(self, I_CALL, be_sym(self, pcall->sym));

			// remove call arguments
			be_instruction(self, I_ADD, RSP, offset);

			if (proc.return_type != SEMEXPR_VOID)
			{
				if (proc.return_type.array > 0) // compound return
				{
					Memory_Address start = {.base = RSP, .offset = -offset};
					Memory_Range r = {.start = start, .size = proc.return_type.array};
					return r;
				}
				else
				{
					Memory_Address dst = {.base = RSP, .offset = -offset};
					return dst;
				}
			}
		}
		else
		{
			// call function
			be_instruction(self, I_CALL, be_sym(self, pcall->sym));
		}

		return {};
	}

	inline static void
	be_push_stack_frame(Backend self)
	{
		// save old stack frame
		be_instruction(self, I_PUSH, RBP);
		be_instruction(self, I_MOV, RBP, RSP);
		self->stack_frame.push(0);
	}

	inline static void
	be_pop_stack_frame(Backend self)
	{
		// restore old stack frame
		be_instruction(self, I_MOV, RSP, RBP);
		be_instruction(self, I_POP, RBP);
		self->stack_frame.pop();
	}

	inline static void
	be_block(Backend self, Block *blk)
	{
		if (blk->used_stack_size > 0)
		{
			be_instruction(self, I_SUB, RSP, blk->used_stack_size);

			for (auto stmt: blk->stmts)
				be_generate(self, stmt);

			be_instruction(self, I_ADD, RSP, blk->used_stack_size);
		}
		else
		{
			for (auto stmt : blk->stmts)
				be_generate(self, stmt);
		}
	}

	inline static void
	be_block_no_stack_restore(Backend self, Block *blk)
	{
		if (blk->used_stack_size > 0)
		{
			be_instruction(self, I_SUB, RSP, blk->used_stack_size);

			for (auto stmt: blk->stmts)
				be_generate(self, stmt);
		}
		else
		{
			for (auto stmt : blk->stmts)
				be_generate(self, stmt);
		}
	}

	inline static void
	be_branch_if_false(Backend self, AST ast, Label branch_to)
	{
		switch (ast.kind)
		{
		case AST::NIL: // cannot branch
			break;

		case AST::LITERAL: case AST::SYMBOL: case AST::PROC_CALL: case AST::ARRAY_ACCESS: {
			auto opr = be_generate(self, ast);
			be_instruction(self, I_BZ, branch_to, opr);
			break;
		}

		case AST::BINARY: {
			auto bin = ast.as_binary;
			auto op = (INSTRUCTION_OP)bin->kind;

			// cond != 0
			if (op_is_logical(op) == false)
			{
				auto opr = be_generate(self, ast);
				be_instruction(self, I_BZ, branch_to, opr);
			}
			else if (op == I_LOG_AND)
			{
				be_branch_if_false(self, bin->left, branch_to);
				be_branch_if_false(self, bin->right, branch_to);
			}
			else if (op == I_LOG_OR)
			{
				Label lbl_true = {.type = Label::OR_TRUE, .id = branch_to.id};

				Label left_is_false = {.type = Label::COND_FALSE, .id = be_new_label_id(self)};
				be_branch_if_false(self, bin->left, left_is_false);
				be_instruction(self, I_BR, lbl_true);

				be_label(self, left_is_false);
				be_branch_if_false(self, bin->right, branch_to);
				be_instruction(self, I_BR, lbl_true);

				be_label(self, lbl_true);
			}
			else
			{
				auto left = be_generate(self, bin->left);
				auto right = be_generate(self, bin->right);

				be_instruction(self, op_invert(op), branch_to, left, right);
			}
			break;
		}

		case AST::UNARY: {
			auto uny = ast.as_unary;
			auto op = (INSTRUCTION_OP)uny->kind;

			// cond != 0
			if (op_is_logical(op) == false)
			{
				auto opr = be_generate(self, ast);
				be_instruction(self, I_BZ, branch_to, opr);
			}
			else // NOT
			{
				Label lbl_true = {.type = Label::NOT_TRUE, .id = branch_to.id};
				be_branch_if_false(self, uny->right, lbl_true);
				be_instruction(self, I_BR, branch_to);

				be_label(self, lbl_true);
			}
			break;
		}

		case AST::SWITCH_CASE: {
			auto swc = ast.as_case;

			// Fetch the expression
			auto expr = be_generate(self, swc->expr);

			// Go to true if any value matches
			Label lbl_true = { .type = Label::CASE, .id = branch_to.id };
			for (auto lit: swc->group)
			{
				be_instruction(self, I_LOG_EQ, lbl_true, expr, be_literal(self, lit));
			}

			be_instruction(self, I_BR, branch_to);
			be_label(self, lbl_true);
			break;
		}

		default:
			break;
		}
	}

	inline static Operand
	be_generate(Backend self, AST ast)
	{
		switch (ast.kind)
		{
		case AST::LITERAL:			return be_literal(self, ast.as_lit);
		case AST::SYMBOL:			return be_sym(self, ast.as_sym);
		case AST::PROC_CALL: 		return be_proc_call(self, ast.as_pcall);
		case AST::ARRAY_ACCESS: 	return be_array_access(self, ast.as_arr_access);

		case AST::BINARY: {
			auto bin = ast.as_binary;
			auto left = be_generate(self, bin->left);
			auto right = be_generate(self, bin->right);
			return be_binary(self, (INSTRUCTION_OP)bin->kind, left, right);
		}

		case AST::UNARY: {
			auto uny = ast.as_unary;
			auto right = be_generate(self, uny->right);
			return be_unary(self, (INSTRUCTION_OP)uny->kind, right);
		}

		case AST::ASSIGN: {
			auto as = ast.as_assign;
			auto dst = be_generate(self, as->dst);
			auto expr = be_generate(self, as->expr);
			be_assign(self, (INSTRUCTION_OP)as->kind, dst, expr);
			return {};
		}

		case AST::DECL: {
			auto decl = ast.as_decl;

			if (decl->expr.kind == AST::NIL)
			{
				be_decl(self, decl->sym);
			}
			else
			{
				auto expr = be_generate(self, decl->expr);
				be_decl_expr(self, decl->sym, expr);
			}

			return {};
		}

		case AST::DECL_PROC: {
			auto proc = ast.as_decl_proc;

			Label proc_lbl = {.type = Label::PROC, .text = proc->sym->id};
			be_label(self, proc_lbl);

			be_push_stack_frame(self);
			be_decl_proc(self, proc, proc_lbl);

			Label return_lbl = {.type = Label::END_PROC, .text = proc->sym->id};
			be_block_no_stack_restore(self, proc->block);

			be_label(self, return_lbl);
			be_pop_stack_frame(self);

			be_instruction(self, I_RET);

			return {};
		}

		case AST::IF_COND: {
			Label end_all = { .type = Label::END_ALL, .id = be_new_label_id(self) };

			for (auto ifc = ast.as_if; ifc != nullptr; ifc = ifc->next)
			{
				Label end_if = { .type = Label::END_IF, .id = be_new_label_id(self) };
				be_branch_if_false(self, ifc->cond, end_if);
				be_clear_reg_all(self);

				be_block(self, ifc->block);

				be_instruction(self, I_BR, end_all);
				be_label(self, end_if);
			}

			be_label(self, end_all);
			return {};
		}

		case AST::SWITCH: {
			Label end_switch = { .type = Label::END_SWITCH, .id = be_new_label_id(self) };

			auto sw = ast.as_switch;

			for (auto &swc: sw->cases)
			{
				Label end_case = { .type = Label::END_CASE, .id = be_new_label_id(self) };
				AST case_ast = { .kind = AST::SWITCH_CASE, .as_case = swc };
				be_branch_if_false(self, case_ast, end_case);
				be_clear_reg_all(self);

				be_block(self, swc->block);

				be_instruction(self, I_BR, end_switch);
				be_label(self, end_case);
			}

			if (sw->case_default)
				be_block(self, sw->case_default);

			be_label(self, end_switch);
			return {};
		}

		case AST::WHILE: {
			auto wh = ast.as_while;

			Label begin_while = { .type = Label::WHILE,     .id = be_new_label_id(self) };
			Label end_while   = { .type = Label::END_WHILE, .id = begin_while.id };

			be_label(self, begin_while);

			be_branch_if_false(self, wh->cond, end_while);
			be_clear_reg_all(self);

			be_block(self, wh->block);

			be_instruction(self, I_BR, begin_while);

			be_label(self, end_while);
			return {};
		}

		case AST::DO_WHILE: {
			auto do_wh = ast.as_do_while;

			Label begin_do_while = { .type = Label::WHILE,     .id = be_new_label_id(self) };
			Label end_do_while   = { .type = Label::END_WHILE, .id = begin_do_while.id };

			be_label(self, begin_do_while);

			be_block(self, do_wh->block);

			be_branch_if_false(self, do_wh->cond, end_do_while);
			be_clear_reg_all(self);

			be_instruction(self, I_BR, begin_do_while);

			be_label(self, end_do_while);
			return {};
		}

		case AST::FOR: {
			auto loop = ast.as_for;

			Label begin_for = { .type = Label::FOR,     .id = be_new_label_id(self) };
			Label end_for   = { .type = Label::END_FOR, .id = begin_for.id };

			// init
			be_generate(self, loop->init);

			// cond
			be_label(self, begin_for);
			be_branch_if_false(self, loop->cond, end_for);
			be_clear_reg_all(self);

			// block
			be_block(self, loop->block);

			// post
			be_generate(self, loop->post);

			be_instruction(self, I_BR, begin_for);

			be_label(self, end_for);
			return {};
		}

		case AST::BLOCK: {
			be_block(self, ast.as_block);
			return {};
		}

		case AST::RETURN: {
			auto &ret = ast.as_return;

			if (auto return_type = ret->proc_sym->type.procedure->return_type; return_type != SEMEXPR_VOID)
			{
				auto expr = be_generate(self, ret->expr);
				if (expr.loc == OP_MEM_RANGE)
				{
					be_assign(self, I_MOV, self->variables[ret->proc_sym].range, expr);
				}
				else
				{
					be_assign(self, I_MOV, self->variables[ret->proc_sym].address, expr);
				}
			}
			
			Label return_lbl = {.type = Label::END_PROC, .text = ret->proc_sym->id};
			be_instruction(self, I_BR, return_lbl);
		
			return {};
		}

		default:
			return {};
		}
	}

	Backend
	backend_instance()
	{
		static IBackend self = {};
		return &self;
	}

	void
	backend_dispose(Backend self)
	{
		if (self == nullptr)
			return;

		be_clear_reg_all(self);

		self->variables = {};
		self->stack_frame = {};
		self->program = {};
		self->label_counter = 0;
	}

	UI_Program
	backend_get_ui_program(Backend self)
	{
		UI_Program program = {};
		for (const auto &ins : self->program)
		{
			auto &line = program.emplace_back();

			if (ins.label.type != s22::Label::NONE)
				line[0] = std::format("{}", ins.label);

			if (ins.op != I_NOP)
			{
				line[1] = std::format("{}", ins.op);

				if (ins.operand_count >= 1)
					line[2] = std::format("{}", ins.dst);

				if (ins.operand_count >= 2)
					line[3] = std::format("{}", ins.src1);

				if (ins.operand_count == 3)
					line[4] = std::format("{}", ins.src2);
			}
		}
		return program;
	}

	void
	backend_compile(Backend self, AST ast)
	{
		// Start root stack frame at 0
		self->stack_frame.push(0);
		be_block(self, ast.as_block);
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

		case Label::OR_TRUE: 	lbl = "OR_TRUE"; 	break;
		case Label::END_OR: 	lbl = "END_OR"; 	break;

		case Label::AND_FALSE: 	lbl = "AND_FALSE"; 	break;
		case Label::END_AND: 	lbl = "END_AND"; 	break;

		case Label::NOT_TRUE: 	lbl = "NOT_TRUE"; 	break;
		case Label::END_NOT: 	lbl = "END_NOT"; 	break;

		case Label::COND_FALSE: lbl = "COND_FALSE"; break;
		case Label::END_COND: 	lbl = "END_COND"; 	break;

		case Label::END_IF: 	lbl = "END_IF";		break;
		case Label::END_ELSEIF: lbl = "END_ELSEIF";	break;
		case Label::END_ALL: 	lbl = "END_ALL";	break;

		case Label::CASE: 		lbl = "CASE";		break;
		case Label::END_CASE: 	lbl = "END_CASE";	break;
		case Label::END_SWITCH: lbl = "END_SWITCH";	break;

		case Label::FOR: 		lbl = "FOR";		break;
		case Label::END_FOR: 	lbl = "END_FOR";	break;

		case Label::WHILE: 		lbl = "WHILE";		break;
		case Label::END_WHILE: 	lbl = "END_WHILE";	break;

		case Label::PROC:
			return format_to(ctx.out(), "{}", label.text);

		case Label::END_PROC:
			return format_to(ctx.out(), "{}$end", label.text);

		default:
			return ctx.out();
		}

		return format_to(ctx.out(), "{}${}", lbl, label.id);
	}
};

template <>
struct std::formatter<s22::INSTRUCTION_OP> : std::formatter<std::string>
{
	auto
	format(s22::INSTRUCTION_OP op, format_context &ctx)
	{
		using namespace s22;
		switch (op)
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

		// Procedures and stack
		case I_PUSH:	format_to(ctx.out(), "PUSH"); 	break;
		case I_POP:		format_to(ctx.out(), "POP"); 	break;
		case I_CALL:	format_to(ctx.out(), "CALL"); 	break;
		case I_RET:		format_to(ctx.out(), "RET"); 	break;
		default: 		return ctx.out();
		}
		return ctx.out();
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

		format_to(ctx.out(), "{}", ins.op);

		if (ins.operand_count >= 1)
			format_to(ctx.out(), " {}", ins.dst);

		if (ins.operand_count >= 2)
			format_to(ctx.out(), ", {}", ins.src1);

		if (ins.operand_count == 3)
			format_to(ctx.out(), ", {}", ins.src2);

		return ctx.out();
	}
};