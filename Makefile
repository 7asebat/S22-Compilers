compile:
	mkdir -p build
	cp src/Common.h build/Common.h
	flex -o build/Lexer.c src/Lexer.l
	bison -d -o build/parser.c -v src/Parser.y 
	g++ build/*.c -o build/out -w -std=c++17 -g

run:
	build/out examples/full.program

debug:
	build/out examples/full.program DEBUG

clean:
	rm -rf build/*

bt: compile run
cl: clean bt