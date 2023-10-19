.PHONY: build/libfort.a
.PHONY: build/libcompiler.a

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
	$(eval OPT_FLAGS = -O3)
	$(eval OPT_TYPE=release)

debug_set:
	$(eval OPT_FLAGS=-DDEBUG_INFO -g3 -fsanitize=undefined -fsanitize=address -Wall -Wno-switch -Wno-misleading-indentation)
	$(eval OPT_TYPE=debug)

bin/ode_shell: src/ode_shell.c build/code_converter.o build/pipe_utils.o build/commands.o build/command_corrector.o build/string_utils.o build/model_config.o build/inotify_helpers.o build/to_latex.o build/md5.o build/gnuplot_utils.o build/libfort.a build/libcompiler.a
	gcc ${OPT_FLAGS} $^ -o bin/ode_shell -lreadline -lpthread

bin/odec: src/ode_compiler.c build/code_converter.o build/string_utils.o build/libcompiler.a
	gcc ${OPT_FLAGS} $^ -o bin/odec

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

build/pipe_utils.o: src/pipe_utils.c src/pipe_utils.h
	gcc ${OPT_FLAGS} -c src/pipe_utils.c -o build/pipe_utils.o

build/command_corrector.o: src/command_corrector.c src/command_corrector.h
	gcc ${OPT_FLAGS} -c src/command_corrector.c -o build/command_corrector.o

build/md5.o: src/md5/md5.c src/md5/md5.h
	gcc ${OPT_FLAGS} -c src/md5/md5.c -o build/md5.o

build/gnuplot_utils.o: src/gnuplot_utils.c src/gnuplot_utils.h
	gcc ${OPT_FLAGS} -c src/gnuplot_utils.c -o build/gnuplot_utils.o

build/libfort.a:
	cd src/libfort/src/ && ${MAKE} ${OPT_TYPE}
	mv src/libfort/src/libfort.a build

build/libcompiler.a:
	cd src/compiler/ && ${MAKE} ${OPT_TYPE}
	mv src/compiler/libcompiler.a build

clean:
	cd src/libfort/src/ && ${MAKE} clean
	cd src/compiler && ${MAKE} clean
	${RM} bin/* build/*.o
