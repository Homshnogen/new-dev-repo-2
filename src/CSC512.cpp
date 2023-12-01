/* New pass manager */
//#include "llvm/Transforms/CSC512/CSC512.h"


#include "llvm/Pass.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

namespace { // anonymous namespace for same-file linkage

struct CSC512Pass : public PassInfoMixin<CSC512Pass> {
    PreservedAnalyses run(Module &M, ModuleAnalysisManager &AM) {
        errs() << "Module ?:\n" << M << "\n";
        for (auto &F : M) {
            errs() << "CSC 512: " << F.getName() << "\n";
            errs() << "Function body:\n" << F << "\n";
            for (auto& B : F) {
            errs() << "Basic block:\n" << B << "\n";
            for (auto& I : B) {
                errs() << "Instruction: " << I << "\n";
            }
            }
        }
        return PreservedAnalyses::all();
    };
};

}

extern "C" LLVM_ATTRIBUTE_WEAK::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
    return {
        .APIVersion = LLVM_PLUGIN_API_VERSION,
        .PluginName = "CSC512 pass",
        .PluginVersion = "v0.1",
        .RegisterPassBuilderCallbacks = [](PassBuilder &PB) {
            PB.registerPipelineStartEPCallback(
                [](ModulePassManager &MPM, OptimizationLevel Level) {
                    MPM.addPass(CSC512Pass());
                });
        }
    };
}