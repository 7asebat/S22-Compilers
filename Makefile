compile:
	cp src/Common.h build/Common.h
	bison -d src/parser.y -o build/parser.c -v
	flex -o build/lexer.c src/lexer.l 
	g++ build/*.c -o build/out -w -std=c++17 -g

run:
	build/out examples/full.program 

clean:
	rm -rf build/*

bt: compile run
cl: clean bt