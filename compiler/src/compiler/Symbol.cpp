#include "compiler/Util.h"
#include "compiler/Symbol.h"
#include "compiler/Semantic_Expr.h"
#include "compiler/Parser.h"

namespace s22
{
	template<typename T>
	inline static T *
	push_entry(Scope *self)
	{
		// NOTE: If symbol pointers change, the code breaks, as some data is hashed by pointers
		// So we reserve size beforehand to ensure the data isn't moved
		if (self->table.empty())
			self->table.reserve(1024);

		self->table.push_back(T{});
		return &std::get<T>(self->table.back());
	}

	Result<Symbol *>
	scope_add_decl(Scope *self, const Symbol &symbol)
	{
		// Try to find duplicate
		for (const auto &entry : self->table)
		{
			if (std::holds_alternative<Symbol>(entry))
			{
				auto &dup = std::get<Symbol>(entry);
				if (dup.id == symbol.id)
					return Error{symbol.defined_at, "duplicate identifier at {}", dup.defined_at};
			}
		}

		auto sym = push_entry<Symbol>(self);
		*sym = symbol;
		return sym;
	}

	Result<Symbol *>
	scope_add_decl(Scope *self, const Symbol &symbol, const Parse_Unit &expr)
	{
		if (expr.err)
			return expr.err;

		if (symbol.type != expr.semexpr)
			return Error{expr.loc, "type mismatch"};

		auto [sym, sym_err] = scope_add_decl(self, symbol);
		if (sym_err)
			return sym_err;

		sym->is_set = true;
		return sym;
	}

	Result<Symbol *>
	scope_add_decl_proc(Scope *&self, const Symbol &symbol)
	{
		// Entry point is a scope where all parameters are defined
		// self = &table.back();
		// Pop scope temporarily
		if (self != &std::get<Scope>(self->parent_scope->table.back()))
		{
			s22_unreachable_msg("scope isn't last in its parent's table");
		}

		auto inner_scope = std::move(*self);
		self = inner_scope.parent_scope;

		// Try to find duplicate
		for (const auto &entry : self->table)
		{
			if (std::holds_alternative<Symbol>(entry))
			{
				auto &dup = std::get<Symbol>(entry);
				if (dup.id == symbol.id)
					return Error{symbol.defined_at, "duplicate identifier at {}", dup.defined_at};
			}
		}

		// Last entry in the parent scope is this subscope
		// Replace with the decl after eating up all the parameters
		// Which should be eaten up by Parser::decl_proc_params_end
		self->table.pop_back();
		auto sym = push_entry<Symbol>(self);
		*sym = symbol;

		// move inner scope below and push it
		self->table.emplace_back(inner_scope);
		self = &std::get<Scope>(self->table.back());
		return sym;
	}


	Scope *
	scope_push(Scope *&self)
	{
		auto parent = self;

		auto inner = push_entry<Scope>(self);
		inner->parent_scope = self;
		inner->idx_in_parent_table = self->table.size()-1;

		self = inner;
		return parent;
	}

	Result<Symbol *>
	scope_return_matches_proc_sym(Scope *self, Semantic_Expr type)
	{
		// Trace scope upwards until a function is found
		for (auto scope = self; scope != nullptr; scope = scope->parent_scope)
		{
			auto sym = scope->proc_sym;
			if (sym == nullptr)
				continue;

			if (sym->type.procedure->return_type != type)
			{
				return Error{"type mismatch"};
			}
			else
			{
				return sym; // valid, return symbol to proc
			}
		}

		return Error{"not within a function"};
	}

	Scope *
	scope_pop(Scope *&self)
	{
		for (const auto &entry : self->table)
		{
			if (std::holds_alternative<Symbol>(entry))
			{
				auto &sym = std::get<Symbol>(entry);

				if (sym.is_used == false)
					parser_log(Error{ sym.defined_at, "unused identifier" }, Log_Level::WARNING);
			}
		}

		auto inner = self;
		self = self->parent_scope;
		return inner;
	}

	Symbol *
	scope_get_sym(Scope *self, const char *id)
	{
		size_t scope_idx_in_parent = self->table.size();
		for (auto scope = self; scope != nullptr; scope = scope->parent_scope)
		{
			for (size_t i = 0; i < scope_idx_in_parent; i++)
			{
				auto &entry = scope->table[i];
				if (std::holds_alternative<Symbol>(entry))
				{
					auto &sym = std::get<Symbol>(entry);

					if (sym.id == id)
						return &sym;
				}
			}

			scope_idx_in_parent = scope->idx_in_parent_table;
		}

		return nullptr;
	}

	Procedure
	scope_make_proc(Scope *self, Semantic_Expr return_type)
	{
		Procedure proc = { .return_type = return_type };

		proc.parameters = Buf<Semantic_Expr>::make(self->table.size());
		for (size_t i = 0; i < proc.parameters.count; i++)
		{
			auto &param = std::get<Symbol>(self->table[i]);
			param.is_set = true;
			proc.parameters[i] = param.type;
		}

		return proc;
	}

	// symbol_id/sub-block, symbol_type, symbol_location, symbol flags
	UI_Symbol_Table
	scope_get_ui_table(const Scope *self)
	{
		UI_Symbol_Table table = { .scope = self };
		for (const auto &entry : self->table)
		{
			if (std::holds_alternative<Symbol>(entry))
			{
				auto &sym = std::get<Symbol>(entry);
				table->emplace_back(UI_Symbol_Row{});

				auto &symbol_row = std::get<UI_Symbol_Row>(table->back());
				symbol_row[0] = std::string{sym.id.data};
				symbol_row[1] = std::format("{}", sym.type);
				symbol_row[2] = std::format("{}", sym.defined_at);
				symbol_row[3] = std::format("{}/{}/{}",
					sym.is_constant	? 'c' : '-',
					sym.is_set		? 'i' : '-',
					sym.is_used		? 'u' : '-'
				);
			}
			else
			{
				auto &scope = std::get<Scope>(entry);
				table->push_back(&scope);
			}
		}
		return table;
	}
}