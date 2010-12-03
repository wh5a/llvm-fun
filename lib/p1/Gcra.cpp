//===-- Gcra.cpp - Graph-coloring Register Allocator --------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===--------------------------------------------------------------------===//
//
// This file does Graph-coloring Register Allocation, for CS 701 Project 4.
//
//===--------------------------------------------------------------------===//

#define DEBUG_TYPE "gcra"
#include <map>
#include "RDfact.h"
#include <stack>
#include <queue>

using namespace llvm;
using namespace std;

typedef map<const MachineBasicBlock *, set<unsigned>*> BBtoRegMap;
typedef map<const MachineInstr *, set<unsigned>*> InstrToRegMap;
typedef map<const MachineBasicBlock *, set<RDfact *>*> BBtoRDfactMap;
typedef map<const MachineInstr *, set<RDfact *>*> InstrToRDfactMap;

namespace {
  class Gcra : public MachineFunctionPass {
  private:
    const TargetRegisterInfo *TRI;
    
    static const bool DEBUG_LIVE = true;
    static const bool DEBUG_RD = true;
    static const bool PRINT_INST = false;
    
    int numRegClasses;
    
    set<RDfact *> RDfactSet;
    
    map<MachineInstr *, unsigned> InstrToNumMap;
    
    BBtoRegMap liveBeforeMap;
    BBtoRegMap liveAfterMap;
    BBtoRegMap liveVarsGenMap;
    BBtoRegMap liveVarsKillMap;
    InstrToRegMap insLiveBeforeMap;
    InstrToRegMap insLiveAfterMap;
    
    BBtoRDfactMap RDbeforeMap;
    BBtoRDfactMap RDafterMap;
    BBtoRDfactMap RDgenMap;
    BBtoRDfactMap RDkillMap;
    InstrToRDfactMap insRDbeforeMap;
    InstrToRDfactMap insRDafterMap;
    
  public:
    static char ID; // Pass identification, replacement for typeid
    
    //**********************************************************************
    // constructor
    //**********************************************************************
    Gcra() : MachineFunctionPass(&ID) {
      numRegClasses = 0;
    }
    
    //**********************************************************************
    // runOnMachineFunction
    //
    //**********************************************************************
    bool runOnMachineFunction(MachineFunction &Fn) {
      
      // get pointer to regster info, which doesn't change over this fn
      TRI = Fn.getTarget().getRegisterInfo();
      
      // INITIALIZE FOR EACH FN
      RDfactSet.clear();
      RDbeforeMap.clear();
      RDafterMap.clear();
      InstrToNumMap.clear();
      liveBeforeMap.clear();
      liveAfterMap.clear();
      liveVarsGenMap.clear();
      liveVarsKillMap.clear();
      insLiveBeforeMap.clear();
      insLiveAfterMap.clear();
      
      RDbeforeMap.clear();
      RDafterMap.clear();
      RDgenMap.clear();
      RDkillMap.clear();
      insRDbeforeMap.clear();
      insRDafterMap.clear();
      
      
      // STEP 1: get sets of regs, set of defs, set of RDfacts,
      //         instruction-to-number map
      doInit(Fn);

      // if debugging, print all instructions to stdout
      if (PRINT_INST) {
	errs() << "START INITIAL INSTRUCTIONS FOR " << Fn.getFunction()->getName()
	     << "\n";
	printInstructions(Fn);
      }
      
      // STEP 2: live analysis for all registers (fill in globals
      //         liveBeforeMap and liveAfterMap for blocks, and
      //         globals insLiveBeforeMap and insLiveAfterMapfor
      //         instructions)
      doLiveAnalysis(Fn);
      if (DEBUG_LIVE) {
	printLiveResults(Fn);
      }
      
      // STEP 3: reaching defs analysis (fill in globals RDbeforeMap and
      //         RDafterMap for blocks, and globals insRDbeforeMap and
      //         insRDafterMap for instructions)
      doReachingDefsAnalysis(Fn);
      if (DEBUG_RD) {
	printRDResults(Fn);
      }
      
      exit(0); // prevent coredump until reg alloc is implemented
      return true;
    }
    
    virtual void getAnalysisUsage(AnalysisUsage &AU) const {
      // Eliminate PHI nodes before we get the CFG.
      // This works by inserting copies into predecessor blocks.
      // So the code is no longer in SSA form.
      AU.addRequiredID(PHIEliminationID); 
      AU.addRequiredID(TwoAddressInstructionPassID);
      MachineFunctionPass::getAnalysisUsage(AU);
    }
    
