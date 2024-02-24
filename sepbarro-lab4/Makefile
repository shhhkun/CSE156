EXECBIN_CLIENT = bin/myclient
EXECBIN_SERVER = bin/myserver
SRCDIR = src
BINDIR = bin

SOURCES_CLIENT = $(wildcard $(SRCDIR)/myclient.c)
SOURCES_SERVER = $(wildcard $(SRCDIR)/myserver.c)
OBJECTS_CLIENT = $(SOURCES_CLIENT:$(SRCDIR)/%.c=$(BINDIR)/%.o)
OBJECTS_SERVER = $(SOURCES_SERVER:$(SRCDIR)/%.c=$(BINDIR)/%.o)

CC = clang
CFLAGS = -Wall -Wpedantic -Werror -Wextra

.PHONY: all clean format

all: $(EXECBIN_CLIENT) $(EXECBIN_SERVER)

$(EXECBIN_CLIENT): $(OBJECTS_CLIENT)
	$(CC) -o $@ $^

$(EXECBIN_SERVER): $(OBJECTS_SERVER)
	$(CC) -o $@ $^

$(BINDIR)/%.o: $(SRCDIR)/%.c | $(BINDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BINDIR):
	mkdir -p $(BINDIR)

clean:
	rm -f $(BINDIR)/*.o
	rm -f $(EXECBIN_CLIENT) $(EXECBIN_SERVER)

format:
	clang-format -i -style=file $(SRCDIR)/*.[ch]
