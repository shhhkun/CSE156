EXECBIN  = bin/myproxy
SRCDIR   = src
BINDIR   = bin

SOURCES  = $(wildcard $(SRCDIR)/*.c)
OBJECTS  = $(SOURCES:$(SRCDIR)/%.c=$(BINDIR)/%.o)

CC       = clang
CFLAGS   = -Wall -Wpedantic -Werror -Wextra

.PHONY: all clean format

all: $(EXECBIN)

$(EXECBIN): $(OBJECTS)
	$(CC) -o $@ $^

$(BINDIR)/%.o: $(SRCDIR)/%.c | $(BINDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BINDIR):
	mkdir -p $(BINDIR)

clean:
	rm -f *.o
	rm -rf $(BINDIR)/*

format:
	clang-format -i -style=file $(SRCDIR)/*.[ch]
