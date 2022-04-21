#pragma once

#include <stddef.h>

#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

template <typename T>
void
free_destruct(T* &ptr)
{
	if (ptr == nullptr)
        return;

    ptr->~T();
    free(ptr);
    ptr = nullptr;
}

template <typename T>
T*
alloc()
{
    return (T *)malloc(sizeof(T));
}

namespace AST
{
	using Value = unsigned long long;

	struct Symbol
	{
		enum TYPE
		{
			INT,
			UINT,
			FLOAT,
			BOOL,

			PROC,
		};

		TYPE type;
		std::string name;
		bool is_constant;
	};

	struct Symbol_Table
	{
		std::unordered_map<std::string_view, Symbol> hash_table;
		std::vector<Symbol_Table> inner_scopes;
		Symbol_Table *parent_scope;
	};

	struct Keyword
	{
		enum KIND
		{
			CONST,
			IF,
			ELSE,
			WHILE,
			DO,
			FOR,
			SWITCH,
			CASE,
			PROC,
			RETURN,
			TRUE,
			FALSE,
		};

		KIND kind;
	};

	struct Constant
	{
		// 64 bits for all constants
		Value value;
	};

	struct Identifier
	{
		// Index in symbol table
		std::string_view name;
	};

	struct Node;
	struct Operation_Assignment
	{
		enum KIND
		{
			NOP, // A = B

			ADD, // A += B
			SUB, // A -= B
			MUL, // A *= B
			DIV, // A /= B
			MOD, // A %= B

			AND, // A &= B
			OR,  // A |= B
			XOR, // A ^= B
			SHL, // A <<= B
			SHR, // A >>= B
		};

		KIND kind;
		Identifier *left;
		Node *right;

        ~Operation_Assignment();
    };

    struct Operation_Binary
	{
		enum KIND
		{
			// Mathematical
			ADD, // A + B
			SUB, // A - B
			MUL, // A * B
			DIV, // A / B
			MOD, // A % B

			AND, // A & B
			OR,  // A | B
			XOR, // A ^ B
			SHL, // A << B
			SHR, // A >> B

			// Logical
			LT,  // A < B
			LEQ, // A <= B

			EQ,  // A == B
			NEQ, // A != B

			GT,  // A > B
			GEQ, // A >= B
		};

		KIND kind;
		Node *left;
		Node *right;

        ~Operation_Binary();
    };

	struct Operation_Unary
	{
		enum KIND
		{
			// Mathematical
			NEG, // -A

			// Logical
			NOT, // !A
			INV, // ~A
		};

		KIND kind;
		Node *right;

        ~Operation_Unary();
    };

    struct Node
	{
		enum KIND
		{
			CONSTANT,
			IDENTIFIER,
			OP_BINARY,
			OP_ASSIGN,
			OP_UNARY,
		};

		KIND kind;
		union
		{
			Constant as_const;
			Identifier as_id;
			Operation_Binary as_binary;
			Operation_Assignment as_assign;
			Operation_Unary as_unary;
		};

        ~Node();


		template <typename T, typename... Args>
		static Node*
		create(Args... args)
		{
            auto self = alloc<Node>();
			if constexpr (std::is_same_v<T, Constant>)
			{
                self->kind = Node::CONSTANT;
                self->as_const = {args...};
            }
            else if constexpr (std::is_same_v<T, Identifier>)
			{
                self->kind = Node::IDENTIFIER;
                self->as_id = {args...};
            }
			else if constexpr (std::is_same_v<T, Operation_Binary>)
			{
                self->kind = Node::OP_BINARY;
                self->as_binary = {args...};
            }
			else if constexpr (std::is_same_v<T, Operation_Assignment>)
			{
                self->kind = Node::OP_ASSIGN;
                self->as_assign = {args...};
            }
			else if constexpr (std::is_same_v<T, Operation_Unary>)
			{
                self->kind = Node::OP_UNARY;
                self->as_unary = {args...};
            }

            return self;
        }
    };
}