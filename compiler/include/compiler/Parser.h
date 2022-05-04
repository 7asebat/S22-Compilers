#pragma once

#include "compiler/AST.h"
#include "compiler/Backend.h"
#include "compiler/Semantic_Expr.h"
#include "compiler/Symbol.h"

#include <stack>
#include <unordered_set>

typedef struct yy_buffer_state *YY_BUFFER_STATE;

namespace s22
{
	// Defined in Lexer.l
	// Scan a null-terminated in-memory buffer
	YY_BUFFER_STATE
	lexer_scan_buffer(char *buf, size_t buf_size);

	// Cleanup allocated buffer
	void
	lexer_delete_buffer(YY_BUFFER_STATE buf);

	// UI buffer which holds the source code
	struct UI_Source_Code
	{
		char buf[1<<13 + 2];// 8KB + 2 needed by flex
		size_t count;
	};

	// Main entity that represents expressions, parser non-terminals are of this type
	struct Parse_Unit
	{
		Semantic_Expr semexpr;	// carries semantic information
		AST ast;				// AST representation
		Source_Location loc;	// location in the source file
		Error err;				// whether the unit has encountered errors
	};

	// Parser token type
	union YY_Symbol
	{
		String id;					// identifiers
		Literal value;			// literals
		Semantic_Expr type;		// variable types
		Parse_Unit unit;		// main non-terminals
	};

	struct Parser
	{
		/* Bison semantic action hooks */
		void
		program_begin();

		void
		program_end();

		UI_Program
		program_write();

		void
		dispose();

		Semantic_Expr
		type_array(Source_Location loc, const Parse_Unit &literal, const Semantic_Expr &type_base);

		void
		block_begin();

		void
		block_add(const Parse_Unit &stmt);

		Parse_Unit
		block_end();

		Parse_Unit
		return_value(Source_Location loc);

		Parse_Unit
		return_value(Source_Location loc, const Parse_Unit &expr);

		Parse_Unit
		literal(Source_Location loc, Literal lit, Semantic_Expr::BASE base);

		Parse_Unit
		id(Source_Location loc, const String &id);

		Parse_Unit
		array_access(Source_Location loc, const String &id, const Parse_Unit &right);

		Parse_Unit
		assign(Source_Location loc, const String &id, Asn op, const Parse_Unit &right);

		Parse_Unit
		array_assign(Source_Location loc, const Parse_Unit &left, Asn op, const Parse_Unit &right);

		Parse_Unit
		binary(Source_Location loc, const Parse_Unit &left, Bin op, const Parse_Unit &right);

		Parse_Unit
		unary(Source_Location loc, Uny op, const Parse_Unit &right);

		void
		pcall_begin();

		void
		pcall_add(const Parse_Unit &arg);

		Parse_Unit
		pcall(Source_Location loc, const String &id);

		Parse_Unit
		decl(Source_Location loc, const String &id, Semantic_Expr type);

		Parse_Unit
		decl_expr(Source_Location loc, const String &id, Semantic_Expr type, const Parse_Unit &right);

		Parse_Unit
		decl_const(Source_Location loc, const String &id, Semantic_Expr type, const Parse_Unit &right);

		void
		decl_proc_begin();

		void
		decl_proc_params_add(const Parse_Unit &arg);

		void
		decl_proc_params_end(Source_Location loc, const String &id, const Semantic_Expr &ret);

		Parse_Unit
		decl_proc_end(const String &id);

		Parse_Unit
		if_cond(const Parse_Unit &cond, const Parse_Unit &block, const Parse_Unit &next);

		Parse_Unit
		else_if_cond(Parse_Unit &prev, const Parse_Unit &cond, const Parse_Unit &block);

		void
		switch_begin(const Parse_Unit &expr);

		Parse_Unit
		switch_end(Source_Location loc);

		void
		switch_case_begin(Source_Location loc);

		void
		switch_case_end(Source_Location loc, const Parse_Unit &block);

		void
		switch_case_add(const Parse_Unit &literal);

		void
		switch_default(Source_Location loc, const Parse_Unit &block);

		Parse_Unit
		while_loop(Source_Location loc, const Parse_Unit &cond, const Parse_Unit &block);

		Parse_Unit
		do_while_loop(Source_Location loc, const Parse_Unit &cond, const Parse_Unit &block);

		void
		for_loop_begin();

		Parse_Unit
		for_loop(Source_Location loc, const Parse_Unit &init, const Parse_Unit &cond, const Parse_Unit &post);

		~Parser();

		struct Context
		{
			Scope *scope;
			std::vector<Parse_Unit> proc_call_arguments;
			std::vector<Decl *> decl_proc_arguments;
			std::vector<AST> block_stmts;
			size_t stack_offset;

			struct Sw_Case
			{
				std::vector<Literal *> group;
				Switch_Case *ast_sw_case;
			};
			AST switch_expr;
			std::vector<Sw_Case> switch_cases;
			Block *switch_default;
		};
		std::stack<Context> context;
		Scope global;
		Backend backend;
		
		// UI elements
		std::vector<std::string> ui_logs;
		UI_Source_Code ui_source_code;
		UI_Program ui_program;
		UI_Symbol_Table ui_table;

		bool has_errors;
	};

	enum class Log_Level
	{
		INFO,
		WARNING,
		ERROR,
		CRITICAL,
	};

	// Parser singleton instance
	Parser*
	parser_instance();

	// Log an error, uses the error's location
	void
	parser_log(const Error &err, Log_Level lvl = Log_Level::ERROR);
}

// Error handler used by Bison/Flex
// Uses parser_log
void
yyerror(const s22::Source_Location *location, s22::Parser *p, const char *message);
