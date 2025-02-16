GCC = gcc-14
SRCS = $(wildcard *.c)
OBJS = $(SRCS:%.c=build/%.o)
DEPS = $(SRCS:%.c=build/%.d)

png2bin: $(OBJS)
	$(GCC) -o png2bin $^ `libpng16-config --ldflags`

build/%.o: %.c
	mkdir -p $(@D)
	$(GCC) -MMD -O3 -c $< -o $@ `libpng16-config --cflags`

.PHONY: clean
clean:
	-rm -f $(OBJS) $(DEPS)

-include $(DEPS)
