
all: src/lexer.c src/lexer.h src/lexer_enum.h src/enum_to_string.c src/parser.c src/parser.h src/ode_compiler.c src/code_generation.h src/stb_ds.h
	@if [ ! -d "./bin" ]; then mkdir "./bin"; fi
	gcc -g src/lexer.c src/enum_to_string.c src/parser.c src/ode_compiler.c -o bin/compiler

clean:
	rm -fr bin/
