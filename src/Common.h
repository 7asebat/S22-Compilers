#pragma once

#include <stddef.h>

#include <string>
#include <unordered_map>
#include <vector>

using Value = unsigned long long;

struct Symbol
{
    enum TYPE
    {
        UNSIGNED,
        SIGNED,
        FLOAT,
        BOOL,

        PROC,
    };

    std::string name;
    bool is_constant;
    TYPE type;
};

struct Symbol_Table
{
    std::unordered_map<std::string_view, Symbol> hash_table;
    std::vector<Symbol_Table> inner_scopes;
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

    Identifier left;
    KIND kind;
    Node *right;
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

    Node *left;
    KIND kind;
    Node *right;
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
};

struct Node
{
	enum KIND
	{
		CONSTANT,
		IDENTIFIER,
		OP_BINARY,
		OP_UNARY,
    };

    KIND kind;
	union
	{
        Constant as_const;
        Identifier as_id;
        Operation_Binary as_binary;
        Operation_Unary as_unary;
    };
};