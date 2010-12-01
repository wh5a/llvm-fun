//===--------------- printCode.cpp - Project 1 for CS 701 ---------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file is a skeleton of an implementation for the "printCode
// pass of Univ. Wisconsin-Madison's CS 701 Project 1.
//
//===----------------------------------------------------------------------===//

#define DEBUG_TYPE "printCode"
#include "llvm/Pass.h"
#include "llvm/Module.h"
#include "llvm/Function.h"
#include "llvm/BasicBlock.h"
#include "llvm/Instruction.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/InstIterator.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/User.h"
#include "llvm/Instructions.h"
using namespace llvm;

namespace {
  class printCode : public FunctionPass {
  private:
    DenseMap<const Value*, int> instMap;

    public:
    static char ID; // Pass identification, replacement for typeid
    printCode() : FunctionPass(&ID) {}

    void addToMap(Function &F) {
      static int id = 1;
      for (inst_iterator i = inst_begin(F), E = inst_end(F); i != E; ++i, ++id)
        // Convert the iterator to a pointer, and insert the pair
        instMap.insert(std::make_pair(&*i, id));
    }
    
    //**********************************************************************
    // runOnFunction
    //**********************************************************************
    virtual bool runOnFunction(Function &F) {
      // Iterate over the instructions in F, creating a map from instruction address to unique integer.
      addToMap(F);

      bool changed = false;
      // Iterate over all basic blocks in the function, and all
      // instructions in each basic block.
      for (Function::iterator b = F.begin(), e = F.end(); b != e; ++b)
        for (BasicBlock::iterator i = b->begin(), e = b->end(); i != e;) {
          // Look for an instruction that stores a value v to the
          // memory location pointed to by virtual register %m,
          // followed by an instruction that loads from the location
          // pointed to by %m into register %k. The second instruction
          // (the load) is unnecessary.
          if (isa<StoreInst>(i)) {
            // store <ty> <value>, <ty>* <pointer>
            Value *v = i->getOperand(0), *m = i->getOperand(1);
            ++i;
            BasicBlock::iterator k = i;
            if (isa<LoadInst>(k)) {
              ++i;
              // load <ty>* <pointer>
              if (m == k->getOperand(0)) {
                errs() << "%" << instMap.lookup(&*k) << " is a useless load\n";
                // Replace all uses of %k with a use of v.
                k->replaceAllUsesWith(v);
                k->eraseFromParent();
                changed = true;
              }
            }
          }
          else
            ++i;
        }
      return changed;
    }

    //**********************************************************************
    // print (do not change this method)
    //
    // If this pass is run with -f -analyze, this method will be called
    // after each call to runOnFunction.
    //**********************************************************************
    virtual void print(std::ostream &O, const Module *M) const {
        O << "This is printCode.\n";
    }

    //**********************************************************************
    // getAnalysisUsage
    //**********************************************************************
    virtual void getAnalysisUsage(AnalysisUsage &AU) const {
    };

  };
  char printCode::ID = 0;

  // register the printCode class: 
  //  - give it a command-line argument
  //  - a name
  //  - a flag saying that we don't modify the CFG
  //  - a flag saying this is not an analysis pass
  RegisterPass<printCode> X("optLoads", "optimize unnecessary loads",
			   false, false);
}
