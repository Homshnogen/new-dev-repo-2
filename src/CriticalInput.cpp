
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

#define DEBUG 0

#if DEBUG
#define debug(args) dbgs() << args << '\n';
#else
#define debug(args) ;
#endif

#define DEBUG2 0

#if DEBUG2
#define debug2(args) dbgs() << args << '\n';
#else
#define debug2(args) ;
#endif
// dbgs(), errs(), outs()

bool MAIN_RT = true;

struct ArgDataIn {
    bool used; // false
    std::map<unsigned, ArgDataIn> members; // {}
};

struct FunctionInputData {
    enum UI_TYPES{
        NONE = 0,
        SCANF,
        FOPEN,
        FGETS0,
        FGETS,
        FREAD0,
        FREAD,
        GETC,
        FEOF,
    };
    bool started, finished; // false
    UI_TYPES uiType = UI_TYPES::NONE; // false
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
/*
struct UserInputData {
    std::string inputType; // main or scanf or fgets or fopen

    std::vector<unsigned> membertree;
};
std::map<CallBase*, UserInputData> userInputDep;
*/
std::unordered_set<std::string> userInputDep;

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
    if (data.uiType != FunctionInputData::UI_TYPES::NONE) {return false;} // input non NONE
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
    retData.used = retData.used || fromData.used;
    for (auto &entry : fromData.members) { // key = first, value = second
        AddArgDataIn(retData.members[entry.first], entry.second);
    }
}
void AddFnInputs(FunctionInputData &retData, FunctionInputData &fromData, unsigned n) {
    // copy args
    //retData.userInput = retData.userInput || fromData.userInput;
    if (fromData.uiType != FunctionInputData::UI_TYPES::NONE) {
        retData.uiType = fromData.uiType;
    } else {
        //retData.uiType = fromData.uiType || retData.uiType; // maybe ERROR
    }
    for (auto &entry : fromData.args) { // key = first, value = second
        if (entry.first < n) {AddArgDataIn(retData.args[entry.first], entry.second);}
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
void AddArgDataOut(ArgDataOut &retData, ArgDataOut &fromData, unsigned n) {
    AddFnInputs(retData.inputs, fromData.inputs, n);
    for (auto &entry : fromData.members) { // key = first, value = second
        AddArgDataOut(retData.members[entry.first], entry.second, n);
    }
}
void AddFnOutputs(FunctionOutputData &retData, FunctionOutputData &fromData, unsigned n) {
    // copy retval
    AddArgDataOut(retData.retval, fromData.retval, n);
    // copy args
    for (auto &entry : fromData.args) { // key = first, value = second
        if (entry.first < n) {AddArgDataOut(retData.args[entry.first], entry.second, n);}
    }
    // copy globals
    for (auto &entry : fromData.globals) { // key = first, value = second
        AddArgDataOut(retData.globals[entry.first], entry.second, n);
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
        AddFnInputs(memberData->inputs, inputs, 20);
        memberData = &(memberData->members[nextmember]);
    }
    AddFnInputs(memberData->inputs, inputs, 20);
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
    } else if (CallBase *ci = dyn_cast<CallBase>(val)) {
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
    } else if (CallBase *ci = dyn_cast<CallBase>(val)) {
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
        debug("no matches" << *val);
    }

    return ret;
}
MemoryRecord getMemoryRecord(Value *val) {
    MemoryRecord ret = getDirtyMemoryRecord(val);
    if ((ret.membertree.size() > 0) && (ret.membertree[0] == 0)) { // remove a zero; or don't 

    }
    if ((ret.membertree.size() > 0) && ret.membertree[ret.membertree.size()-1] == unsigned(-1)) {
        ret.membertree.pop_back();
    }
    return ret;
}

std::string getInputRecordName(MemoryRecord record) {
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
        if (i == 0) {
            if (record.membertree[i] != 0) { ret[-1] = '+'; ret += std::to_string(record.membertree[i]) + ")";}
        } else {
            ret += "["+std::to_string(record.membertree[i])+"]";
        }
    }
    return ret;
}
void backtrackValuesFromCall(CallBase *call, MemoryRecord &Query) {
    // probably move the function name switch here
    // probably inplement use-all and use-none flags

}

std::unordered_set<Function*> fptrTryGetValues(Value *val);
FunctionInputData &FunctionBacktrackRuntime(Function *F, std::unordered_set<Function*> &analysisParents) {
    debug2("FBR : " << fnRunDep[F].started << " " << fnRunDep[F].finished << " " << F->getName())
    if (!fnRunDep[F].started) {
        if (analysisParents.find(F) != analysisParents.end()) {
            debug("uncaught FunctionBacktrackRuntime recursive call")
        }
        fnRunDep[F].started = true;
        analysisParents.insert(F);
        if (F->isDeclaration()) { // defined elsewhere; assume runtime is dependent on no inputs
            // nothing needs to be done
        } else {
            std::vector<CallBase*> callbases = std::vector<CallBase*>();
            for (BasicBlock &B : *F) {
                for (Instruction &I : B) {
                    // find branches and fp calls and perform instruction analysis
                    if (BranchInst *bi = dyn_cast<BranchInst>(&I)) {
                        if (bi->isConditional()) {
                            // branch instruction
                            if (Instruction *next = dyn_cast<Instruction>(bi->getCondition())) {
                                // runtime depends on value of next;
                                AddFnInputs(fnRunDep[F], InstructionBacktrackValue(next, analysisParents), F->arg_size());
                            }
                        }
                    } if (SwitchInst *sw = dyn_cast<SwitchInst>(&I)) {
                        // switch instruction
                        if (Instruction *next = dyn_cast<Instruction>(sw->getCondition())) {
                            // runtime depends on value of next;
                            AddFnInputs(fnRunDep[F], InstructionBacktrackValue(next, analysisParents), F->arg_size());
                        }
                    } else if (CallBase *ci = dyn_cast<CallBase>(&I)) {
                        callbases.push_back(ci);
                    }
                }
            }
            for (CallBase *ci : callbases) {
                if (Function *nextF = ci->getCalledFunction()) {
                    //debug("called function: " << ci->getCalledFunction()->getName())
                    FunctionInputData &nextInputs = FunctionBacktrackRuntime(nextF, analysisParents);
                    for (auto &entry : nextInputs.globals) {
                        AddArgDataIn(fnRunDep[F].globals[entry.first], entry.second); // copy global dependency
                    }
                    for (auto &entry : nextInputs.args) {
                        if (entry.first < ci->arg_size()) {
                            Value *next = ci->getArgOperand(entry.first);
                            AddFnInputs(fnRunDep[F], InstructionBacktrackValue(next, analysisParents), F->arg_size()); // arg dependency is transferred (truncated for inaccuracy) TODO: don't truncate, propagate
                        }
                    }
                } else {
                    // called function pointer
                    Value *next = ci->getCalledOperand();
                    debug2("Called function pointer: " << *next)
                    AddFnInputs(fnRunDep[F], InstructionBacktrackValue(next, analysisParents), F->arg_size()); // recursion happens inside
                    for (Function *nextF : fptrTryGetValues(next)) {
                        FunctionInputData &nextInputs = FunctionBacktrackRuntime(nextF, analysisParents);
                        for (auto &entry : nextInputs.globals) {
                            AddArgDataIn(fnRunDep[F].globals[entry.first], entry.second); // copy global dependency
                        }
                        for (auto &entry : nextInputs.args) {
                            if (entry.first < ci->arg_size()) {
                                Value *next = ci->getArgOperand(entry.first);
                                AddFnInputs(fnRunDep[F], InstructionBacktrackValue(next, analysisParents), F->arg_size()); // arg dependency is transferred (truncated for inaccuracy) TODO: don't truncate, propagate
                            }
                        }
                    }
                    //AddFnArgsFromInstData(fnRunDep[F].args, next, analysisParents);
                }
            }
        }
        analysisParents.erase(F);
    } else if (!fnRunDep[F].finished) {
        // recursive call
        debug("FunctionBacktrackRuntime recursive call")
        return fnRunDep[F];
    }
    fnRunDep[F].finished = true;
    return fnRunDep[F];
}

