all: release

debug: debug_set common
release: release_set common

release_set:
	$(eval OPT_FLAGS=-O2)

debug_set:
	$(eval OPT_FLAGS=-g3 -fsanitize=address -Wall -Wno-switch -Wno-stringop-overflow -Wno-misleading-indentation -mavx -maes)

common: libfort.a

objects := $(patsubst %.c,%.o,$(wildcard *.c))

libfort.a: $(objects)
	ar rcs libfort.a $(objects)

%.o: %.c
	gcc ${OPT_FLAGS} -c -o $@ $<

clean:
	@rm -f *.o *.a