  private:
    //**********************************************************************
    // doInit
    //
    // fill in
    //  RDfactSet:     set of all reaching-def facts in this function
    //  InstrToNumMap: map from instruction to unique # (for debugging)
    //**********************************************************************
    void doInit(MachineFunction &Fn) {
      // iterate over all basic blocks, all instructions in a block,
      // all operands in an instruction
      int insNum = 1;
      for (MachineFunction::iterator MFIt = Fn.begin(), MFendIt = Fn.end();
	   MFIt != MFendIt; MFIt++) {
	for (MachineBasicBlock::iterator MBBIt = MFIt->begin(),
	       MBBendIt = MFIt->end(); MBBIt != MBBendIt; MBBIt++) {
	  //*MBBIt is a MachineInstr
	  InstrToNumMap[MBBIt] = insNum;
	  insNum++;
	  int numOp = MBBIt->getNumOperands();
	  for (int i = 0; i < numOp; i++) {
	    MachineOperand MOp = MBBIt->getOperand(i);  
	    if (MOp.isReg() && MOp.getReg() && MOp.isDef()) {
	      unsigned reg = MOp.getReg();
	      // Here if this operand is
	      //  (a) a register
	      //  (b) not special reg 0
	      //  (c) a def
	      RDfactSet.insert(new RDfact(reg, MBBIt));
	      // also add new reaching-defs facts for all aliases
	      if (TargetRegisterInfo::isPhysicalRegister(reg)) {
		const unsigned *aliasSet = TRI->getAliasSet(reg);
		while (aliasSet != NULL && *aliasSet != 0) {
		  RDfactSet.insert(new RDfact(*aliasSet, MBBIt));
		  aliasSet++;
		}
	      } // end a preg, so deal with aliases
	    } // end a def of a reg
	  } // end for each operand
	} // end iterate over all instructions in 1 basic block
      } // end iterate over all basic blocks in this fn
    } // end doInit
    
    
    //**********************************************************************
    // doLiveAnalysis
    //**********************************************************************
    void doLiveAnalysis(MachineFunction &Fn) {
      // initialize live maps to empty
      liveBeforeMap.clear();
      liveAfterMap.clear();
      liveVarsGenMap.clear();
      liveVarsKillMap.clear();
      insLiveBeforeMap.clear();
      insLiveAfterMap.clear();
      
      analyzeBasicBlocksLiveVars(Fn);
      analyzeInstructionsLiveVars(Fn);
    }
    
    //**********************************************************************
    // doReachingDefsAnalysis
    //**********************************************************************
    void doReachingDefsAnalysis(MachineFunction &Fn) {
      analyzeBasicBlocksRDefs(Fn);
      analyzeInstructionsRDefs(Fn);
    }
    
    //**********************************************************************
    // analyzeBasicBlocksLiveVars
    //
    // iterate over all basic blocks bb
    //    bb.gen = all upwards-exposed uses in bb
    //    bb.kill = all defs in bb
    //    put bb on the worklist
    //**********************************************************************
    void analyzeBasicBlocksLiveVars(MachineFunction &Fn) {
      
      // initialize all before/after/gen/kill sets and
      // put all basic blocks on the worklist
      set<MachineBasicBlock *> worklist;
      for (MachineFunction::iterator MFIt = Fn.begin(), MFendIt = Fn.end();
	   MFIt != MFendIt; MFIt++) {
	liveBeforeMap[MFIt] = new set<unsigned>();
	liveAfterMap[MFIt] = new set<unsigned>();
	liveVarsGenMap[MFIt] = getUpwardsExposedUses(MFIt);
	liveVarsKillMap[MFIt] = getAllDefs(MFIt);
	worklist.insert(MFIt);
      }
      
      // while the worklist is not empty {
      //   remove one basic block bb
      //   compute new bb.liveAfter = union of liveBefore's of all successors
      //   replace old liveAfter with new one
      //   compute new bb.liveBefore = (bb.liveAfter - bb.kill) union bb.gen
      //   if bb.liveBefore changed {
      //      replace old liveBefore with new one
      //      add all of bb's predecessors to the worklist
      //   }
      // }
      while (! worklist.empty()) {
	// remove one basic block and compute its new liveAfter set
	set<MachineBasicBlock *>::iterator oneBB = worklist.begin();
	MachineBasicBlock *bb = *oneBB;
	worklist.erase(bb);
	
	set<unsigned> *newLiveAfter = computeLiveAfter(bb);
	
	// update the liveAfter map
	liveAfterMap.erase(bb);
	liveAfterMap[bb] = newLiveAfter;
	// compute its new liveBefore, see if it has changed (it can only
	// get bigger)
	set<unsigned> *newLiveBefore = computeLiveBefore(bb);
	set<unsigned> *oldLiveBefore = liveBeforeMap[bb];
	if (newLiveBefore->size() > oldLiveBefore->size()) {
	  // update the liveBefore map and put all preds of bb on worklist
	  liveBeforeMap.erase(bb);
	  liveBeforeMap[bb] = newLiveBefore;
	  for (MachineBasicBlock::pred_iterator PI = bb->pred_begin(),
		 E = bb->pred_end();
	       PI != E; PI++) {
	    worklist.insert(*PI);
	  }
	}
      }
    }
    
