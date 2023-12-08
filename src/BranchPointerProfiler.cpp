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
          "printf",
          FunctionType::get(IntegerType::get(M.getContext(), 32), PointerType::getUnqual(IntegerType::get(M.getContext(), 8)), true) //(Type *Result, ArrayRef< Type * > Params, bool isVarArg)
        );
        Constant *branchFString = ConstantDataArray::getString(M.getContext(), StringRef("branch : %d\n"));
        ConstantInt *constIntZero = ConstantInt::getSigned(IntegerType::get(M.getContext(), 32), 0);
        
        GlobalVariable* branchGlobalString = new GlobalVariable(M, 
        branchFString->getType(),
        true,
        GlobalValue::PrivateLinkage,
        branchFString, // has initializer, specified below
        ".log.str.branch"); 
        branchGlobalString->setAlignment(Align(1));
        branchGlobalString->setUnnamedAddr(GlobalVariable::UnnamedAddr::Global);
        errs() << "trying thing\n";
        Constant *branchGEPExpr = ConstantExpr::getInBoundsGetElementPtr(branchFString->getType() , branchGlobalString, ArrayRef<Constant *>({constIntZero, constIntZero}));
        errs() << *branchGEPExpr << "\n";
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
                  //static GetElementPtrConstantExpr * 	Create (Type *SrcElementTy, Constant *C, ArrayRef< Constant * > IdxList, Type *DestTy, unsigned Flags)
                  Value *branchIDValue = ConstantInt::get(Type::getInt32Ty(F.getContext()), currentBranchID);
                  builder.CreateCall(logBranchFunc, {branchGEPExpr, branchIDValue});
                        
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