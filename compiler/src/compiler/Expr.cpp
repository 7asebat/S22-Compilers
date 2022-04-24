#include "compiler/Expr.h"

namespace s22
{
	inline static Result<Symbol_Type>
	type_allows_operation(const Symbol_Type &self)
	{
		if (self.array || self.procedure || self.pointer)
			return Error{ "invalid type" };

		return self;
	}

	Result<Symbol_Type>
	op_assign_evaluate(Op_Assign &self, Scope *scope)
	{
		auto left = self.left->type;

		Symbol_Type right = self.right->type;
		if (right == SYMTYPE_VOID)
		{
			auto err = expr_evaluate(*self.right, scope);
			if (err)
				return err;
			right = self.right->type;
		}

		if (left != right)
			return Error{ "type mismatch" };

		if (self.left->is_constant)
			return Error{ "assignment to constant" };

		if (right == SYMTYPE_ERROR)
			return SYMTYPE_ERROR;

		return type_allows_operation(left);
	}

	Result<Symbol_Type>
	op_binary_evaluate(Op_Binary &self, Scope *scope)
	{
		Symbol_Type left = self.left->type;
		if (left == SYMTYPE_VOID)
		{
			auto err = expr_evaluate(*self.left, scope);
			if (err)
				return err;
			left = self.left->type;
		}

		Symbol_Type right = self.right->type;
		if (right == SYMTYPE_VOID)
		{
			auto err = expr_evaluate(*self.right, scope);
			if (err)
				return err;
			right = self.right->type;
		}

		if (left != right)
			return Error{ "type mismatch" };

		if (left == SYMTYPE_ERROR || right == SYMTYPE_ERROR)
			return SYMTYPE_ERROR;

		return type_allows_operation(left);
	}

	Result<Symbol_Type>
	op_unary_evaluate(Op_Unary &self, Scope *scope)
	{
		Symbol_Type right = self.right->type;
		if (right == SYMTYPE_VOID)
		{
			auto err = expr_evaluate(*self.right, scope);
			if (err)
				return err;
			right = self.right->type;
		}

		if (right == SYMTYPE_ERROR)
			return SYMTYPE_ERROR;

		return type_allows_operation(right);
	}

	Result<Symbol_Type>
	proc_call_evaluate(Proc_Call &self, Scope *scope)
	{
		// Get matching symbol
		auto sym = scope_get_id(scope, self.id);
		if (sym == nullptr)
		{
			return Error{ "undeclared identifier" };
		}
		else if (sym->type.procedure == false)
		{
			return Error{ "identifier is not a procedure" };
		}
		sym->is_used = true;

		if (sym->type.procedure->parameters.count != self.params.count)
		{
			return Error{ "wrong parameter count" };
		}

		for (size_t i = 0; i < self.params.count; i++)
		{
			Symbol_Type type = self.params[i].type;
			if (self.params[i].type == SYMTYPE_VOID)
			{
				auto err = expr_evaluate(self.params[i], scope);
				if (err)
					return err;

				type = self.params[i].type;
			}

			if (type != sym->type.procedure->parameters[i])
			{
				return Error{ self.params[i].loc, "invalid argument" };
			}
		}

		return sym->type.procedure->return_type | SYMTYPE_VOID;
	}

