PROGRAM := ./pltfall

CC := gcc
CFLAGS := -Wall


EXAMPLES_PLT := $(wildcard example/*.plt)
EXAMPLES_OBF := $(EXAMPLES_PLT:.plt=.obf)


.PHONY: all clean examples tests

$(PROGRAM): pltfall.c
	$(CC) $(CFLAGS) -o $@ $^

all: $(PROGRAM) examples

examples: $(EXAMPLES_OBF)

example/example: example/example.c
	$(CC) $(CFLAGS) -o $@ $<

example/%.obf: example/%.plt example/example $(PROGRAM)
	$(PROGRAM) example/example $<
	mv example/example.obf $@
	chmod +x $@

tests: test.sh examples
	./test.sh

clean:
	rm -f $(PROGRAM) example/example example/*.obf
