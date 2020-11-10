
all:
	@if [ ! -d "./bin" ]; then mkdir "./bin"; fi
	gcc -g src/lexer.c src/enum_to_string.c src/parser.c src/ode_compiler.c -o bin/compiler

clean:
	rm -fr bin/