	Error
	expr_evaluate(Expr &expr, Scope *scope)
	{
		switch (expr.kind)
		{
		case Expr::LITERAL: expr.type = { expr.as_literal.base };
			break;

		case Expr::IDENTIFIER:
			if (auto sym = scope_get_id(scope, expr.as_id.id); sym == nullptr)
			{
				expr.type = SYMTYPE_ERROR;
				return Error{ expr.loc, "undeclared identifier" };
			}
			else
			{
				expr.type = sym->type;
			}
			break;

		case Expr::OP_ASSIGN:
			if (auto[type, err] = op_assign_evaluate(expr.as_assign, scope); err)
			{
				expr.type = SYMTYPE_ERROR;
				if (err.loc == Location{})
					err.loc = expr.loc;
				return err;
			}
			else
			{
				expr.type = type;
			}
			break;

		case Expr::OP_BINARY:
			if (auto[type, err] = op_binary_evaluate(expr.as_binary, scope); err)
			{
				expr.type = SYMTYPE_ERROR;
				if (err.loc == Location{})
					err.loc = expr.loc;
				return err;
			}
			else
			{
				expr.type = type;
			}
			break;

		case Expr::OP_UNARY:
			if (auto[type, err] = op_unary_evaluate(expr.as_unary, scope); err)
			{
				expr.type = SYMTYPE_ERROR;
				if (err.loc == Location{})
					err.loc = expr.loc;
				return err;
			}
			else
			{
				expr.type = type;
			}
			break;

		case Expr::PROC_CALL:
			if (auto[type, err] = proc_call_evaluate(expr.as_proc_call, scope); err)
			{
				expr.type = SYMTYPE_ERROR;
				if (err.loc == Location{})
					err.loc = expr.loc;
				return err;
			}
			else
			{
				expr.type = type;
			}
			break;

		default: return Error{ "unrecognized type" };
		}

		return Error{};
	}

	Result<Expr>
	literal_new(Scope *scope, uint64_t value, Location loc, Symbol_Type::BASE base)
	{
		Expr self = { Expr::LITERAL };
		self.as_literal = { .base = base, .value = value };
		self.type = { base };
		self.loc = loc;

		return self;
	}

	Result<Expr>
	identifier_new(Scope *scope, const char *id, Location loc)
	{
		Expr self = { Expr::IDENTIFIER };
		self.loc = loc;
		auto &identifier = self.as_id;

		if (auto sym = scope_get_id(scope, id); sym == nullptr)
		{
			return Error{ loc, "undeclared identifier" };
		}
		else
		{
			sym->is_used = true;

			strcpy_s(identifier.id, id);
			self.type = sym->type;
		}

		return self;
	}

	Result<Expr>
	op_assign_new(Scope *scope, const char *id, Location loc, Expr &right, Op_Assign::KIND kind)
	{
		Expr self = { Expr::OP_ASSIGN };
		self.loc = loc;
		auto &assign = self.as_assign;

		if (auto sym = scope_get_id(scope, id); sym == nullptr)
		{
			return Error{ loc, "undeclared identifier" };
		}
		else
		{
			sym->is_used = true;
			assign.left = sym;
		}

		assign.kind = kind;
		assign.right = &right;

		if (auto err = expr_evaluate(self, scope))
			return err;

		return self;
	}

	Result<Expr>
	op_binary_new(Scope *scope, Expr &left, Location loc, Expr &right, Op_Binary::KIND kind)
	{
		Expr self = { Expr::OP_BINARY };
		self.loc = loc;
		auto &binary = self.as_binary;

		binary.kind = kind;
		binary.left = &left;
		binary.right = &right;

		if (auto err = expr_evaluate(self, scope))
			return err;

		return self;
	}

	Result<Expr>
	op_unary_new(Scope *scope, Expr &right, Location loc, Op_Unary::KIND kind)
	{
		Expr self = { Expr::OP_UNARY };
		self.loc = loc;
		auto &unary = self.as_unary;

		unary.kind = kind;
		unary.right = &right;

		if (auto err = expr_evaluate(self, scope))
			return err;

		return self;
	}

	Result<Expr>
	proc_call_new(Scope *scope, const char *id, Location loc, Buf<Expr> params)
	{
		Expr self = { Expr::PROC_CALL };
		self.loc = loc;
		auto &proc = self.as_proc_call;

		strcpy_s(proc.id, id);
		proc.params = params;

		if (auto err = expr_evaluate(self, scope))
			return err;

		return self;
	}
}