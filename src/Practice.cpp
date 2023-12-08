
#include "llvm/Pass.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"

#include "llvm/IR/DebugInfo.h"
#include "llvm/IR/TypeFinder.h"
//#include "llvm/BinaryFormat/Dwarf.h"

#include <string>
#include <regex>
#include <vector>
#include <unordered_set>

using namespace llvm;

namespace { // anonymous namespace for same-file linkage

// for (BasicBlock *pred : predecessors(BB)) pred->getTerminator()


DIGlobalVariable *getGlobalVariable(GlobalVariable *val) {
    for (unsigned index = 0; MDNode *md = val->getMetadata(index); index++) {
        if (DIGlobalVariableExpression *expr = dyn_cast<DIGlobalVariableExpression>(md)) {
            return expr->getVariable();
        }
    }
    return nullptr;
}

StructType *typeAsStruct(Type *t) {
    Type *type = t;
    while(true) {
        if (StructType *stru = dyn_cast<StructType>(type)) {
            return stru;
        } else if (PointerType *ptr = dyn_cast<PointerType>(type)) {
            if (!ptr->isOpaque()) {
                type = ptr->getNonOpaquePointerElementType();
            }
        } else if (ArrayType *ptr = dyn_cast<ArrayType>(type)) {
            type = ptr->getElementType();
        } else if (VectorType *ptr = dyn_cast<VectorType>(type)) {
            type = ptr->getElementType();
        } else {
            return nullptr;
        }
    }
    return nullptr; // unreachable
}

std::string findVariableName (Value *val);
std::string nameFromGEP (User *gep, int fromLoad) { // for each fromLoad, add a zero before second. For string, remove a zero after
    std::string ret;
    llvm::raw_string_ostream rets(ret);
    return ret;
    bool first = true, second = true, stringSkip = false;
    Value *addrArg;
    Type *type;// = gep->getType(); // return type; we want inbound type
    //std::vector<Type *> typeHist;
    //typeHist.pushBack(type);
    DICompositeType *struMD = nullptr;
    for (unsigned i = 0; i < gep->getNumOperands(); i++) {
        Use const &op = gep->getOperandUse(i);
        if (first) {
            // address arg
            addrArg = op.get();
            
            if (GlobalVariable *gvar = dyn_cast<GlobalVariable>(addrArg)) {
                rets << "(GVar skipping) ";
                stringSkip = true;
                if (ConstantDataSequential *gstr = dyn_cast<ConstantDataSequential>(gvar->getInitializer())) { // is string for sure
                    if (gstr->isString()) {
                        //rets << "(String skipping) ";
                        stringSkip = true;
                    }
                }
            }
            type = op->getType(); // inbound type - pointer
            first = false;
        } else if (stringSkip) {
            // do nothing
            type = dyn_cast<PointerType>(type)->getNonOpaquePointerElementType(); // removed (most) typechecks
            stringSkip = false;
        } else if (second) {
            if (fromLoad > 0) { // for each fromLoad, add a zero before (and as) second; don't change types
                rets << "(From load second) ";
                rets << findVariableName(addrArg);
                fromLoad -= 1;
                i -= 1; // this should be the last thing that happens
            } else if (dyn_cast<ConstantInt>(op.get()) && static_cast<ConstantInt *>(op.get())->isZero()) { // offset is zero
                rets << findVariableName(addrArg);
                if (type->isPointerTy() && !type->isOpaquePointerTy()) {
                    type = type->getNonOpaquePointerElementType();
                }
            } else { // offset is non-zero
                rets << '(' << findVariableName(addrArg) << " + " << findVariableName(op.get()) << ')';
                if (type->isPointerTy() && !type->isOpaquePointerTy()) {
                    type = type->getNonOpaquePointerElementType();
                }
            }
            second = false;
        } else {
            while (fromLoad > 0) { // for each fromLoad, add a zero before (and as) second; don't change types
                rets << '[' << "(From load again) " << '0' << ']';
                fromLoad -= 1;
            }
            if (StructType *stru = dyn_cast<StructType>(type)) { // struct or union
                if (ConstantInt *ind = dyn_cast<ConstantInt>(op.get())) {
                    if (!struMD) {
                        // find first struct data
                        stru->getStructName();
                    }
                    unsigned struIndex = ind->getZExtValue();
                    if (stru->hasName()) {
                        // TODO: do name lookup
                        rets << '.' << "name" << struIndex;
                    } else {
                        // TODO: find variable metadata
                        rets << '[' << struIndex << ']';
                    }
                    type = stru->getTypeAtIndex(struIndex);
                }
            } else if (PointerType *ptr = dyn_cast<PointerType>(type)) {
                if (!ptr->isOpaque()) {
                    rets << '[' << findVariableName(op.get()) << ']';
                    type = ptr->getNonOpaquePointerElementType();
                }
            } else if (ArrayType *arr = dyn_cast<ArrayType>(type)) {
                rets << '[' << findVariableName(op.get()) << ']';
                type = arr->getElementType();
            } else if (VectorType *arr = dyn_cast<VectorType>(type)) {
                rets << '[' << findVariableName(op.get()) << ']';
                type = arr->getElementType();
            } else { // no type info, or union?
                rets << '[' << "err " << findVariableName(op.get()) << ']';
            }
        }
    }

    return ret;
}
std::string findVariableName (Value *val, int fromLoad);
std::string findVariableName (Value *val) {
    return findVariableName(val, 0);
}
std::string findVariableName (Value *val, int fromLoad) {
    std::string ret;
    llvm::raw_string_ostream rets(ret);

    if (Constant *con = dyn_cast<Constant>(val)) {
        if (ConstantExpr *expr = dyn_cast<ConstantExpr>(con)) {
            if (std::strcmp(expr->getOpcodeName(), "getelementptr") == 0) {
                rets << nameFromGEP(expr, fromLoad);
            } else {
                rets << "expr of ";
                for (Use &next : expr->operands()) {
                    rets << findVariableName(next.get(), fromLoad) << ", "; // should be constants
                }
            }
        } else if (GlobalAlias *alias = dyn_cast<GlobalAlias>(con)) {
            rets << *val << " Global Alias";
        } else if (Function *func = dyn_cast<Function>(con)) {
            rets << func->getName();
        } else if (GlobalVariable *gvar = dyn_cast<GlobalVariable>(con)) {
            if (DIGlobalVariable *gvinfo = getGlobalVariable(gvar)) {
                // var has name
                for (int i = 0; i < fromLoad; i++) {
                    rets << '(' << '*';
                }
                rets << gvinfo->getName();
                for (int i = 0; i < fromLoad; i++) {
                    rets << ')';
                }
            } else {
                // var has no name; global const string
                //rets << *gvar->getInitializer() << " string or ";
                if (ConstantDataSequential *gstr = dyn_cast<ConstantDataSequential>(gvar->getInitializer())) { // is string for sure
                    if (gstr->isString()) {
                        rets << '"' << gstr->getAsString() << '"';
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
        rets << "store " << findVariableName(stor->getValueOperand(), fromLoad) << " in " << findVariableName(stor->getPointerOperand(), fromLoad);
    } else if (AllocaInst *alloca = dyn_cast<AllocaInst>(val)){
        for (DbgDeclareInst *ddi : FindDbgDeclareUses(val)) { // should be only one
            //dbgs() << "Debug instruction: " << *ddi << '\n';
            DILocalVariable *varb = ddi->getVariable();
            rets << varb->getName();
        }
    } else if (GetElementPtrInst *gep = dyn_cast<GetElementPtrInst>(val)){
        /*
        bool first = true;
        for (Use &ind : gep->indices()) { // should be only one
            if (first) {
                if (dyn_cast<ConstantInt>(ind.get()) && static_cast<ConstantInt *>(ind.get())->isZero()) {
                    rets << findVariableName(gep->getPointerOperand(), fromLoad);
                } else {
                    rets << '(' << findVariableName(gep->getPointerOperand(), fromLoad) << " + " << findVariableName(ind.get(), fromLoad) << ')';
                }
                first = false;
            } else {
                rets << '[' << findVariableName(ind, fromLoad) << ']';
            }
        }
        rets << " or " << nameFromGEP(gep, fromLoad);
        */
        rets << nameFromGEP(gep, fromLoad);
    } else if (LoadInst *load = dyn_cast<LoadInst>(val)) {
        for (Use &next : load->operands()) { // should be only one
            rets << findVariableName(next.get(), fromLoad+1);
        }
    } else if (CastInst *cast = dyn_cast<CastInst>(val)) { // maybe works????
        for (Use &next : cast->operands()) { // should be only one
            rets << findVariableName(next.get(), fromLoad-1);
        }
    } else if (UnaryInstruction *unary = dyn_cast<UnaryInstruction>(val)) { // includes cast, but not alloca and load
        for (Use &next : unary->operands()) { // should be only one
            rets << findVariableName(next.get(), fromLoad);
        }
    }
    return ret;
}
bool valIsVariable(Value *val) {
    return (dyn_cast<GlobalVariable>(val)) || (dyn_cast<AllocaInst>(val));
}
void sanitizeVariableName(std::string &input);
std::string findVariableName2 (Value *val);
std::string nameFromGEP2 (User *gep) { // for each fromLoad, add a zero before second. For string, remove a zero after
    std::string ret;
    llvm::raw_string_ostream rets(ret);
    for (unsigned i = 0; i < gep->getNumOperands(); i++) {
        Use const &op = gep->getOperandUse(i);
        if (i == 0) {
            // address arg
            rets << "(&" << findVariableName2(op.get()) << ")&";
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
            rets << '[' << findVariableName2(op.get()) << ']';
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
            rets << '[' << findVariableName2(op.get()) << ']';
            sanitizeVariableName(ret);
        }
    }
    ret = "(&" + ret + ")&";
    sanitizeVariableName(ret);

    return ret;
}
std::string findVariableName2 (Value *val) {
    std::string ret;
    llvm::raw_string_ostream rets(ret);

    if (Constant *con = dyn_cast<Constant>(val)) {
        if (ConstantInt *constint = dyn_cast<ConstantInt>(con)) {
            rets << constint->getSExtValue();
        } else if (ConstantExpr *expr = dyn_cast<ConstantExpr>(con)) {
            if (std::strcmp(expr->getOpcodeName(), "getelementptr") == 0) {
                rets << nameFromGEP2(expr);
            } else {
                rets << "expr of ";
                for (Use &next : expr->operands()) {
                    rets << findVariableName2(next.get()) << ", "; // should be constants
                }
            }
        } else if (GlobalAlias *alias = dyn_cast<GlobalAlias>(con)) {
            rets << *val << " Global Alias";
        } else if (Function *func = dyn_cast<Function>(con)) {
            rets << func->getName();
        } else if (GlobalVariable *gvar = dyn_cast<GlobalVariable>(con)) {
            if (DIGlobalVariable *gvinfo = getGlobalVariable(gvar)) {
                // var has name
                rets << gvinfo->getName();
            } else {
                // var has no name; global const string
                //rets << *gvar->getInitializer() << " string or ";
                if (ConstantDataSequential *gstr = dyn_cast<ConstantDataSequential>(gvar->getInitializer())) { // is string for sure
                    if (gstr->isString()) {
                        rets << '"' << gstr->getAsString() << '"';
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
        rets << "store " << findVariableName2(stor->getValueOperand()) << " in " << findVariableName2(stor->getPointerOperand());
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
        rets << nameFromGEP2(gep);
    } else if (LoadInst *load = dyn_cast<LoadInst>(val)) {
        for (Use &next : load->operands()) { // should be only one
            rets << "(*" << findVariableName2(next.get()) << ")*";
            sanitizeVariableName(ret);
        }
    } else if (CastInst *cast = dyn_cast<CastInst>(val)) { // maybe works????
        for (Use &next : cast->operands()) { // should be only one
            rets << findVariableName2(next.get());
        }
    } else if (CallInst *ci = dyn_cast<CallInst>(val)) {
        rets << findVariableName2(ci->getCalledOperand());
        rets << '(';
        for (unsigned i = 0; i < ci->arg_size(); i++) {
            rets << ((i==0) ? "" : ", ") << findVariableName2(ci->getArgOperand(i));
        }
        rets << ")";
    } else if (UnaryInstruction *unary = dyn_cast<UnaryInstruction>(val)) { // includes cast, but not alloca and load
        for (Use &next : unary->operands()) { // should be only one
            rets << findVariableName2(next.get());
        }
    }
    if (ret.length() == 0) {
        //rets << "unnamed(" << val->getName() << ")";
        if (val->getName().equals("")) {
            rets << "unnamed(" << *val->getType() << ")";
        } else {
            rets << "unnamed(" << val->getName() << ")";
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
std::string findVariableName3(Value *val) {
    std::string ret = findVariableName2(val);
    trimVariableName(ret);
    return ret;
}


void addUsesCodes(std::unordered_set<std::string> &codes, User *U) {
    if (ConstantExpr *expr = dyn_cast<ConstantExpr>(U)) {
        codes.insert(expr->getOpcodeName());
    }
    for (auto *val : U->operand_values()) {
        if (auto *next = dyn_cast<User>(val)) {
            addUsesCodes(codes, next);
        }
    }
}

struct PracticePass : public PassInfoMixin<PracticePass> {
    PreservedAnalyses run(Module &M, ModuleAnalysisManager &AM) {
        std::unordered_set<std::string> instCodes;
        std::unordered_set<std::string> exprCodes;
        std::unordered_set<std::string> funcNames;
        std::unordered_set<std::string> funcDecls;
        std::unordered_set<std::string> varNames;
        std::unordered_set<std::string> varUnnamed;
        //errs() << "Module ?:\n" << M << "\n";
        for (auto &F : M) {
            funcNames.insert(F.getName().str());
            //errs() << "CSC 512: " << F.getName() << "\n";
            if (F.isDeclaration()) {
                //funcDecls.insert(F.getName().str());
                std::string temp;
                raw_string_ostream temps(temp);
                temps << F;
                funcDecls.insert(temp);
            } else if (F.isVarArg()) {
                errs() << "VarArg: " << F.getName() << "\n";
            }
            //errs() << "Function body:\n" << F << "\n";
            for (auto& B : F) {
            //errs() << "Basic block:\n" << B << "\n";
                for (auto& I : B) {
                    instCodes.insert(I.getOpcodeName());
                    addUsesCodes(exprCodes, &I);
                    if (AllocaInst *alloca = dyn_cast<AllocaInst>(&I)) {
                        std::string varstr = findVariableName3(alloca);
                        varNames.insert(varstr);
                        if (strncmp(varstr.c_str(), "unnamed", 7) == 0) {
                            std::string temp;
                            raw_string_ostream temps(temp);
                            temps << *alloca;
                            varUnnamed.insert(temp);
                        }
                    }
                }
            }
        }
        for (std::string const &str : instCodes) {
            errs() << "Opcode : " << str << '\n';
        }
        for (std::string const &str : exprCodes) {
            errs() << "Expr opcode : " << str << '\n';
        }
        for (std::string const &str : funcNames) {
            errs() << "func names : " << str << '\n';
        }
        for (std::string const &str : varNames) {
            errs() << "var names : " << str << '\n';
        }
        for (std::string const &str : varUnnamed) {
            errs() << "var unnamed : " << str << '\n';
        }
        for (std::string const &str : funcDecls) {
            errs() << "func decls : " << str << '\n';
        }
        return PreservedAnalyses::all();
    };
};

}

        /*
        for (StructType * stype : M.getIdentifiedStructTypes()) {
            errs() << "Struct Type: name '" << stype->getName() << ", values " << *stype << "\n";
        }
        */
        /*
        TypeFinder tfinder;
        tfinder.run(M, false);
        for (StructType * stype : tfinder) {
            errs() << "Struct Type: name '" << stype->getName() << ", values " << *stype << "\n";
        }
        for (const MDNode *md : tfinder.getVisitedMetadata()) {
            if (DICompositeType * structDI = const_cast<DICompositeType *>(dyn_cast<DICompositeType>(md))) {
                errs() << "Metadata : " << *structDI << '\n';
                errs() << "type : " << structDI->getIdentifier() << '\n';
                //errs() << "tag " << structDI->getTag() << "\n";
                //errs() << "tag struct " << dwarf::DW_TAG_structure_type << "\n";
                //errs() << "tag union " << dwarf::DW_TAG_union_type << "\n";
            }
            //errs() << "Metadata : " << *md << "\n";
        }
        */

                    /*
                    errs() << "Instruction: var name " << findVariableName3(&I) << " :: " << I << "\n";
                    for (Use &use : I.operands()) {
                        errs() << "    Use: var name " << findVariableName3(use.get()) << "\n";
                        
                        errs() << "                Use name or as operand: ";
                        use->printAsOperand(errs());
                        errs() << "\n";
                        //errs() << "    Op name: " << *use << "\n";
                        if (ConstantExpr *expr = dyn_cast<ConstantExpr>(use.get())) {
                            //errs() << "Base Instruction: " << I << "\n";
                            if (std::strcmp(expr->getOpcodeName(), "getelementptr") == 0) {
                                errs() << "Base Var name: " << findVariableName2(&I) << "\n";
                                //errs() << "    GEP Expression: " << *expr << "\n";
                                errs() << "    GEP Expr Var name: " << findVariableName2(expr) << "\n";

                                errs() << "    GEP type: " << *expr->getType() << "\n";
                                errs() << "    GEP deref type: " << *expr->getType()->getNonOpaquePointerElementType() << "\n";
                                if (StructType *st = typeAsStruct(expr->getType())) {
                                    errs() << "        GEP name: " << expr->getName() << "\n";
                                    errs() << "        GEP struct subtype: " << *st << "\n";
                                }
                                for (Use &op : expr->operands()) {
                                    errs() << "        Op name: " << *op << "\n";
                                    errs() << "        Op type: " << *op->getType() << "\n";
                                    if (StructType *st = dyn_cast<StructType>(op->getType())) {
                                        errs() << "        Struct name: " << op->getName() << "\n";
                                    }
                                }
                            }
                        }
                    }
                    if (GetElementPtrInst *gep = dyn_cast<GetElementPtrInst>(&I)) {
                        //errs() << "GEP Instruction: " << *gep << "\n";
                        errs() << "    GEP Inst Var name: " << findVariableName2(gep) << "\n";
                        errs() << "    GEP type: " << *gep->getType() << "\n";
                        errs() << "    GEP deref type: " << *gep->getType()->getNonOpaquePointerElementType() << "\n";
                        if (std::strcmp(gep->getOpcodeName(), "getelementptr") == 0) {
                            if (StructType *st = typeAsStruct(gep->getType())) {
                                errs() << "        GEP name: " << gep->getName() << "\n";
                                errs() << "        GEP struct subtype: " << *st << "\n";
                            }
                            for (Use &op : gep->operands()) {
                                errs() << "        Op name: " << *op << "\n";
                                errs() << "        Op type: " << *op->getType() << "\n";
                                if (StructType *st = dyn_cast<StructType>(op->getType())) {
                                    errs() << "        Struct name: " << op->getName() << "\n";
                                }
                            }
                        }
                    }
                    if (PHINode *phi = dyn_cast<PHINode>(&I)) {
                        errs() << "PHI Instruction: " << *phi << "\n";
                        for (Use &op : phi->incoming_values()) {
                            errs() << "    incoming value: " << *op.get() << "\n";
                        }
                    } else if (LoadInst *li = dyn_cast<LoadInst>(&I)) {
                        errs() << "Load Instruction: " << *li << "\n";
                        if (Instruction *addr = dyn_cast<Instruction>(li->getPointerOperand())) {
                            //errs() << "Load Addr: " << *addr << "\n";
                            User *lastUser;
                            for (User *user : addr->users()) {
                                errs() << "    Load Addr user: " << *user << "\n";
                                lastUser = user;
                            }
                            errs() << "    Load Addr value generator: " << *lastUser << "\n";
                            //errs() << "    Load Addr inst type?: " << addr->getOpcodeName() << "\n";
                        }
                    } else if (dyn_cast<AllocaInst>(&I)) {
                        for (DbgDeclareInst *ddi : FindDbgDeclareUses(&I)) {
                            //dbgs() << "Debug instruction: " << *ddi << '\n';
                            DILocalVariable *varb = ddi->getVariable();
                            if (varb->isParameter()) { // varb->getArg() > 0;
                                dbgs() << "Alloca Variable is arg: " << varb->getName() << ", function " << I.getFunction()->getName() << ", arg #" << varb->getArg()-1 << '\n';
                            } else {
                                dbgs() << "Alloca Variable: " << varb->getName() << ", Line " << varb->getLine() << '\n';
                            }
                        }
                    } else if (CallInst *ci = dyn_cast<CallInst>(&I)) {
                        // dbgs() << "Call Instruction: " << I << '\n';
                        for (Use &arg : ci->args()) {
                            //dbgs() << "    Call Arg: " << *arg.get() << ", num? #" << ci->getArgOperandNo(&arg) << '\n';
                        }
                        for (unsigned i = 0; i < ci->arg_size(); i++) {
                            //dbgs() << "    Call Arg 2: " << *ci->getArgOperand(i) << ", num #" << i << '\n';
                        }
                    } else if (GetElementPtrInst *gep = dyn_cast<GetElementPtrInst>(&I)) {
                        dbgs() << "GEP (array get) instruction: " << I << '\n';
                        dbgs() << "    Ptr arg: " << *gep->getPointerOperand() << '\n';
                        dbgs() << "    Index arg?: " << gep->getAddressSpace() << '\n';
                        dbgs() << "    numIndices: " << gep->getNumIndices() << '\n';
                        for (Use &op : gep->indices()) {
                            dbgs() << "        num Index arg?: " << *op << '\n';
                        }
                    } else if (CastInst *cast = dyn_cast<CastInst>(&I)) {
                        dbgs() << "Cast instruction: " << I << '\n';
                        for (Use &op : cast->operands()) {
                            dbgs() << "    cast op: " << *op << '\n';
                        }
                    }
                    */

                    /*
                    //errs() << "Instruction: " << I << "\n";
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
                    } else if (dyn_cast<AllocaInst>(&I)) {
                        for (DbgDeclareInst *ddi : FindDbgDeclareUses(&I)) {
                            //dbgs() << "Debug instruction: " << *ddi << '\n';
                            dbgs() << "Alloca Variable: " << ddi->getVariable()->getName() << ", Line " << ddi->getVariable()->getLine() << '\n';
                            DILocalVariable *varb = ddi->getVariable();
                            if (varb->isParameter()) { // varb->getArg() > 0;
                                dbgs() << "Alloca Variable is arg: " << ddi->getVariable()->getName() << ", function " << I.getFunction()->getName() << ", arg #" << varb->getArg()-1 << '\n';
                            }
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
                    */


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