cmake_minimum_required(VERSION 3.20)

project(S22-Compilers LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include_directories(${CMAKE_SOURCE_DIR}/compiler/include)

# Lexer output file
set(LEXER ${CMAKE_BINARY_DIR}/Lexer.cpp)
add_custom_command(
	OUTPUT ${LEXER}
	COMMAND flex -o ${LEXER} ${CMAKE_SOURCE_DIR}/compiler/Lexer.l
	DEPENDS ${CMAKE_SOURCE_DIR}/compiler/Lexer.l
)

# Parser output file
set(PARSER ${CMAKE_BINARY_DIR}/Parser.cpp)
add_custom_command(
	OUTPUT ${PARSER} ${CMAKE_BINARY_DIR}/Parser.hpp ${CMAKE_BINARY_DIR}/Parser.output
	COMMAND bison -d -o ${PARSER} -v ${CMAKE_SOURCE_DIR}/compiler/Parser.y -Wcounterexamples
	DEPENDS ${CMAKE_SOURCE_DIR}/compiler/Parser.y
)

set(COMPILER_SOURCE_FILES
	compiler/src/compiler/Expr.cpp
	compiler/src/compiler/Symbol.cpp
	compiler/src/compiler/Parser.cpp
)

set(COMPILER_HEADER_FILES
	compiler/include/compiler/Util.h
	compiler/include/compiler/Expr.h
	compiler/include/compiler/Symbol.h
	compiler/include/compiler/Parser.h
)

# Main compiler
add_executable(compiler
	${LEXER}
	${PARSER}
	${COMPILER_SOURCE_FILES}
)

# Run compiler
add_custom_target(run
	COMMAND compiler examples/full.program
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

# Debug compiler
add_custom_target(debug
	COMMAND compiler examples/full.program DEBUG
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

add_executable(compiler-test
	${COMPILER_HEADER_FILES}
	${COMPILER_SOURCE_FILES}
	compiler/src/compiler/main.cpp
)
