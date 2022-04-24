#include "compiler/Symbol.h"
#include "compiler/Util.h"
#include "compiler/Expr.h"
#include "compiler/Parser.h"

#include <stdio.h>
#include <format>

extern int yylineno;
extern int yyleng;
extern char *yytext;
extern FILE *yyin;

namespace s22
{
	void
	location_reduce(Location &current, Location *rhs, size_t N)
	{
		if (N != 0)
		{
			current.first_line = rhs[1].first_line;
			current.first_column = rhs[1].first_column;
			current.last_line = rhs[N].last_line;
			current.last_column = rhs[N].last_column;
		}
		else
		{
			current.first_line = current.last_line = rhs[0].last_line;
			current.first_column = current.last_column = rhs[0].last_column;
		}
	}

	void
	location_update(Location *loc)
	{
		static int column = 1;

		// New line
		if (yytext[yyleng - 1] == '\n')
		{
			column = 1;
			return;
		}

		// Current line and column
		loc->first_line = yylineno;
		loc->first_column = column;

		// Next line and column
		loc->last_line = yylineno;
		loc->last_column = loc->first_column + yyleng - 1;

		// Update current column
		column = loc->last_column + 1;
	}

	void
	location_print(FILE *out, const Location *const loc)
	{
		auto str = std::format("{}", *loc);
		fprintf(out, str.data());
	}

	void
	symtype_print(FILE *out, const Symbol_Type &type)
	{
		auto fmt = std::format("{}", type);
		fprintf(out, "%s", fmt.data());
	}

	template<typename T>
	inline static T *
	push_entry(Symbol_Table *self)
	{
		if (self->table.empty())
			self->table.reserve(256);

		Symbol_Table::Entry entry = T{};
		self->table.push_back(entry);
		return &std::get<T>(self->table.back());
	}

	Result<Symbol *>
	symtab_add_decl(Symbol_Table *self, const Symbol &symbol)
	{
		// Try to find duplicate
		for (const auto &entry : self->table)
		{
			if (std::holds_alternative<Symbol>(entry))
			{
				auto &dup = std::get<Symbol>(entry);
				if (dup.id == symbol.id)
					return Error{"Duplicate identifier at {}", dup.defined_at};
			}
		}

		auto sym = push_entry<Symbol>(self);
		*sym = symbol;
		return sym;
	}

	Result<Symbol *>
	symtab_add_decl(Symbol_Table *self, const Symbol &symbol, Expr &expr)
	{
		auto [type, err] = expr_evaluate(expr, self);
		if (err)
			return err;

		if (symbol.type != type)
			return Error{"type mismatch"};

		auto [sym, sym_err] = symtab_add_decl(self, symbol);
		if (sym_err)
			return sym_err;

		sym->is_set = true;
		return sym;
	}

	Symbol_Table *
	symtab_push_scope(Symbol_Table *&self)
	{
		auto parent = self;

		auto inner = push_entry<Symbol_Table>(self);
		inner->parent_scope = self;
		inner->idx_in_parent_table = self->table.size()-1;

		self = inner;
		return parent;
	}

	Symbol_Table *
	symtab_pop_scope(Symbol_Table *&self)
	{
		for (const auto &entry : self->table)
		{
			if (std::holds_alternative<Symbol>(entry))
			{
				auto &sym = std::get<Symbol>(entry);
				if (sym.is_used == false)
				{
					yyerror(&sym.defined_at, nullptr, "WARNING: Unused identifier");
				}
			}
		}
		auto inner = self;
		self = self->parent_scope;
		return inner;
	}

	Symbol *
	symtab_get_id(Symbol_Table *self, const char *id)
	{
		size_t current_scope_idx_in_parent = self->table.size();
		for (auto current_scope = self; current_scope != nullptr;)
		{
			for (size_t i = 0; i < current_scope_idx_in_parent; i++)
			{
				auto &entry = current_scope->table[i];
				if (std::holds_alternative<Symbol>(entry))
				{
					auto &sym = std::get<Symbol>(entry);

					if (sym.id == id)
						return &sym;
				}
			}

			current_scope_idx_in_parent = current_scope->idx_in_parent_table;
			current_scope = current_scope->parent_scope;
		}
		return nullptr;
	}

	Procedure *
	symtab_make_procedure(Symbol_Table *self, Symbol_Type return_type)
	{
		auto *proc = alloc<Procedure>();
		proc->parameters = Buf<Symbol_Type>::make(self->table.size());
		for (size_t i = 0; i < proc->parameters.count; i++)
		{
			auto &param = std::get<Symbol>(self->table[i]);
			proc->parameters[i] = param.type;
		}

		if (return_type.base != Symbol_Type::VOID)
		{
			proc->return_type = alloc<Symbol_Type>();
			*proc->return_type = return_type;
		}

		return proc;
	}
}