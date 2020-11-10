
all:
	gcc -g lexer.c enum_to_string.c parser.c ode_compiler.c -o compiler

clean:
	rm compiler