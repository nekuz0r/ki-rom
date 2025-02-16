GCC = gcc-14
SRCS = $(wildcard *.c)
OBJS = $(SRCS:%.c=build/%.o)
DEPS = $(SRCS:%.c=build/%.d)

fontgen: $(OBJS)
	$(GCC) -o fontgen $^

build/%.o: %.c
	mkdir -p $(@D)
	$(GCC) -MMD -O3 -c $< -o $@

.PHONY: clean
clean:
	-rm -f $(OBJS) $(DEPS)

-include $(DEPS)
