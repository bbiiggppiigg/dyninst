/*
 * Copyright (c) 1996-2004 Barton P. Miller
 * 
 * We provide the Paradyn Parallel Performance Tools (below
 * described as "Paradyn") on an AS IS basis, and do not warrant its
 * validity or performance.  We reserve the right to update, modify,
 * or discontinue this software at any time.  We shall have no
 * obligation to supply such updates or modifications or any other
 * form of support to you.
 * 
 * This license is for research uses.  For such uses, there is no
 * charge. We define "research use" to mean you may freely use it
 * inside your organization for whatever purposes you see fit. But you
 * may not re-distribute Paradyn or parts of Paradyn, in any form
 * source or binary (including derivatives), electronic or otherwise,
 * to any other organization or entity without our permission.
 * 
 * (for other uses, please contact us at paradyn@cs.wisc.edu)
 * 
 * All warranties, including without limitation, any warranty of
 * merchantability or fitness for a particular purpose, are hereby
 * excluded.
 * 
 * By your use of Paradyn, you understand and agree that we (or any
 * other person or entity with proprietary rights in Paradyn) are
 * under no obligation to provide either maintenance services,
 * update services, notices of latent defects, or correction of
 * defects for Paradyn.
 * 
 * Even if advised of the possibility of such damages, under no
 * circumstances shall we (or any other person or entity with
 * proprietary rights in the software licensed hereunder) be liable
 * to you or any third party for direct, indirect, or consequential
 * damages of any character regardless of type of action, including,
 * without limitation, loss of profits, loss of use, loss of good
 * will, or computer failure or malfunction.  You agree to indemnify
 * us (and any other person or entity with proprietary rights in the
 * software licensed hereunder) for any and all liability it may
 * incur to third parties resulting from your use of Paradyn.
 */

// $Id: inst.h,v 1.104 2008/09/15 18:37:49 jaw Exp $

#ifndef INST_HDR
#define INST_HDR

#include <string>
#include "common/h/Vector.h"
#include "common/h/Dictionary.h"
#include "opcode.h" // enum opCode now defined here.
#include "common/h/Types.h"
#include "arch.h" // codeBufIndex_t 
#include "dyninstAPI/src/ast.h" // astNodePtr

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/

class instPoint;
class miniTramp;
class baseTramp;
class process;
class int_function;
class metricFocusNode;
class codeGen;
class registerSpace;
class AddressSpace;

typedef enum { callNoArgs, callRecordType, callFullArgs } callOptions;
typedef enum { callPreInsn, callPostInsn, callBranchTargetInsn, callUnset } callWhen;
typedef enum { orderFirstAtPoint, orderLastAtPoint } callOrder;

extern pdvector<Address> getTrampAddressesAtPoint(process *proc, 
                                                  const instPoint *loc,
                                                  callWhen when);

class AstNode;

/*
 * Insert instrumentation at the specified codeLocation.
 * TODO: make these methods of class process
 */

// writes to (*mtInfo)
#if 0
loadMiniTramp_result loadMergedTramp(miniTramp *&mtHandle,
                                   process *proc, 
                                   instPoint *&location,
                                   AstNodePtr ast, // the ast could be changed 
                                   callWhen when, callOrder order, bool noCost,
                                   returnInstance *&retInstance,
                                   bool trampRecursiveDesired = false,
                                   bool allowTramp = true);
#endif
/* Utility functions */

int getPointCost(process *proc, const instPoint *point);


/* return the function asociated with a point. */
int_function *getFunction(instPoint *point);

/*
 * struct to define a list of inst requests 
 *
 */
#define FUNC_ENTRY      0x1             /* entry to the function */
#define FUNC_EXIT       0x2             /* exit from function */
#define FUNC_CALL       0x4             /* subroutines called from func */
#define FUNC_ARG  	0x8             /* use arg as argument */

// Container class for "instrument this point with this function". 
// What I want to know is who is allergic to multi-letter arguments? Yeesh.
class instMapping {

   public:

      instMapping(const std::string f, const std::string i, const int w, 
            callWhen wn, callOrder o, AstNodePtr a = AstNodePtr(), std::string l = "")
         : func(f), inst(i), lib(l),
         where(w), when(wn), order(o), useTrampGuard(true),
         mt_only(false), allow_trap(false) {
            if (a != AstNodePtr()) args.push_back(a);
         }

      instMapping(const std::string f, const std::string i, const int w, 
            AstNodePtr a = AstNodePtr(), std::string l = "")
         : func(f), inst(i), lib(l),
         where(w), when(callPreInsn), order(orderLastAtPoint),
         useTrampGuard(true), mt_only(false), allow_trap(false) {
            if (a != AstNodePtr()) args.push_back(a);
         }

      instMapping(const std::string f, const std::string i, const int w, 
            pdvector<AstNodePtr> &aList, std::string l = "") :
         func(f), inst(i), lib(l),
         where(w), when(callPreInsn), order(orderLastAtPoint),
         useTrampGuard(true), mt_only(false), allow_trap(false) {
            for(unsigned u=0; u < aList.size(); u++) {
               if (aList[u] != AstNodePtr()) args.push_back(aList[u]);
            }
         };

