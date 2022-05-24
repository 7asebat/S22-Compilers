#include "compiler/Backend.h"
#include "compiler/Symbol.h"
#include "compiler/Parser.h"

#include <unordered_map>

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
		OP_TMP,				// intermediate value
		OP_IMM,				// immediate value
		OP_SYM,				// symbol
		OP_LBL,				// label
	};

	struct Operand
	{
		OPERAND_LOCATION loc;

		inline Operand() = default;
		inline Operand(OPERAND_LOCATION l)		{ *this = {}; loc = l; }
		inline Operand(int v)					{ *this = {}; loc = OP_IMM; value = (uint64_t)v; }
		inline Operand(uint64_t v)				{ *this = {}; loc = OP_IMM; value = v; }
		inline Operand(String s)				{ *this = {}; loc = OP_SYM; sym = s; }
		inline Operand(Label lbl)				{ *this = {}; loc = OP_LBL; label = lbl; }

		template<typename... TArgs>
		inline explicit Operand(const char *fmt, TArgs &&...args)
		{
			*this = {};
			loc = OP_SYM;
			sym = std::format(fmt, std::forward<TArgs>(args)...).c_str();
		}

		union // immediate value/memory offset
		{
			uint64_t value;
			uint64_t tmp_label_suffix;
			String sym;
		};
		Label label;	// set when loc is a label
						// TODO: procedures carry both their label and their address here, change this
	};

	struct Instruction
	{
		INSTRUCTION_OP op;
		Operand dst, src1, src2;
		
		size_t operand_count;
		Label label;// adds a label to the instruction
	};

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
	op_invert(INSTRUCTION_OP op)
	{
		if (op_is_logical(op) == false)
			return op;

		switch (op)
		{
		case I_LOG_LT: return I_LOG_GEQ;
		case I_LOG_LEQ: return I_LOG_GT;
		case I_LOG_EQ: return I_LOG_NEQ;
		case I_LOG_NEQ: return I_LOG_EQ;
		case I_LOG_GT: return I_LOG_LEQ;
		case I_LOG_GEQ: return I_LOG_LT;

		default:
			s22_unreachable_msg("unrecognized op");
			return I_BR;
		}
	}

	struct IBackend
	{
		std::unordered_map<const Symbol *, Operand> variables; // maps symbol to memory locations
															   // also maps procs to their labels/locations
		std::vector<Instruction> program;
		size_t label_counter;
		size_t temp_counter;
	};

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

	inline static size_t
	be_new_label_id(Backend self)
	{
		return self->label_counter++;
	}

	inline static void
	be_clear_temps(Backend self)
	{
		self->temp_counter = 0;
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
		if (op == I_MOV)
			be_instruction(self, op, left, right);
		else
			be_instruction(self, op, left, left, right);
		
		be_clear_temps(self);
	}

	inline static void
	be_decl(Backend self, const Symbol *sym)
	{
		self->variables[sym] = {sym->id};
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

		if (proc->sym->type.procedure->return_type != SEMEXPR_VOID)
		{
			self->variables[proc->sym].sym = std::format("t${}", proc->sym->id).c_str();
		}

		// Add arguments
		for (const auto &arg : proc->args)
		{
			be_decl(self, arg->sym);
		}
	}

	Operand
	be_generate(Backend self, AST ast);

	inline static Operand
	be_temp(Backend self)
	{
		Operand opr = {};
		opr.loc = OP_TMP;
		opr.tmp_label_suffix = self->temp_counter++;
		return opr;
	}

	inline static Operand
	be_literal(Backend self, Literal *lit)
	{
		Operand opr = {lit->value};
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
		return Operand{"{}({})", be_generate(self, arr->index), be_sym(self, arr->sym)};
	}

	inline static Operand
	be_binary(Backend self, INSTRUCTION_OP op, Operand left, Operand right)
	{
		// Collect operands
		auto dst = be_temp(self);
		if (op_is_logical(op) == false)
		{
			be_instruction(self, op, dst, left, right);
		}
		else
		{
			// Logical operations as intermediate boolean result, dst is set to either 0 or 1
			Instruction ins = {.op = op, .dst = dst, .src1 = left, .src2 = right};
			if (op == I_LOG_AND)		be_logical_and(self, ins);
			else if (op == I_LOG_OR)	be_logical_or(self, ins);
			else						be_compare(self, ins);
		}

		return dst;
	}
	
	inline static Operand
	be_unary(Backend self, INSTRUCTION_OP op, Operand right)
	{
		auto dst = be_temp(self);
		if (op_is_logical(op) == false)
		{
			be_instruction(self, op, dst, right);
		}
		else
		{
			// Logical operations as intermediate boolean result, dst is set to either 0 or 1
			Instruction ins = {.op = I_LOG_NOT, .dst = dst, .src1 = right};
			be_logical_not(self, ins);
		}

		return dst;
	}

	inline static Operand
	be_proc_call(Backend self, Proc_Call *pcall)
	{
		int offset = 0;
		auto &proc = *pcall->sym->type.procedure;
		
		for (size_t i = 0; i < proc.parameters.count; i++)
		{
			auto src = be_generate(self, pcall->args[i]);
			auto dst = Operand{"{}${}", be_sym(self, pcall->sym), i};
			be_assign(self, I_MOV, dst, src);
		}
		
		be_instruction(self, I_CALL, be_sym(self, pcall->sym));
		if (proc.return_type != SEMEXPR_VOID)
		{
			return Operand{"t${}", be_sym(self, pcall->sym)};
		}
		
		return {};
	}

	inline static void
	be_block(Backend self, Block *blk)
	{
		for (auto stmt : blk->stmts)
			be_generate(self, stmt);
	}

	inline static void
	be_branch_if_false(Backend self, AST ast, Label branch_to)
	{
		switch (ast.kind)
		{
		case AST::NIL:// cannot branch
			break;

		case AST::LITERAL:
		case AST::SYMBOL:
		case AST::PROC_CALL:
		case AST::ARRAY_ACCESS: {
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
			else// NOT
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
			Label lbl_true = {.type = Label::CASE, .id = branch_to.id};
			for (auto lit : swc->group)
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
		case AST::LITERAL: return be_literal(self, ast.as_lit);
		case AST::SYMBOL: return be_sym(self, ast.as_sym);
		case AST::PROC_CALL: return be_proc_call(self, ast.as_pcall);
		case AST::ARRAY_ACCESS: return be_array_access(self, ast.as_arr_access);

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

			be_decl_proc(self, proc, proc_lbl);
			
			be_block(self, proc->block);
			
			Label return_lbl = {.type = Label::END_PROC, .text = proc->sym->id};
			be_label_with_instruction(self, return_lbl, I_RET);

			return {};
		}

		case AST::IF_COND: {
			Label end_all = {.type = Label::END_ALL, .id = be_new_label_id(self)};

			for (auto ifc = ast.as_if; ifc != nullptr; ifc = ifc->next)
			{
				Label end_if = {.type = Label::END_IF, .id = be_new_label_id(self)};
				be_branch_if_false(self, ifc->cond, end_if);
				be_clear_temps(self);

				be_block(self, ifc->block);

				be_instruction(self, I_BR, end_all);
				be_label(self, end_if);
			}

			be_label(self, end_all);
			return {};
		}

		case AST::SWITCH: {
			Label end_switch = {.type = Label::END_SWITCH, .id = be_new_label_id(self)};

			auto sw = ast.as_switch;

			for (auto &swc : sw->cases)
			{
				Label end_case = {.type = Label::END_CASE, .id = be_new_label_id(self)};
				AST case_ast = {.kind = AST::SWITCH_CASE, .as_case = swc};
				be_branch_if_false(self, case_ast, end_case);
				be_clear_temps(self);

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

			Label begin_while = {.type = Label::WHILE, .id = be_new_label_id(self)};
			Label end_while = {.type = Label::END_WHILE, .id = begin_while.id};

			be_label(self, begin_while);

			be_branch_if_false(self, wh->cond, end_while);
			be_clear_temps(self);

			be_block(self, wh->block);

			be_instruction(self, I_BR, begin_while);

			be_label(self, end_while);
			return {};
		}

		case AST::DO_WHILE: {
			auto do_wh = ast.as_do_while;

			Label begin_do_while = {.type = Label::WHILE, .id = be_new_label_id(self)};
			Label end_do_while = {.type = Label::END_WHILE, .id = begin_do_while.id};

			be_label(self, begin_do_while);

			be_block(self, do_wh->block);

			be_branch_if_false(self, do_wh->cond, end_do_while);
			be_clear_temps(self);

			be_instruction(self, I_BR, begin_do_while);

			be_label(self, end_do_while);
			return {};
		}

		case AST::FOR: {
			auto loop = ast.as_for;

			Label begin_for = {.type = Label::FOR, .id = be_new_label_id(self)};
			Label end_for = {.type = Label::END_FOR, .id = begin_for.id};

			// init
			be_generate(self, loop->init);

			// cond
			be_label(self, begin_for);
			be_branch_if_false(self, loop->cond, end_for);
			be_clear_temps(self);

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
				be_assign(self, I_MOV, self->variables[ret->proc_sym].sym, expr);
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

		self->variables.clear();
		self->program.clear();
		self->label_counter = 0;
		self->temp_counter = 0;
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
		be_block(self, ast.as_block);
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
		case OP_NIL: return ctx.out();
		case OP_TMP: return format_to(ctx.out(), "t{}", opr.tmp_label_suffix);
		case OP_IMM: return format_to(ctx.out(), "{}", opr.value);
		case OP_SYM: return format_to(ctx.out(), "{}", opr.sym);
		case OP_LBL: return format_to(ctx.out(), "{}", opr.label);
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

		case Label::PROC:		return format_to(ctx.out(), "{}", label.text);
		case Label::END_PROC:	return format_to(ctx.out(), "{}$end", label.text);

		default: return ctx.out();
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
		case I_NOP: return ctx.out();
		case I_MOV: return format_to(ctx.out(), "=");

		// Arithmetic
		case I_ADD: return format_to(ctx.out(), "+");
		case I_SUB: return format_to(ctx.out(), "-");
		case I_MUL: return format_to(ctx.out(), "*");
		case I_DIV: return format_to(ctx.out(), "/");
		case I_MOD: return format_to(ctx.out(), "%");
		case I_AND: return format_to(ctx.out(), "&");
		case I_OR:	return format_to(ctx.out(), "|");
		case I_XOR: return format_to(ctx.out(), "^");
		case I_SHL: return format_to(ctx.out(), "<<");
		case I_SHR: return format_to(ctx.out(), ">>");
		case I_NEG: return format_to(ctx.out(), "neg");
		case I_INV: return format_to(ctx.out(), "~");

		// Logical
		case I_LOG_LT:	return format_to(ctx.out(), "BLT");
		case I_LOG_LEQ: return format_to(ctx.out(), "BLE");
		case I_LOG_EQ:	return format_to(ctx.out(), "BEQ");
		case I_LOG_NEQ: return format_to(ctx.out(), "BNE");
		case I_LOG_GEQ: return format_to(ctx.out(), "BGE");
		case I_LOG_GT:	return format_to(ctx.out(), "BGT");

		// Branch
		case I_BR:	return format_to(ctx.out(), "BR");
		case I_BZ:	return format_to(ctx.out(), "BZ");
		case I_BNZ: return format_to(ctx.out(), "BNZ");

		// Procedures
		case I_CALL: return format_to(ctx.out(), "CALL");
		case I_RET:  return format_to(ctx.out(), "RET");
		
		default: return ctx.out();
		}
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
		if (ins.operand_count >= 1) format_to(ctx.out(), " {}", ins.dst);
		if (ins.operand_count >= 2) format_to(ctx.out(), ", {}", ins.src1);
		if (ins.operand_count == 3) format_to(ctx.out(), ", {}", ins.src2);

		return ctx.out();
	}
};