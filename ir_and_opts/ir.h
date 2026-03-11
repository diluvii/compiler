#ifndef IR_H
#define IR_H

#include "../ast.h"
#include <llvm-c/Core.h>

LLVMModuleRef genIR(astNode* root);

#endif
