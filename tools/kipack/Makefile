GCC ?= gcc-14
# Not -Werror: this has to keep building on whatever compiler a contributor
# has. The three unchecked freads that shipped with the first version of this
# tool were all diagnosed by the default warning set alone.
WARNINGS = -Wall -Wextra
SRCS = main.c pack.c unpack.c
OBJS = $(SRCS:%.c=build/%.o)
DEPS = $(SRCS:%.c=build/%.d)

.DELETE_ON_ERROR:

kipack: $(OBJS)
	$(GCC) -o kipack $^

build/%.o: %.c
	mkdir -p $(@D)
	$(GCC) -MMD $(WARNINGS) -O3 -c $< -o $@

FIXTURE_TOOLS = fixtures/mkcorpus fixtures/mkromimage

fixtures/%: fixtures/%.c kipack.h
	$(GCC) $(WARNINGS) -O2 -o $@ $<

.PHONY: check
check: kipack $(FIXTURE_TOOLS)
	./check.sh

.PHONY: clean
clean:
	-rm -f $(OBJS) $(DEPS) kipack $(FIXTURE_TOOLS)

-include $(DEPS)