    //**********************************************************************
    // analyzeBasicBlocksRDefs
    //**********************************************************************
    void analyzeBasicBlocksRDefs(MachineFunction &Fn) {
      // iterate over all basic blocks bb computing
      //    bb.gen = for each reg v defined in bb at inst: the RDfact
      //             (v, inst)
      //    bb.kill = all dataflow facts with reg v
      // also put bb on the worklist
      
      set<MachineBasicBlock *> worklist;
      for (MachineFunction::iterator MFIt = Fn.begin(), MFendIt = Fn.end();
	   MFIt != MFendIt; MFIt++) {
	RDbeforeMap[MFIt] = new set<RDfact *>();
	RDafterMap[MFIt] = new set<RDfact *>();
	RDgenMap[MFIt] = getRDgen(MFIt);
	RDkillMap[MFIt] = getRDkill(MFIt);
	worklist.insert(MFIt);
      }
      
      // while the worklist is not empty {
      //   remove one basic block bb
      //   compute new bb.RDbefore = union of RDafter's of all preds
      //   replace old RDbefore with new one
      //   compute new bb.RDafter = (bb.RDbefore - bb.RDkill) union
      //                              bb.RDgen
      //   if bb.RDafter changed {
      //      replace old RDbefore with new one
      //      add all of bb's succs to the worklist
      //   }
      // }
      while (! worklist.empty()) {
	// remove one basic block and compute its new RDbefore set
	set<MachineBasicBlock *>::iterator oneBB = worklist.begin();
	MachineBasicBlock *bb = *oneBB;
	worklist.erase(bb);
	
	set<RDfact *> *newRDbefore = computeRDbefore(bb);
	
	// update the RDbefore map
	RDbeforeMap.erase(bb);
	RDbeforeMap[bb] = newRDbefore;
	// compute its new RDafter, see if it has changed (it can only
	// get bigger)
	set<RDfact *> *newRDafter = computeRDafter(bb);
	set<RDfact *> *oldRDafter = RDafterMap[bb];
	if (newRDafter->size() > oldRDafter->size()) {
	  // update the RDafter map and put all succs of bb on worklist
	  RDafterMap.erase(bb);
	  RDafterMap[bb] = newRDafter;
	  for (MachineBasicBlock::succ_iterator PI = bb->succ_begin(),
		 E = bb->succ_end();
	       PI != E; PI++) {
	    worklist.insert(*PI);
	  }
	}
      }
    }
    
    // **********************************************************************
    // computeLiveBefore
    //
    // given: bb          ptr to a MachineBasicBlock 
    //
    // do:    compute and return bb's current LiveBefore set:
    //          (bb.liveAfter - bb.kill) union bb.gen
    // **********************************************************************
    set<unsigned> *computeLiveBefore(MachineBasicBlock *bb) {
      return regSetUnion(regSetSubtract(liveAfterMap[bb],
					liveVarsKillMap[bb]
					),
			 liveVarsGenMap[bb]
			 );
    }
    
    
    // **********************************************************************
    // computeLiveAfter
    //
    // given: bb  ptr to a MachineBasicBlock 
    //
    // do:    compute and return bb's current LiveAfter set: the union
    //        of the LiveBefore sets of all of bb's CFG successors
    // **********************************************************************
    set<unsigned> *computeLiveAfter(MachineBasicBlock *bb) {
      set<unsigned> *result = new set<unsigned>();
      for (MachineBasicBlock::succ_iterator SI = bb->succ_begin();
	   SI != bb->succ_end(); SI++) {
	MachineBasicBlock *oneSucc = *SI;
	result = regSetUnion(result, liveBeforeMap[oneSucc]);
      }
      
      return result;
    }
    
    
    // **********************************************************************
    // computeRDbefore
    //
    // given: bb  ptr to a MachineBasicBlock 
    //
    // do:    compute and return bb's current RDbefore set: the union
    //        of the RDafter sets of all of bb's CFG preds
    // **********************************************************************
    set<RDfact *> *computeRDbefore(MachineBasicBlock *bb) {
      set<RDfact *> *result = new set<RDfact *>();
      for (MachineBasicBlock::pred_iterator SI = bb->pred_begin();
	   SI != bb->pred_end(); SI++) {
	MachineBasicBlock *onePred = *SI;
	result = RDsetUnion(result, RDafterMap[onePred]);
      }
      
      return result;
    }
    
