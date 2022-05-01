#include "compiler/Symbol.h"
#include "compiler/Util.h"
#include "compiler/Semantic_Expr.h"
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
	location_reduce(Source_Location &current, Source_Location *rhs, size_t N)
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
	location_update(Source_Location *loc)
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
	location_print(FILE *out, const Source_Location *const loc)
	{
		auto str = std::format("{}", *loc);
		fprintf(out, "%s", str.data());
	}

	bool
	symtype_allows_arithmetic(const Symbol_Type &type)
	{
		// Arrays are different from array access Expr{arr} != Expr{arr[i]}
		return (type.array || type.procedure) == false;
	}

	bool
	symtype_is_integral(const Symbol_Type &type)
	{
		constexpr Symbol_Type SYMTYPE_INT  = { .base = Symbol_Type::INT };
		constexpr Symbol_Type SYMTYPE_UINT = { .base = Symbol_Type::UINT };

		return type == SYMTYPE_INT || type == SYMTYPE_UINT || type == SYMTYPE_BOOL;
	}

	void
	symtype_print(FILE *out, const Symbol_Type &type)
	{
		auto fmt = std::format("{}", type);
		fprintf(out, "%s", fmt.data());
	}

	template<typename T>
	inline static T *
	push_entry(Scope *self)
	{
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

		if (symbol.type != expr.semexpr.type)
			return Error{expr.loc, "type mismatch"};

		auto [sym, sym_err] = scope_add_decl(self, symbol);
		if (sym_err)
			return sym_err;

		sym->is_set = true;
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
	scope_return_matches_proc_sym(Scope *self, Symbol_Type type)
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
	scope_make_proc(Scope *self, Symbol_Type return_type)
	{
		Procedure proc = {};
		proc.parameters = Buf<Symbol_Type>::make(self->table.size());
		for (size_t i = 0; i < proc.parameters.count; i++)
		{
			auto &param = std::get<Symbol>(self->table[i]);
			proc.parameters[i] = param.type;
		}

		if (return_type.base != Symbol_Type::VOID)
		{
			proc.return_type = return_type;
		}

		return proc;
	}
}

template <>
struct std::formatter<s22::Symbol_Type> : std::formatter<std::string>
{
	auto
	format(const s22::Symbol_Type &type, format_context &ctx)
	{
		using namespace s22;
		if (type.procedure)
		{
			format_to(ctx.out(), "proc(");
			{
				format_to(ctx.out(), "{}", type.procedure->parameters);
			}
			format_to(ctx.out(), ")");

			if (type.procedure->return_type != SYMTYPE_VOID)
			{
				format_to(ctx.out(), " -> ");
				format_to(ctx.out(), "{}", type.procedure->return_type);
			}
		}
		else
		{
			if (type.array)
			{
				format_to(ctx.out(), "[{}]", type.array);
			}

			switch (type.base)
			{
			case s22::Symbol_Type::INT: return format_to(ctx.out(), "int");
			case s22::Symbol_Type::UINT: return format_to(ctx.out(), "uint");
			case s22::Symbol_Type::FLOAT: return format_to(ctx.out(), "float");
			case s22::Symbol_Type::BOOL: return format_to(ctx.out(), "bool");
			default: break;
			}
		}
		return ctx.out();
	}
};