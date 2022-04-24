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
	op_assign_evaluate(const Op_Assign &self, Symbol_Table *scope)
	{
		auto left = self.left->type;

		Symbol_Type right = self.right->type;
		if (right == Symbol_Type{Symbol_Type::VOID})
		{
			auto [eval_right, err] = expr_evaluate(*self.right, scope);
			if (err)
				return err;
			right = eval_right;
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
	op_binary_evaluate(const Op_Binary &self, Symbol_Table *scope)
	{
		Symbol_Type left = self.left->type;
		if (left == Symbol_Type{Symbol_Type::VOID})
		{
			auto [eval_left, err] = expr_evaluate(*self.left, scope);
			if (err)
				return err;
			left = eval_left;
		}

		Symbol_Type right = self.right->type;
		if (right == Symbol_Type{Symbol_Type::VOID})
		{
			auto [eval_right, err] = expr_evaluate(*self.right, scope);
			if (err)
				return err;
			right = eval_right;
		}

		if (left != right)
			return Error{ "type mismatch" };

		if (left == SYMTYPE_ERROR || right == SYMTYPE_ERROR)
			return SYMTYPE_ERROR;

		return type_allows_operation(left);
	}

	Result<Symbol_Type>
	op_unary_evaluate(const Op_Unary &self, Symbol_Table *scope)
	{
		Symbol_Type right = self.right->type;
		if (right == Symbol_Type{Symbol_Type::VOID})
		{
			auto [eval_right, err] = expr_evaluate(*self.right, scope);
			if (err)
				return err;
			right = eval_right;
		}

		if (right == SYMTYPE_ERROR)
			return SYMTYPE_ERROR;

		return type_allows_operation(right);
	}

	Result<Symbol_Type>
	expr_evaluate(Expr &expr, Symbol_Table *scope)
	{
		switch (expr.kind)
		{
		case Expr::CONSTANT:
			expr.type = Symbol_Type{ Symbol_Type::ANY };
			return expr.type;

		case Expr::IDENTIFIER:
			if (auto sym = symtab_get_id(scope, expr.as_id.id); sym == nullptr)
			{
				expr = EXPR_ERROR;
				return Error{ "undeclared identifier" };
			}
			else
			{
				expr.type = sym->type;
				return expr.type;
			}

		case Expr::OP_ASSIGN:
			if (auto [type, err] = op_assign_evaluate(expr.as_assign, scope); err)
			{
				expr = EXPR_ERROR;
				return err;
			}
			else
			{
				expr.type = type;
				return expr.type;
			}

		case Expr::OP_BINARY:
			if (auto [type, err] = op_binary_evaluate(expr.as_binary, scope); err)
			{
				expr = EXPR_ERROR;
				return err;
			}
			else
			{
				expr.type = type;
				return expr.type;
			}

		case Expr::OP_UNARY:
			if (auto [type, err] = op_unary_evaluate(expr.as_unary, scope); err)
			{
				expr = EXPR_ERROR;
				return err;
			}
			else
			{
				expr.type = type;
				return expr.type;
			}

		default: return Error{ "unrecognized type" };
		}
	}

	Expr
	constant_new(uint64_t value)
	{
		Expr self = { Expr::CONSTANT };
		self.as_const = { value };
		self.type = { Symbol_Type::ANY };

		return self;
	}

	Result<Expr>
	identifier_new(Symbol_Table *scope, const char *id)
	{
		Expr self = { Expr::IDENTIFIER };
		auto &identifier = self.as_id;

		if (auto sym = symtab_get_id(scope, id); sym == nullptr)
		{
			return Error{ "undeclared identifier" };
		}
		else
		{
			identifier.id = id;
			self.type = sym->type;
			sym->is_used = true;
		}

		return self;
	}

	Result<Expr>
	op_assign_new(Symbol_Table *scope, const char *id, Op_Assign::KIND kind, Expr &right)
	{
		Expr self = { Expr::OP_ASSIGN };
		auto &assign = self.as_assign;

		if (auto sym = symtab_get_id(scope, id); sym == nullptr)
		{
			return Error{ "undeclared identifier" };
		}
		else
		{
			assign.left = sym;
		}

		assign.kind = kind;
		assign.right = &right;

		if (auto [_, err] = expr_evaluate(self, scope); err)
			return err;

		return self;
	}

	Result<Expr>
	op_binary_new(Symbol_Table *scope, Expr &left, Op_Binary::KIND kind, Expr &right)
	{
		Expr self = { Expr::OP_BINARY };
		auto &binary = self.as_binary;
		binary.kind = kind;
		binary.left = &left;
		binary.right = &right;

		if (auto [_, err] = expr_evaluate(self, scope); err)
			return err;

		return self;
	}

	Result<Expr>
	op_unary_new(Symbol_Table *scope, Op_Unary::KIND kind, Expr &right)
	{
		Expr self = { Expr::OP_UNARY };
		auto &unary = self.as_unary;
		unary.kind = kind;
		unary.right = &right;

		if (auto [_, err] = expr_evaluate(self, scope); err)
			return err;

		return self;
	}
}