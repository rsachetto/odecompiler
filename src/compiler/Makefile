##
# Ode compiler lib
#
# @file
# @version 0.1

all: release

clean:
	${RM} *.a *.o

debug: debug_set common
release: release_set common

release_set:
	$(eval OPT_FLAGS=-O2)

debug_set:
	$(eval OPT_FLAGS=-g3 -fsanitize=address -Wall -Wno-switch -Wno-stringop-overflow -Wno-misleading-indentation -mavx -maes)

common: libcompiler.a

libcompiler.a: token.o lexer.o ast.o parser.o program.o sds.o file_utils.o enum_to_string.o
	ar rcs libcompiler.a $^

token.o: token.c token.h token_enum.h
	gcc ${OPT_FLAGS} -c token.c -o token.o

lexer.o: lexer.c lexer.h
	gcc ${OPT_FLAGS} -c  lexer.c -o lexer.o

ast.o: ast.c ast.h
	gcc ${OPT_FLAGS} -c ast.c -o ast.o

parser.o: parser.c  parser.h
	gcc ${OPT_FLAGS} -c parser.c -o parser.o

program.o: program.c  program.h
	gcc ${OPT_FLAGS} -c program.c -o program.o

enum_to_string.o: enum_to_string.c enum_to_string.h token_enum.h
	gcc ${OPT_FLAGS} -c enum_to_string.c -o enum_to_string.o

sds.o: ../string/sds.c ../string/sds.h
	gcc ${OPT_FLAGS} -c ../string/sds.c -o  sds.o

file_utils.o: ../file_utils/file_utils.c ../file_utils/file_utils.h
	gcc ${OPT_FLAGS} -c ../file_utils/file_utils.c -o  file_utils.o

# end