    // **********************************************************************
    // computeRDafter
    //
    // given: bb          ptr to a MachineBasicBlock 
    //
    // do:    compute and return bb's current RDafter set:
    //          (bb.RDbefore - bb.kill) union bb.gen
    // **********************************************************************
    set<RDfact *> *computeRDafter(MachineBasicBlock *bb) {
      return RDsetUnion(RDsetSubtract(RDbeforeMap[bb],
				      RDkillMap[bb]
				      ),
			RDgenMap[bb]
			);
    }
    
    
    
    // **********************************************************************
    // regSetUnion
    //
    // given: S1, S2          ptrs to sets of regs
    // do:    return a ptr to (*S1 union *S2)
    // **********************************************************************
    set<unsigned> *regSetUnion(set<unsigned> *S1, set<unsigned> *S2) {
      set<unsigned> *result = new set<unsigned>();
      // iterate over S1
      for (set<unsigned>::iterator oneRegPtr = S1->begin();
	   oneRegPtr != S1->end();
	   oneRegPtr++) {
	result->insert(*oneRegPtr);
      }
      
      // iterate over S2
      for (set<unsigned>::iterator oneRegPtr = S2->begin();
	   oneRegPtr != S2->end();
	   oneRegPtr++) {
	result->insert(*oneRegPtr);
      }
      
      return result;
    }
    
    // **********************************************************************
    // RDsetUnion
    //
    // given: S1, S2          ptrs to sets of ptrs to RDfacts
    // do:    return a ptr to (*S1 union *S2)
    // **********************************************************************
    set<RDfact *> *RDsetUnion(set<RDfact *> *S1, set<RDfact *> *S2) {
      set<RDfact *> *result = new set<RDfact *>();
      // iterate over S1
      for (set<RDfact *>::iterator oneRDfact = S1->begin();
	   oneRDfact != S1->end();
	   oneRDfact++) {
	result->insert(*oneRDfact);
      }
      
      // iterate over S2
      for (set<RDfact *>::iterator oneRDfact = S2->begin();
	   oneRDfact != S2->end();
	   oneRDfact++) {
	result->insert(*oneRDfact);
      }
      
      return result;
    }
    
    
    // **********************************************************************
    // regSetSubtract
    //
    // given: S1, S2          ptrs to sets of regs
    // do:    return a ptr to (*S1 - *S2)
    //
    // **********************************************************************
    set<unsigned> *regSetSubtract(set<unsigned> *S1, set<unsigned> *S2) {
      set<unsigned> *result = new set<unsigned>();
      // iterate over S1; for each element, if it is NOT in S2, then
      // add it to the result
      for (set<unsigned>::iterator S1RegPtr = S1->begin();
	   S1RegPtr != S1->end();
	   S1RegPtr++) {
	if (S2->count(*S1RegPtr) == 0) {
	  result->insert(*S1RegPtr);
	}
      }
      
      return result;
    }
    
    // **********************************************************************
    // RDsetSubtract
    //
    // given: S1, S2          ptrs to sets of RDfact ptrs
    // do:    return a ptr to (*S1 - *S2)
    //
    // **********************************************************************
    set<RDfact *> *RDsetSubtract(set<RDfact *> *S1, set<RDfact *> *S2) {
      set<RDfact *> *result = new set<RDfact *>();
      // iterate over S1; for each element, if it is NOT in S2, then
      // add it to the result
      for (set<RDfact *>::iterator S1RegPtr = S1->begin();
	   S1RegPtr != S1->end();
	   S1RegPtr++) {
	if (S2->count(*S1RegPtr) == 0) {
	  result->insert(*S1RegPtr);
	}
      }
      
      return result;
    }

