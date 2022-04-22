compile:
	mkdir -p build
	flex -o build/Lexer.c src/Lexer.l
	bison -d -o build/Parser.c -v src/Parser.y 
	g++ build/*.c -o build/out -w -std=c++17 -g

ast:
	g++ src/AST/*.cpp -std=c++17 -g -o build/ast

run:
	build/out examples/full.program

debug:
	build/out examples/full.program DEBUG

clean:
	rm -rf build/*