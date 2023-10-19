.PHONY: build/libcompiler.a

MKDIR_P = mkdir -p

all: build_dir build/libcompiler.a
	gcc -g ../src/code_converter.c test.c ../build/libcompiler.a -o test -lcriterion

build_dir:
	@${MKDIR_P} ../build

build/libcompiler.a:
	cd ../src/compiler/ && ${MAKE} ${OPT_TYPE}
	mv ../src/compiler/libcompiler.a ../build

clean:
	${RM} test
