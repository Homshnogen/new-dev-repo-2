
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


struct ArgDataIn {
    bool used; // false
    std::map<unsigned, ArgDataIn> members; // {}
};

struct FunctionInputData {
    bool started, finished; // false
    std::map<unsigned, ArgDataIn> args; // {}
    std::map<GlobalVariable*, ArgDataIn> globals; // {}
};

struct ArgDataOut {
    FunctionInputData inputs; 
    std::map<unsigned, ArgDataOut> members; // {}
};

struct FunctionOutputData {
    bool started, finished; // false
    ArgDataOut retval;
    std::map<unsigned, ArgDataOut> args; // {}
    std::map<GlobalVariable*, ArgDataOut> globals; // {}
};
//struct InputData {
//    Value *value;
//    std::string descriptor;
//}
/*
typedef InstructionDataType Use*
struct InstructionData {
    bool started, finished;
    std::unordered_set<InstructionDataType> inputlines;
}; */

struct MemoryRecord {
    bool isGlobal;
    bool isRetval;
    bool isArg;
    union {
        unsigned argIndex;
        GlobalVariable *globalVar;
    };
    std::vector<unsigned> membertree;
};

std::map<Function*, FunctionInputData> fnRunDep;
std::map<Function*, FunctionOutputData> fnValDep;
//std::map<Function*, std::map<unsigned, FunctionInputData>> fnArgValDep;
std::map<Value*, FunctionInputData> instFnArgs;
std::map<Value*, FunctionInputData> instValDep;
std::map<GlobalVariable*, std::unordered_set<Function*>> GlobalFDep;

