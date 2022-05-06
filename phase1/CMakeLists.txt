cmake_minimum_required(VERSION 3.20)

set(CMAKE_CXX_STANDARD 23)

set(PHASE1_L ${CMAKE_CURRENT_SOURCE_DIR}/Lexer.l)
set(PHASE1_Y ${CMAKE_CURRENT_SOURCE_DIR}/Parser.y)

set(PHASE1_LEXER ${CMAKE_BINARY_DIR}/p1_Lexer.cpp)
add_custom_command(
	OUTPUT ${PHASE1_LEXER}
	COMMAND flex -o ${PHASE1_LEXER} ${PHASE1_L}
	DEPENDS ${PHASE1_L}
)

set(PHASE1_PARSER ${CMAKE_BINARY_DIR}/p1_Parser.cpp)
add_custom_command(
	OUTPUT ${PHASE1_PARSER} ${CMAKE_BINARY_DIR}/p1_Parser.hpp
	COMMAND bison -d -o ${PHASE1_PARSER} ${PHASE1_Y}
	DEPENDS ${PHASE1_Y}
)

add_executable(phase1
	${PHASE1_LEXER}
	${PHASE1_PARSER}
)