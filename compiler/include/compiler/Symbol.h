#pragma once

#include "compiler/Util.h"
#include "compiler/Semantic_Expr.h"

#include <string>
#include <vector>
#include <variant>

namespace s22
{
	struct Parse_Unit;

	struct Symbol
	{
		String id;
		Semantic_Expr type;
		Source_Location defined_at;

		bool is_constant;
		bool is_set;
		bool is_used;
	};

	struct Scope
	{
		using Entry = std::variant<Symbol, Scope>;
		std::vector<Entry> table;

		Scope *parent_scope;
		size_t idx_in_parent_table; // index of current scope in the parent scope, used to track use before declaration

		Symbol *proc_sym; // if scope is a procedure, holds the symbol to it
	};

	// Declare symbol in current scope
	// Fails if duplicates exist
	Result<Symbol *>
	scope_add_decl(Scope *self, const Symbol &symbol);

	// Declare symbol in current scope, with assignment
	// Fails on type mismatch, or assignment error
	Result<Symbol *>
	scope_add_decl(Scope *self, const Symbol &symbol, const Parse_Unit &expr);

	// Push new scope, modifying the passed pointer and returning the old pointer
	Scope *
	scope_push(Scope *&self);

	// Returns the symbol representing the proc the current scope is a part of
	Result<Symbol *>
	scope_return_matches_proc_sym(Scope *self, Semantic_Expr type);

	// Pop current scope, modifying the passed pointer and returning the old pointer
	Scope *
	scope_pop(Scope *&self);

	// Return the symbol represented by the identifier, nullptr if not found
	Symbol *
	scope_get_sym(Scope *self, const char *id);

	// Builds a procedure, sets its arguments to all variables defined so far in the scope
	Procedure
	scope_make_proc(Scope *self, Semantic_Expr return_type);
}