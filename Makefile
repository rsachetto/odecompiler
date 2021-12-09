.PHONY: build/libfort.a

MKDIR_P = mkdir -p

all: release

common: directories bin/odec bin/ode_shell

debug: debug_set common
release: release_set common

directories: bin_dir build_dir

bin_dir:
	@${MKDIR_P} bin

build_dir:
	@${MKDIR_P} build

release_set:
	$(eval OPT_FLAGS=-O2 -Wno-stringop-overflow -mavx -maes)
	$(eval OPT_TYPE=release)

debug_set:
	$(eval OPT_FLAGS=-DDEBUG_INFO -g3 -fsanitize=address -Wall -Wno-switch -Wno-stringop-overflow -Wno-misleading-indentation -mavx -maes)
	$(eval OPT_TYPE=debug)

bin/ode_shell: src/ode_shell.c build/code_converter.o build/parser.o build/lexer.o build/ast.o build/token.o build/file_utils.o build/sds.o build/pipe_utils.o build/commands.o build/command_corrector.o build/string_utils.o build/model_config.o build/inotify_helpers.o build/to_latex.o build/enum_to_string.o build/libfort.a
	gcc ${OPT_FLAGS} $^ -o bin/ode_shell -lreadline -lpthread

bin/odec: src/ode_compiler.c build/code_converter.o build/enum_to_string.o build/parser.o build/lexer.o build/ast.o build/token.o build/file_utils.o build/sds.o
	gcc ${OPT_FLAGS} $^ -o bin/odec

build/token.o: src/compiler/token.c src/compiler/token.h src/compiler/token_enum.h
	gcc ${OPT_FLAGS} -c  src/compiler/token.c -o  build/token.o

build/lexer.o: src/compiler/lexer.c src/compiler/lexer.h
	gcc ${OPT_FLAGS} -c  src/compiler/lexer.c -o  build/lexer.o

build/sds.o: src/string/sds.c src/string/sds.h
	gcc ${OPT_FLAGS} -c src/string/sds.c -o  build/sds.o

build/file_utils.o: src/file_utils/file_utils.c src/file_utils/file_utils.h
	gcc ${OPT_FLAGS} -c src/file_utils/file_utils.c -o  build/file_utils.o

build/ast.o: src/compiler/ast.c src/compiler/ast.h
	gcc ${OPT_FLAGS} -c  src/compiler/ast.c -o  build/ast.o

build/parser.o: src/compiler/parser.c  src/compiler/parser.h
	gcc ${OPT_FLAGS} -c  src/compiler/parser.c -o  build/parser.o

build/code_converter.o: src/code_converter.c src/code_converter.h
	gcc ${OPT_FLAGS} -c  src/code_converter.c -o build/code_converter.o

build/commands.o: src/commands.c src/commands.h
	gcc ${OPT_FLAGS} -c  src/commands.c -o  build/commands.o

build/string_utils.o: src/string_utils.c  src/string_utils.h
	gcc ${OPT_FLAGS} -c  src/string_utils.c -o build/string_utils.o

build/model_config.o: src/model_config.c src/model_config.h
	gcc ${OPT_FLAGS} -c  src/model_config.c -o build/model_config.o

build/inotify_helpers.o: src/inotify_helpers.c src/inotify_helpers.h
	gcc ${OPT_FLAGS} -c src/inotify_helpers.c -o build/inotify_helpers.o

build/to_latex.o: src/to_latex.c src/to_latex.h
	gcc ${OPT_FLAGS} -c  src/to_latex.c -o build/to_latex.o

build/enum_to_string.o: src/compiler/enum_to_string.c src/compiler/enum_to_string.h src/compiler/token_enum.h
	gcc ${OPT_FLAGS} -c src/compiler/enum_to_string.c -o build/enum_to_string.o

build/pipe_utils.o: src/pipe_utils.c src/pipe_utils.h
	gcc ${OPT_FLAGS} -c src/pipe_utils.c -o build/pipe_utils.o

build/command_corrector.o: src/command_corrector.c src/command_corrector.h
	gcc ${OPT_FLAGS} -c src/command_corrector.c -o build/command_corrector.o

build/libfort.a:
	cd src/libfort/src/ && ${MAKE} ${OPT_TYPE}
	mv src/libfort/src/libfort.a build

clean:
	cd src/libfort/src/ && ${MAKE} clean
	rm bin/* build/*.o

