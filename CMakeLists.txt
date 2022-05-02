cmake_minimum_required(VERSION 3.20)

project(S22-Compilers LANGUAGES CXX)

add_subdirectory(thirdparty/imgui-1.85)
add_subdirectory(thirdparty/portable-file-dialogs-0.1.0)

get_target_property(IMGUI_SOURCES imgui INTERFACE_SOURCES)
get_target_property(IMGUI_INCLUDE_DIRS imgui INTERFACE_INCLUDE_DIRECTORIES)
get_target_property(IMGUI_LINK_LIBS imgui INTERFACE_LINK_LIBRARIES)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

message("${IMGUI_INCLUDE_DIRS}")

include_directories(
	${CMAKE_SOURCE_DIR}/compiler/include
	${IMGUI_INCLUDE_DIRS}
)

link_libraries(${IMGUI_LINK_LIBS} portable_file_dialogs)

# Visual Studio hot reloading
if (MSVC AND WIN32 AND NOT MSVC_VERSION VERSION_LESS 142)
    add_link_options($<$<CONFIG:Debug>:/INCREMENTAL>)
    add_compile_options($<$<CONFIG:Debug>:/ZI>)
endif()

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
	compiler/src/compiler/AST.cpp
	compiler/src/compiler/Backend.cpp
	compiler/src/compiler/Symbol.cpp
	compiler/src/compiler/Semantic_Expr.cpp
	compiler/src/compiler/Parser.cpp
	compiler/src/compiler/Window.cpp
	compiler/src/compiler/main.cpp
)

set(COMPILER_HEADER_FILES
	compiler/include/compiler/Util.h
	compiler/include/compiler/AST.h
	compiler/include/compiler/Backend.h
	compiler/include/compiler/Symbol.h
	compiler/include/compiler/Semantic_Expr.h
	compiler/include/compiler/Parser.h
	compiler/include/compiler/Window.h
)

# Main compiler
add_executable(compiler
	${LEXER}
	${PARSER}
	${COMPILER_SOURCE_FILES}
	${COMPILER_HEADER_FILES}
	${IMGUI_SOURCES}
)