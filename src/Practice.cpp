
#include "llvm/Pass.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"

#include "llvm/IR/DebugInfo.h"

using namespace llvm;

namespace { // anonymous namespace for same-file linkage

struct PracticePass : public PassInfoMixin<PracticePass> {
    PreservedAnalyses run(Module &M, ModuleAnalysisManager &AM) {
        //errs() << "Module ?:\n" << M << "\n";
        for (auto &F : M) {
            errs() << "CSC 512: " << F.getName() << "\n";
            //errs() << "Function body:\n" << F << "\n";
            for (auto& B : F) {
            //errs() << "Basic block:\n" << B << "\n";
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
                    if (ci->getCalledFunction()->getName().equals("logBranchDecision")) {
                        dbgs() << "LogBD call: " << *ci << '\n';
                        if (Instruction *a = dyn_cast<Instruction>(ci->getNextNode())) {
                            dbgs() << "Next (flagged) instruction: " << *a << '\n';
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
        .PluginName = "Practice pass",
        .PluginVersion = "v0.1",
        .RegisterPassBuilderCallbacks = [](PassBuilder &PB) {
            PB.registerPipelineStartEPCallback(
                [](ModulePassManager &MPM, OptimizationLevel Level) {
                    MPM.addPass(PracticePass());
                });
        }
    };
}