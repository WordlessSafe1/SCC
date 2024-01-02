CC = gcc
OUT = scc.exe
BUILDDIR = ./target

$(BUILDDIR)/$(OUT): $(BUILDDIR)/main.o $(BUILDDIR)/types.o $(BUILDDIR)/symTable.o $(BUILDDIR)/lex.o $(BUILDDIR)/gen.o | $(BUILDDIR)
	$(CC) $(CFLAGS) -o $(BUILDDIR)/$(OUT) $(BUILDDIR)/main.o $(BUILDDIR)/types.o $(BUILDDIR)/symTable.o $(BUILDDIR)/lex.o $(BUILDDIR)/gen.o

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

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

build: $(BUILDDIR)/$(OUT)

clean:
	rm -f $(BUILDDIR)/*.o

# Triple test; build to scc0.exe, build to scc1.exe using scc0.exe, build to scc2.exe using scc1.exe
triple:
	@echo " === Cleaning build directory... === "
	@make clean -s
	@echo " === Building scc0.exe... === "
	@make build OUT=scc0.exe -s
	@echo " === Cleaning build directory... === "
	@rm -f $(BUILDDIR)/*.o
	@sleep 2
	@echo " === Building scc1.exe... === "
	@make build OUT=scc1.exe CC="$(BUILDDIR)/scc0.exe" CFLAGS="-q" -s --no-print-directory
	@echo " === Cleaning build directory... === "
	@rm -f $(BUILDDIR)/*.o
	@echo " === Building scc2.exe... === "
	@make build CFLAGS= OUT=scc2.exe CC="$(BUILDDIR)/scc1.exe" CFLAGS="-q" -s --no-print-directory
	@echo " === Checking if scc2.exe is identical to scc1.exe... === "
	@cmp $(BUILDDIR)/scc2.exe $(BUILDDIR)/scc1.exe
	@echo "Passed!"