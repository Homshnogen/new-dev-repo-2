#include "llvm/Pass.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"

#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"

using namespace llvm;

namespace { // anonymous namespace for same-file linkage

struct BranchPointerProfiler : public PassInfoMixin<BranchPointerProfiler> {
    PreservedAnalyses run(Module &M, ModuleAnalysisManager &AM) {
        // Initialization for logging function
        // logBranchDecision takes branch ID and logs it
        FunctionCallee logBranchFunc = M.getOrInsertFunction(
          "logBranchDecision",
          Type::getVoidTy(M.getContext()),  // return type
          Type::getInt32Ty(M.getContext())  // branch ID
        );
        // Assign unique ID to each branch
        static int branchCounter = 0;
        
        for (auto &F : M) {
          for (auto &BB : F) {
            for (auto &I : BB) {
              if (BranchInst *branchInst = dyn_cast<BranchInst>(&I)) {
                if (branchInst->isConditional()) {
                
                  // Conditional branch check
                  int currentBranchID = branchCounter++;
                  
                  // Instrument the branch
                  IRBuilder<> builder(branchInst);
                  Value *branchIDValue = ConstantInt::get(Type::getInt32Ty(F.getContext()), currentBranchID);
                  builder.CreateCall(logBranchFunc, branchIDValue);
                        
                  // Store metadata for later use
                  if (DebugLoc DL = branchInst->getDebugLoc()) {
                    unsigned line = DL.getLine();
                    StringRef file = DL->getFilename();
                    // Store 'file' and 'line' for later use
                  }             
                }
              }
            }
          }
        }
        return PreservedAnalyses::none(); // function was modified
    };
};

}

extern "C" LLVM_ATTRIBUTE_WEAK::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
    return {
        .APIVersion = LLVM_PLUGIN_API_VERSION,
        .PluginName = "Branch Pointer Profiler Pass",
        .PluginVersion = "v0.1",
        .RegisterPassBuilderCallbacks = [](PassBuilder &PB) {
            PB.registerPipelineStartEPCallback(
                [](ModulePassManager &MPM, OptimizationLevel Level) {
                    MPM.addPass(BranchPointerProfiler());
                });
        }
    };
}