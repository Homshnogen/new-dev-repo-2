
#include "llvm/Pass.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"

#include "llvm/IR/DebugInfo.h"
#include <map>
#include <unordered_set>
#include <string>
#include <regex>


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
std::map<GlobalVariable*, std::unordered_set<Function*>> globalFDep;

FunctionInputData &InstructionBacktrackValue(Value *val, std::unordered_set<Function*> &analysisParents);

bool isArgDataInEmpty(ArgDataIn &data) {
    if (data.used) {
        return false;
    }
    for (auto &entry : data.members) { // key = first, value = second
        if (!isArgDataInEmpty(entry.second)) {
            return false;
        }
    }
    return true;
}

bool isFnInputEmpty(FunctionInputData &data) {
    for (auto &entry : data.args) { // key = first, value = second
        if (!isArgDataInEmpty(entry.second)) {
            return false;
        }
    }
    for (auto &entry : data.globals) { // key = first, value = second
        if (!isArgDataInEmpty(entry.second)) {
            return false;
        }
    }
    return true;
}

void AddArgDataIn(ArgDataIn &retData, ArgDataIn &fromData) {
    retData.used = fromData.used;
    for (auto &entry : fromData.members) { // key = first, value = second
        AddArgDataIn(retData.members[entry.first], entry.second);
    }
}
void AddFnInputs(FunctionInputData &retData, FunctionInputData &fromData) {
    // copy args
    for (auto &entry : fromData.args) { // key = first, value = second
        AddArgDataIn(retData.args[entry.first], entry.second);
    }
    // copy globals
    for (auto &entry : fromData.globals) { // key = first, value = second
        AddArgDataIn(retData.globals[entry.first], entry.second);
    }
}
std::vector<std::vector<unsigned>> getMemberTrees(ArgDataIn &fromData) {
    std::vector<std::vector<unsigned>> ret = std::vector<std::vector<unsigned>>();
    if (fromData.used) {
        ret.push_back(std::vector<unsigned>());
    } else {
        for (auto &entry : fromData.members) { // key = first, value = second
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
    std::vector<MemoryRecord> ret = std::vector<MemoryRecord>();
    for (auto &entry : fromData.args) { // key = first, value = second
        for (auto mtree : getMemberTrees(entry.second)) {
            MemoryRecord mdata = MemoryRecord();
            mdata.isArg = true;
            mdata.argIndex = entry.first;
            mdata.membertree = mtree;
            ret.push_back(mdata);
        }
    }
    // copy globals
    for (auto &entry : fromData.globals) { // key = first, value = second
        for (auto mtree : getMemberTrees(entry.second)) {
            MemoryRecord mdata = MemoryRecord();
            mdata.isGlobal = true;
            mdata.globalVar = entry.first;
            ret.push_back(mdata);
        }
    }
    return ret;
}
void AddArgDataOut(ArgDataOut &retData, ArgDataOut &fromData) {
    AddFnInputs(retData.inputs, fromData.inputs);
    for (auto &entry : fromData.members) { // key = first, value = second
        AddArgDataOut(retData.members[entry.first], entry.second);
    }
}
void AddFnOutputs(FunctionOutputData &retData, FunctionOutputData &fromData) {
    // copy retval
    AddArgDataOut(retData.retval, fromData.retval);
    // copy args
    for (auto &entry : fromData.args) { // key = first, value = second
        AddArgDataOut(retData.args[entry.first], entry.second);
    }
    // copy globals
    for (auto &entry : fromData.globals) { // key = first, value = second
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

void sanitizeVariableName(std::string &input) {
    std::smatch match;
    std::regex find;
    //outs() << "input : "<< input ;
    find = std::regex("^\\(\\&(.*)\\[0\\]\\)\\&$");
    while (std::regex_search(input, match, find)) {
        input = (match.str(1));
        //match.position(1)
    }
    find = std::regex("^\\(\\&(.*)\\)\\&\\[0\\]$");
    while (std::regex_search(input, match, find)) {
        input = (match.str(1));
    }
    find = std::regex("^\\(\\*\\(\\&(.*)\\)\\&\\)\\*$");
    while (std::regex_search(input, match, find)) {
        input = (match.str(1));
    }
    find = std::regex("^\\(\\&\\(\\*(.*)\\)\\*\\)\\&$"); // dunno about this one
    while (std::regex_search(input, match, find)) {
        input = (match.str(1));
    }
    //outs() << ", output : "<< input << '\n';
}
void trimVariableName(std::string &input) {
    std::smatch match;
    std::regex find("^(.*\\))[\\*\\&](.*)$");
    while (std::regex_search(input, match, find)) {
        input = (match.str(1) + match.str(2));
        //match.position(1)
    }
}
std::string findVariableNameImpl (Value *val);
std::string nameFromGEP (User *gep) { // for each fromLoad, add a zero before second. For string, remove a zero after
    std::string ret;
    llvm::raw_string_ostream rets(ret);
    for (unsigned i = 0; i < gep->getNumOperands(); i++) {
        Use const &op = gep->getOperandUse(i);
        if (i == 0) {
            // address arg
            rets << "(&" << findVariableNameImpl(op.get()) << ")&";
            sanitizeVariableName(ret);
            std::smatch match;
            std::regex find;
            //outs() << "input : "<< input ;
            find = std::regex("^\\(\\&(.*)\\)\\&$"); // should work logically
            if (std::regex_search(ret, match, find)) {
                ret = (match.str(1));
                //match.position(1)
            }
        } else if (i == 1) {
            // address arg
            rets << '[' << findVariableNameImpl(op.get()) << ']';
            sanitizeVariableName(ret);
            ret = "(&" + ret +")&";
            sanitizeVariableName(ret);
            std::smatch match;
            std::regex find;
            //outs() << "input : "<< input ;
            find = std::regex("^\\(\\&(.*)\\)\\&$"); // should work logically
            if (std::regex_search(ret, match, find)) {
                ret = (match.str(1));
                //match.position(1)
            }
        } else {
            rets << '[' << findVariableNameImpl(op.get()) << ']';
            sanitizeVariableName(ret);
        }
    }
    ret = "(&" + ret + ")&";
    sanitizeVariableName(ret);

    return ret;
}
DIGlobalVariable *getGlobalVariableMD(GlobalVariable *val) {
    for (unsigned index = 0; MDNode *md = val->getMetadata(index); index++) {
        if (DIGlobalVariableExpression *expr = dyn_cast<DIGlobalVariableExpression>(md)) {
            return expr->getVariable();
        }
    }
    return nullptr;
}
DILocalVariable *getLocalVariableMD(AllocaInst *val) {
    for (DbgDeclareInst *ddi : FindDbgDeclareUses(static_cast<Instruction*>(val))) {
        //dbgs() << "Debug instruction: " << *ddi << '\n';
        return ddi->getVariable();
    }
    return nullptr;
}
std::string findVariableNameImpl (Value *val) {
    std::string ret;
    llvm::raw_string_ostream rets(ret);

    if (Constant *con = dyn_cast<Constant>(val)) {
        if (ConstantInt *constint = dyn_cast<ConstantInt>(con)) {
            rets << constint->getSExtValue();
        } else if (ConstantExpr *expr = dyn_cast<ConstantExpr>(con)) {
            if (std::strcmp(expr->getOpcodeName(), "getelementptr") == 0) {
                rets << nameFromGEP(expr);
            } else {
                rets << expr->getOpcodeName() << " expr of ";
                for (Use &next : expr->operands()) {
                    rets << findVariableNameImpl(next.get()) << ", "; // should be constants
                }
            }
        } else if (GlobalAlias *alias = dyn_cast<GlobalAlias>(con)) {
            rets << *val << " Global Alias";
        } else if (Function *func = dyn_cast<Function>(con)) {
            rets << func->getName();
        } else if (GlobalVariable *gvar = dyn_cast<GlobalVariable>(con)) {
            if (DIGlobalVariable *gvinfo = getGlobalVariableMD(gvar)) {
                // var has name
                rets << gvinfo->getName();
            } else if (gvar->hasExternalLinkage()) { // should be var with external initializer
                rets << "(extern : " << gvar->getGlobalIdentifier() << ")";
            } else {
                // var has no name; global const string
                //rets << *gvar->getInitializer() << " string or ";
                if (gvar->hasInitializer()) {
                    if (ConstantDataSequential *gstr = dyn_cast<ConstantDataSequential>(gvar->getInitializer())) { // is string for sure
                        if (gstr->isString()) {
                            std::string temp = gstr->getAsString().str();
                            for (int i = temp.find('\n'); i != std::string::npos; i = temp.find('\n')) {
                                temp = temp.substr(0, i) + "\\n" + temp.substr(i+1, std::string::npos);
                            }
                            for (int i = temp.find('"'); i != std::string::npos; i = temp.find('"')) {
                                temp = temp.substr(0, i) + "\\\"" + temp.substr(i+1, std::string::npos);
                            }
                            rets << '"' << temp << '"';
                        }
                    }
                }
                /*
                std::string temp;
                llvm::raw_string_ostream temps(temp);
                temps << *gvar->getInitializer();
                temp.erase(0, temp.find("\""));
                rets << temp;
                */
            }
        } else if (GlobalIFunc *gifunc = dyn_cast<GlobalIFunc>(con)) {
            rets << *val << " GIFunc";
        } else {
            rets << *val;
        }
        //rets << " is constant!!!";
    } else if (StoreInst *stor = dyn_cast<StoreInst>(val)){ // technically wrong
        rets << "store " << findVariableNameImpl(stor->getValueOperand()) << " in " << findVariableNameImpl(stor->getPointerOperand());
    } else if (AllocaInst *alloca = dyn_cast<AllocaInst>(val)){
        if (alloca->getName().equals("retval")) { // retval isn't marked?
            rets << "retval(" << alloca->getFunction()->getName() << ")";
        }
        for (DbgDeclareInst *ddi : FindDbgDeclareUses(val)) { // should be only one
            //dbgs() << "Debug instruction: " << *ddi << '\n';
            DILocalVariable *varb = ddi->getVariable();
            rets << varb->getName();
        }
    } else if (GetElementPtrInst *gep = dyn_cast<GetElementPtrInst>(val)){
        rets << nameFromGEP(gep);
    } else if (LoadInst *load = dyn_cast<LoadInst>(val)) {
        for (Use &next : load->operands()) { // should be only one
            rets << "(*" << findVariableNameImpl(next.get()) << ")*";
            sanitizeVariableName(ret);
        }
    } else if (CastInst *cast = dyn_cast<CastInst>(val)) { // maybe works????
        for (Use &next : cast->operands()) { // should be only one
            rets << findVariableNameImpl(next.get());
        }
    } else if (CallInst *ci = dyn_cast<CallInst>(val)) {
        rets << findVariableNameImpl(ci->getCalledOperand());
        rets << '(';
        for (unsigned i = 0; i < ci->arg_size(); i++) {
            rets << ((i==0) ? "" : ", ") << findVariableNameImpl(ci->getArgOperand(i));
        }
        rets << ")";
    } else if (UnaryInstruction *unary = dyn_cast<UnaryInstruction>(val)) { // includes cast, but not alloca and load
        for (Use &next : unary->operands()) { // should be only one
            rets << findVariableNameImpl(next.get());
        }
    }
    if (ret.length() == 0) {
        //rets << "unnamed(" << val->getName() << ")";
        if (val->getName().equals("")) {
            rets << "unnamed(" << *val->getType() << ")";
        } else {
            rets << "unnamed(" << *val << ")";
        }
        /*
        errs() << "unnamed: " << val->getName()  << '\n';
        for (User *user : val->users()) {
            errs() << "used by: " << *user << '\n';
        }
        */
    }
    return ret;
}
std::string findVariableName(Value *val) {
    std::string ret = findVariableNameImpl(val);
    trimVariableName(ret);
    return ret;
}

MemoryRecord getDirtyMemoryRecord(Value *val) {
    // very similar to findVariableName
    // expand out for args (alloca/Argument) >>
    // expand out for global variables >>

    // expand out for call >> <<

    // expand out for LoadInst <<
    // expand out for StoreInst <<
    // expand out for GetElementPtrInst <<
    // expand out for GetElementPtr ConstantExpr <<
    MemoryRecord ret = MemoryRecord();
    ret.membertree.push_back(0);

    if (Constant *con = dyn_cast<Constant>(val)) {
        if (ConstantInt *constint = dyn_cast<ConstantInt>(con)) {
            // not a var
        } else if (ConstantExpr *expr = dyn_cast<ConstantExpr>(con)) {
            if (std::strcmp(expr->getOpcodeName(), "getelementptr") == 0) {
                for (unsigned i = 0; i < expr->getNumOperands(); i++) { // begin gep
                    Use const &op = expr->getOperandUse(i);
                    if (i == 0) {
                        // address arg
                        ret = getDirtyMemoryRecord(op.get());
                    } else if (i == 1) {
                        // address arg
                        if (ret.membertree[ret.membertree.size()-1] == unsigned(-1)) {
                            // end
                            break;
                        } else {
                            if (ConstantInt *index = dyn_cast<ConstantInt>(op.get())) {
                                ret.membertree[ret.membertree.size()-1] += (index->getSExtValue());
                            } else {
                                ret.membertree[ret.membertree.size()-1] = unsigned(-1);
                                break;
                            }
                        }
                    } else {
                        if (ConstantInt *index = dyn_cast<ConstantInt>(op.get())) {
                            ret.membertree.push_back(index->getSExtValue());
                        } else {
                            ret.membertree.push_back(unsigned(-1));
                            break;
                        }
                    }
                } // end gep
            } else {
                // other expr - too much
                debug("Other expression : " << *expr)
                /*
                rets << expr->getOpcodeName() << " expr of ";
                for (Use &next : expr->operands()) {
                    rets << findVariableNameImpl(next.get()) << ", "; // should be constants
                }
                */
            }
        } else if (GlobalAlias *alias = dyn_cast<GlobalAlias>(con)) {
            debug(*val << " Global Alias")
        } else if (Function *func = dyn_cast<Function>(con)) {
            debug(func->getName() << " Func")
        } else if (GlobalVariable *gvar = dyn_cast<GlobalVariable>(con)) {
            if (!gvar->isConstant()) {
                //debug("global " << *gvar);
                ret.isGlobal = true;
                ret.globalVar = gvar;
            } else {
                if (gvar->hasExternalLinkage()) { // should be var with external initializer
                    debug( "(extern : " << gvar->getGlobalIdentifier() << ")" );
                } else {
                    debug( "const global variable " )
                }
            }
        } else {
            debug( "other const" )
        }
        //rets << " is constant!!!";
    } else if (StoreInst *store = dyn_cast<StoreInst>(val)) {
        ret = getDirtyMemoryRecord(store->getPointerOperand()); // probably the right behavior; TODO: check this
    } else if (AllocaInst *alloca = dyn_cast<AllocaInst>(val)){
        if (alloca->getName().equals("retval")) { // retval isn't marked?
            ret.isRetval = true;
        }
        DILocalVariable *varb = nullptr;
        for (DbgDeclareInst *ddi : FindDbgDeclareUses(val)) { // should be only one
            //dbgs() << "Debug instruction: " << *ddi << '\n';
            varb = ddi->getVariable();
        }
        if (varb != nullptr) {
            if (varb->isParameter()) {
                debug("arg " << *varb)
                ret.isArg = true;
                ret.argIndex = varb->getArg()-1; // 0 is non-arg
            } else {
                //debug("alloca not parameter")
            }
        } else {
            if (alloca->getName().equals("retval")) {
                ret.isRetval = true;
            } else {
                debug("alloca unnamed")
                debug(ret.isArg << ret.isGlobal << ret.isRetval << *val)
            }
        }
    } else if (ReturnInst *retInst = dyn_cast<ReturnInst>(val)){ // shouldn't happen
        debug("ret - shouldn't happen")
        ret.isRetval = true;
        ret.membertree.push_back(0);
    } else if (GetElementPtrInst *gep = dyn_cast<GetElementPtrInst>(val)){
        for (unsigned i = 0; i < gep->getNumOperands(); i++) { // begin gep
            Use const &op = gep->getOperandUse(i);
            if (i == 0) {
                // address arg
                ret = getDirtyMemoryRecord(op.get());
            } else if (i == 1) {
                // address arg
                if (ret.membertree[ret.membertree.size()-1] == unsigned(-1)) {
                    // end
                    break; // TODO: check breaks
                } else {
                    if (ConstantInt *index = dyn_cast<ConstantInt>(op.get())) {
                        ret.membertree[ret.membertree.size()-1] += (index->getSExtValue());
                    } else {
                        ret.membertree[ret.membertree.size()-1] = unsigned(-1);
                        break;
                    }
                }
            } else {
                if (ConstantInt *index = dyn_cast<ConstantInt>(op.get())) {
                    ret.membertree.push_back(index->getSExtValue());
                } else {
                    ret.membertree.push_back(unsigned(-1));
                    break;
                }
            }
        } // end gep
    } else if (LoadInst *load = dyn_cast<LoadInst>(val)) {
        for (Use &next : load->operands()) { // should be only one
            ret = getDirtyMemoryRecord(next.get());
        }
    } else if (CastInst *cast = dyn_cast<CastInst>(val)) { // maybe works????
        for (Use &next : cast->operands()) { // should be only one
            ret = getDirtyMemoryRecord(next.get());
        }
    } else if (CallInst *ci = dyn_cast<CallInst>(val)) {
        // value out of scope; TODO: branch scope to find retval of function
        // TODO: check external functions
        // now - do nothing
    } else if (UnaryInstruction *unary = dyn_cast<UnaryInstruction>(val)) { // includes, but not alloca and load and cast
        for (Use &next : unary->operands()) { // should be only one
            // probably copy
            debug("using unknown unary inst");
            ret = getDirtyMemoryRecord(next.get());
        }
    } else {
        // do nothing
        debug("no matches");
    }

    return ret;
}
MemoryRecord getMemoryRecord(Value *val) {
    MemoryRecord ret = getDirtyMemoryRecord(val);
    if ((ret.membertree.size() > 0) && ret.membertree[ret.membertree.size()-1] == unsigned(-1)) {
        ret.membertree.pop_back();
    }
    return ret;
}
std::string getRecordName(MemoryRecord record) {
    std::string ret;
    //debug (record.isArg << record.isGlobal << record.isRetval)
    if (record.isArg) {
        ret = "(arg"+std::to_string(record.argIndex)+")";
    } else if (record.isGlobal) {
        ret = record.globalVar->getName();
    } else if (record.isRetval) {
        ret = "retval";
    } else {
        return "";
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
    AddFnInputs(retData, InstructionBacktrackValue(I, analysisParents));
}
FunctionInputData &FunctionBacktrackRuntime(Function *F, std::unordered_set<Function*> &analysisParents) {
    if (analysisParents.find(F) != analysisParents.end()) {
        debug("uncaught FunctionBacktrackRuntime recursive call")
    }
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
                                // runtime depends on value of next;
                                AddFnInputs(fnRunDep[F], InstructionBacktrackValue(next, analysisParents));
                                AddFnArgsFromInstData(fnRunDep[F], next, analysisParents);
                            }
                        }
                    } else if (CallBase *ci = dyn_cast<CallBase>(&I)) {
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
                                for (auto &entry : nextInputs.globals) {
                                    AddArgDataIn(fnRunDep[F].globals[entry.first], entry.second); // copy global dependency
                                }
                                for (auto &entry : nextInputs.args) {
                                    if (entry.first < ci->arg_size()) {
                                        Value *next = ci->getArgOperand(entry.first);
                                        AddFnInputs(fnRunDep[F], InstructionBacktrackValue(next, analysisParents)); // arg dependency is transferred (truncated for inaccuracy) TODO: don't truncate, propagate
                                    }
                                }
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
                            AddFnInputs(fnRunDep[F], InstructionBacktrackValue(next, analysisParents)); // recursion happens inside
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
FunctionInputData retUsesAllInputs() {
    FunctionInputData ret = FunctionInputData();
    for (unsigned i = 0; i < 20; i++) { // as high as i will go for varargs; TODO: actual infinite using flags
        ret.args[i].used = true;
    }
    return ret;
}
FunctionOutputData &FunctionBacktrackValue(Function *F, std::unordered_set<Function*> &analysisParents) {
    if (analysisParents.find(F) != analysisParents.end()) {
        debug("uncaught FunctionBacktrackValue recursive call")
    }
    if (!fnValDep[F].started) {
        fnValDep[F].started = true;
        analysisParents.insert(F);
        if (F->isDeclaration()) { // defined elsewhere; assume return value is dependent on all inputs
            static ArgDataIn argused = {.used=true};
            static FunctionInputData declret = retUsesAllInputs();
            fnValDep[F].retval.inputs = declret;

            StringRef fname = F->getName();
            // begin nonvoid used
            if (fname.equals("atoi")) { // int atoi (const char * str), returns int from string
            } else if (fname.equals("llvm.fmuladd.f64")) { // double @llvm.fmuladd.f64(double, double, double), A * B + C
            // begin output
            } else if (fname.equals("fprintf")) { // int fprintf(FILE *stream, const char *format, ...), output
            } else if (fname.equals("printf")) { // int printf(char * str, ...), output
            // begin memsets
            } else if (fname.equals("strcpy")) { // char *strcpy( char *dest, const char *src ), string 2 into string 1, and return string 1
            } else if (fname.equals("sprintf")) { // int sprintf( char *buffer, const char *format, ... ), varargs and arg2 into string 1
            } else if (fname.equals("sscanf") || fname.equals("__isoc99_sscanf")) { // int sscanf( const char *buffer, const char *format, ... ), input from string 1, to string 2 and on varargs
            } else if (fname.equals("llvm.memcpy.p0i8.p0i8.i64")) { // void @llvm.memcpy.p0i8.p0i8.i64(i8* noalias nocapture writeonly, i8* noalias nocapture readonly, i64, i1 immarg), copies mem 2 into mem 1 for arg3 entries
            // begin mem define
            } else if (fname.equals("calloc")) { // i8* @calloc(i64 noundef, i64 noundef), no data manip
            } else if (fname.equals("malloc")) { // malloc, no data manip
            } else if (fname.equals("realloc")) { // i8* @realloc(i8* noundef, i64 noundef), no data manip
            // begin unused no data manip
            } else if (fname.equals("time")) { // time, no data manip
            } else if (fname.equals("fclose")) { // int fclose( FILE *stream ), no data manip
            } else if (fname.equals("fflush")) { // i32 @fflush(%struct._IO_FILE* noundef), dumps but does not generate output to stream arg // unused no manip
            // begin void no manip
            } else if (fname.equals("llvm.memset.p0i8.i64")) { // void @llvm.memset.p0i8.i64(i8* nocapture writeonly, i8, i64, i1 immarg), terminates the program (no data manip), initializes mem 1 to arg2 for arg3 entries
            } else if (fname.equals("qsort")) { // void @qsort(i8* ptr, i64 count, i64 size, i32 (i8*, i8*)* comp), sorts input, no real change
            } else if (fname.equals("free")) { // void @free(i8* noundef), no data manip (could return error)
            } else if (fname.equals("llvm.dbg.declare")) { // llvm.dbg.declare, metadata (no data manip)
            } else if (fname.equals("llvm.dbg.label")) { // void @llvm.dbg.label(metadata), metadata (no data manip)
            // begin endpoints
            } else if (fname.equals("exit")) { // void @exit(i32 noundef), terminates the program (no data manip)
            } else if (fname.equals("__assert_fail")) { // void __assert_fail(const char * assertion, const char * file, unsigned int line, const char * function), terminates the program if false (ignore)
            } else {
                // other external function or intrinsic; do not care
                // double @llvm.fmuladd.f64(double, double, double)
                // void @llvm.dbg.label(metadata)

            }
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

void InstructionBacktrackLoadAddr(FunctionInputData &retData, Instruction *I, Value *val, std::unordered_set<Function*> &analysisParents, std::unordered_set<BasicBlock*> &loopDetection) {// recursive walking for load inst

    // val is the load address, THERE ARE NO NESTED CONSTEXPR
    bool stop = false;
    Instruction *walk = I;
    GlobalVariable *global = dyn_cast<GlobalVariable>(val);

    while (walk) {
        if (val == walk) {return;}
        if (StoreInst *store = dyn_cast<StoreInst>(walk)) {
            for (Use &op : store->uses()) {
                if (op.get() == val) {
                    AddFnInputs(retData, InstructionBacktrackValue(store->getValueOperand(), analysisParents)); // halt here
                    return;
                }
            }
        } else if (CallBase *call = dyn_cast<CallBase>(walk)) {
            bool used = false;
            if (Function *cf = call->getCalledFunction()) {
                if (global && (globalFDep[global].find(cf) != globalFDep[global].end())) { // Todo: fix global list to recurse
                    // global dependency
                    FunctionOutputData &calldata = FunctionBacktrackValue(cf, analysisParents);
                    FunctionInputData &newinputs = calldata.globals[global].inputs;
                    if (!isFnInputEmpty(newinputs)) {
                        used = true;
                        for (unsigned argno = 0; argno < cf->arg_size(); argno++) {
                            if ((newinputs.args.find(argno) != newinputs.args.end()) && !isArgDataInEmpty(newinputs.args[argno])) { // truncated; TODO: add to data structure - what do the children of this depend on
                                AddFnInputs(retData, InstructionBacktrackValue(call->getArgOperand(argno), analysisParents));
                            }
                        }
                        for (auto &entry : newinputs.globals) {
                            AddArgDataIn(retData.globals[entry.first], entry.second);
                        }
                    }
                }
                for (Use &arg : call->args()) {
                    if (arg.get() == val) {
                        // may contain multiple times
                        FunctionOutputData &calldata = FunctionBacktrackValue(cf, analysisParents);
                        FunctionInputData &newinputs = calldata.args[call->getArgOperandNo(&arg)].inputs;
                        if (!isFnInputEmpty(newinputs)) {
                            used = true;
                            for (unsigned argno = 0; argno < cf->arg_size(); argno++) {
                                if ((newinputs.args.find(argno) != newinputs.args.end()) && !isArgDataInEmpty(newinputs.args[argno])) { // truncated
                                    AddFnInputs(retData, InstructionBacktrackValue(call->getArgOperand(argno), analysisParents));
                                }
                            }
                            for (auto &entry : newinputs.globals) {
                                AddArgDataIn(retData.globals[entry.first], entry.second);
                            }
                        }
                    } else if (global && isa<ConstantExpr>(arg)) {
                        auto *expr = dyn_cast<ConstantExpr>(arg);
                        for (Use &arg2 : expr->operands()) {
                            if (arg2.get() == val) {
                                FunctionOutputData &calldata = FunctionBacktrackValue(cf, analysisParents);
                                FunctionInputData &newinputs = calldata.args[call->getArgOperandNo(&arg)].inputs; // correct use of arg over arg2
                                if (!isFnInputEmpty(newinputs)) {
                                    used = true;
                                    for (unsigned argno = 0; argno < cf->arg_size(); argno++) {
                                        if ((newinputs.args.find(argno) != newinputs.args.end()) && !isArgDataInEmpty(newinputs.args[argno])) { // truncated
                                            AddFnInputs(retData, InstructionBacktrackValue(call->getArgOperand(argno), analysisParents));
                                        }
                                    }
                                    for (auto &entry : newinputs.globals) {
                                        AddArgDataIn(retData.globals[entry.first], entry.second);
                                    }
                                }
                            }
                        }
                    }
                }
            }
            if (used) {
                return;
            }
        } // there are no phi users of val
        // end while
        walk = walk->getPrevNonDebugInstruction();
    }
    // recurse on terminators of predecessors of block
    debug("inst:" << I)
    BasicBlock *B = I->getParent();
    for (BasicBlock *pred : predecessors(B)) {
        if (loopDetection.find(B) != loopDetection.end()) { // complete blocks
            loopDetection.insert(pred);
            InstructionBacktrackLoadAddr(retData, pred->getTerminator(), val, analysisParents, loopDetection);
        }
    }
}
FunctionInputData &InstructionBacktrackValue(Value *val, std::unordered_set<Function*> &analysisParents) {
    if (!instValDep[val].started) {
        instValDep[val].started = true;
        if (Instruction *I = dyn_cast<Instruction>(val)) {
            if (PHINode *xi = dyn_cast<PHINode>(I)) {
                debug("PHI Instruction: " << *xi)
                for (Use &op : xi->incoming_values()) {
                    Value *prev = op.get();
                    AddFnInputs(instValDep[val], InstructionBacktrackValue(prev, analysisParents));
                }
            } else if (BranchInst *xi = dyn_cast<BranchInst>(I)) {
                if (xi->isConditional()) { // value dependent on what conditiondepends on
                    debug("BrCond Instruction: " << *xi)
                    Value *prev = xi->getCondition();
                    AddFnInputs(instValDep[val], InstructionBacktrackValue(prev, analysisParents));
                }
            } else if (SwitchInst *xi = dyn_cast<SwitchInst>(I)) { // value dependent on what conditiondepends on
                debug("Switch Instruction: " << *xi)
                Value *prev = xi->getCondition();
                AddFnInputs(instValDep[val], InstructionBacktrackValue(prev, analysisParents));
            } else if (AllocaInst *xi = dyn_cast<AllocaInst>(I)) {
                debug("Alloca Instruction: " << *xi)
                if (DILocalVariable *varb = getLocalVariableMD(xi)) { // this depends on the parameter
                    if (varb->isParameter()) {
                        instValDep[val].args[varb->getArg()-1].used = true;
                    }
                }
            } else if (BinaryOperator *xi = dyn_cast<BinaryOperator>(I)) {
                debug("BinOp Instruction: " << *xi)
                for (Use &op : xi->operands()) {
                    Value *prev = op.get();
                    AddFnInputs(instValDep[val], InstructionBacktrackValue(prev, analysisParents));
                }
            } else if (LoadInst *xi = dyn_cast<LoadInst>(I)) { // must come before UnaryInstruction!!!!
                debug("Load Instruction: " << *xi)
                Value *prev = xi->getPointerOperand();
                AddFnInputs(instValDep[val], InstructionBacktrackValue(prev, analysisParents));
                {
                    std::unordered_set<BasicBlock*> loopDetection;
                    InstructionBacktrackLoadAddr(instValDep[val], xi, prev, analysisParents, loopDetection);
                }
                if (GetElementPtrInst *gep = dyn_cast<GetElementPtrInst>(prev)) {
                    if (CastInst *cast = dyn_cast<CastInst>(gep->getPointerOperand())) {
                        debug("Fancy load")
                        std::unordered_set<BasicBlock*> loopDetection;
                        InstructionBacktrackLoadAddr(instValDep[val], xi, cast, analysisParents, loopDetection);
                    }
                }
            } else if (CmpInst *xi = dyn_cast<CmpInst>(I)) {
                debug("Compare Instruction: " << *xi)
                for (Use &op : xi->operands()) {
                    Value *prev = op.get();
                    AddFnInputs(instValDep[val], InstructionBacktrackValue(prev, analysisParents));
                }
            } else if (SelectInst *xi = dyn_cast<SelectInst>(I)) {
                debug("Select Instruction: " << *xi)
                for (Use &op : xi->operands()) { // getCondition(), getTrueValue(), getFalseValue()
                    Value *prev = op.get();
                    AddFnInputs(instValDep[val], InstructionBacktrackValue(prev, analysisParents)); // this depends on the inputs that all ops depend on
                }
            } else if (ReturnInst *xi = dyn_cast<ReturnInst>(I)) {
                debug("Return Instruction: " << *xi)
                Value *prev = xi->getReturnValue();
                AddFnInputs(instValDep[val], InstructionBacktrackValue(prev, analysisParents)); // retval depends on the inputs that prev depends on
            } else if (UnaryInstruction *xi = dyn_cast<UnaryOperator>(I)) {
                debug("UnaryOp Instruction: " << *xi)
                for (Use &op : xi->operands()) {
                    Value *prev = op.get();
                    AddFnInputs(instValDep[val], InstructionBacktrackValue(prev, analysisParents)); // this depends on the inputs that prev depends on
                }
            } else {
                debug("Unmarked inst: " << I->getOpcodeName())
            }
        } else if (Constant *con = dyn_cast<Constant>(val)) {
            if (ConstantInt *constint = dyn_cast<ConstantInt>(con)) {
                // not a var
            } else if (ConstantExpr *expr = dyn_cast<ConstantExpr>(con)) {
                if (std::strcmp(expr->getOpcodeName(), "getelementptr") == 0) {
                    // depends on all operands, but they are const
                    Use const &addr = expr->getOperandUse(0);
                    //Value *prev = 
                    /*
                    for (unsigned i = 0; i < expr->getNumOperands(); i++) { // begin gep
                        Use const &op = expr->getOperandUse(i);
                        if (i == 0) {
                            // address arg
                            ret = getDirtyMemoryRecord(op.get());
                        } else if (i == 1) {
                            // address arg
                            if (ret.membertree[ret.membertree.size()-1] == unsigned(-1)) {
                                // end
                                break;
                            } else {
                                if (ConstantInt *index = dyn_cast<ConstantInt>(op.get())) {
                                    ret.membertree[ret.membertree.size()-1] += (index->getSExtValue());
                                } else {
                                    ret.membertree[ret.membertree.size()-1] = unsigned(-1);
                                    break;
                                }
                            }
                        } else {
                            if (ConstantInt *index = dyn_cast<ConstantInt>(op.get())) {
                                ret.membertree.push_back(index->getSExtValue());
                            } else {
                                ret.membertree.push_back(unsigned(-1));
                                break;
                            }
                        }
                    } // end gep
                    */
                } else {
                    // other expr - too much
                    debug("Other expression : " << *expr)
                    /*
                    rets << expr->getOpcodeName() << " expr of ";
                    for (Use &next : expr->operands()) {
                        rets << findVariableNameImpl(next.get()) << ", "; // should be constants
                    }
                    */
                }
            } else if (GlobalAlias *alias = dyn_cast<GlobalAlias>(con)) {
                debug(*val << " Global Alias")
            } else if (Function *func = dyn_cast<Function>(con)) {
                debug(func->getName() << " Func")
            } else if (GlobalVariable *gvar = dyn_cast<GlobalVariable>(con)) {
                if (!gvar->isConstant()) {
                    //debug("global " << *gvar);
                    instValDep[val].globals[gvar].used = true;
                } else {
                    if (gvar->hasExternalLinkage()) { // should be var with external initializer
                        debug( "(extern : " << gvar->getGlobalIdentifier() << ")" );
                    } else {
                        debug( "const global variable " )
                    }
                }
            } else {
                debug( "other const" )
            }
            //rets << " is constant!!!";
        } else {
            // not an instruction
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
        std::vector<std::string> recordNames;
        
        for (auto &G : M.globals()) {
            if (!G.isConstant()) {
                for (User *user : G.users()) {
                    std::string temp;
                    raw_string_ostream temps(temp);
                    if (Instruction *I = dyn_cast<Instruction>(user)) {
                        globalFDep[&G].insert(I->getFunction());
                    } else if (ConstantExpr * expr = dyn_cast<ConstantExpr>(user)) {
                        //temps << "gname: " << gname << ", CExpr: " << expr->getOpcodeName();
                        bool hasUsers = false;
                        for (auto *expruser : expr->users()) {
                            hasUsers = true;
                            if (Instruction *I = dyn_cast<Instruction>(expruser)) {
                                globalFDep[&G].insert(I->getFunction());
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
            for (auto& B : F) { for (auto& I : B) {
                if (StoreInst *si = dyn_cast<StoreInst>(&I)) {
                    recordNames.push_back(getRecordName(getMemoryRecord(si)));
                }
            }}
        }
        if (Function *Main = M.getFunction("main")) {
            std::unordered_set<Function*> mainAnalysisParents = std::unordered_set<Function*>();
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
        for (std::string const &str : recordNames) {
            errs() << "record names : " << str << '\n';
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