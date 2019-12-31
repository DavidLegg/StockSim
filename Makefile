CC := gcc
SRCDIR := src
BINDIR := bin
OBJDIR := obj
INCDIR := include
EXEC := stock-sim
INC := -I $(INCDIR)
CFLAGS := -Wall -Wextra -pedantic $(INC) $(STD)

SRCFILES := $(shell find $(SRCDIR) -type f -name *.c)
OBJFILES := $(patsubst $(SRCDIR)/%.c,$(OBJDIR)/%.o,$(SRCFILES))

.PHONY: clean makedirs debug perf all run
.SUBLIME_TARGETS: all debug perf run clean

all: makedirs $(EXEC)

debug: CFLAGS += -g -DDEBUG
debug: all

perf: CFLAGS += -O3
perf: all

clean:
	-/bin/rm -rf $(OBJDIR) $(BINDIR)

makedirs:
	/bin/mkdir -p bin obj

$(OBJDIR)/%.o: $(SRCDIR)/%.c
	$(CC) -c -o $@ $< $(CFLAGS)

$(EXEC): $(OBJFILES)
	$(CC) $^ -o $(BINDIR)/$@
	@echo "---=== COMPILE SUCCESS ===---"

run: all
	$(BINDIR)/$(EXEC)
