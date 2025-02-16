GCC = gcc-14
SRCS = $(wildcard *.c)
OBJS = $(SRCS:%.c=build/%.o)
DEPS = $(SRCS:%.c=build/%.d)

lzss: $(OBJS)
	$(GCC) -o lzss $^

build/%.o: %.c
	mkdir -p $(@D)
	$(GCC) -MMD -O3 -c $< -o $@

.PHONY: clean
clean:
	-rm -f $(OBJS) $(DEPS)

-include $(DEPS)
