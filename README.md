# miniC compiler
A quick overview of the running instructions—

- `make` to generate the `./minic` executable
- `./minic ${FILEPATH}` to generate the assembly code for the specified `.c` file (e.g. `./minic tests/fib.c` generates `/out/fib.s`)
- `make run FILE=${FILENAME}` to run the assembly code for the specified program (e.g. `make run FILE=fib`)

Note—due to syntax errors (double `;;`; `int max = 0` instead of `int max; max = 0`) I had to make changes to `tests/fact.c` and `tests/max_n.c` before I could run them—otherwise the compiler tells me there is a syntax error.

## Directory structure
```
├── main.c
├── Makefile
├── minic
├── minic.l             # lex file
├── minic.y             # yacc file
├── README.md
├── semantic/           # part 1: semantic analysis checks
├── ir_and_opts/        # part 2: IR builder; part 3: optimizations
├── codegen/            # part 4: Assembly code generation
├── tests/
│   ├── main.c          # & a bunch of codegen tests are right here at this level
│   ├── semantic/
│   └── codegen/
└── out/
    ├── a.out           # final executable
    ├── out.ll          # generated LLVM file from part 2
    └── out_opt.ll      # optimized LLVM file after part 3
```

## Overview
### Part 4: Codegen
- **Input:** LLVM IR
- **Output:** assembly code

### Part 2: IR builder & Part 3: Optimizations
- **Input:** AST tree root node
- **Output:** (optimized) LLVM IR

Local optimizations implemented: common subexpression elimination, dead code elimination, constant folding

Global optimizations implemented: constant propagation

### Part 1: Syntax & semantic analysis
For syntax analysis (implicit in `main.c` plus the `minic.l` and `minic.y` files)—
- **Input:** miniC program file path
- **Output:** AST tree root node

For semantic analysis—
- **Input:** AST tree root node
- **Output:** integer representing if there is a semantic analysis error

There are two checks we implemented for semantic analysis: 1) a variable is only declared once per scope, and 2) a variable must be declared before it is used.







