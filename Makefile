CC = gcc
OUT = scc.exe
BUILDDIR = ./target

$(BUILDDIR)/$(OUT): $(BUILDDIR)/main.o $(BUILDDIR)/types.o $(BUILDDIR)/symTable.o $(BUILDDIR)/lex.o $(BUILDDIR)/gen.o $(BUILDDIR)/parse.o | $(BUILDDIR)
	$(CC) $(CFLAGS) -o $(BUILDDIR)/$(OUT) $(BUILDDIR)/main.o $(BUILDDIR)/types.o $(BUILDDIR)/symTable.o $(BUILDDIR)/lex.o $(BUILDDIR)/gen.o $(BUILDDIR)/parse.o

$(BUILDDIR)/main.o: main.c | $(BUILDDIR)
	$(CC) $(CFLAGS) -c main.c -o $(BUILDDIR)/main.o

$(BUILDDIR)/types.o: types.c types.h | $(BUILDDIR)
	$(CC) $(CFLAGS) -c types.c -o $(BUILDDIR)/types.o

$(BUILDDIR)/symTable.o: symTable.c symTable.h | $(BUILDDIR)
	$(CC) $(CFLAGS) -c symTable.c -o $(BUILDDIR)/symTable.o

$(BUILDDIR)/lex.o: lex.c lex.h | $(BUILDDIR)
	$(CC) $(CFLAGS) -c lex.c -o $(BUILDDIR)/lex.o

$(BUILDDIR)/gen.o: gen.c gen.h | $(BUILDDIR)
	$(CC) $(CFLAGS) -c gen.c -o $(BUILDDIR)/gen.o

$(BUILDDIR)/parse.o: parse.c parse.h | $(BUILDDIR)
	$(CC) $(CFLAGS) -c parse.c -o $(BUILDDIR)/parse.o

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

build: $(BUILDDIR)/$(OUT)

clean:
	rm -rf $(BUILDDIR)

# Triple test; build to scc0.exe, build to scc1.exe using scc0.exe, build to scc2.exe using scc1.exe
triple:
	@echo " === Cleaning build directory... === "
	@make clean -s
	@echo " === Building scc0.exe... === "
	@make build OUT=scc0.exe -s
	@echo " === Building scc1.exe... === "
	@make build OUT=scc1.exe CC="$(BUILDDIR)/scc0.exe" -s --no-print-directory
	@echo " === Building scc2.exe... === "
	@make build OUT=scc2.exe CC="$(BUILDDIR)/scc1.exe" -s --no-print-directory
	@echo " === Checking if scc0.exe is identical to scc1.exe... === "
	cmp $(BUILDDIR)/scc0.exe $(BUILDDIR)/scc1.exe
	@echo " === Checking if scc1.exe is identical to scc2.exe... === "
	cmp $(BUILDDIR)/scc1.exe $(BUILDDIR)/scc2.exe
	@echo "Passed!"