    //**********************************************************************
    // analyzeInstructionsLiveVars
    //
    // do live-var analysis at the instruction level:
    //   iterate over all basic blocks
    //   for each, iterate backwards over instructions, propagating
    //             live-var info:
    //     for each instruction inst
    //             live-before = (live-after - kill) union gen
    //     where kill is the defined reg of inst (if any) and
    //           gen is all reg-use operands of inst
    //**********************************************************************
    void analyzeInstructionsLiveVars(MachineFunction &Fn) {
      for (MachineFunction::iterator bb = Fn.begin(), bbe = Fn.end(); 
	   bb != bbe; bb++) {
	// no reverse iterator and recursion doesn't work,
	// so create vector of instructions for backward traversal
	vector<MachineInstr *> instVector;
	for (MachineBasicBlock::iterator inIt = bb->begin();
	     inIt != bb->end();
	     inIt++) {
	  instVector.push_back(inIt);
	}
	
	liveForInstr(instVector, liveAfterMap[bb]);
      }
    }
    
    //**********************************************************************
    // analyzeInstructionsRDefs
    //
    // given reaching-defs before and after facts for basic block,
    // compute before/after facts for each instruction in each basic block
    //
    // for one instruction: RDafter = (RDbefore - kill) union gen
    // where kill is all dataflow facts with the regs that are defined
    // by this instruction (if any), and gen is the set of facts (reg, inst)
    // for all regs defined by this instruction (if any)
    //**********************************************************************
    void analyzeInstructionsRDefs(MachineFunction &Fn) {
      // iterate over all basic blocks in this function
      for (MachineFunction::iterator bb = Fn.begin(), bbe = Fn.end(); 
	   bb != bbe; bb++) {
	set<RDfact *> *RDbefore = RDbeforeMap[bb];
	// iterate over all instructions in this basic block
	for (MachineBasicBlock::iterator inIt = bb->begin();
	     inIt != bb->end();
	     inIt++) {
	  insRDbeforeMap[inIt] = RDbefore;
	  set<RDfact *> *kill = new set<RDfact *>();
	  set<RDfact *> *gen = new set<RDfact *>();
	  set<unsigned> *regDefs = getOneInstrRegDefs(inIt);
	  // if at least one reg was defined
	  // then compute gen and kill sets for this instruction
	  if (regDefs->size() > 0) {
	    for (set<unsigned>::iterator regIt = regDefs->begin();
		 regIt != regDefs->end(); regIt++) {
	      unsigned oneDef = *regIt;
	      gen->insert(new RDfact(oneDef, inIt));
	      // iterate over all RDfacts, see which are killed
	      for (set<RDfact *>::iterator IT = RDfactSet.begin();
		   IT != RDfactSet.end(); IT++) {
		RDfact *oneRDfact = *IT;
		unsigned oneReg = oneRDfact->getReg();
		if (oneReg == oneDef) {
		  kill->insert(oneRDfact);
		}
	      } // end iterate over all RDfacts to compute kill
	    } // end iterate over set of regs defined by one instruction

	    // we've now defined the gen and kill sets so we can
	    // compute the "after" fact for this instruction
	    set<RDfact *> *RDafter = RDsetUnion(RDsetSubtract(RDbefore, kill),
						gen);
	    insRDafterMap[inIt] = RDafter;
	    RDbefore = RDafter;
	  } else {
	    // this instruction doesn't define any reg
	    insRDafterMap[inIt] = RDbefore;
	  }
	} // end iterate over all instructions in 1 basic block
      } // end iterate over all basic blocks
    }
    
    // **********************************************************************
    // getUpwardsExposedUses
    //
    // given: bb      ptr to a basic block
    // do:    return a ptr to the set of regs that are used before
    //        being defined in bb; include aliases!
    // **********************************************************************
    set<unsigned> *getUpwardsExposedUses(MachineBasicBlock *bb) {
      set<unsigned> *result = new set<unsigned>();
      set<unsigned> *defs = new set<unsigned>();
      for (MachineBasicBlock::iterator instruct = bb->begin(),
	     instructEnd = bb->end(); instruct != instructEnd; instruct++) {
	set<unsigned> *uses = getOneInstrRegUses(instruct);
	set<unsigned> *upUses = regSetSubtract(uses, defs);
	result = regSetUnion(result, upUses);
	set<unsigned> *defSet = getOneInstrRegDefs(instruct);
	for (set<unsigned>::iterator IT = defSet->begin();
	     IT != defSet->end(); IT++) {
	  unsigned oneDef = *IT;
	  defs->insert(oneDef);
	}
      } // end iterate over all instrutions in this basic block
      
      return result;
    }
    
    
    // **********************************************************************
    // getRDgen
    //
    // given: bb      ptr to a basic block
    // do:    return a set of reaching-def facts: the ones that occur in bb
    // **********************************************************************
    set<RDfact *> *getRDgen(MachineBasicBlock *bb) {
      set<RDfact *> *result = new set<RDfact *>();
      for (MachineBasicBlock::iterator instruct = bb->begin(),
	     instructEnd = bb->end(); instruct != instructEnd; instruct++) {
	set<unsigned> *defSet = getOneInstrRegDefs(instruct);
	for (set<unsigned>::iterator IT = defSet->begin();
	     IT != defSet->end(); IT++) {
	  unsigned oneDef = *IT;
	  result->insert(new RDfact(oneDef, instruct));
	}
      } // end iterate over all instructions in this basic block
      
      return result;
    }
    
