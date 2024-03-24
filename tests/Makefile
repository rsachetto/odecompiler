.PHONY: build/libcompiler.a

MKDIR_P = mkdir -p

all: build_dir libcompiler.a
	gcc ${OPT_FLAGS} ../src/code_converter.c test.c ../build/libcompiler.a -o test -lcriterion

debug: debug_set all

debug_set:
	$(eval OPT_FLAGS=-DDEBUG_INFO -g3 -fsanitize=undefined -fsanitize=address -fsanitize=enum -fsanitize=signed-integer-overflow -fsanitize=undefined -Wall -Wno-switch -Wno-misleading-indentation)
	$(eval OPT_TYPE=debug)


build_dir:
	@${MKDIR_P} ../build

libcompiler.a:
	cd ../src/compiler/ && ${MAKE} ${OPT_TYPE}
	mv ../src/compiler/libcompiler.a ../build

clean:
	${RM} test
