#ifndef OPTS_H
#define OPTS_H

#include "../ast.h"
#include <llvm-c/Core.h>

int optimizations(LLVMModuleRef mod);

#endif
