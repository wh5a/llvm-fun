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
      // print fn name
      errs() << "FUNCTION " << F.getName() << "\n";

      //          1. Iterate over the instructions in F, creating a
      //             map from instruction address to unique integer.
      addToMap(F);

      //          2. Iterate over the basic blocks in the function and
      //             print each instruction in that block using the format
      //             given in the assignment.
      for (Function::iterator b = F.begin(), e = F.end(); b != e; ++b) {
        errs() << "\nBASIC BLOCK " << b->getName() << "\n";
        for (BasicBlock::iterator i = b->begin(), e = b->end(); i != e; ++i) {
          int id = instMap.lookup(&*i);
          errs() << "%" << id << ":\t" << i->getOpcodeName() << "\t";
          unsigned n = i->getNumOperands();
          for (unsigned j = 0; j < n; j++) {
            Value *v = i->getOperand(j);
            if (isa<Instruction>(v)) {
              Instruction *op = cast<Instruction>(v);
              errs() << "%" << instMap.lookup(op);
            }
            else if (v->hasName())
              errs() << v->getName();
            else
              errs() << "XXX";
            errs() << " ";
          }
          errs() << "\n";
        }
      }

      return false;  // because we have NOT changed this function
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

    // We don't modify the program, so we preserve all analyses
    virtual void getAnalysisUsage(AnalysisUsage &AU) const {
      AU.setPreservesAll();
    };

  };
  char printCode::ID = 0;

  // register the printCode class: 
  //  - give it a command-line argument (printCode)
  //  - a name ("print code")
  //  - a flag saying that we don't modify the CFG
  //  - a flag saying this is not an analysis pass
  RegisterPass<printCode> X("printCode", "print code",
			   true, false);
}