void getStoreValFromLoad(std::unordered_set<Value*> &retData, Instruction *load, std::unordered_set<BasicBlock*> &loopDetection) { // best effort
    Instruction *walk = load->getPrevNonDebugInstruction();
    while (walk) {
        if (StoreInst *store = dyn_cast<StoreInst>(walk)) {
            retData.insert(store->getValueOperand());
            return;
        }
        // end while
        walk = walk->getPrevNonDebugInstruction();
    }
    BasicBlock *B = load->getParent();
    for (BasicBlock *pred : predecessors(B)) {
        if (loopDetection.find(pred) == loopDetection.end()) {
            loopDetection.insert(pred);
            getStoreValFromLoad(retData, pred->getTerminator(), loopDetection);
        }
    }
}
void checkNewUserInput(FunctionInputData &retData, Value *val) {
    {
        std::string temp;
        raw_string_ostream temps(temp);
        switch (instValDep[val].uiType) {
            case FunctionInputData::UI_TYPES::NONE:
            break;
            case FunctionInputData::UI_TYPES::SCANF:
            temps << "User input depends on scanf call : " << *val;
            break;
            case FunctionInputData::UI_TYPES::FOPEN:
            temps << "User input depends on FOPEN call : " << *val;
            break;
            case FunctionInputData::UI_TYPES::FGETS0:
            temps << "User input depends on FGETS0 call : " << *val;
            break;
            case FunctionInputData::UI_TYPES::FGETS:
            temps << "User input depends on FGETS call : " << *val;
            break;
            case FunctionInputData::UI_TYPES::FREAD0:
            temps << "User input depends on FREAD0 call : " << *val;
            break;
            case FunctionInputData::UI_TYPES::FREAD:
            temps << "User input depends on FREAD call : " << *val;
            break;
            case FunctionInputData::UI_TYPES::GETC:
            temps << "User input depends on GETC call : " << *val;
            break;
            case FunctionInputData::UI_TYPES::FEOF:
            temps << "User input depends on FEOF call : " << *val;
            break;
        }
        if (MAIN_RT && temp.length()>0) {
            userInputDep.insert(temp); 
            instValDep[val].uiType = FunctionInputData::UI_TYPES::NONE;
        }
    }
}
void propUserInput(FunctionInputData &retData, CallBase *cb, Use *use, std::unordered_set<Function*> &analysisParents, FunctionInputData::UI_TYPES uiType) {
    {
        bool isRetval = (use == nullptr) || (use->get() == cb->getCalledOperand());
        Function *F = cb->getFunction();
        switch (uiType) {
            case FunctionInputData::UI_TYPES::NONE:
            break;
            case FunctionInputData::UI_TYPES::SCANF:
            break;
            case FunctionInputData::UI_TYPES::FOPEN:
                AddFnInputs(retData, InstructionBacktrackValue(cb->getArgOperand(0), analysisParents), F->arg_size());
            break;
            case FunctionInputData::UI_TYPES::FGETS0:
                AddFnInputs(retData, InstructionBacktrackValue(cb->getArgOperand(2), analysisParents), F->arg_size());
            break;
            case FunctionInputData::UI_TYPES::FGETS:
                AddFnInputs(retData, InstructionBacktrackValue(cb->getArgOperand(2), analysisParents), F->arg_size());
            break;
            case FunctionInputData::UI_TYPES::FREAD0:
                AddFnInputs(retData, InstructionBacktrackValue(cb->getArgOperand(3), analysisParents), F->arg_size());
            break;
            case FunctionInputData::UI_TYPES::FREAD:
                AddFnInputs(retData, InstructionBacktrackValue(cb->getArgOperand(3), analysisParents), F->arg_size());
            break;
            case FunctionInputData::UI_TYPES::GETC:
                AddFnInputs(retData, InstructionBacktrackValue(cb->getArgOperand(0), analysisParents), F->arg_size());
            break;
            case FunctionInputData::UI_TYPES::FEOF:
                AddFnInputs(retData, InstructionBacktrackValue(cb->getArgOperand(0), analysisParents), F->arg_size());
            break;
        }
    }
}
void fptrTryGetValuesRecurse(std::unordered_set<Function*> &retData, Value *val) { // nullptr if couldn't find value
    if (val == nullptr) { // failed try
        retData.insert(nullptr);
    } else if (auto *load = dyn_cast<LoadInst>(val)) {
        std::unordered_set<Value*> stores = std::unordered_set<Value*>();
        std::unordered_set<BasicBlock*> loopDetection = std::unordered_set<BasicBlock*>();
        getStoreValFromLoad(stores, load, loopDetection);
        for (Value *store : stores) {
            fptrTryGetValuesRecurse(retData, store);
        }
    } else if (auto *phi = dyn_cast<PHINode>(val)) {
        for (Use &op : phi->incoming_values()) {
            fptrTryGetValuesRecurse(retData, op.get());
        }
    } else if (auto *select = dyn_cast<SelectInst>(val)) {
        fptrTryGetValuesRecurse(retData, select->getTrueValue());
        fptrTryGetValuesRecurse(retData, select->getFalseValue());
    } else if (auto *func = dyn_cast<Function>(val)) {
        
        debug2("tryfptr " << func->getName())
        retData.insert(func);
    } else { // failed try, unimplemented
        debug2("tryfptr " << "failed")
        retData.insert(nullptr);
    }
}
std::unordered_set<Function*> fptrTryGetValues(Value *val) { // nullptr if couldn't find value
    std::unordered_set<Function*> ret = std::unordered_set<Function*>();
    fptrTryGetValuesRecurse(ret, val);
    if (ret.find(nullptr) != ret.end()) {
        debug2("failed to find all fptr values")
        ret.erase(nullptr);
    }
    for (auto *F : ret) {
        debug2("try fptr values : " << F->getName())
    }
    return ret;
}

