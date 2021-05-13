.PHONY: directories install common

MKDIR_P = mkdir -p

all: release
common: directories ode_compiler ode_shell

directories: bin_dir build_dir

bin_dir:
	${MKDIR_P} bin

build_dir:
	${MKDIR_P} build

debug: debug_set common
release: release_set common

release_set:
	$(eval OPT_FLAGS=-O2 -Wno-stringop-overflow)

debug_set:
	$(eval OPT_FLAGS=-g3 -Wall -Wno-stringop-overflow)


AST=src/compiler/ast.h src/compiler/ast.c
PARSER=src/compiler/parser.c src/compiler/parser.h
LEXER=src/compiler/lexer.c src/compiler/lexer.h
TOKEN= src/compiler/token.c src/compiler/token.h
SDS=src/string/sds.c src/string/sds.h
COMMANDS=src/commands.c src/commands.h
STR_UTILS=src/string_utils.c src/string_utils.h
FILE_UTILS=src/file_utils/file_utils.c src/file_utils/file_utils.h
C_CONVERTER=src/code_converter.c src/code_converter.h

ode_shell: src/ode_shell.c code_converter.o parser.o lexer.o ast.o token.o file_utils.o sds.o commands.o string_utils.o
	gcc ${OPT_FLAGS} src/ode_shell.c build/parser.o build/lexer.o build/ast.o build/token.o build/file_utils.o build/sds.o build/code_converter.o build/string_utils.o build/commands.o -o bin/ode_shell -lreadline

ode_compiler: src/ode_compiler.c code_converter.o parser.o lexer.o ast.o token.o file_utils.o sds.o
	gcc ${OPT_FLAGS} src/ode_compiler.c build/parser.o build/lexer.o build/ast.o build/token.o build/file_utils.o build/sds.o build/code_converter.o -o bin/odec

token.o: ${TOKEN}
	gcc ${OPT_FLAGS} -c  src/compiler/token.c -o  build/token.o

lexer.o: ${LEXER} ${TOKEN}
	gcc ${OPT_FLAGS} -c  src/compiler/lexer.c -o  build/lexer.o

sds.o: ${SDS}
	gcc ${OPT_FLAGS} -c src/string/sds.c -o  build/sds.o

file_utils.o: ${FILE_UTILS}
	gcc ${OPT_FLAGS} -c src/file_utils/file_utils.c -o  build/file_utils.o

ast.o: ${TOKEN} ${AST} ${SDS}
	gcc ${OPT_FLAGS} -c  src/compiler/ast.c -o  build/ast.o

parser.o: ${AST} ${LEXER} ${TOKEN} ${PARSER}
	gcc ${OPT_FLAGS} -c  src/compiler/parser.c -o  build/parser.o

code_converter.o: ${CODE_CONVERTER} ${AST} ${LEXER} ${TOKEN} ${PARSER}
	gcc ${OPT_FLAGS} -c  src/code_converter.c -o build/code_converter.o

commands.o: ${COMMANDS}
	gcc ${OPT_FLAGS} -c  src/commands.c -o  build/commands.o

string_utils.o: ${STR_UTILS}
	gcc ${OPT_FLAGS} -c  src/string_utils.c -o  build/string_utils.o


clean:
	rm bin/* build/*.o

run:
	./bin/ode_compiler hh.ode hh.c
