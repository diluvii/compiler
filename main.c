#include <stdio.h>
#include "ast.h"
#include "y.tab.h"
#include <llvm-c/Core.h>
extern int yylex_destroy();
extern FILE *yyin;
extern astNode* root;
extern int semanticAnalysis(astNode*);
extern LLVMModuleRef genIR(astNode*);
extern LLVMModuleRef optimize(LLVMModuleRef);
extern int codegen(LLVMModuleRef, char*);

int main(int argc, char* argv[]) {
	if (argc == 2) {
		yyin = fopen(argv[1], "r");
		if (yyin == NULL) {
			fprintf(stderr, "file open error\n");
			return 1;
		}
	}
	yyparse();

	// syntax analysis & semantic analysis
	if (root == NULL) {
		printf("oh no! we have no root!\n");
		printf("semantic analysis cannot proceed :(\n");
		return 2;
	} else {
		// generate IR if program passes semantic analysis checks
		if (semanticAnalysis(root) == 0) {
			LLVMModuleRef mod = genIR(root);

			// optimize IR
			LLVMModuleRef mod_opt = optimize(mod);

			// code generation if optimized IR module is valid
			if (mod_opt != NULL) {
				codegen(mod_opt, argv[1]);
			}
		}
	}

	if (argc == 2) fclose(yyin);
	yylex_destroy();
	return 0;
}