FunctionInputData &InstructionBacktrackValue(Value *val, std::unordered_set<Function*> &analysisParents);
FunctionInputData &GetFnArgsFromInstData(Instruction *I, std::unordered_set<Function*> &analysisParents) {
    if (!instFnArgs[I].started) {
        FunctionInputData & unknown = InstructionBacktrackValue(I, analysisParents);
        //AddFnArgsFromInstData(instFnArgs[I], InstructionBacktrackValue(I, analysisParents));

        /*
        for (Use *use : InstructionBacktrackValue(I, analysisParents)) {
            if (StoreInst *si = dyn_cast<StoreInst>(use->getUser())) {
                if (Argument *arg = dyn_cast<Argument>(si->getValueOperand())) {
                    instFnArgs[I].args.insert(arg->getArgNo());
                }
            }
        }*/
    } else if (!instFnArgs[I].finished) {
        // recursive call; shouldn't happen
        debug("GetFnArgsFromInstData recursive call")
    }
    instFnArgs[I].finished = true;
    return instFnArgs[I];
}
void AddArgDataIn(ArgDataIn &retData, ArgDataIn &fromData) {
    retData.used = fromData.used;
    for (auto entry : fromData.members) { // key = first, value = second
        AddArgDataIn(retData.members[entry.first], entry.second);
    }
}
void AddFnInputs(FunctionInputData &retData, FunctionInputData &fromData) {
    // copy args
    for (auto entry : fromData.args) { // key = first, value = second
        AddArgDataIn(retData.args[entry.first], entry.second);
    }
    // copy globals
    for (auto entry : fromData.globals) { // key = first, value = second
        AddArgDataIn(retData.globals[entry.first], entry.second);
    }
}
std::vector<std::vector<unsigned>> getMemberTrees(ArgDataIn &fromData) {
    std::vector<std::vector<unsigned>> ret;
    if (fromData.used) {
        ret.push_back(std::vector<unsigned>());
    } else {
        for (auto entry : fromData.members) { // key = first, value = second
            for (auto &&temp : getMemberTrees(entry.second)) {
                auto &&temp2 = std::vector<unsigned>();
                temp2.push_back(entry.first);
                for(unsigned idx : temp) {
                    temp2.push_back(idx);
                }
                ret.push_back(temp);
            }
        }
    }
    return ret;
}
std::vector<MemoryRecord> getInputMemory(FunctionInputData &fromData) {
    // copy args
    std::vector<MemoryRecord> ret;
    for (auto entry : fromData.args) { // key = first, value = second
        for (auto &&mtree : getMemberTrees(entry.second)) {
            MemoryRecord mdata;
            mdata.isArg = true;
            mdata.argIndex = entry.first;
            mdata.membertree = mtree;
            ret.push_back(mdata);
        }
    }
    // copy globals
    for (auto entry : fromData.globals) { // key = first, value = second
        for (auto &&mtree : getMemberTrees(entry.second)) {
            MemoryRecord mdata;
            mdata.isGlobal = true;
            mdata.globalVar = entry.first;
            ret.push_back(mdata);
        }
    }
    return ret;
}
void AddArgDataOut(ArgDataOut &retData, ArgDataOut &fromData) {
    AddFnInputs(retData.inputs, fromData.inputs);
    for (auto entry : fromData.members) { // key = first, value = second
        AddArgDataOut(retData.members[entry.first], entry.second);
    }
}
void AddFnOutputs(FunctionOutputData &retData, FunctionOutputData &fromData) {
    // copy retval
    AddArgDataOut(retData.retval, fromData.retval);
    // copy args
    for (auto entry : fromData.args) { // key = first, value = second
        AddArgDataOut(retData.args[entry.first], entry.second);
    }
    // copy globals
    for (auto entry : fromData.globals) { // key = first, value = second
        AddArgDataOut(retData.globals[entry.first], entry.second);
    }
}
void markArgDataIn(ArgDataIn &retData, std::vector<unsigned> membertree) {
    ArgDataIn *memberData = &retData;
    for (unsigned nextmember : membertree) {
        memberData = &(memberData->members[nextmember]);
    }
    memberData->used = true;
}
void markArgDataIn(FunctionInputData &retData, MemoryRecord &index) {
    if (index.isGlobal) {
        markArgDataIn(retData.globals[index.globalVar], index.membertree);
    } else if (index.isArg) {
        markArgDataIn(retData.args[index.argIndex], index.membertree);
    }
}
bool getArgDataIn(ArgDataIn &retData, std::vector<unsigned> membertree) {
    ArgDataIn *memberData = &retData;
    for (unsigned nextmember : membertree) {
        if (memberData->used) { // parent is used, assume all children are used
            return true;
        } else if (memberData->members.find(nextmember) == memberData->members.end()) { // no child members left, all children are false
            return false;
        }
        memberData = &(memberData->members[nextmember]);
    }
    return memberData->used;
}
bool getArgDataIn(FunctionInputData &retData, MemoryRecord &index) {
    if (index.isGlobal) {
        return getArgDataIn(retData.globals[index.globalVar], index.membertree);
    } else if (index.isArg) {
        return getArgDataIn(retData.args[index.argIndex], index.membertree);
    }
    return false;
}
void markArgDataOut(ArgDataOut &retData, FunctionInputData &inputs, std::vector<unsigned> membertree) {
    ArgDataOut *memberData = &retData;
    for (unsigned nextmember : membertree) {
        AddFnInputs(memberData->inputs, inputs);
        memberData = &(memberData->members[nextmember]);
    }
    AddFnInputs(memberData->inputs, inputs);
}
void markArgDataOut(FunctionOutputData &retData, FunctionInputData &inputs, MemoryRecord &index) {
    if (index.isGlobal) {
        markArgDataOut(retData.globals[index.globalVar], inputs, index.membertree);
    } else if (index.isArg) {
        markArgDataOut(retData.args[index.argIndex], inputs, index.membertree);
    } else if (index.isRetval) {
        markArgDataOut(retData.retval, inputs, index.membertree);
    }
}
FunctionInputData &getArgDataOut(ArgDataOut &retData, std::vector<unsigned> membertree) {
    ArgDataOut *memberData = &retData;
    for (unsigned nextmember : membertree) {
        if (memberData->members.find(nextmember) == memberData->members.end()) { // no child this deep, all children are modified by what modifies this
            return memberData->inputs;
        }
        memberData = &(memberData->members[nextmember]);
    }
    return memberData->inputs;
}
FunctionInputData &getArgDataOut(FunctionOutputData &retData, MemoryRecord &index) {
    if (index.isGlobal) {
        return getArgDataOut(retData.globals[index.globalVar], index.membertree);
    } else if (index.isArg) {
        return getArgDataOut(retData.args[index.argIndex], index.membertree);
    } else if (index.isRetval) {
        return getArgDataOut(retData.retval, index.membertree);
    }
    static FunctionInputData none;
    return none;
}
MemoryRecord getMemoryRecord(Value *val) {
    // very similar to findVariableName
    // expand out for args (alloca/Argument) >>
    // expand out for global variables >>

    // expand out for call >> <<

    // expand out for LoadInst <<
    // expand out for StoreInst <<
    // expand out for GetElementPtrInst <<
    // expand out for GetElementPtr ConstantExpr <<
    return MemoryRecord();
}
std::string getRecordName(MemoryRecord &record) {
    std::string ret;
    if (record.isArg) {
        ret = "(arg"+std::to_string(record.argIndex)+")";
    } else if (record.isGlobal) {
        ret = record.globalVar->getName();
    }
    for (int i = 0; i < record.membertree.size(); i++) {
        ret += "["+std::to_string(record.membertree[i])+"]";
    }
    return ret;
}
void backtrackValuesFromCall(CallInst *call, MemoryRecord &Query) {
    // probably move the function name switch here
    // probably inplement use-all and use-none flags

}

