#pragma once
#include "compiler/Util.h"

namespace s22
{
	struct Literal;			// 1, 2u, 3.14, true
	struct Symbol;			// x, y
	struct Proc_Call;		// my_proc(x, y)
	struct Array_Access;	// arr[i]
	struct Binary_Op;		// x + 5
	struct Unary_Op;		// -4
	struct Assignment;		// x = 4
	struct Decl;			// x: int;
	struct Decl_Proc;		// my_proc: proc(x: int, y: int) -> int {}
	struct If_Condition;	// if cond {} else if cond2 {} else {}
	struct Switch_Case;		// case 1, 2, 3 {}
	struct Switch;			// switch expr { <CASE> default {} }
	struct While_Loop;		// while cond do {}
	struct Do_While_Loop;	// do {} while cond
	struct For_Loop;		// for <INIT>; <COND>; <POST> {}
	struct Block;			// { <STATEMENTS> }
	struct Return;			// return expr;

	// Each abstract syntax subtree represents a statement, which can be either one of all the previous expressions
	// AST is a tagged union of different statement types
	struct AST
	{
		enum KIND
		{
			NIL,
			LITERAL, SYMBOL, PROC_CALL, ARRAY_ACCESS,
			BINARY, UNARY,

			ASSIGN,
			DECL, DECL_PROC,

			IF_COND,
			SWITCH, SWITCH_CASE,

			WHILE,
			DO_WHILE,
			FOR,

			BLOCK,
			RETURN,
		};
		KIND kind;

		union
		{
			void *as_nil;
			Literal *as_lit;
			Symbol *as_sym;
			Proc_Call *as_pcall;
			Array_Access *as_arr_access;
			Binary_Op *as_binary;
			Unary_Op *as_unary;
			Assignment *as_assign;
			Decl *as_decl;
			Decl_Proc *as_decl_proc;
			If_Condition *as_if;
			Switch *as_switch;
			Switch_Case *as_case;
			While_Loop *as_while;
			Do_While_Loop *as_do_while;
			For_Loop *as_for;
			Block *as_block;
			Return *as_return;
		};
	};

	struct Literal
	{
		union
		{
			uint64_t value;

			// Value interpretations
			uint64_t u64;
			int64_t s64;
			double f64;
			bool b;
		};

		inline bool
		operator==(const Literal &other) { return this->value == other.value; }

		inline bool
		operator!=(const Literal &other) { return !this->operator==(other); }
	};

	// Constructors for different types of ASTs
	AST
	ast_literal(Literal *literal);

	AST
	ast_symbol(Symbol *sym);

	struct Proc_Call
	{
		Symbol *sym;
		Buf<AST> args;
	};

	AST
	ast_pcall(Symbol *sym, const Buf<AST> &args);

	struct Array_Access
	{
		Symbol *sym;
		AST index;
	};

	AST
	ast_array_access(Symbol *sym, AST index);

	struct Binary_Op
	{
		enum KIND {} kind;
		AST left;
		AST right;
	};

	AST
	ast_binary(Binary_Op::KIND kind, AST left, AST right);

	struct Unary_Op
	{
		enum KIND {} kind;
		AST right;
	};

	AST
	ast_unary(Unary_Op::KIND kind, AST right);

	struct Assignment
	{
		enum KIND {} kind;
		AST dst;
		AST expr;
	};

	AST
	ast_assign(Assignment::KIND kind, AST dst, AST expr);

	struct Decl
	{
		Symbol *sym;
		AST expr;
	};

	AST
	ast_decl(Symbol *sym, AST expr);

	struct Decl_Proc
	{
		Symbol *sym;
		Buf<Decl *> args;
		Block *block;
	};

	AST
	ast_decl_proc(Symbol *sym, const Buf<Decl *> &args, Block *block);

	struct If_Condition
	{
		AST cond; 				// NIL for else
		If_Condition *prev;		// NIL for if
		If_Condition *next;		// NIL for else

		Block *block;
	};

	AST
	ast_if(If_Condition *prev, AST cond, Block *block, If_Condition *next);

	struct Switch_Case
	{
		AST expr;
		Buf<Literal*> group;
		Block *block;
	};

	AST
	ast_switch_case(AST expr, const Buf<Literal*> &group, Block *block);

	struct Switch
	{
		Buf<Switch_Case*> cases;
		Block *case_default;
	};

	AST
	ast_switch(const Buf<Switch_Case*> &cases, Block *case_default);

	struct While_Loop
	{
		AST cond;
		Block *block;
	};

	AST
	ast_while(AST cond, Block *block);

	struct Do_While_Loop
	{
		AST cond;
		Block *block;
	};

	AST
	ast_do_while(AST cond, Block *block);

	struct For_Loop
	{
		AST init;
		AST cond;
		AST post;
		Block *block;
	};

	AST
	ast_for(AST init, AST cond, AST post, Block *block);

	struct Block
	{
		Buf<AST> stmts;
		size_t used_stack_size;
	};

	AST
	ast_block(const Buf<AST> &stmts, size_t used_stack_size = 0);

	struct Return
	{
		AST expr;
		Symbol *proc_sym;
	};

	AST
	ast_return(AST expr, Symbol *sym);
}