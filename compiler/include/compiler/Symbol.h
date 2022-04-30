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
		};

		BASE base;
		size_t array;   // array size
		Optional<Procedure> procedure;

		inline bool
		operator==(const Symbol_Type &other) const
		{
			if (base != other.base)
				return false;

			if (procedure != other.procedure)
				return false;

			if (array != other.array)
				return false;

			return true;
		}

		inline bool
		operator!=(const Symbol_Type &other) const { return !operator==(other); }
	};

	constexpr Symbol_Type SYMTYPE_VOID  = { .base = Symbol_Type::VOID };
	constexpr Symbol_Type SYMTYPE_BOOL  = { .base = Symbol_Type::BOOL };

	bool
	symtype_allows_arithmetic(const Symbol_Type &type);

	bool
	symtype_is_integral(const Symbol_Type &type);

	void
	symtype_print(FILE *out, const Symbol_Type &type);

	struct Symbol
	{
		Str id;
		Symbol_Type type;
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
		size_t idx_in_parent_table;
		size_t stack_offset;

		Optional<Symbol_Type> procedure;
	};

	Result<Symbol *>
	scope_add_decl(Scope *self, const Symbol &symbol);

	struct Parse_Unit;
	Result<Symbol *>
	scope_add_decl(Scope *self, const Symbol &symbol, const Parse_Unit &expr);

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