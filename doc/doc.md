# Team-17
## Members
| Name                            | Sec | BN  |
| ------------------------------- | --- | --- |
| Ahmed Nasser Abdrabo            | 1   | 8   |
| Ahmed Hesham Eid                | 1   | 9   |
| Abdelrahman Ahmed Mohamed Farid | 1   | 33  |
| Youssef Walid Hassan            | 2   | 34  |

## Project Overview
- The project source code is in: `/compiler`
- A sample program is given in: `/examples/sample.program`
- Documentation is in `/doc`
- The project source code consists of the following modules:

| Module            | Function                                                                                                      | Files                 |
| ----------------- | ------------------------------------------------------------------------------------------------------------- | --------------------- |
| Lexer(**Flex**)   | Receives text and produces tokens to the parser                                                               | `Lexer.l`             |
| Parser(**Bison**) | Receives tokens from the Lexer and calls functions using semantic actions that do the bulk of the compilation | `Parser.y`            |
| Parser(**C++**)   | The main unit of the compiler, receives calls from Parser(**Bison**)                                          | `Parser.h/cpp`        |
| Semantic_Expr     | Handles semantic rules for different expressions                                                              | `Semantic_Expr.h/cpp` |
| Symbol            | Holds the symbol table for the compiler                                                                       | `Symbol.h/cpp`        |
| AST               | What the parser produces. An abstract syntax tree that represents a valid program                             | `AST.h/cpp`           |
| Backend           | Performs code generation using a hybrid of x86 and MIPS instructions. **NOT USED FOR THIS PROJECT** ❌        | `Backend.h/cpp`       |
| Backend2          | Performs code generation using Quadruples. **USED FOR THIS PROJECT** ✅                                       | `Backend2.h/cpp`      |
| Window            | Holds the GUI for the application. **Windows Only**                                                           | `Window.h/cpp`        |
### Compilation Steps
1. A buffer which holds the source code string is filled by the GUI
2. Parser entry point `yyparse(s22::Parser *)` is called
3. The Parser receives tokens from the Lexer and runs the Bottom-Up parsing algorithm
4. With each reduction, an action rule is called that handles the parsing logic for that unit. Parsing logic involves:
	1. Semantic rule checking
	2. Adding declarations to the symbol table
	3. Building abstract syntax tree
5. Once the program has finished parsing, the result is an AST that describes the program
6. The AST is then passed to the Backend which produces the quadruples

### Semantic Rules Implemented
1. Use before declaration
2. Use before initialization
3. Unused variables
4. Operation type mismatch
5. Assignment to constant
6. Operand validity (*i.e. Addition to procedure*)
7. Array access
8. Function call parameters
9. Function call return types
### Bonus Features
1. Block structure
2. Functions
3. Arrays

## Tools and Technologies
| Module | Tool/Technology |
| ------ | --------------- |
| GUI    | ImGUI           |
| Lexer  | Flex 2.6.4      |
| Parser | Bison 3.8.2     |

## Tokens
### Keywords
- CONST WHILE DO FOR SWITCH CASE DEFAULT PROC RETURN
- TRUE FALSE
- INT UINT FLOAT BOOL
- IF ELSE
### Operators
| Token                                                                         | Description                                                       |
| ----------------------------------------------------------------------------- | ----------------------------------------------------------------- |
| L_AND, L_OR, LEQ, EQ, NEQ, GEQ                                                | Logical Operators                                                 |
| SHL, SHR                                                                      | Bitwise Shift Operators                                           |
| AS_ADD, AS_SUB, AS_MUL, AS_DIV, AS_MOD, AS_AND, AS_OR, AS_XOR, AS_SHL, AS_SHR | Assignment Operators                                              |
| ARROW                                                                         | Separates function declaration parameters from return type        |
| DBL_COLON                                                                     | Separates the `proc` keyword from function declaration parameters | 
### Others
| Token                        | Description                 |
| ---------------------------- | --------------------------- |
| IDENTIFIER                   | Identifier                  |
| LIT_INT, LIT_UINT, LIT_FLOAT | Literals of different types |
### Non-terminals
| Token              | Description                                     |
| ------------------ | ----------------------------------------------- |
| program            | Main token that represents the complete program |
| stmt               | Single statement                                |
| literal            | Literal of different types                      |
| proc_call          | Function call                                   |
| array_access       | Array identifier access with index              |
| expr               | Expression                                      |
| expr_math          | Mathematical expression                         |
| expr_logic         | Logical expression                              |
| type               | Any data type                                   |
| type_base          | Base data type                                  |
| type_array         | Array data type                                 |
| decl_proc_return   | Procedure return type                           |
| conditional        | Any conditional block                           |
| c_else             | Else block                                      |
| c_else_ifs         | List of else if blocks                          |
| assignment         | Any assignment operation                        |
| block              | Scoped data block                               |
| loop               | Any loop block                                  |
| loop_for_init      | For loop initial statement                      |
| loop_for_condition | For loop condition                              |
| loop_for_post      | For loop post expression                        |
| decl_var           | Any variable declaration                        |
| decl_const         | Any constant variable declaration               |
| decl_proc          | Any procedure declaration                       |

## Quadruples
> ### _Notes_
> - `V` is either a variable (`x`), a temporary (`t3`), or an immediate value (`42`)
> - `L` is a label

| op      | dst  | arg1 | arg2 | Description                                   |
| ------- | ---- | ---- | ---- | --------------------------------------------- |
| `OP`    | `Rx` | `V`  |      | `Rx = op V`                                   |
| `OP`    | `Rx` | `V1` | `V2` | `Rx = V1 op V2`                               |
| `BR`    | `L`  |      |      | Branch to `L`                                 |
| `Bcond` | `L`  | `V1` |      | Branch to `L` on `cond V1`                    |
| `Bcond` | `L`  | `V1` | `V2` | Branch to `L` on `V1 cond V2`                 |
| `CALL`  | `F`  |      |      | Push return address onto stack, branch to `F` |
| `RET`   |      |      |      | Pop return address and branch to it           |

### Operations
| Op  | Description                  |
| --- | ---------------------------- |
| =   | dst = src1                   |
| neg | dst = -src1                  |
| ~   | dst = ~src1                  |
| +   | dst = src1 + src2            |
| BLT | Branch to dst on src1 < src2 |
| BZ  | Branch to dst on src1 == 0   |
| BNZ | Branch to dst on src1 != 0   |
