#pragma once

#include "compiler/Util.h"

#include <string>
#include <vector>
#include <variant>
#include <format>

namespace s22
{
	struct Symbol_Type;
	struct Procedure
	{
		Buf<Symbol_Type> parameters;
		Optional<Symbol_Type> return_type;

		inline bool
		operator==(const Procedure &other) const
		{
			if (parameters != other.parameters)
				return false;

			if (return_type != other.return_type)
				return false;

			return true;
		}
	};

	struct Symbol_Type
	{
		enum BASE
		{
			VOID,
			PROC,

			INT,
			UINT,
			FLOAT,
			BOOL,

			// INTERNAL
			ERROR,
		};

		BASE base;
		Optional<size_t> array;
		Optional<size_t> pointer;
		Optional<Procedure> procedure;

		inline bool
		operator==(const Symbol_Type &other) const
		{
			if (base != other.base)
				return false;

			if (procedure != other.procedure)
			{
				return false;
			}
			else
			{
				if (array != other.array)
					return false;

				if (pointer != other.pointer)
					return false;
			}

			return true;
		}

		inline bool
		operator!=(const Symbol_Type &other) const { return !operator==(other); }
	};

	constexpr Symbol_Type SYMTYPE_ERROR = { .base = Symbol_Type::ERROR };
	constexpr Symbol_Type SYMTYPE_INT   = { .base = Symbol_Type::INT };
	constexpr Symbol_Type SYMTYPE_UINT  = { .base = Symbol_Type::UINT };
	constexpr Symbol_Type SYMTYPE_VOID  = { .base = Symbol_Type::VOID };

	void
	symtype_print(FILE *out, const Symbol_Type &type);

	struct Symbol
	{
		std::string id;
		Symbol_Type type;
		Location defined_at;

		bool is_constant;
		bool is_set;
		bool is_used;
	};

	struct Scope
	{
		using Entry = std::variant<Symbol, Scope>;
		std::vector<Entry> table;

		Scope *parent_scope;
		size_t idx_in_parent_table;

		Optional<Symbol_Type> procedure;
	};

	Result<Symbol *>
	scope_add_decl(Scope *self, const Symbol &symbol);

	struct Expr;
	Result<Symbol *>
	scope_add_decl(Scope *self, const Symbol &symbol, Expr &expr);

	Scope *
	scope_push(Scope *&self);

	Error
	scope_return_is_valid(Scope *self, Symbol_Type type);

	Scope *
	scope_pop(Scope *&self);

	Symbol *
	scope_get_id(Scope *self, const char *id);

	Procedure
	scope_make_proc(Scope *self, Symbol_Type return_type);
}

template<>
struct std::formatter<s22::Location> : std::formatter<std::string>
{
	auto
	format(s22::Location loc, format_context &ctx)
	{
		return format_to(ctx.out(), "{},{}", loc.first_line, loc.first_column);
	}
};

template<>
struct std::formatter<s22::Symbol_Type> : std::formatter<std::string>
{
	auto
	format(const s22::Symbol_Type &type, format_context &ctx)
	{
		if (type.procedure)
		{
			format_to(ctx.out(), "proc(");
			{
				format_to(ctx.out(), "{}", type.procedure->parameters);
			}
			format_to(ctx.out(), ")");

			if (type.procedure->return_type)
			{
				format_to(ctx.out(), " -> ");
				format_to(ctx.out(), "{}", type.procedure->return_type);
			}
		}
		else
		{
			if (type.array)
			{
				format_to(ctx.out(), "[{}]", *type.array);
			}

			if (type.pointer)
			{
				for (size_t i = 0; i < *type.pointer; i++)
					format_to(ctx.out(), "*");
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