FunctionInputData::UI_TYPES isFptrUserInput(CallBase *call, Use *use, Function* cf) {
    FunctionInputData::UI_TYPES ret = FunctionInputData::UI_TYPES::NONE;
    // fp, technically could be input; TODO: check fp possible values
    Value *fptr = call->getCalledOperand();
    bool isRetval = (use == nullptr) || (fptr == use->get());
    StringRef fname = cf->getName();
    
        debug("ftest " << *call)
        debug("ftest " << isRetval)
        debug("ftest " << fname)
    // begin user input
    if (fname.equals("scanf") || fname.equals("__isoc99_scanf")) { // int scanf ( const char * format, ... ), user input, numarg - 1
        if (!isRetval) {
            std::string temp;
            raw_string_ostream temps(temp);
            temps << "Runtime depends on user input from console, line " << call->getDebugLoc().getLine() << ", input #" << call->getArgOperandNo(use);
            if (MAIN_RT) {userInputDep.insert(temp);}
            ret = FunctionInputData::UI_TYPES::SCANF;
        }
    } else if (fname.equals("fgets")) { // char *fgets(char *str, int count, FILE *stream), input from file stream, retval is null if end of file - depend on size of file
        if (isRetval) {
            std::string temp;
            raw_string_ostream temps(temp);
            temps << "Runtime depends on size of file or file contents, line " << call->getDebugLoc().getLine() << ", file : " << findVariableName(call->getArgOperand(2));
            ret = FunctionInputData::UI_TYPES::FGETS0;
            if (MAIN_RT) {userInputDep.insert(temp);}
        } else if (call->getArgOperandNo(use) == 0) {
            std::string temp;
            raw_string_ostream temps(temp);
            temps << "Runtime depends on file contents, line " << call->getDebugLoc().getLine() << ", file : " << findVariableName(call->getArgOperand(2));
            if (MAIN_RT) {userInputDep.insert(temp);}
            ret = FunctionInputData::UI_TYPES::FGETS;
        }
    } else if (fname.equals("getc") || fname.equals("fgetc")) { // 
        if (isRetval) {
            std::string temp;
            raw_string_ostream temps(temp);
            temps << "Runtime depends on file size, line " << call->getDebugLoc().getLine() << ", file : " << findVariableName(call->getArgOperand(0));
            ret = FunctionInputData::UI_TYPES::GETC;
            if (MAIN_RT) {userInputDep.insert(temp);}
        }
    } else if (fname.equals("feof")) { // 
        if (isRetval) {
            std::string temp;
            raw_string_ostream temps(temp);
            temps << "Runtime depends on size or existance of file, line " << call->getDebugLoc().getLine() << ", file : " << findVariableName(call->getArgOperand(0));
            ret = FunctionInputData::UI_TYPES::FEOF;
            if (MAIN_RT) {userInputDep.insert(temp);}
        }
    } else if (fname.equals("fread")) { // 
        if (isRetval) {
            std::string temp;
            raw_string_ostream temps(temp);
            temps << "Runtime depends on size of file, line " << call->getDebugLoc().getLine() << ", file : " << findVariableName(call->getArgOperand(3));
            ret = FunctionInputData::UI_TYPES::FREAD0;
            if (MAIN_RT) {userInputDep.insert(temp);}
        } else if (call->getArgOperandNo(use) == 0) {
            std::string temp;
            raw_string_ostream temps(temp);
            temps << "Runtime depends on file contents, line " << call->getDebugLoc().getLine() << ", file : " << findVariableName(call->getArgOperand(3));
            if (MAIN_RT) {userInputDep.insert(temp);}
            ret = FunctionInputData::UI_TYPES::FREAD;
        }
    } else if (fname.equals("fopen")) { // FILE * fopen ( const char * filename, const char * mode ), input from file, name filename, -assume mode is reading if from fgets
        if (isRetval) { // todo - check mode
            std::string temp;
            raw_string_ostream temps(temp);
            std::string mode = findVariableName(call->getOperand(1));
            if(mode.find("r") != std::string::npos) {
                temps << "Runtime depends on file contents or existance, line " << call->getDebugLoc().getLine() << ", file name location: " << findVariableName(call->getArgOperand(0));
                if (MAIN_RT) {userInputDep.insert(temp);}
                ret = FunctionInputData::UI_TYPES::FOPEN;
            }
        }
    } else {}
    return ret;
}
FunctionInputData::UI_TYPES isNonFptrUserInput(CallBase *call, Use *use) {
    FunctionInputData::UI_TYPES ret = FunctionInputData::UI_TYPES::NONE;
    if (Function *cf = call->getCalledFunction()) {
        bool isRetval =  (use == nullptr) || (cf == use->get());
        StringRef fname = cf->getName();
        debug("ftest " << *call)
        debug("ftest " << isRetval)
        debug("ftest " << fname)
        // begin user input
        if (fname.equals("scanf") || fname.equals("__isoc99_scanf")) { // int scanf ( const char * format, ... ), user input, numarg - 1
            if (!isRetval) {
                std::string temp;
                raw_string_ostream temps(temp);
                temps << "Runtime depends on user input from console, line " << call->getDebugLoc().getLine() << ", input #" << call->getArgOperandNo(use);
                if (MAIN_RT) {userInputDep.insert(temp);}
                ret = FunctionInputData::UI_TYPES::SCANF;
            }
        } else if (fname.equals("fgets")) { // char *fgets(char *str, int count, FILE *stream), input from file stream, retval is null if end of file - depend on size of file
            if (isRetval) {
                std::string temp;
                raw_string_ostream temps(temp);
                temps << "Runtime depends on size of file or file contents, line " << call->getDebugLoc().getLine() << ", file : " << findVariableName(call->getArgOperand(2));
                ret = FunctionInputData::UI_TYPES::FGETS0;
                if (MAIN_RT) {userInputDep.insert(temp);}
            } else if (call->getArgOperandNo(use) == 0) {
                std::string temp;
                raw_string_ostream temps(temp);
                temps << "Runtime depends on file contents, line " << call->getDebugLoc().getLine() << ", file : " << findVariableName(call->getArgOperand(2));
                if (MAIN_RT) {userInputDep.insert(temp);}
                ret = FunctionInputData::UI_TYPES::FGETS;
            }
        } else if (fname.equals("getc") || fname.equals("fgetc")) { // 
            if (isRetval) {
                std::string temp;
                raw_string_ostream temps(temp);
                temps << "Runtime depends on file size, line " << call->getDebugLoc().getLine() << ", file : " << findVariableName(call->getArgOperand(0)); // common usage
                ret = FunctionInputData::UI_TYPES::GETC;
                if (MAIN_RT) {userInputDep.insert(temp);}
            }
        } else if (fname.equals("feof")) { // 
            if (isRetval) {
                std::string temp;
                raw_string_ostream temps(temp);
                temps << "Runtime depends on size or existance of file, line " << call->getDebugLoc().getLine() << ", file : " << findVariableName(call->getArgOperand(0));
                ret = FunctionInputData::UI_TYPES::FEOF;
                if (MAIN_RT) {userInputDep.insert(temp);}
            }
        } else if (fname.equals("fread")) { // 
            if (isRetval) {
                std::string temp;
                raw_string_ostream temps(temp);
                temps << "Runtime depends on size of file, line " << call->getDebugLoc().getLine() << ", file : " << findVariableName(call->getArgOperand(3));
                ret = FunctionInputData::UI_TYPES::FREAD0;
                if (MAIN_RT) {userInputDep.insert(temp);}
            } else if (call->getArgOperandNo(use) == 0) {
                std::string temp;
                raw_string_ostream temps(temp);
                temps << "Runtime depends on file contents, line " << call->getDebugLoc().getLine() << ", file : " << findVariableName(call->getArgOperand(3));
                if (MAIN_RT) {userInputDep.insert(temp);}
                ret = FunctionInputData::UI_TYPES::FREAD;
            }
        } else if (fname.equals("fopen")) { // FILE * fopen ( const char * filename, const char * mode ), input from file, name filename, -assume mode is reading if from fgets
            if (isRetval) { // todo - check mode
                std::string temp;
                raw_string_ostream temps(temp);
                std::string mode = findVariableName(call->getOperand(1));
                if(mode.find("r") != std::string::npos) {
                temps << "Runtime depends on file contents or existance, line " << call->getDebugLoc().getLine() << ", file name location: " << findVariableName(call->getArgOperand(0));
                    if (MAIN_RT) {userInputDep.insert(temp);}
                    ret = FunctionInputData::UI_TYPES::FOPEN;
                }
            }
        } else {}
    } else { // fp, technically could be input; TODO: check fp possible values
    }
    return ret;
}
FunctionOutputData &FunctionBacktrackValue(Function *F, std::unordered_set<Function*> &analysisParents) {
    debug2("FBV : " << fnValDep[F].started << " " << fnValDep[F].finished << " " << F->getName())
    if (!fnValDep[F].started) {
        if (analysisParents.find(F) != analysisParents.end()) {
            debug("uncaught FunctionBacktrackValue recursive call")
        }
        fnValDep[F].started = true;
        analysisParents.insert(F);
        if (F->isDeclaration()) { // defined elsewhere; assume return value is dependent on all inputs

            StringRef fname = F->getName();
            // begin user input; handled in is...UserInput
            if (fname.equals("scanf") || fname.equals("__isoc99_scanf")) { // int scanf ( const char * format, ... ), user input, numarg - 1
                debug2("new scanf")
                for (unsigned i = 1; i < 20; i++) { // as high as i will go for varargs; TODO: actual infinite using flags
                    fnValDep[F].args[i].members[0].inputs.uiType = FunctionInputData::UI_TYPES::SCANF;
                    fnValDep[F].args[i].inputs.uiType = FunctionInputData::UI_TYPES::SCANF;
                }
            } else if (fname.equals("fgets")) { // char *fgets(char *str, int count, FILE *stream), input from file stream, retval is null if end of file - depend on size of file
                debug2("new fgets")
                fnValDep[F].retval.members[0].inputs.uiType = FunctionInputData::UI_TYPES::FGETS0;
                fnValDep[F].retval.inputs.uiType = FunctionInputData::UI_TYPES::FGETS0;
                fnValDep[F].args[0].members[0].inputs.uiType = FunctionInputData::UI_TYPES::FGETS;
                fnValDep[F].args[0].inputs.uiType = FunctionInputData::UI_TYPES::FGETS;
                fnValDep[F].retval.members[0].inputs.args[2].used = true;
                fnValDep[F].retval.inputs.args[2].used = true;
                fnValDep[F].args[0].members[0].inputs.args[2].used = true;
                fnValDep[F].args[0].inputs.args[2].used = true;
            } else if (fname.equals("fopen")) { // FILE * fopen ( const char * filename, const char * mode ), input from file, name filename, -assume mode is reading if from fgets
                debug2("new fopen")
                fnValDep[F].retval.members[0].inputs.args[0].used = true;
                fnValDep[F].retval.inputs.args[0].used = true;
                fnValDep[F].retval.members[0].inputs.uiType = FunctionInputData::UI_TYPES::FOPEN;
                fnValDep[F].retval.inputs.uiType = FunctionInputData::UI_TYPES::FOPEN;
            } else if (fname.equals("fread")) { // size_t fread ( void * ptr, size_t size, size_t count, FILE * stream ), fread gets data from arg4 into arg1 and returns size read;
                debug2("new fread")
                fnValDep[F].retval.members[0].inputs.uiType = FunctionInputData::UI_TYPES::FREAD0;
                fnValDep[F].retval.inputs.uiType = FunctionInputData::UI_TYPES::FREAD0;
                fnValDep[F].args[0].members[0].inputs.uiType = FunctionInputData::UI_TYPES::FREAD;
                fnValDep[F].args[0].inputs.uiType = FunctionInputData::UI_TYPES::FREAD;
                fnValDep[F].retval.members[0].inputs.args[3].used = true;
                fnValDep[F].retval.inputs.args[3].used = true;
                fnValDep[F].args[0].members[0].inputs.args[3].used = true;
                fnValDep[F].args[0].inputs.args[3].used = true;
            } else if (fname.equals("fgetc") || fname.equals("getc")) { // int fgetc ( FILE * stream ), getc gets charactrer from stream;
                debug2("new getc/fgetc")
                fnValDep[F].retval.members[0].inputs.uiType = FunctionInputData::UI_TYPES::GETC;
                fnValDep[F].retval.inputs.uiType = FunctionInputData::UI_TYPES::GETC;
                fnValDep[F].retval.members[0].inputs.args[0].used = true;
                fnValDep[F].retval.inputs.args[0].used = true;
            // begin nonvoid used
            } else if (fname.equals("atoi")) { // int atoi (const char * str), returns int from string
                fnValDep[F].retval.members[0].inputs.args[0].used = true;
                fnValDep[F].retval.inputs.args[0].used = true;
            } else if (fname.equals("llvm.fmuladd.f64")) { // double @llvm.fmuladd.f64(double, double, double), A * B + C
                fnValDep[F].retval.members[0].inputs.args[0].used = true;
                fnValDep[F].retval.inputs.args[0].used = true;
                fnValDep[F].retval.members[0].inputs.args[1].used = true;
                fnValDep[F].retval.inputs.args[1].used = true;
                fnValDep[F].retval.members[0].inputs.args[2].used = true;
                fnValDep[F].retval.inputs.args[2].used = true;
            // begin output
            } else if (fname.equals("fprintf")) { // int fprintf(FILE *stream, const char *format, ...), output
            } else if (fname.equals("printf")) { // int printf(char * str, ...), output
            // begin memsets
            } else if (fname.equals("strcpy")) { // char *strcpy( char *dest, const char *src ), string 2 into string 1, and return string 1
                fnValDep[F].retval.members[0].inputs.args[1].used = true;
                fnValDep[F].retval.inputs.args[1].used = true;
                fnValDep[F].args[0].members[0].inputs.args[1].used = true;
                fnValDep[F].args[0].inputs.args[1].used = true;
            } else if (fname.equals("sprintf")) { // int sprintf( char *buffer, const char *format, ... ), varargs and arg2 into string 1
                fnValDep[F].args[0].members[0].inputs.args[1].used = true;
                fnValDep[F].args[0].inputs.args[1].used = true;
                for (unsigned i = 2; i < 20; i++) { // as high as i will go for varargs; TODO: actual infinite using flags
                    fnValDep[F].args[0].members[0].inputs.args[i].used = true;
                    fnValDep[F].args[0].inputs.args[i].used = true;
                }
            } else if (fname.equals("sscanf") || fname.equals("__isoc99_sscanf")) { // int sscanf( const char *buffer, const char *format, ... ), input from string 1, to string 2 and on varargs
                for (unsigned i = 2; i < 20; i++) { // as high as i will go for varargs; TODO: actual infinite using flags
                    fnValDep[F].args[i].members[0].inputs.args[0].used = true;
                    fnValDep[F].args[i].inputs.args[0].used = true;
                    fnValDep[F].args[i].members[0].inputs.args[1].used = true;
                    fnValDep[F].args[i].inputs.args[1].used = true;
                }
            } else if (fname.equals("llvm.memcpy.p0i8.p0i8.i64")) { // void @llvm.memcpy.p0i8.p0i8.i64(i8* noalias nocapture writeonly, i8* noalias nocapture readonly, i64, i1 immarg), copies mem 2 into mem 1 for arg3 entries
                fnValDep[F].args[0].members[0].inputs.args[1].used = true;
                fnValDep[F].args[0].inputs.args[1].used = true;
            // begin mem define; should be automatically be handled i think
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
                // void @llvm.dbg.label(metadata)
                debug("err: other external function call")
            }
        } else {
            std::vector<CallBase*> callbases = std::vector<CallBase*>();
            for (BasicBlock &B : *F) { // TODO: branches possibly
                for (Instruction &I : B) {
                    // find store and return instructions and perform instruction analysis
                    if (ReturnInst *ri = dyn_cast<ReturnInst>(&I)) {
                        // return instruction
                        Value *next = ri->getReturnValue();
                        AddFnInputs(fnValDep[F].retval.inputs, InstructionBacktrackValue(next, analysisParents), F->arg_size());
                    } else if (StoreInst *store = dyn_cast<StoreInst>(&I)) {
                        // store instruction
                        Value *next = store->getPointerOperand();
                        MemoryRecord index = getMemoryRecord(next);
                        markArgDataOut(fnValDep[F], InstructionBacktrackValue(next, analysisParents), index);
                    } else if (CallBase *call = dyn_cast<CallBase>(&I)){
                        callbases.push_back(call); // do calls at the end and in order (should be tree-logical but isn't)
                    }

                }
                /*
                if (BranchInst *bi = dyn_cast<BranchInst>(I)) {
                    if (bi->isConditional()) {
                        // branch instruction
                        Value *next = bi->getCondition();
                        AddFnInputs(fnValDep[F], InstructionBacktrackValue(next, analysisParents));
                    }
                } if (SwitchInst *sw = dyn_cast<SwitchInst>(I)) {
                    // switch instruction
                    Value *next = sw->getCondition();
                    AddFnInputs(fnValDep[F], InstructionBacktrackValue(next, analysisParents));
                    //AddFnArgsFromInstData(fnValDep[F], next, analysisParents);
                } else */
            }
            for (CallBase *call : callbases){
                if (Function *cf = call->getCalledFunction()) {
                    debug("FBV Call Func: " << *call)
                    if (FunctionInputData::UI_TYPES uiType = isNonFptrUserInput(call, nullptr)) {
                        //used = true;
                        //propUserInput(instValDep[val], call, nullptr, analysisParents, uiType);
                    } else {
                        // get val dep of arg
                        FunctionOutputData &calldata = FunctionBacktrackValue(cf, analysisParents);
                        for (Use &arg : call->args()) {
                            MemoryRecord memrec = getMemoryRecord(arg.get());
                            if (memrec.isArg) { // can also be global
                                debug2("FBV found arg dependency")
                            }
                            //FunctionInputData &newinputs = getArgDataIn(calldata, memrec);
                            FunctionInputData &newinputs = calldata.args[call->getArgOperandNo(&arg)].members[0].inputs; // arg no
                            checkNewUserInput(newinputs, arg.get());
                            markArgDataOut(fnValDep[F], newinputs, memrec);
                        }
                        // copy globals
                        for (auto &entry : calldata.globals) { // key = first, value = second
                            AddArgDataOut(fnValDep[F].globals[entry.first], entry.second, F->arg_size());
                        }
                    }
                } else {
                    debug("FBV Call Func pointer Instruction: " << *call)
                    for (Function *cf : fptrTryGetValues(call->getCalledOperand())) {
                        if (FunctionInputData::UI_TYPES uiType = isFptrUserInput(call, nullptr, cf)) {
                            //used = true;
                            //propUserInput(instValDep[val], call, nullptr, analysisParents, uiType);
                        } else {
                            // get val dep of arg
                            FunctionOutputData &calldata = FunctionBacktrackValue(cf, analysisParents);
                            for (Use &arg : call->args()) {
                                MemoryRecord memrec = getMemoryRecord(arg.get());
                                if (memrec.isArg) { // can also be global
                                    debug2("FBV found fptr arg dependency")
                                }
                                FunctionInputData &newinputs = calldata.args[call->getArgOperandNo(&arg)].members[0].inputs; // arg no
                                checkNewUserInput(newinputs, arg.get());
                                markArgDataOut(fnValDep[F], newinputs, memrec);
                            }
                            // copy globals
                            for (auto &entry : calldata.globals) { // key = first, value = second
                                AddArgDataOut(fnValDep[F].globals[entry.first], entry.second, F->arg_size());
                            }
                        }
                    }
                }
            }
        }
        analysisParents.erase(F);
    } else if (!fnValDep[F].finished) {
        // recursive call
        debug("FunctionBacktrackValue recursive call")
        return fnValDep[F];
    }
    fnValDep[F].finished = true;
    return fnValDep[F];
}