    // **********************************************************************
    // getRDkill
    //
    // given: bb      ptr to a basic block
    // do:    return a set of reaching-def facts: the ones whose reg
    //        component is defined in bb
    // **********************************************************************
    set<RDfact *> *getRDkill(MachineBasicBlock *bb) {
      set<RDfact *> *result = new set<RDfact *>();
      for (MachineBasicBlock::iterator instruct = bb->begin(),
	     instructEnd = bb->end(); instruct != instructEnd; instruct++) {
	set<unsigned> *defSet = getOneInstrRegDefs(instruct);
	for (set<unsigned>::iterator IT = defSet->begin();
	     IT != defSet->end(); IT++) {
	  unsigned oneDef = *IT;
	  for (set<RDfact *>::iterator IT = RDfactSet.begin();
	       IT != RDfactSet.end(); IT++) {
	    RDfact *oneRDfact = *IT;
	    unsigned oneReg = oneRDfact->getReg();
	    if (oneReg == oneDef) {
	      result->insert(oneRDfact);
	    }
	  } // end iterate over all RDfacts in the whole fn
	} // end iterate over all defs in this instruction
      } // end iterate over all instructions in this basic block
      
      return result;
    }
    
    //**********************************************************************
    // getOneInstrRegUses
    //
    // return the set of registers (virtual or physical) used by the
    // given instruction, including aliases of any physical registers
    //**********************************************************************
    set<unsigned> *getOneInstrRegUses(MachineInstr *instruct) {
      set<unsigned> *result = new set<unsigned>();
      unsigned numOperands = instruct->getNumOperands();
      for (unsigned n=0; n<numOperands; n++) {
	MachineOperand MOp = instruct->getOperand(n);
	if (MOp.isReg() && MOp.getReg() && MOp.isUse()) {
	  unsigned reg = MOp.getReg();
	  if (TargetRegisterInfo::isPhysicalRegister(reg)) {
	    addAliases(result, reg);
	  }
	  result->insert(reg);
	}
      } // end for each operand of current instruction
      return result;
    }
    
    //**********************************************************************
    // getOneInstrRegDefs
    //
    // return a ptr to a set of the registers defined by this instruction
    // including aliases
    //**********************************************************************
    set<unsigned> *getOneInstrRegDefs(MachineInstr *instruct) {
      set<unsigned> *result = new set<unsigned>();
      unsigned numOperands = instruct->getNumOperands();
      for (unsigned n=0; n<numOperands; n++) {
	MachineOperand MOp = instruct->getOperand(n);
	if (MOp.isReg() && MOp.getReg() && MOp.isDef()) {
	  unsigned reg = MOp.getReg();
	  addAliases(result, reg);
	  result->insert(reg);
	}
      } // end for each operand of current instruction
      return result;
    }
    
    // **********************************************************************
    // getAllDefs
    //
    // given: bb      ptr to a basic block
    // do:    return the set of regs that are defined in bb
    // **********************************************************************
    set<unsigned> *getAllDefs(MachineBasicBlock *bb) {
      set<unsigned> *result = new set<unsigned>();
      
      // iterate over all instructions in bb
      //   for each operand that is a non-zero reg:
      //     if it is a def then add it to the result set
      // return result
      // 
      for (MachineBasicBlock::iterator instruct = bb->begin(),
	     instructEnd = bb->end(); instruct != instructEnd; instruct++) {
	unsigned numOperands = instruct->getNumOperands();
	for (unsigned n=0; n<numOperands; n++) {
	  MachineOperand MOp = instruct->getOperand(n);
	  if (MOp.isReg() && MOp.getReg() && MOp.isDef()) {
	    result->insert(MOp.getReg());
	  }
	} // end for each operand of current instruction
      } // end iterate over all instrutions in this basic block
      return result;
    }
    