void AddFnArgsFromInstData(FunctionInputData &retData, Instruction *I, std::unordered_set<Function*> &analysisParents) {
    AddFnInputs(retData, GetFnArgsFromInstData(I, analysisParents));
}

FunctionInputData &FunctionBacktrackRuntime(Function *F, std::unordered_set<Function*> &analysisParents) {
    if (!fnRunDep[F].started) {
        fnRunDep[F].started = true;
        analysisParents.insert(F);
        if (F->isDeclaration()) { // defined elsewhere; assume runtime is dependent on no inputs
            // nothing needs to be done
        } else {
            for (BasicBlock &B : *F) {
                for (Instruction &I : B) {
                    // find branches and fp calls and perform instruction analysis
                    if (BranchInst *bi = dyn_cast<BranchInst>(&I)) {
                        if (bi->isConditional()) {
                            // branch instruction
                            if (Instruction *next = dyn_cast<Instruction>(bi->getCondition())) {
                                AddFnArgsFromInstData(fnRunDep[F], next, analysisParents);
                            }
                        }
                    } else if (CallInst *ci = dyn_cast<CallInst>(&I)) {
                        if (Function *nextF = ci->getCalledFunction()) {
                            if (nextF->getName().equals("logBranchDecision")) { // else if for all special functions
                                debug("LogBD call: " << *ci)
                                if (Instruction *a = dyn_cast<Instruction>(ci->getNextNode())) {
                                    debug("Next (flagged) instruction: " << *a)
                                }
                            } else if (nextF->getName().equals("llvm.dbg.declare")) { // else if for all special functions
                            } else {
                                debug("called non-LogBD function: " << ci->getCalledFunction()->getName())
                                FunctionInputData &nextInputs = FunctionBacktrackRuntime(nextF, analysisParents);
                                /*
                                for (unsigned index : FunctionBacktrackRuntime(nextF, analysisParents)) {
                                    // check runtime-determining arguments passed into call
                                    if (Instruction *nextI = dyn_cast<Instruction>(ci->getArgOperand(index))) {
                                        //AddArgDataIn(fnRunDep[F], nextI, analysisParents);
                                    } else {
                                        // arg was constant?
                                        debug("arg was constant?: " << *ci->getArgOperand(index))
                                    }
                                }
                                */
                            }
                        } else if (Instruction *next = dyn_cast<Instruction>(ci->getCalledOperand())) {
                            // called function pointer
                            debug("Called function pointer: " << *next)
                            //AddFnArgsFromInstData(fnRunDep[F].args, next, analysisParents);
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
    return fnRunDep[F];
}
FunctionOutputData &FunctionBacktrackValue(Function *F, std::unordered_set<Function*> &analysisParents) {
    if (!fnValDep[F].started) {
        fnValDep[F].started = true;
        analysisParents.insert(F);
        if (F->isDeclaration()) { // defined elsewhere; assume return value is dependent on all inputs
            /*
            for (unsigned i = 0; i < F->getFunctionType()->getNumParams(); i++) {
                fnValDep[F].args[i].used = true;
            }
            */
        } else {
            for (BasicBlock &B : *F) {
                for (Instruction &I : B) {
                    // find branch and return instructions and perform instruction analysis
                    if (BranchInst *bi = dyn_cast<BranchInst>(&I)) {
                        if (bi->isConditional()) {
                            // branch instruction
                            if (Instruction *next = dyn_cast<Instruction>(bi->getCondition())) {
                                //AddFnArgsFromInstData(fnValDep[F], next, analysisParents);
                            }
                        }
                    } else if (ReturnInst *ri = dyn_cast<ReturnInst>(&I)) {
                        // return instruction
                        if (Instruction *next = dyn_cast<Instruction>(ri->getReturnValue())) {
                            //AddFnArgsFromInstData(fnValDep[F], next, analysisParents);
                        } else if (GlobalVariable *next = dyn_cast<GlobalVariable>(ri->getReturnValue())) {
                            //AddFnArgsFromInstData(fnValDep[F], next, analysisParents);
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
    return fnValDep[F];
}

void AddInstInputsFromInputs(FunctionInputData &retData, FunctionInputData &fromData) {
    AddFnInputs(retData, fromData);
}
FunctionInputData &InstructionBacktrackValue(Value *val, std::unordered_set<Function*> &analysisParents) {
    if (!instValDep[val].started) {
        instValDep[val].started = true;
        if (Instruction *I = dyn_cast<Instruction>(val)) {
            if (PHINode *xi = dyn_cast<PHINode>(I)) {
                debug("PHI Instruction: " << *xi)
                for (Use &op : xi->incoming_values()) {
                    Value *prev = op.get();
                    //AddInstInputsFromInputs(instValDep[val].inputlines, InstructionBacktrackValue(prev));
                }
            } else if (BranchInst *xi = dyn_cast<BranchInst>(I)) {
                if (xi->isConditional()) {
                    debug("BrCond Instruction: " << *xi)
                    Value *prev = xi->getCondition();
                    //AddInstInputsFromInputs(instValDep[val].inputlines, InstructionBacktrackValue(prev));
                }
            } else if (BinaryOperator *xi = dyn_cast<BinaryOperator>(I)) {
                debug("BinOp Instruction: " << *xi)
                for (Use &op : xi->operands()) {
                    Value *prev = op.get();
                    //AddInstInputsFromInputs(instValDep[val].inputlines, InstructionBacktrackValue(prev));
                }
            } else if (LoadInst *xi = dyn_cast<LoadInst>(I)) { // must come before UnaryInstruction!!!!
                debug("Load Instruction: " << *xi)
                if (Instruction *addr = dyn_cast<Instruction>(xi->getPointerOperand())) {
                    Use *lastUse;
                    for (Use &use : addr->uses()) {
                        lastUse = &use;
                    }
                    User *lastUser = lastUse->getUser();

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
                            if (cfn->isDeclaration()) { // external/undefined function call; manually check
                                StringRef fname = cfn->getName();
                                if (fname.equals("scanf") || fname.equals("__isoc99_scanf")) { // input from command line, on varargs
                                    // scanf; #'th user input
                                    //outs() << "Critical user input: function '" << I->getFunction->getName() << "' , line " << cfn->getDebugLoc().getLine() << " , variable '" << varb->getName() << ", scanf input #" << ci->getDebugLoc()->getLine() << '\n';
                                } else if (fname.equals("sscanf") || fname.equals("__isoc99_sscanf")) { // int sscanf( const char *buffer, const char *format, ... ), input from string 1, on varargs
                                } else if (fname.equals("fprintf")) { // fprintf, output
                                } else if (fname.equals("printf")) { // printf(char * str, ...), output
                                } else if (fname.equals("fgets")) { // char *fgets(char *str, int count, FILE *stream), input from file stream
                                } else if (fname.equals("strcpy")) { // char *strcpy( char *dest, const char *src ), string 2 into string 1
                                } else if (fname.equals("sprintf")) { // int sprintf( char *buffer, const char *format, ... ), varargs and arg2 into string 1
                                } else if (fname.equals("fprintf")) { // int fprintf(FILE *stream, const char *format, ...), output
                                } else if (fname.equals("fclose")) { // int fclose( FILE *stream ), no data manip
                                } else if (fname.equals("fflush")) { // i32 @fflush(%struct._IO_FILE* noundef), dumps but does not generate output to stream arg
                                } else if (fname.equals("time")) { // time, no data manip
                                } else if (fname.equals("calloc")) { // i8* @calloc(i64 noundef, i64 noundef), no data manip
                                } else if (fname.equals("fopen")) { // FILE *fopen(const char *filename, const char *mode), opens filename to fp (return value) (TODO: read this)
                                } else if (fname.equals("atoi")) { // int atoi (const char * str), returns int from string
                                } else if (fname.equals("free")) { // void @free(i8* noundef), no data manip (could return error)
                                } else if (fname.equals("realloc")) { // i8* @realloc(i8* noundef, i64 noundef), no data manip
                                } else if (fname.equals("malloc")) { // malloc, no data manip
                                } else if (fname.equals("qsort")) { // void @qsort(i8* ptr, i64 count, i64 size, i32 (i8*, i8*)* comp), sorts input
                                } else if (fname.equals("exit")) { // void @exit(i32 noundef), terminates the program (no data manip)
                                } else if (fname.equals("llvm.dbg.declare")) { // llvm.dbg.declare, metadata (no data manip)
                                } else if (fname.equals("llvm.memcpy.p0i8.p0i8.i64")) { // void @llvm.memcpy.p0i8.p0i8.i64(i8* noalias nocapture writeonly, i8* noalias nocapture readonly, i64, i1 immarg), copies mem 2 into mem 1 for arg3 entries
                                } else if (fname.equals("llvm.memset.p0i8.i64")) { // void @llvm.memset.p0i8.i64(i8* nocapture writeonly, i8, i64, i1 immarg), terminates the program (no data manip), initializes mem 1 to arg2 for arg3 entries
                                } else if (fname.equals("__assert_fail")) { // void __assert_fail(const char * assertion, const char * file, unsigned int line, const char * function), terminates the program if false (ignore)
                                } else {
                                    // other external function or intrinsic; do not care
                                }
                            } else {
                                // defined function call; track the function call
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
                    //AddInstInputsFromInputs(instValDep[val].inputlines, InstructionBacktrackValue(prev));
                }
            } else if (SelectInst *xi = dyn_cast<SelectInst>(I)) {
                debug("Select Instruction: " << *xi)
                for (Use &op : xi->operands()) { // getCondition(), getTrueValue(), getFalseValue()
                    Value *prev = op.get();
                    //AddInstInputsFromInputs(instValDep[val].inputlines, InstructionBacktrackValue(prev));
                }
            } else if (ReturnInst *xi = dyn_cast<ReturnInst>(I)) {
                debug("Return Instruction: " << *xi)
                for (Use &op : xi->operands()) { // getCondition(), getTrueValue(), getFalseValue()
                    Value *prev = op.get();
                    //AddInstInputsFromInputs(instValDep[val].inputlines, InstructionBacktrackValue(prev));
                }
            } else if (UnaryInstruction *xi = dyn_cast<UnaryOperator>(I)) {
                debug("UnaryOp Instruction: " << *xi)
                for (Use &op : xi->operands()) {
                    Value *prev = op.get();
                    //AddInstInputsFromInputs(instValDep[val].inputlines, InstructionBacktrackValue(prev));
                }
            } else {
                debug("UnaryOp Instruction: " << *xi)
            }
        }
    } else if (!instValDep[val].finished) {
        // recursive call
        // shouldn't happen?? show a dependency on itself
        debug("Recursive inst val: " << *val)
        //instValDep[val].inputlines.insert(I);
    }
    instValDep[val].finished = true;
    return instValDep[val];
}
struct CriticalInputPass : public PassInfoMixin<CriticalInputPass> {
    PreservedAnalyses run(Module &M, ModuleAnalysisManager &AM) {
        std::unordered_set<std::string> globalUses;
        std::vector<std::string> inputDeps;
        
        for (auto &G : M.globals()) {
            if (!G.isConstant()) {
                for (User *user : G.users()) {
                    std::string temp;
                    raw_string_ostream temps(temp);
                    if (Instruction *I = dyn_cast<Instruction>(user)) {
                        GlobalFDep[&G].insert(I->getFunction());
                    } else if (ConstantExpr * expr = dyn_cast<ConstantExpr>(user)) {
                        //temps << "gname: " << gname << ", CExpr: " << expr->getOpcodeName();
                        bool hasUsers = false;
                        for (auto *expruser : expr->users()) {
                            hasUsers = true;
                            if (Instruction *I = dyn_cast<Instruction>(expruser)) {
                                GlobalFDep[&G].insert(I->getFunction());
                            } else {
                                temps << "expr used by expr?: " << *expruser;
                            }
                        }
                        if (!hasUsers) {
                            temps << "unused expr: " << *expr;
                        }
                    } else {
                        temps << "guse: " << *user << ", unknown1";
                    }
                    if (temp.length() > 0) {
                        globalUses.insert(temp);
                    }
                }
            }
        }
        for (auto &F : M) {
            //if (F.getName().equals("main")) {}
            //for (auto& B : F) { for (auto& I : B) {}}
        }
        if (Function *Main = M.getFunction("main")) {
            std::unordered_set<Function*> mainAnalysisParents;
            FunctionInputData &mainArgs = FunctionBacktrackRuntime(Main, mainAnalysisParents);
            for (MemoryRecord &input : getInputMemory(mainArgs)) {
                inputDeps.push_back(getRecordName(input));
            }
        }
        for (std::string const &str : inputDeps) {
            errs() << "Function depends on input : " << str << '\n';
        }
        for (std::string const &str : globalUses) {
            errs() << "global uses : " << str << '\n';
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