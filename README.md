# miniC compiler
Compiler for a C-subset targetting x86-64 assembly. Generates LLVM IR and emits x86-64 assembly via LLVM API calls, with optimization passes (CSE, DCE, constant folding, constant propagation) and a linear scan algorithm for register allocation.

## Pipeline
### Frontend: Syntax and semantic analysis
Uses lex and yacc to parse miniC file into an AST. Conducts semantic checks to ensure variables are declared once per scope and declared before use.

### IR generation
AST lowered into LLVM IR via LLVM API calls.

### Optimizations
We run local optimizations (CSE, DCE, constant folding) and a global optimization (constant propagation) on the LLVM IR.

(Will add descriptions of CSE, DCE, constant folding, constant propagation.)

### Codegen
We run the linear scan algorithm to allocate registers, then emit x86-64 assembly via LLVM API calls.

(Will add descriptions of linear scan algorithm.)

## Directory Structure
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

## Usage
- `make` to generate the `./minic` executable
- `./minic ${FILEPATH}` to generate the assembly code for the specified `.c` file (e.g. `./minic tests/fib.c` generates `/out/fib.s`)
- `make run FILE=${FILENAME}` to run the assembly code for the specified program (e.g. `make run FILE=fib`)
