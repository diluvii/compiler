# miniC compiler

## Directory structure
```
├── semantic/            # part 1: semantic analysis checks
├── ir_and_opts/         # part 2: IR builder; part 3: optimizations
├── codegen/             # part 4: Assembly code generation
├── tests/
│   ├── ir/
│   ├── semantic/
│   └── codegen/
├── out/
│   ├── out
│   ├── out.ll          # generated LLVM file from part 2
│   └── out_opt.ll      # optimized LLVM file after part 3
├── main.c
├── Makefile
├── minic
├── minic.l              # lex file
├── minic.y              # yacc file
└── README.md
```

## Running instructions
### Part 4: Codegen
**Input:** LLVM IR
**Output:** Assembly code

### Part 2: IR builder & Part 3: Optimizations
**Input:** AST tree root node
**Output:** (optimized) LLVM IR

IR builder code lives in `ir_and_opts/ir.c`; IR optimizer code lives in `ir_and_opts/opts.c`. 

Run `make` to generate the `./minic` executable. 

For testing, run `./minic [target program]` (e.g. `./minic tests/ir/p1.c`) from the terminal. This will put the resulting IR file in `tests/out/out.ll`; find also the optimized IR file in `tests/out/out_opt.ll`. Run `make run` to see the resulting (optimized) program execute.

Local optimizations implemented: common subexpression elimination, dead code elimination, constant folding
Global optimizations implemented: constant propagation

### Part 1: Syntax & semantic analysis
For syntax analysis (implicit in `main.c` plus the `minic.l` and `minic.y` files)—
**Input:** miniC program file path
**Output:** AST tree root node

For semantic analysis—
**Input:** AST tree root node
**Output:** integer representing if there is a semantic analysis error

There are two checks we implemented for semantic analysis: 1) a variable is only declared once per scope, and 2) a variable must be declared before it is used.



