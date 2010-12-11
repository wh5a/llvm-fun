// Demo "Register Allocator" for LLVM.
// Author: Lang Hames
// Date: 2009-10-19
// (Half-)Tested against LLVM revision 84462.
//
//
// This demo "allocator" will be called by the pass manager to perform register
// allocation for MachineFunction objects.
//
// It iterates over all basic blocks in the function, and for each basic block
// over all instructions.
// For each instruction it will print out the list of explicit register
// operands, whether the register is physical or not and,
//  a) if it's physical its name and class
//  b) if it's virtual its number, class name, and the names of the allocable
//     physregs in that class.
// 
// At present, since it does no actual allocation, it will terminate compilation
// with an error after handling the first function.
//
// To "install" just drop this file into
//
//   lib/CodeGen/
//
// And follow the instructions at the bottom of this file to register the pass.
//
// The demo allocator will be available in llc using the switch
//
//  -regalloc=demo
//


#include "llvm/Function.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/RegAllocRegistry.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Target/TargetRegisterInfo.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/Target/TargetInstrInfo.h"
#include "llvm/CodeGen/MachineFrameInfo.h"

using namespace llvm;

namespace {

  class DemoRegAlloc : public MachineFunctionPass {
  public:

    static char ID;

    DemoRegAlloc() : MachineFunctionPass(&ID) {}

    // Let LLVM know what passes this allocator requires.
    virtual void getAnalysisUsage(AnalysisUsage &au) const {
      // You'll probably want something like
      // au.addRequired<LiveIntervals>(); 
      // here, but for now we're not using anything.
      MachineFunctionPass::getAnalysisUsage(au);
    }

    virtual bool runOnMachineFunction(MachineFunction &mf) {
      DenseMap<unsigned, unsigned> regMap;  // maps from virtual reg to stack slot

      errs() << "\"Register Allocating\" for function "
             << mf.getFunction()->getName() << "\n";      

      // Grab pointers to the register info classes.
      MachineRegisterInfo *mri = &mf.getRegInfo();
      const TargetRegisterInfo *tri = mf.getTarget().getRegisterInfo();

      const TargetInstrInfo *TII = mf.getTarget().getInstrInfo();

      // Iterate over the basic blocks in the machine function.
      for (MachineFunction::iterator mbbItr = mf.begin(), mbbEnd = mf.end();
           mbbItr != mbbEnd; ++mbbItr) {

        MachineBasicBlock &mbb = *mbbItr;

        errs() << "bb" << mbb.getNumber() << ":\n";

        // Iterate over the instructions in the basic block.
        for (MachineBasicBlock::iterator miItr = mbb.begin(), miEnd = mbb.end();
             miItr != miEnd; ++miItr) {

          MachineInstr &mi = *miItr;

          errs() << "*  " << mi;

          // We employ a simple stack-based design:
          // 1. For each instruction, always allocate physreg 27/EAX for def, and 29/EBX, 38/ECX for use.
          // 2. Load the used virtregs from stack slot into EBP and EBX.
          // 3. Copy EAX to a newly allocated stack slot after the instruction
          unsigned defReg = 27, useReg = 29;

          // Iterate over the operands in the machine function.
          for (unsigned i = 0; i < mi.getNumOperands(); ++i) {
            MachineOperand &mo = mi.getOperand(i);

            // MachineOperand::isReg lets us know if this is a register operand,
            // as opposed to a memory operand, immediate, etc.
            if (!mo.isReg())
              continue;

            // Register number 0 is reserved. It doesn't represent a real
            // register (of any kind) and we can't get any info out of it.
            if (mo.getReg() == 0)
              continue;

            if (mo.isUse())
              errs() << "    U  ";
            else
              errs() << "    D  ";

            // Grab the register number.
            unsigned reg = mo.getReg();

            // Test whether it's a physreg.
            if (TargetRegisterInfo::isPhysicalRegister(reg)) {
              // It's a physreg - handle it.
              // Note we query TargetRegisterInfo for physregs.
              const TargetRegisterClass *trc =
                tri->getPhysicalRegisterRegClass(reg);
              errs() << "physical register " << tri->getName(reg)
                     << " with class " << trc->getName() << "\n";
            }
            else {
              // Not a physreg. Must be a virtreg.
              // Query MachineRegisterInfo for virtregs.
              const TargetRegisterClass *trc = mri->getRegClass(reg);
              errs() << "virtual register %reg" << reg
                     << " with class " << trc->getName()
                     << " and allocable set { ";
              
              // Iterate over the allocable regs for this virtreg.
              for (TargetRegisterClass::iterator
                     rItr = trc->allocation_order_begin(mf),
                     rEnd = trc->allocation_order_end(mf);
                   rItr != rEnd; ++rItr) {
                unsigned preg = *rItr;
                errs() << preg << tri->getName(preg) << " ";
              }

              errs() << "}\n";

              if (mo.isUse()) {
                int frameIndex = regMap[reg];
                TII->loadRegFromStackSlot(mbb, mi, useReg, frameIndex, trc);
                mo.setReg(useReg);
                useReg = 38;  // ECX
              }
              else {
                mo.setReg(defReg);
                MachineBasicBlock::iterator nextI(mi);
                ++nextI;
                int frameIndex = mf.getFrameInfo()->CreateSpillStackObject(trc->getSize(), trc->getAlignment());
                TII->storeRegToStackSlot(mbb, nextI, defReg, true, frameIndex, trc);
                regMap[reg] = frameIndex;
              }
            }
          }
        }
        // Chuck an extra space in between each basic block.
        errs() << "\n";
      }

      return false;
    }

  };

  char DemoRegAlloc::ID;

}

// The following few lines register the register allocator and provide a
// construction function. To get this to work you'll want to add a reference to
//
// FunctionPass* createDemoRegisterAllocator();
//
// To the files:
//
//  include/llvm/CodeGen/Passes.h
//  include/llvm/CodeGen/LinkAllCodeGenComponents.h

FunctionPass* createDemoRegisterAllocator() {
  return new DemoRegAlloc();
}

RegisterRegAlloc
registerDemoRegAlloc(
              "demo",
              "Prints instrs and reg operand info. DOES NOT ACTUALLY ALLOCATE.",
              createDemoRegisterAllocator);
