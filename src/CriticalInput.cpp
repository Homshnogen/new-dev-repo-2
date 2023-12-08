
#include "llvm/Pass.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"

#include "llvm/IR/DebugInfo.h"
#include <map>
#include <unordered_set>


using namespace llvm;

namespace { // anonymous namespace for same-file linkage

#define DEBUG 1

#if DEBUG
#define debug(args) dbgs() << args << '\n';
#else
#define debug(args) ;
#endif
// dbgs(), errs(), outs()

typedef FunctionDataType unsigned
struct FunctionData {
    bool started, finished;
    std::unordered_set<FunctionDataType> args;
};
//struct InputData {
//    Value *value;
//    std::string descriptor;
//}
typedef InstructionDataType Use*
struct InstructionData {
    bool started, finished;
    std::unordered_set<InstructionDataType> inputlines;
};

std::map<Function*, FunctionData> fnRunDep;
std::map<Function*, FunctionData> fnValDep;
//std::map<Function*, std::map<unsigned, FunctionData>> fnArgValDep;
std::map<Value*, FunctionData> instFnArgs;
std::map<Value*, InstructionData> instValDep;

std::unordered_set<InstructionDataType> &InstructionBacktrackValue(Instruction *I, std::unordered_set<Function*> &analysisParents);
std::unordered_set<FunctionDataType> &GetFnArgsFromInstData(Instruction *I, std::unordered_set<Function*> &analysisParents) {
    if (!instFnArgs[I].started) {
        for (Use *use : InstructionBacktrackValue(I, analysisParents)) {
            if (StoreInst *si = dyn_cast<StoreInst>(use->getUser())) {
                if (Argument *arg = dyn_cast<Argument>(si->getValueOperand())) {
                    instFnArgs[I].args.insert(arg->getArgNo());
                }
            }
        }
    } else if (!instFnArgs[I].finished) {
        // recursive call; shouldn't happen
        debug("GetFnArgsFromInstData recursive call")
    }
    instFnArgs[I].finished = true;
    return instFnArgs[I].args;
}
void AddFnArgsFromArgs(std::unordered_set<FunctionDataType> &retArgs, std::unordered_set<FunctionDataType> &fromArgs) {
    for (FunctionDataType index : fromArgs) {
        retArgs.insert(index);
    }
}
void AddFnArgsFromInstData(std::unordered_set<FunctionDataType> &retArgs, Instruction *I, std::unordered_set<Function*> &analysisParents) {
    AddFnArgsFromArgs(retArgs, GetFnArgsFromInstData(I, analysisParents));
}

std::unordered_set<FunctionDataType> &FunctionBacktrackRuntime(Function *F, std::unordered_set<Function*> &analysisParents) {
    if (!fnRunDep[F].started) {
        fnRunDep[F].started = true;
        analysisParents.insert(F);
        if (F->isDeclaration()) { // defined elsewhere; assume runtime is dependent on no inputs

        } else {
            for (BasicBlock &B : *F) {
                for (Instruction &I : B) {
                    // find branches and fp calls and perform instruction analysis
                    if (BranchInst *bi = dyn_cast<BranchInst>(&I)) {
                        if (bi->isConditional()) {
                            // branch instruction
                            if (Instruction *next = dyn_cast<Instruction>(bi->getCondition())) {
                                AddFnArgsFromInstData(fnRunDep[F].args, next, analysisParents);
                            }
                        }
                    } else if (CallInst *ci = dyn_cast<CallInst>(&I)) {
                        if (Function *next = ci->getCalledFunction()) {
                            if (ci->getCalledFunction()->getName().equals("logBranchDecision")) { // else if for all special functions
                                debug("LogBD call: " << *ci)
                                if (Instruction *a = dyn_cast<Instruction>(ci->getNextNode())) {
                                    debug("Next (flagged) instruction: " << *a)
                                }
                            } else {
                                debug("called non-LogBD function: " << ci->getCalledFunction()->getName())
                                for (unsigned index : FunctionBacktrackRuntime(nextF, analysisParents)) {
                                    // check runtime-determining arguments passed into call
                                    if (Instruction *nextI = dyn_cast<Instruction>(ci->getArgOperand(index))) {
                                        AddFnArgsFromInstData(fnRunDep[F].args, nextI, analysisParents);
                                    } else {
                                        // arg was constant?
                                        debug("arg was constant?: " << *ci->getArgOperand(index))
                                    }
                                }
                            }
                        } else if (Instruction *next = dyn_cast<Instruction>(ci->getCalledOperand())) {
                            // called function pointer
                            debug("Called function pointer: " << *next)
                            AddFnArgsFromInstData(fnRunDep[F].args, next, analysisParents);
                        }
                    }
                }
            }
        }
        analysisParents.erase(F);
    } else if (!fnRunDep[F].finished) {
        // recursive call
        debug("FunctionBacktrackRuntime recursive call")

    }
    fnRunDep[F].finished = true;
    return fnRunDep[F].args;
}
std::unordered_set<FunctionDataType> &FunctionBacktrackValue(Function *F, std::unordered_set<Function*> &analysisParents) {
    if (!fnValDep[F].started) {
        fnValDep[F].started = true;
        analysisParents.insert(F);
        if (F->isDeclaration()) { // defined elsewhere; assume return value is dependent on all inputs
            for (unsigned i = 0; i < F->getFunctionType()->getNumParams()) {
                fnValDep[f].args.insert(i);
            }
        } else {
            for (BasicBlock &B : *F) {
                for (Instruction &I : B) {
                    // find branch and return instructions and perform instruction analysis
                    if (BranchInst *bi = dyn_cast<BranchInst>(&I)) {
                        if (bi->isConditional()) {
                            // branch instruction
                            if (Instruction *next = dyn_cast<Instruction>(bi->getCondition())) {
                                AddFnArgsFromInstData(fnValDep[F].args, next, analysisParents);
                            }
                        }
                    } else if (ReturnInst *ri = dyn_cast<ReturnInst>(&I)) {
                        // return instruction
                        if (Instruction *next = dyn_cast<Instruction>(ri->getReturnValue())) {
                            AddFnArgsFromInstData(fnValDep[F].args, next, analysisParents);
                        } else if (GlobalVariable *next = dyn_cast<GlobalVariable>(ri->getReturnValue())) {
                            AddFnArgsFromInstData(fnValDep[F].args, next, analysisParents);
                        }
                    }
                }
            }
        }
        analysisParents.erase(F);
    } else if (!fnValDep[F].finished) {
        // recursive call
        debug("FunctionBacktrackValue recursive call")

    }
    fnValDep[F].finished = true;
    return fnValDep[F].args;
}
std::unordered_set<FunctionDataType> &FunctionBacktrackPointerArgumentValue(Function *F, unsigned argIndex, std::unordered_set<Function*> &analysisParents) {
    if (!fnArgValDep[F][argIndex].started) {
        fnArgValDep[F][argIndex].started = true;
        analysisParents.insert(F);
        if (F->isDeclaration()) { // defined elsewhere; assume arg value is dependent on all inputs (or just itself)
            for (unsigned i = 0; i < F->getFunctionType()->getNumParams()) {
                if (i == argIndex) { // just itself
                    fnArgValDep[F][argIndex].args.insert(i);
                }
            }
        } else {
            // find users of this value and check their arguments
            std::vector<BasicBlock *> bbs;
            for (Use &use : F->getArg(argIndex)->uses()) {
                if (AllocaInst *ai = dyn_cast<AllocaInst>(use.getUser())) {
                    for (StoreInst *stor : ai->users()) {
                        AddFnArgsFromInstData(fnArgValDep[F][argIndex].args, stor, &analysisParents);
                        for (BasicBlock *B : stor->getParent()->predecessors()) {
                            bbs.pushBack(stor->getParent());
                        }
                    }
                }
            }
            std::unordered_set<BasicBlock *> checked;
            for (int i = 0; i < bbs.size(); i++) {
                BasicBlock *B = bbs[i];
                if (checked.find(B) == checked.end()) { // not in set
                    checked.insert(B);
                    for (BasicBlock *pred : B->predecessors()) {
                        bbs.pushBack(pred);
                    }
                }
            }
            for (BasicBlock *B : checked) {
                if (BranchInst *branch = dyn_cast<BranchInst>(B->getTerminator())) {
                    if (branch->isConditional()) { // redundant check
                        AddFnArgsFromInstData(fnArgValDep[F][argIndex].args, branch, &analysisParents);
                    }
                }
            }
        }
        analysisParents.erase(F);
    } else if (!fnArgValDep[F][argIndex].finished) {
        // recursive call

    }
    fnArgValDep[F][argIndex].finished = true;
    return fnArgValDep[F][argIndex].args;
}

void AddInstInputsFromInputs(std::unordered_set<InstructionDataType> &retArgs, std::unordered_set<InstructionDataType> &fromArgs) {
    for (InstructionDataType index : fromArgs) {
        retArgs.insert(index);
    }
}
std::unordered_set<InstructionDataType> &InstructionBacktrackValue(Value *val, std::unordered_set<Function*> &analysisParents) {
    if (!instValDep[val].started) {
        instValDep[val].started = true;
        if (Instruction *I = dyn_cast<Instruction>(val)) {
            if (PHINode *xi = dyn_cast<PHINode>(I)) {
                debug("PHI Instruction: " << *xi)
                for (Use &op : xi->incoming_values()) {
                    Value *prev = op.get();
                    AddInstInputsFromInputs(instValDep[val].inputlines, InstructionBacktrackValue(prev));
                }
            } else if (BranchInst *xi = dyn_cast<BranchInst>(I)) {
                if (xi->isConditional()) {
                    debug("BrCond Instruction: " << *xi)
                    Value *prev = xi->getCondition();
                    AddInstInputsFromInputs(instValDep[val].inputlines, InstructionBacktrackValue(prev));
                }
            } else if (BinaryOperator *xi = dyn_cast<BinaryOperator>(I)) {
                debug("BinOp Instruction: " << *xi)
                for (Use &op : xi->operands()) {
                    Value *prev = op.get();
                    AddInstInputsFromInputs(instValDep[val].inputlines, InstructionBacktrackValue(prev));
                }
            } else if (LoadInst *xi = dyn_cast<LoadInst>(I)) { // must come before UnaryInstruction!!!!
                debug("Load Instruction: " << *xi)
                if (Instruction *addr = dyn_cast<Instruction>(xi->getPointerOperand())) {
                    Use *lastUse;
                    for (Use *use : addr->uses()) {
                        lastUse = use;
                    }
                    User *lastUser = lastUse->getUser();

                    std::string varName = findVariableName3(addr);
                    /*
                    DILocalVariable *varb;
                    if (AllocaInst *varInst = dyn_cast<AllocaInst>(addr)) {
                        for (DbgDeclareInst *ddi : FindDbgDeclareUses(static_cast<Instruction*>(varInst))) {
                            //dbgs() << "Debug instruction: " << *ddi << '\n';
                            varb = ddi->getVariable();
                        }
                    } else if (GetElementPtrInst *varInst = dyn_cast<GetElementPtrInst>(addr)) {
                        
                    }*/

                    if (StoreInst *si = dyn_cast<StoreInst>(lastUser)) {
                        Instruction *prev = static_cast<Instruction*>(si);

                    } else if (CallInst *ci = dyn_cast<CallInst>(lastUser)){
                        if (Function *cfn = ci->getCalledFunction()) {
                            StringRef fname = cfn->getName();
                            if (fname.equals("scanf") || fname.equals("__isoc99_scanf")) {
                                // scanf; #'th user input
                                outs() << "Critical user input: function '" << I->getFunction->getName() << "' , line " << cfn->getDebugLoc().getLine() << " , variable '" << varb->getName() << ", scanf input #" << ci->getDebugLoc()->getLine() << '\n';
                            } else if (fname.equals("fprintf")) { // fprintf, output
                            } else if (fname.equals("printf")) { // printf(char * str, ...), output
                            } else if (fname.equals("fgets")) { // char *fgets(char *str, int count, FILE *stream), input from file stream
                            } else if (fname.equals("strcpy")) { // strcpy, string 2 into string 1
                            } else if (fname.equals("sprintf")) { // sprintf, varargs into string 1
                            } else if (fname.equals("fclose")) { // fclose, no data manip
                            } else if (fname.equals("fflush")) { // fflush, dumps but does not generate output to stream arg
                            } else if (fname.equals("time")) { // time, no data manip
                            } else if (fname.equals("calloc")) { // calloc, no data manip
                            } else if (fname.equals("fopen")) { // fopen, opens filename to fp (TODO: read this)
                            } else if (fname.equals("atoi")) { // int atoi (const char * str), returns int from string
                            } else if (fname.equals("free")) { // free, no data manip
                            } else if (fname.equals("realloc")) { // realloc, no data manip
                            } else if (fname.equals("malloc")) { // malloc, no data manip
                            } else if (fname.equals("qsort")) { // qsort, sorts input
                            } else if (fname.equals("exit")) { // exit, terminates the program
                            } else if (fname.equals("__assert_fail")) { // exit, terminates the program
                            } else {
                                // track where the pointer value is used

func decls : ; Function Attrs: nofree nosync nounwind readnone speculatable willreturn
declare void @llvm.dbg.label(metadata) #1

func decls : declare i32 @fprintf(%struct._IO_FILE* noundef, i8* noundef, ...) #2

func decls : ; Function Attrs: nofree nosync nounwind readnone speculatable willreturn
declare double @llvm.fmuladd.f64(double, double, double) #1

func decls : declare i32 @fclose(%struct._IO_FILE* noundef) #2

func decls : ; Function Attrs: nounwind
declare noalias i8* @calloc(i64 noundef, i64 noundef) #4

func decls : declare void @qsort(i8* noundef, i64 noundef, i64 noundef, i32 (i8*, i8*)* noundef) #2

func decls : declare i32 @fflush(%struct._IO_FILE* noundef) #2

func decls : ; Function Attrs: argmemonly nofree nounwind willreturn
declare void @llvm.memcpy.p0i8.p0i8.i64(i8* noalias nocapture writeonly, i8* noalias nocapture readonly, i64, i1 immarg) #7

func decls : ; Function Attrs: nounwind readonly willreturn
declare i32 @atoi(i8* noundef) #6

func decls : ; Function Attrs: nounwind
declare i64 @time(i64* noundef) #4

func decls : ; Function Attrs: noreturn nounwind
declare void @exit(i32 noundef) #3

func decls : ; Function Attrs: nounwind
declare i8* @realloc(i8* noundef, i64 noundef) #4

func decls : ; Function Attrs: argmemonly nofree nounwind willreturn writeonly
declare void @llvm.memset.p0i8.i64(i8* nocapture writeonly, i8, i64, i1 immarg) #5

func decls : declare noalias %struct._IO_FILE* @fopen(i8* noundef, i8* noundef) #2

func decls : ; Function Attrs: noreturn nounwind
declare void @__assert_fail(i8* noundef, i8* noundef, i32 noundef, i8* noundef) #3

func decls : ; Function Attrs: nounwind
declare i8* @strcpy(i8* noundef, i8* noundef) #4

func decls : ; Function Attrs: nounwind
declare i32 @sprintf(i8* noundef, i8* noundef, ...) #4

func decls : ; Function Attrs: nofree nosync nounwind readnone speculatable willreturn
declare void @llvm.dbg.declare(metadata, metadata, metadata) #1

func decls : ; Function Attrs: nounwind
declare void @free(i8* noundef) #4

func decls : ; Function Attrs: nounwind
declare i32 @__isoc99_sscanf(i8* noundef, i8* noundef, ...) #4

func decls : ; Function Attrs: nounwind
                            }
                        } else {
                            // called function pointer, dunno what to do next
                        }
                    }
                }
            } else if (CmpInst *xi = dyn_cast<CmpInst>(I)) {
                debug("Compare Instruction: " << *xi)
                for (Use &op : xi->operands()) {
                    Value *prev = op.get();
                    AddInstInputsFromInputs(instValDep[val].inputlines, InstructionBacktrackValue(prev));
                }
            } else if (SelectInst *xi = dyn_cast<SelectInst>(I)) {
                debug("Select Instruction: " << *xi)
                for (Use &op : xi->operands()) { // getCondition(), getTrueValue(), getFalseValue()
                    Value *prev = op.get();
                    AddInstInputsFromInputs(instValDep[val].inputlines, InstructionBacktrackValue(prev));
                }
            } else if (ReturnInst *xi = dyn_cast<SelectInst>(I)) {
                debug("Return Instruction: " << *xi)
                for (Use &op : xi->operands()) { // getCondition(), getTrueValue(), getFalseValue()
                    Value *prev = op.get();
                    AddInstInputsFromInputs(instValDep[val].inputlines, InstructionBacktrackValue(prev));
                }
            } else if (UnaryInstruction *xi = dyn_cast<UnaryOperator>(I)) {
                debug("UnaryOp Instruction: " << *xi)
                for (Use &op : xi->operands()) {
                    Value *prev = op.get();
                    AddInstInputsFromInputs(instValDep[val].inputlines, InstructionBacktrackValue(prev));
                }
            } else {
                debug("UnaryOp Instruction: " << *xi)
            }
        }
    } else if (!instValDep[val].finished) {
        // recursive call
        // shouldn't happen?? show a dependency on itself
        debug("Recursive inst val: " << *I)
        instValDep[val].inputlines.insert(I);
    }
    instValDep[val].finished = true;
    return instValDep[val].inputlines;
}

struct CriticalInputPass : public PassInfoMixin<CriticalInputPass> {
    PreservedAnalyses run(Module &M, ModuleAnalysisManager &AM) {
        for (auto &F : M) {
            //for (F.getArguments())
            if (F.getName().equals("main")) {
                std::unordered_set<Function*> mainAnalysisParents;
                std::unordered_set<unsigned> &mainArgs = FunctionBacktrackRuntime(&F, mainAnalysisParents);
            }
        /*
        for (auto& B : F) {
        for (auto& I : B) {
        }
        }
        */
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