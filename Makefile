LLVM_FLAGS = `llvm-config-17 --cxxflags --ldflags --libs core`
LLVM_INCLUDES = -I /usr/include/llvm-c-17/

make:
	yacc -d minic.y
	lex minic.l
	g++ -g lex.yy.c y.tab.c main.c ast.c semantic/sem.c ir_and_opts/ir.c ir_and_opts/opts.c codegen/codegen.c $(LLVM_FLAGS) -o minic

run:
	clang tests/ir/main.c out/out_opt.ll -o out/out
	./out/out
