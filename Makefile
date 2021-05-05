MKDIR_P = mkdir -p

.PHONY: directories install

all: directories ode_compiler

directories: bin_dir build_dir

bin_dir:
	${MKDIR_P} bin

build_dir:
	${MKDIR_P} build

AST=src/compiler/ast.h src/compiler/ast.c
PARSER=src/compiler/parser.c src/compiler/parser.h
LEXER=src/compiler/lexer.c src/compiler/lexer.h
TOKEN= src/compiler/token.c src/compiler/token.h
SDS=src/string/sds.c src/string/sds.h
C_CONVERTER=src/code_converter.c src/code_converter.h

ode_compiler: src/ode_compiler.c code_converter.o parser.o lexer.o ast.o token.o sds.o
	gcc -g src/ode_compiler.c build/parser.o build/lexer.o build/ast.o build/token.o build/sds.o build/code_converter.o -o bin/odec

token.o: ${TOKEN}
	gcc -g -c  src/compiler/token.c -o  build/token.o

lexer.o: ${LEXER} ${TOKEN}
	gcc -g -c  src/compiler/lexer.c -o  build/lexer.o

sds.o: ${SDS}
	gcc -g -c src/string/sds.c -o  build/sds.o

ast.o: ${TOKEN} ${AST} ${SDS}
	gcc -g -c  src/compiler/ast.c -o  build/ast.o

parser.o: ${AST} ${LEXER} ${TOKEN} ${PARSER}
	gcc -g -c  src/compiler/parser.c -o  build/parser.o

code_converter.o: ${CODE_CONVERTER} ${AST} ${LEXER} ${TOKEN} ${PARSER}
	gcc -g -c  src/code_converter.c -o build/code_converter.o


clean:
	rm bin/* build/*.o

run:
	./bin/ode_compiler hh.ode hh.c