    // **********************************************************************
    // liveForInstr
    //
    // given: instVector vector of ptrs to Instructions for one basic block
    //        liveAfter  live after set for the *last* instruction in the block
    //
    // do:    compute and set liveAfter and liveBefore for each instruction
    //        liveAfter = liveBefore of next instruction
    //        liveBefore = (liveAfter - kill) union gen
    // **********************************************************************
    void liveForInstr(vector<MachineInstr *>instVector,
		      set<unsigned> *liveAfter) {
      while (instVector.size() > 0) {
	MachineInstr *oneInstr = instVector.back();
	instVector.pop_back();
	insLiveAfterMap[oneInstr] = liveAfter;
	
	// create liveBefore for this instruction
	// (which is also liveAfter for the previous one in the block)
	//   remove the reg defined here (if any) from the set
	//   then add all used reg operands
	
	set<unsigned> *liveBefore;
	set<unsigned> *gen = getOneInstrRegUses(oneInstr);
	set<unsigned> *kill = getOneInstrRegDefs(oneInstr);
	if (kill->size() != 0) {
	  liveBefore = regSetUnion(regSetSubtract(liveAfter, kill), gen);
	} else {
	  liveBefore = regSetUnion(liveAfter, gen);
	}
	
	// add this instruction's liveBefore set to the map
	// and prepare for the next iteration of the loop
	insLiveBeforeMap[oneInstr] = liveBefore;
	liveAfter = liveBefore;
      } // end while
    }
    
    //**********************************************************************
    // addAliases
    //
    // given: ptr to set of registers
    //        one reg
    //
    // do: add all aliases of reg to set (only a preg has aliases)
    //**********************************************************************
    void addAliases(set<unsigned> *S, unsigned reg) {
      if (TargetRegisterInfo::isPhysicalRegister(reg)) {
	const unsigned *aliasSet = TRI->getAliasSet(reg);
	while (aliasSet != NULL && *aliasSet != 0) {
	  S->insert(*aliasSet);
	  aliasSet++;
	}
      }      
    }
    
    // **********************************************************************
    // printInstructions
    // **********************************************************************
    void printInstructions(MachineFunction &F) {
      // iterate over all basic blocks
      for (MachineFunction::iterator bb = F.begin(); bb != F.end(); bb++) {
	// iterate over instructions, printing each
	errs() << "Basic Block " << bb->getNumber() << "\n";
	for (MachineBasicBlock::iterator inIt = bb->begin(), ine = bb->end();
	     inIt != ine; inIt++) {
	  MachineInstr *oneI = inIt;
	  errs() << "%" << InstrToNumMap[oneI] << "( " << oneI << "): ";
	  errs() << *oneI << "\n";
	}
      }
    }
    
    
    // **********************************************************************
    // printLiveResults
    //
    // given: MachineFunction F
    //
    // do:    for each basic block in F {
    //           print fn name, bb number, liveBefore and After sets
    //           for each instruction, print instruction num, liveBefore and
    //               liveAfter
    //        }
    // 
    // **********************************************************************
    void printLiveResults(MachineFunction &F) {
      errs() << "\nLIVE VARS\n";
      
      // iterate over all basic blocks
      for (MachineFunction::iterator bb = F.begin(); bb != F.end(); bb++) {
	// print number of basic block
	errs() << "BASIC BLOCK #" << bb->getNumber();
	// print live before and after sets
	errs() << "  L-Before: ";
	printRegSet(liveBeforeMap[bb]);
	errs() << "  L-After: ";
	printRegSet(liveAfterMap[bb]);
	errs() << "\n";
	
	// iterate over instructions, printing each live set
	// (note that liveAfter of one instruction is liveBefore of the next one)
	for (MachineBasicBlock::iterator inIt = bb->begin(), ine = bb->end();
	     inIt != ine; inIt++) {
	  errs() << "%" << InstrToNumMap[inIt] << ": ";
	  errs() << " L-Before: ";
	  printRegSet(insLiveBeforeMap[inIt]);
	  errs() << "\tL-After: ";
	  printRegSet(insLiveAfterMap[inIt]);
	  errs() << "\n";
	}
      }
    }
    
