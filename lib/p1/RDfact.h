//**********************************************************************
// A RDfact is a pair (unsigned reg, MachineInstr * inst).
// reg is the variable defined and inst is the instruction where it is
// defined
//**********************************************************************

#include "llvm/CodeGen/Passes.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/CodeGen/MachineLoopInfo.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/RegAllocRegistry.h"
//#include "llvm/CodeGen/VirtRegMap.h"
#include "llvm/Target/TargetInstrDesc.h"
#include "llvm/Target/TargetInstrInfo.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/Compiler.h"
#include "llvm/ADT/Statistic.h"

using namespace std;
using namespace llvm;

class RDfact {
public:

  // constructor
  RDfact(unsigned reg, MachineInstr *inst);
  unsigned getReg();
  MachineInstr *getInstr();

private:
  unsigned myReg;
  MachineInstr *myInstr;

};
