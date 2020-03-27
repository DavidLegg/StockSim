CC := gcc
SRCDIR := src
BINDIR := bin
OBJDIR := obj
INCDIR := include
EXEC := stock-sim
LINK := -lm -lpthread -lrt
INC := -I $(INCDIR)
CFLAGS := $(LINK) -Wall -Wextra -pedantic $(INC)

SRCFILES := $(shell find $(SRCDIR) -type f -name *.c)
OBJFILES := $(patsubst $(SRCDIR)/%.c,$(OBJDIR)/%.o,$(SRCFILES))

.PHONY: clean makedirs debug perf all run
.SUBLIME_TARGETS: all debug perf run clean

all: makedirs $(EXEC)

debug: CFLAGS += -g -DDEBUG
debug: all

perf: CFLAGS += -O3 -DPERF
perf: all

clean:
	-/bin/rm -rf $(OBJDIR) $(BINDIR)

makedirs:
	/bin/mkdir -p bin obj

$(OBJDIR)/%.o: $(SRCDIR)/%.c
	$(CC) -c -o $@ $< $(CFLAGS)

$(EXEC): $(OBJFILES)
	$(CC) $^ -o $(BINDIR)/$@ $(CFLAGS)
	@echo "---=== COMPILE SUCCESS ===---"

run: all
	$(BINDIR)/$(EXEC)

db_run: debug
	gdb -tui $(BINDIR)/$(EXEC)
