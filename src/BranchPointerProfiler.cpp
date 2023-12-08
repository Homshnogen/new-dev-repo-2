/**
 * @file BranchPointerProfiler.cpp
 * @summary Branch and Function Pointer Profiler for LLVM
 * 
 * This an implementation of the BranchPointerProfiler pass for LLVM.
 * The profiler aims to instrument a given LLVM module to log information about
 * dynamic branch decisions and function pointer calls during its execution.
 * 
 * The profiler assigns unique IDs to each branch and function pointer call encountered
 * in the LLVM IR and logs these along with additional metadata like the source file
 * and line number where they occur.
 * 
 * The pass instruments the code to output a branch-pointer trace after the execution,
 * which is critical for analyzing the runtime behavior of the program.
 * 
 * Usage:
 * The pass can be added to an LLVM pass pipeline and will instrument the module
 * to which it is applied. The output trace can be used for further analysis of
 * dynamic program behavior.
 * 
 * Author: Yahaira Barron Guadarrama, Omar Kalam
 * Date: 10/20/2023
 * CSC 412/512 001 Fall 2023
 * 
 * 
 * Additional Notes:
 * - The pass ignores external libraries and focuses on user-defined code.
 * - This pass is compatible with LLVM Version 14.0
 */


#include "llvm/Pass.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"

#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"

using namespace llvm;

namespace { // anonymous namespace for same-file linkage
// Define the BranchPointerProfiler pass
struct BranchPointerProfiler : public PassInfoMixin<BranchPointerProfiler> {

    // Main function to run the pass 
    PreservedAnalyses run(Module &M, ModuleAnalysisManager &AM) {
    
        // Setup for branch logging
        // Create or insert the printf function for logging
        FunctionCallee printfFunc = M.getOrInsertFunction(
          "printf",
          FunctionType::get(IntegerType::get(M.getContext(), 32), PointerType::getUnqual(IntegerType::get(M.getContext(), 8)), true) //(Type *Result, ArrayRef< Type * > Params, bool isVarArg)
        );
        
        // Setup format string for branch logging
        Constant *branchFString = ConstantDataArray::getString(M.getContext(), StringRef("branch : %d\n"));
        ConstantInt *constIntZero = ConstantInt::getSigned(IntegerType::get(M.getContext(), 32), 0);
        
        // Create global variable for branch format string 
        GlobalVariable* branchGlobalString = new GlobalVariable(M, 
            branchFString->getType(),
            true,
            GlobalValue::PrivateLinkage,
            branchFString, // has initializer, specified below
            ".log.str.branch"); 
        branchGlobalString->setAlignment(Align(1));
        branchGlobalString->setUnnamedAddr(GlobalVariable::UnnamedAddr::Global);
        errs() << "trying thing\n"; // Debudgging delete
        
        // Generate a constant expression for getting the address of the branch format string
        Constant *branchGEPExpr = ConstantExpr::getInBoundsGetElementPtr(branchFString->getType() , branchGlobalString, ArrayRef<Constant *>({constIntZero, constIntZero}));
        errs() << *branchGEPExpr << "\n";
        
        // Setup for function pointer logging
        // Create format string for function pointer logging
        Constant *funcPtrFString = ConstantDataArray::getString(M.getContext(), StringRef("func_ptr: %d\n"));
        
        // Create global variable for function pointer format string
        GlobalVariable* funcPtrGlobalString = new GlobalVariable(M, 
            funcPtrFString->getType(),
            true,
            GlobalValue::PrivateLinkage,
            funcPtrFString,
            ".log.str.func_ptr");
        funcPtrGlobalString->setAlignment(Align(1));
        funcPtrGlobalString->setUnnamedAddr(GlobalVariable::UnnamedAddr::Global);
        
        // Generate a constant expression for getting the address of the function pointer format string
        Constant *funcPtrGEPExpr = ConstantExpr::getInBoundsGetElementPtr(funcPtrFString->getType(), funcPtrGlobalString, ArrayRef<Constant *>({constIntZero, constIntZero}));

        // Assign unique ID to each branch and function pointers
        static int branchCounter = 0;
        static int funcPtrCounter = 0; 

        // Iterate over all functions, basic blocks, and instructions in the module
        for (auto &F : M) {
          for (auto &BB : F) {
            for (auto &I : BB) {
              
              // Check if the instruction is a conditional branch
              if (BranchInst *branchInst = dyn_cast<BranchInst>(&I)) {
                if (branchInst->isConditional()) {
                
                  // Increment branch counter 
                  int currentBranchID = branchCounter++;
                  
                  // Instrument the branch
                  IRBuilder<> builder(branchInst);
                  //static GetElementPtrConstantExpr * 	Create (Type *SrcElementTy, Constant *C, ArrayRef< Constant * > IdxList, Type *DestTy, unsigned Flags)
                  Value *branchIDValue = ConstantInt::get(Type::getInt32Ty(F.getContext()), currentBranchID);
                  builder.CreateCall(printfFunc, {branchGEPExpr, branchIDValue});
                        
                  // Store metadata for later use
                  if (DebugLoc DL = branchInst->getDebugLoc()) {
                    unsigned line = DL.getLine();
                    StringRef file = DL->getFilename();
                    // Store 'file' and 'line' for later use
                  }             
                }
              } 
              
              // Check if the instruction is a function pointer call 
              else if (CallInst *ci = dyn_cast<CallInst>(&I)){
                // Function pointer call 
                if (!ci->getCalledFunction()) {
                
                  // Increment function pointer counter and log the call
                  int currentFuncPtrID = funcPtrCounter++;
                  IRBuilder<> builder(ci);
                  Value *funcPtrIDValue = ConstantInt::get(Type::getInt32Ty(F.getContext()), currentFuncPtrID);
                  builder.CreateCall(printfFunc, {funcPtrGEPExpr, funcPtrIDValue});

                  // Store metadata for later use
                  if (DebugLoc DL = ci->getDebugLoc()) {
                    unsigned line = DL.getLine();
                    StringRef file = DL->getFilename();
            
                    // Log the file and line information
                    // Create another format string and global variable for this purpose
                    // For example, use a format string like "func_ptr_loc: %s:%d\n"
                    // Then, log the information similar to logging the function pointer ID
                  } 
                }
              }
            }
          }
        }
        
        return PreservedAnalyses::none(); // Indicate that the function was modified 
    };
};

}

// Plugin entry point 
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