      // Fork
      instMapping(const instMapping *parMapping, process *child);

  ~instMapping() {
  }

public:
  void dontUseTrampGuard() { useTrampGuard = false; }
  void markAs_MTonly() { mt_only = true; }
  void canUseTrap(bool t) { allow_trap = t; }
  bool is_MTonly() { return mt_only; }

  std::string func;                 /* function to instrument */
  std::string inst;                 /* inst. function to place at func */
  std::string lib;                  /* library name */
  int where;                   /* FUNC_ENTRY, FUNC_EXIT, FUNC_CALL */
  callWhen when;               /* callPreInsn, callPostInsn */
  callOrder order;             /* orderFirstAtPoint, orderLastAtPoint */
  pdvector<AstNodePtr> args;      /* what to pass as arg0 ... n */
  bool useTrampGuard;
  bool mt_only;
  bool allow_trap;
  pdvector<miniTramp *> miniTramps;
};


/*
 * get information about the cost of primitives.
 *
 */
void initPrimitiveCost();
void initDefaultPointFrequencyTable();

//
// Return the expected runtime of the passed function in instruction times.
//
unsigned getPrimitiveCost(const std::string &name);

/*
 * Generate an instruction.
 * Previously this was handled by the polymorphic "emit" function, which
 * took a variety of argument types and variously returned either an
 * Address or a Register or nothing of value.  The following family of
 * functions replace "emit" with more strongly typed versions.
 */

// The return value is a magic "hand this in when we update" black box;
// emitA handles emission of things like ifs that need to be updated later.
codeBufIndex_t emitA(opCode op, Register src1, Register src2, Register dst, 
                     codeGen &gen, bool noCost);

// for operations requiring a Register to be returned
// (e.g., getRetValOp, getParamOp, getSysRetValOp, getSysParamOp)
Register emitR(opCode op, Register src1, Register src2, Register dst, 
               codeGen &gen, bool noCost, 
               const instPoint *location, bool for_multithreaded);

// for general arithmetic and logic operations which return nothing
void     emitV(opCode op, Register src1, Register src2, Register dst, 
               codeGen &gen, bool noCost, 
               registerSpace *rs = NULL, int size = 4,
               const instPoint * location = NULL, AddressSpace * proc = NULL);

// for loadOp and loadConstOp (reading from an Address)
void     emitVload(opCode op, Address src1, Register src2, Register dst, 
                   codeGen &gen, bool noCost, 
                   registerSpace *rs = NULL, int size = 4, 
                   const instPoint * location = NULL, AddressSpace * proc = NULL);

// for storeOp (writing to an Address)
void     emitVstore(opCode op, Register src1, Register src2, Address dst, 
                    codeGen &gen, bool noCost, 
                    registerSpace *rs = NULL, int size = 4, 
                    const instPoint * location = NULL, AddressSpace * proc = NULL);

// and the retyped original emitImm companion
void     emitImm(opCode op, Register src, RegValue src2imm, Register dst, 
                 codeGen &gen, bool noCost,
                 registerSpace *rs = NULL);


//#include "dyninstAPI/h/BPatch_memoryAccess_NP.h"
class BPatch_addrSpec_NP;
typedef BPatch_addrSpec_NP BPatch_countSpec_NP;

// Don't need the above: countSpec is typedefed to addrSpec

void emitJmpMC(int condition, int offset, codeGen &gen);

void emitASload(const BPatch_addrSpec_NP *as, Register dest, codeGen &gen, bool noCost);

void emitCSload(const BPatch_countSpec_NP *as, Register dest, codeGen &gen, bool noCost);

// VG(11/06/01): moved here and added location
Register emitFuncCall(opCode op, codeGen &gen,
                      pdvector<AstNodePtr> &operands,
					  bool noCost, 
                      int_function *func);

// Obsolete version that uses an address. DON'T USE THIS or expect it to survive.
Register emitFuncCall(opCode op, codeGen &gen,
                      pdvector<AstNodePtr> &operands, 
		      		  bool noCost, 
                      Address callee_addr_);

int getInsnCost(opCode t);

/*
 * get the requested parameter into a register.
 *
 */
Register getParameter(Register dest, int param);

extern std::string getProcessStatus(const AddressSpace *p);

// TODO - what about mangled names ?
// expects the symbol name advanced past the underscore
extern unsigned findTags(const std::string funcName);

extern Address getMaxBranch();

// find these internal functions before finding any other functions
// extern dictionary_hash<std::string, unsigned> tagDict;
extern dictionary_hash <std::string, unsigned> primitiveCosts;

bool writeFunctionPtr(AddressSpace *p, Address addr, int_function *f);

/**
 * A set of optimized emiters for common idioms.  Return 
 * false if the platform can't perform any optimizations.
 **/
//Store constant in memory at address
bool emitStoreConst(Address addr, int imm, codeGen &gen, bool noCost);
//Add constant to memory at address
bool emitAddSignedImm(Address addr, long int imm, codeGen &gen, bool noCost);
//Subtract constant from memory at address
bool emitSubSignedImm(Address addr, long int imm, codeGen &gen, bool noCost);

#endif