    // **********************************************************************
    // printRDResults
    //
    // given: MachineFunction F
    //
    // do:    for each basic block in F {
    //           print fn name, bb number, RDBefore and After sets
    //           for each instruction, print instruction num, RDBefore and
    //               RDAfter
    //        }
    // 
    // **********************************************************************
    void printRDResults(MachineFunction &F) {
      errs() << "\n";
      
      // iterate over all basic blocks
      for (MachineFunction::iterator bb = F.begin(); bb != F.end(); bb++) {
	// print number of basic block
	errs() << "BASIC BLOCK #" << bb->getNumber();
	// print RD before and after sets
	errs() << "  RD-Before: ";
	printRDSet(RDbeforeMap[bb]);
	errs() << "  RD-After: ";
	printRDSet(RDafterMap[bb]);
	errs() << "\n";
	
	// iterate over instructions, printing each RD set
	// (note that RDAfter of one instruction is RDBefore of the next one)
	for (MachineBasicBlock::iterator inIt = bb->begin(), ine = bb->end();
	     inIt != ine; inIt++) {
	  errs() << "%" << InstrToNumMap[inIt] << ": ";
	  errs() << " RD-Before: ";
	  printRDSet(insRDbeforeMap[inIt]);
	  errs() << "\nRD-After: ";
	  printRDSet(insRDafterMap[inIt]);
	  errs() << "\n";
	}
      }
    }
    
    // **********************************************************************
    // printRegSet
    //
    // given: S      ptr to set of regs (unsigned)
    // do:    print the set
    // ********************************************************************
    void printRegSet(set<unsigned> *S) {
      errs() << "{";
      for (set<unsigned>::iterator IT = S->begin(); IT != S->end(); IT++) {
	unsigned reg = *IT;
	errs() << " " << reg;
      }
      errs() << " }\n";
    }
      
    // **********************************************************************
    // printRegSetWithAliases
    //
    // given: S      ptr to set of regs (unsigned)
    // do:    print the set
    // ********************************************************************
    void printRegSetWithAliases(set<unsigned> *S) {
      errs() << "{";
      const unsigned *aliasSet;
      set<unsigned> aliases;
      for (set<unsigned>::iterator IT = S->begin(); IT != S->end(); IT++) {
	unsigned reg = *IT;
	errs() << " " << reg;
	if (TargetRegisterInfo::isPhysicalRegister(reg)) {
	  aliasSet = TRI->getAliasSet(reg);      
	  while (aliasSet != NULL && *aliasSet != 0) {
	    aliases.insert(*aliasSet);
	    aliasSet++;
	  }
	}
      }
      errs() << " }\n";
      errs() << "ALIASES: {";
      for(set<unsigned>::iterator IT = aliases.begin(); IT != aliases.end();
	  IT++) {
	errs() << " " << *IT;
      }
      errs() << "}\n";
    }
      
    // **********************************************************************
    // printRDSet
    //
    // given: S      ptr to set of RDfact
    // do:    print the set
    // **********************************************************************
    void printRDSet(set<RDfact *> *S) {
      errs() << "{";
      for (set<RDfact *>::iterator IT = S->begin(); IT != S->end(); IT++) {
	RDfact *oneRDfact = *IT;
	MachineInstr *oneIns = oneRDfact->getInstr();
	errs() << "(" << oneRDfact->getReg() << ", %"
	     << InstrToNumMap[oneIns] << ") ";
      }
      errs() << " }";
    }
    
    //**********************************************************************
    // getDefReg
    //
    // given: instr  ptr to MachineInstr
    // do:    return the reg defined there
    //**********************************************************************
    unsigned getDefReg(MachineInstr *instr) {
      int numOp = instr->getNumOperands();
      for (int i = 0; i < numOp; i++) {
	MachineOperand MOp = instr->getOperand(i);  
	if (MOp.isReg() && MOp.getReg() 
	    && MOp.isDef()) {
	  return MOp.getReg();
	}
      }
      errs() << "INTERNAL ERROR: NO DEFINED REG IN getDefReg\n";
      exit(1);
    }
    
    //**********************************************************************
    // member
    // given: oneFact  ptr to an RDfact
    //        S        ptr to set of RDfact *
    //
    // do:    return true iff oneFact is in S
    //**********************************************************************
    bool member(RDfact *oneFact, set<RDfact *> *S) {
      for (set<RDfact *>::iterator IT = S->begin(); IT != S->end(); IT++) {
	RDfact *curr = *IT;
	if ((curr->getReg() == oneFact->getReg()) &&
	    (curr->getInstr() == oneFact->getInstr())) return true;
      }
      return false;
    }
    
    //**********************************************************************
    // printRegSet
    //**********************************************************************
    void printRegSet(set<unsigned> S) {
      for (set<unsigned>::iterator IT = S.begin(); IT != S.end(); IT++) {
	unsigned reg = *IT;
	errs() << reg << " ";
      }
    }
    
  };
  
  // The library-inclusion mechanism requires the following runes:
  char Gcra::ID = 0;
  
  FunctionPass *createGcra() { return new Gcra(); }
  
  static RegisterRegAlloc register_gcra("gc",
					"graph-coloring register allocator",
					createGcra);
}
