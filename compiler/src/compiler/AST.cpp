#include "compiler/Util.h"
#include "compiler/Symbol.h"
#include "compiler/AST.h"
#include "compiler/Semantic_Expr.h"

namespace s22
{
	AST
	ast_literal(Literal *literal)
	{
		AST self = { .kind = AST::LITERAL };

		auto &lit = self.as_lit;
		lit = alloc<Literal>();
		*lit = *literal;

		return self;
	}

	AST
	ast_symbol(Symbol *sym)
	{
		AST self = { .kind = AST::SYMBOL };
		self.as_sym = sym;
		return self;
	};

	AST
	ast_pcall(Symbol *sym, const Buf<AST> &args)
	{
		AST self = { .kind = AST::PROC_CALL };

		auto &pcall = self.as_pcall;
		pcall = alloc<Proc_Call>();
		pcall->sym = sym;
		pcall->args = args;

		return self;
	}

	AST
	ast_array_access(Symbol *sym, AST index)
	{
		AST self = { .kind = AST::ARRAY_ACCESS };

		auto &array = self.as_arr_access;
		array = alloc<Array_Access>();
		array->sym = sym;
		array->index = index;

		return self;
	}

	AST
	ast_binary(Binary_Op::KIND kind, AST left, AST right)
	{
		AST self = { .kind = AST::BINARY };

		auto &bin = self.as_binary;
		bin = alloc<Binary_Op>();
		bin->kind = kind;
		bin->left = left;
		bin->right = right;

		return self;
	}

	AST
	ast_unary(Unary_Op::KIND kind, AST right)
	{
		AST self = { .kind = AST::UNARY };

		auto &uny = self.as_unary;
		uny = alloc<Unary_Op>();
		uny->kind = kind;
		uny->right = right;

		return self;
	}

	AST
	ast_assign(Assignment::KIND kind, AST dst, AST expr)
	{
		AST self = { .kind = AST::ASSIGN };

		auto &assign = self.as_assign;
		assign = alloc<Assignment>();
		assign->kind = kind;
		assign->dst = dst;
		assign->expr = expr;

		return self;
	}

	AST
	ast_decl(Symbol *sym, AST expr)
	{
		AST self = { .kind = AST::DECL };

		auto &decl = self.as_decl;
		decl = alloc<Decl>();
		decl->sym = sym;
		decl->expr = expr;

		return self;
	}

	AST
	ast_decl_proc(Symbol *sym, const Buf<Decl *> &args, Block *block)
	{
		AST self = { .kind = AST::DECL_PROC };

		auto &decl = self.as_decl_proc;
		decl = alloc<Decl_Proc>();
		decl->sym = sym;
		decl->args = args;
		decl->block = block;

		return self;
	}

	AST
	ast_if(If_Condition *prev, AST cond, Block *block, If_Condition *next)
	{
		AST self = { .kind = AST::IF_COND };

		auto &if_cond = self.as_if;
		if_cond = alloc<If_Condition>();
		if_cond->cond = cond;
		if_cond->prev = prev;
		if_cond->next = next;
		if_cond->block = block;

		return self;
	}

	AST
	ast_switch_case(AST expr, const Buf<Literal*> &group, Block *block)
	{
		AST self = { .kind = AST::SWITCH_CASE };

		auto &swc = self.as_case;
		swc = alloc<Switch_Case>();
		swc->expr = expr;
		swc->group = group;
		swc->block = block;

		return self;
	}

	AST
	ast_switch(const Buf<Switch_Case*> &cases, Block *case_default)
	{
		AST self = { .kind = AST::SWITCH };

		auto &sw = self.as_switch;
		sw = alloc<Switch>();
		sw->cases = cases;
		sw->case_default = case_default;

		return self;
	}

	AST
	ast_while(AST cond, Block *block)
	{
		AST self = { .kind = AST::WHILE };

		auto &loop = self.as_while;
		loop = alloc<While_Loop>();
		loop->cond = cond;
		loop->block = block;

		return self;
	}

	AST
	ast_do_while(AST cond, Block *block)
	{
		AST self = { .kind = AST::DO_WHILE };

		auto &loop = self.as_do_while;
		loop = alloc<Do_While_Loop>();
		loop->cond = cond;
		loop->block = block;

		return self;
	}

	AST
	ast_for(AST init, AST cond, AST post, Block *block)
	{
		AST self = { .kind = AST::FOR };

		auto &loop = self.as_for;
		loop = alloc<For_Loop>();
		loop->init = init;
		loop->cond = cond;
		loop->post = post;
		loop->block = block;

		return self;
	}

	AST
	ast_block(const Buf<AST> &stmts, size_t used_stack_size)
	{
		AST self = { .kind = AST::BLOCK };

		auto &block = self.as_block;
		block = alloc<Block>();
		block->stmts = stmts;
		block->used_stack_size = used_stack_size;

		return self;
	}
}