bool isConstantValue(Value *val) {
    if (auto *gvar = dyn_cast<GlobalVariable>(val)) {
        if (gvar->isConstant()) {return true;}
    }else if (auto *expr = dyn_cast<ConstantExpr>(val)) {
        debug("const expr : " << *expr)
        for (Use &op : expr->operands()) {
            if (!isConstantValue(op.get())) {
                return false;
            }
        }
        return true;
    } else if (isa<Constant>(val)) {
        return true;
    }
    return false;
}

void InstructionBacktrackLoadAddr(FunctionInputData &retData, Instruction *I, Value *val, std::unordered_set<Function*> &analysisParents, std::unordered_set<BasicBlock*> &loopDetection) {// recursive walking for load inst
    debug2("IBLA inst " << *I)
    debug2("IBLA val " << *val)
    // val is the load address, THERE ARE NO NESTED CONSTEXPR
    Instruction *walk = I;
    GlobalVariable *global = dyn_cast<GlobalVariable>(val);
    Function *F = I->getFunction();
    while (walk) {
        if (StoreInst *store = dyn_cast<StoreInst>(walk)) {
            if (store->getPointerOperand() == val) {
                AddFnInputs(retData, InstructionBacktrackValue(store->getValueOperand(), analysisParents), F->arg_size()); // halt here
                return;
            }
        } else if (auto *gep = dyn_cast<GetElementPtrInst>(walk)) { // maybe overkill
            if (gep->getPointerOperand() == val) {
                AddFnInputs(retData, InstructionBacktrackValue(gep, analysisParents), F->arg_size());
                std::unordered_set<BasicBlock*> loop2  = std::unordered_set<BasicBlock*>();
                    //std::unordered_set<BasicBlock*> loop2  = std::unordered_set<BasicBlock*>(loopDetection.begin(), loopDetection.end());
                InstructionBacktrackLoadAddr(retData, I, gep, analysisParents, loop2);
            }
        } else if (auto *cast = dyn_cast<CastInst>(walk)) { // maybe overkill
            for (Use &op : cast->operands()) { // only one
                if (op.get() == val) {
                    AddFnInputs(retData, InstructionBacktrackValue(cast, analysisParents), F->arg_size());
                    std::unordered_set<BasicBlock*> loop2  = std::unordered_set<BasicBlock*>();
                    InstructionBacktrackLoadAddr(retData, I, cast, analysisParents, loop2);
                }
            }
        } else if (CallBase *call = dyn_cast<CallBase>(walk)) {
            debug("call in load " << *call)
            bool used = false;
            bool in_call = false;
            for (Use &arg : call->args()) {
                if (arg.get() == val) {
                    in_call = true;
                    if (FunctionInputData::UI_TYPES uiType = isNonFptrUserInput(call, &arg)) {
                        propUserInput(instValDep[val], call, &arg, analysisParents, uiType);
                        used = true;
                    } else if (Function *cf = call->getCalledFunction()) {
                        debug("loadwalk test1")
                        FunctionOutputData &calldata = FunctionBacktrackValue(cf, analysisParents);
                        FunctionInputData &newinputs = calldata.args[call->getArgOperandNo(&arg)].inputs;
                        checkNewUserInput(newinputs, arg.get());
                        if (!isFnInputEmpty(newinputs)) {
                            used = true;
                            for (unsigned argno = 0; argno < call->arg_size(); argno++) {
                                if ((newinputs.args.find(argno) != newinputs.args.end()) && !isArgDataInEmpty(newinputs.args[argno])) { // truncated
                                    AddFnInputs(retData, InstructionBacktrackValue(call->getArgOperand(argno), analysisParents), F->arg_size());
                                }
                            }
                            for (auto &entry : newinputs.globals) {
                                AddArgDataIn(retData.globals[entry.first], entry.second);
                            }
                        }
                    } else { // fptr
                        for (Function *cf : fptrTryGetValues(call->getCalledOperand())) {
                            debug("loadwalk test2")
                            if (FunctionInputData::UI_TYPES uiType = isFptrUserInput(call, &arg, cf)) {
                                used = true;
                                propUserInput(instValDep[val], call, &arg, analysisParents, uiType);
                            } else {
                                FunctionOutputData &calldata = FunctionBacktrackValue(cf, analysisParents);
                                // may contain multiple times
                                FunctionInputData &newinputs = calldata.args[call->getArgOperandNo(&arg)].inputs;
                                checkNewUserInput(newinputs, arg.get());
                                if (!isFnInputEmpty(newinputs)) {
                                    used = true;
                                    for (unsigned argno = 0; argno < call->arg_size(); argno++) {
                                        if ((newinputs.args.find(argno) != newinputs.args.end()) && !isArgDataInEmpty(newinputs.args[argno])) { // truncated
                                            AddFnInputs(retData, InstructionBacktrackValue(call->getArgOperand(argno), analysisParents), F->arg_size());
                                        }
                                    }
                                    for (auto &entry : newinputs.globals) {
                                        AddArgDataIn(retData.globals[entry.first], entry.second);
                                    }
                                }
                            }
                        }
                    }
                } else if (global && isa<ConstantExpr>(arg)) {
                    auto *expr = dyn_cast<ConstantExpr>(arg);
                    for (Use &arg2 : expr->operands()) {
                        if (arg2.get() == val) {
                            in_call = true;
                            if (FunctionInputData::UI_TYPES uiType = isNonFptrUserInput(call, &arg)) {
                                propUserInput(instValDep[val], call, &arg, analysisParents, uiType);
                                used = true;
                            } else if (Function *cf = call->getCalledFunction()) {
                                debug("loadwalk test3")
                                FunctionOutputData &calldata = FunctionBacktrackValue(cf, analysisParents);
                                FunctionInputData &newinputs = calldata.args[call->getArgOperandNo(&arg)].inputs; // correct use of arg over arg2
                                checkNewUserInput(newinputs, arg.get());
                                if (!isFnInputEmpty(newinputs)) {
                                    used = true;
                                    for (unsigned argno = 0; argno < call->arg_size(); argno++) {
                                        if ((newinputs.args.find(argno) != newinputs.args.end()) && !isArgDataInEmpty(newinputs.args[argno])) { // truncated
                                            AddFnInputs(retData, InstructionBacktrackValue(call->getArgOperand(argno), analysisParents), F->arg_size());
                                        }
                                    }
                                    for (auto &entry : newinputs.globals) {
                                        AddArgDataIn(retData.globals[entry.first], entry.second);
                                    }
                                }
                            } else { // fptr
                                for (Function *cf : fptrTryGetValues(call->getCalledOperand())) {
                                    debug("loadwalk test4")
                                    if (FunctionInputData::UI_TYPES uiType = isFptrUserInput(call, &arg, cf)) {
                                        used = true;
                                        propUserInput(instValDep[val], call, nullptr, analysisParents, uiType);
                                    } else {
                                        FunctionOutputData &calldata = FunctionBacktrackValue(cf, analysisParents);
                                        // may contain multiple times
                                        FunctionInputData &newinputs = calldata.args[call->getArgOperandNo(&arg)].inputs; // correct use of arg over arg2
                                        checkNewUserInput(newinputs, arg.get());
                                        if (!isFnInputEmpty(newinputs)) {
                                            used = true;
                                            for (unsigned argno = 0; argno < call->arg_size(); argno++) {
                                                if ((newinputs.args.find(argno) != newinputs.args.end()) && !isArgDataInEmpty(newinputs.args[argno])) { // truncated
                                                    AddFnInputs(retData, InstructionBacktrackValue(call->getArgOperand(argno), analysisParents), F->arg_size());
                                                }
                                            }
                                            for (auto &entry : newinputs.globals) {
                                                AddArgDataIn(retData.globals[entry.first], entry.second);
                                            }
                                        }
                                    }
                                }
                            }\
                        }
                    }
                }
            }
                    //if (global && (globalFDep[global].find(cf) != globalFDep[global].end())) {
            if (!in_call && global) { // TODO: fix global list to recurse
            // maybe check if input???
                // there is no use
                if (Function *cf = call->getCalledFunction()) {
                    if (!cf->isDeclaration()) {
                    debug("global check " << *cf)
                        FunctionOutputData &calldata = FunctionBacktrackValue(cf, analysisParents);
                        // global dependency
                        FunctionInputData &newinputs = calldata.globals[global].inputs;
                        if (!isFnInputEmpty(newinputs)) {
                            used = true;
                            for (unsigned argno = 0; argno < call->arg_size(); argno++) {
                                if ((newinputs.args.find(argno) != newinputs.args.end()) && !isArgDataInEmpty(newinputs.args[argno])) { // truncated; TODO: add to data structure - what do the children of this depend on
                                    AddFnInputs(retData, InstructionBacktrackValue(call->getArgOperand(argno), analysisParents), F->arg_size());
                                }
                            }
                            for (auto &entry : newinputs.globals) {
                                AddArgDataIn(retData.globals[entry.first], entry.second);
                            }
                        }
                    }
                } else { // fptr
                    for (Function *cf : fptrTryGetValues(call->getCalledOperand())) {
                        if (!cf->isDeclaration()) {
                            debug("global pftr check " << *cf)
                            FunctionOutputData &calldata = FunctionBacktrackValue(cf, analysisParents);
                            // global dependency
                            FunctionInputData &newinputs = calldata.globals[global].inputs;
                            if (!isFnInputEmpty(newinputs)) {
                                used = true;
                                for (unsigned argno = 0; argno < cf->arg_size(); argno++) {
                                    if ((newinputs.args.find(argno) != newinputs.args.end()) && !isArgDataInEmpty(newinputs.args[argno])) { // truncated; TODO: add to data structure - what do the children of this depend on
                                        AddFnInputs(retData, InstructionBacktrackValue(call->getArgOperand(argno), analysisParents), F->arg_size());
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
        if (loopDetection.find(B) == loopDetection.end()) { // complete blocks
            loopDetection.insert(pred);
            InstructionBacktrackLoadAddr(retData, pred->getTerminator(), val, analysisParents, loopDetection);
        }
    }
}
std::unordered_set<std::string> unmarkInst;
FunctionInputData &InstructionBacktrackValue(Value *val, std::unordered_set<Function*> &analysisParents) {
    if (val == nullptr) {
        debug2("IBVnullptr")
        static FunctionInputData none = FunctionInputData();
        return none;
    }
    debug2("IBV " << instValDep[val].started << " " << instValDep[val].finished << " : "<< *val)
    if (!instValDep[val].started) {
        instValDep[val].started = true;
        if (Instruction *I = dyn_cast<Instruction>(val)) {
            Function *F = I->getFunction();
            if (PHINode *xi = dyn_cast<PHINode>(I)) {
                debug("PHI Instruction: " << *xi)
                for (Use &op : xi->incoming_values()) {
                    Value *prev = op.get();
                    AddFnInputs(instValDep[val], InstructionBacktrackValue(prev, analysisParents), F->arg_size());
                }
            } else if (GetElementPtrInst *gep = dyn_cast<GetElementPtrInst>(I)) {
                debug("GEP Instruction: " << *gep)
                for (Use &op : gep->operands()) { // depend on all inputs; truncated TODO: use MemoryRecord
                    Value *prev = op.get();
                    AddFnInputs(instValDep[val], InstructionBacktrackValue(prev, analysisParents), F->arg_size());
                }
            } else if (BranchInst *xi = dyn_cast<BranchInst>(I)) {
                if (xi->isConditional()) { // value dependent on what conditiondepends on
                    debug("BrCond Instruction: " << *xi)
                    Value *prev = xi->getCondition();
                    AddFnInputs(instValDep[val], InstructionBacktrackValue(prev, analysisParents), F->arg_size());
                }
            } else if (SwitchInst *xi = dyn_cast<SwitchInst>(I)) { // value dependent on what conditiondepends on
                debug("Switch Instruction: " << *xi)
                Value *prev = xi->getCondition();
                AddFnInputs(instValDep[val], InstructionBacktrackValue(prev, analysisParents), F->arg_size());
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
                    AddFnInputs(instValDep[val], InstructionBacktrackValue(prev, analysisParents), F->arg_size());
                }
            }  else if (CallBase *call = dyn_cast<CallBase>(I)) {
                
                if (Function *cf = call->getCalledFunction()) {
                    debug("Call Input: " << *call)
                    if (FunctionInputData::UI_TYPES uiType = isNonFptrUserInput(call, nullptr)) { // external function call
                        debug("User Input")
                        propUserInput(instValDep[val], call, nullptr, analysisParents, uiType);
                    } else {
                        // get val dep of retval
                        FunctionOutputData &calldata = FunctionBacktrackValue(cf, analysisParents);
                        for (Use &arg : call->args()) {
                            FunctionInputData &newinputs = calldata.retval.inputs; // retval
                            checkNewUserInput(newinputs, arg.get());
                            for (unsigned argno = 0; argno < cf->arg_size(); argno++) {
                                if ((newinputs.args.find(argno) != newinputs.args.end()) && !isArgDataInEmpty(newinputs.args[argno])) { // truncated
                                    AddFnInputs(instValDep[val], InstructionBacktrackValue(call->getArgOperand(argno), analysisParents), F->arg_size());
                                }
                            }
                            for (auto &entry : newinputs.globals) {
                                AddArgDataIn(instValDep[val].globals[entry.first], entry.second);
                            }
                        }
                    }
                } else {
                    debug("Call Func pointer Instruction: " << *call)
                    for (Function *cf : fptrTryGetValues(call->getCalledOperand())) {
                        if (FunctionInputData::UI_TYPES uiType = isFptrUserInput(call, nullptr, cf)) {
                            debug("Fptr user input")
                            propUserInput(instValDep[val], call, nullptr, analysisParents, uiType);
                        } else {
                            // get val dep of retval
                            FunctionOutputData &calldata = FunctionBacktrackValue(cf, analysisParents);
                            for (Use &arg : call->args()) {
                                FunctionInputData &newinputs = calldata.retval.inputs; // retval
                                checkNewUserInput(newinputs, arg.get());
                                for (unsigned argno = 0; argno < cf->arg_size(); argno++) {
                                    if ((newinputs.args.find(argno) != newinputs.args.end()) && !isArgDataInEmpty(newinputs.args[argno])) { // truncated
                                        AddFnInputs(instValDep[val], InstructionBacktrackValue(call->getArgOperand(argno), analysisParents), F->arg_size());
                                    }
                                }
                                for (auto &entry : newinputs.globals) {
                                    AddArgDataIn(instValDep[val].globals[entry.first], entry.second);
                                }
                            }
                        }
                    }
                    // called function pointer, assume dependance on all inputs and no globals
                    /*
                    for (Use &op : call->operands()) {
                        Value *prev = op.get();
                        AddFnInputs(instValDep[val], InstructionBacktrackValue(prev, analysisParents));
                    }
                    */
                }

            } else if (LoadInst *xi = dyn_cast<LoadInst>(I)) { // must come before UnaryInstruction!!!!
                debug("Load Instruction: " << *xi)
                Value *prev = xi->getPointerOperand();
                if (!isConstantValue(prev)) { // input is constant
                    AddFnInputs(instValDep[val], InstructionBacktrackValue(prev, analysisParents), F->arg_size());
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
                    if (auto *expr = dyn_cast<ConstantExpr>(prev)) {
                        for (Use &op : expr->operands()) {
                            if (!isConstantValue(op.get())) {
                                std::unordered_set<BasicBlock*> loopDetection;
                                InstructionBacktrackLoadAddr(instValDep[val], xi, op.get(), analysisParents, loopDetection);
                            }
                        }
                    }
                } else {
                    debug("const load " << *prev)
                }
            } else if (CmpInst *xi = dyn_cast<CmpInst>(I)) {
                debug("Compare Instruction: " << *xi)
                for (Use &op : xi->operands()) {
                    Value *prev = op.get();
                    AddFnInputs(instValDep[val], InstructionBacktrackValue(prev, analysisParents), F->arg_size());
                }
            } else if (SelectInst *xi = dyn_cast<SelectInst>(I)) {
                debug("Select Instruction: " << *xi)
                for (Use &op : xi->operands()) { // getCondition(), getTrueValue(), getFalseValue()
                    Value *prev = op.get();
                    AddFnInputs(instValDep[val], InstructionBacktrackValue(prev, analysisParents), F->arg_size()); // this depends on the inputs that all ops depend on
                }
            } else if (ReturnInst *xi = dyn_cast<ReturnInst>(I)) {
                debug("Return Instruction: " << *xi)
                Value *prev = xi->getReturnValue();
                AddFnInputs(instValDep[val], InstructionBacktrackValue(prev, analysisParents), F->arg_size()); // retval depends on the inputs that prev depends on
            } else if (UnaryInstruction *xi = dyn_cast<UnaryInstruction>(I)) {
                debug("UnaryOp Instruction: " << *xi)
                for (Use &op : xi->operands()) {
                    Value *prev = op.get();
                    AddFnInputs(instValDep[val], InstructionBacktrackValue(prev, analysisParents), F->arg_size()); // this depends on the inputs that prev depends on
                }
            } else {
                debug("Unmarked inst: " << I->getOpcodeName())
                unmarkInst.insert(I->getOpcodeName());
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
        std::vector<std::string> outputDeps;
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
            MAIN_RT = true;
            std::unordered_set<Function*> mainAnalysisParents = std::unordered_set<Function*>();
            FunctionInputData &mainArgs = FunctionBacktrackRuntime(Main, mainAnalysisParents);
            //MAIN_RT = false;
            std::unordered_set<Function*> mainAnalysisParents2 = std::unordered_set<Function*>();
            FunctionOutputData &mainOuts = FunctionBacktrackValue(Main, mainAnalysisParents2);
            for (MemoryRecord &input : getInputMemory(mainArgs)) {
                inputDeps.push_back(getInputRecordName(input));
                if (input.isGlobal) {
                    for (MemoryRecord &output : getInputMemory(getArgDataOut(mainOuts, input))) {
                        if (output.isArg) {
                            outputDeps.push_back(getInputRecordName(output));
                            std::string temp;
                            raw_string_ostream temps(temp);
                            if (output.argIndex == 0) { // main argc
                                temps << getInputRecordName(input) << " depends on number of command line arguments (main - argc)";
                            } else if (output.argIndex == 1) {// main argv 
                                int i = (output.membertree.size() > 0) ? output.membertree[1] : 0; // or input.membertree[1]
                                temps << getInputRecordName(input) << " depends on command line argument, #" << i+1 << " (main - argv[" << i << "])";
                            }
                            userInputDep.insert(temp);
                        }
                    }
                } else if (input.isArg) {
                    std::string temp;
                    raw_string_ostream temps(temp);
                    if (input.argIndex == 0) { // main argc
                        temps << "Runtime depends on number of command line arguments (main - argc)";
                    } else if (input.argIndex == 1) {// main argv 
                        int i = (input.membertree.size() >= 2) ? input.membertree[2] : 0; // or input.membertree[1]
                        temps << "Runtime depends on command line argument, #" << i+1 << " (main - argv[" << i << "])";
                    }
                    userInputDep.insert(temp);
                }
            }
        }
        for (std::string const &str : globalUses) {
            debug("global uses : " << str << '\n')
        }
        for (std::string const &str : recordNames) {
            debug("record names : " << str << '\n')
        }
        for (std::string const &str : unmarkInst) {
            debug2("unmark inst : " << str << '\n')
        }
        for (std::string const &str : inputDeps) {
            debug2("Function depends on input : " << str << '\n')
        }
        for (std::string const &str : outputDeps) {
            debug2("Function depends on (output) : " << str << '\n')
        }
        for (std::string const &str : userInputDep) {
            errs() << "user input dep : " << str << '\n';
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