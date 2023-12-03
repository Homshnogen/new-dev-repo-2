
#include "llvm/Pass.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"

#include "llvm/IR/DebugInfo.h"

using namespace llvm;

namespace { // anonymous namespace for same-file linkage

struct CriticalInputPass : public PassInfoMixin<CriticalInputPass> {
    PreservedAnalyses run(Module &M, ModuleAnalysisManager &AM) {
        for (auto &F : M) {
        for (auto& B : F) {
        for (auto& I : B) {
            errs() << "Instruction: " << I << "\n";
            // errs() << "Debug_loc: " << I.getDebugLoc() << "\n"; // line information
            if (auto *si = dyn_cast<StoreInst>(&I)) {
                if (dyn_cast<Constant>(si->getValueOperand())) {
                    errs() << "Store Value is constant: " << *si->getValueOperand() << "\n";
                } else {
                    errs() << "Store Value is not constant: " << *si->getValueOperand() << "\n";
                    for (User *user : si->getValueOperand()->users()) {
                        errs() << "Store Value user: " << *user << "\n";
                    }
                }
                for (User *user : si->getPointerOperand()->users()) {
                    errs() << "Store Pointer user: " << *user << "\n";
                }
                /* for (auto op = I.op_begin(); op != I.op_end(); op++) {
                    errs() << "Op name: " << op->get()->getName() << "\n";
                } */
            } else if (dyn_cast<AllocaInst>(&I)) {
                for (DbgDeclareInst *ddi : FindDbgDeclareUses(&I)) {
                    //dbgs() << "Debug instruction: " << *ddi << '\n';
                    dbgs() << "Alloca Variable: " << ddi->getVariable()->getName() << ", Line " << ddi->getVariable()->getLine() << '\n';
                }
            } else if (CallInst *ci = dyn_cast<CallInst>(&I)) {
                if (Function *callf = ci->getCalledFunction()) {
                    if (callf->getName().equals("logBranchDecision")) {
                        dbgs() << "LogBD call: " << *ci << '\n';
                        if (Instruction *a = dyn_cast<Instruction>(ci->getNextNode())) {
                            dbgs() << "Next (flagged) instruction: " << *a << '\n';
                        }
                    }
                } else {
                    // called function pointer
                    dbgs() << "Called function pointer with operands: \n";
                    for (auto op = I.op_begin(); op != I.op_end(); op++) {
                        errs() << "    Op: " << *op->get() << "\n";
                    }
                }
            }
            for (User *user: I.users()) {
                dbgs() << "Instruction user: " << *user << '\n';
            }
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
        .PluginName = "Critical Input Detector pass",
        .PluginVersion = "v0.1",
        .RegisterPassBuilderCallbacks = [](PassBuilder &PB) {
            PB.registerPipelineStartEPCallback(
                [](ModulePassManager &MPM, OptimizationLevel Level) {
                    MPM.addPass(CriticalInputPass());
                });
        }
    };
}