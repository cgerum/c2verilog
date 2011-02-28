/* -*- C++ -*- Nadav Rotem  - C-to-Verilog.com */
#ifndef VTARGETMACHINE_H
#define VTARGETMACHINE_H

#include "llvm/Target/TargetMachine.h"
#include "llvm/Target/TargetData.h"

namespace llvm {

struct VTargetMachine : public TargetMachine {
  VTargetMachine(const Target &T, const std::string &TT, const std::string &FS)
    : TargetMachine(T) {} 

  virtual bool WantsWholeFile() const { return true; }
  virtual bool addPassesToEmitFile(PassManagerBase &PM,
                                   formatted_raw_ostream &Out,
                                   CodeGenFileType FileType,
                                   CodeGenOpt::Level OptLevel,
                                   bool DisableVerify);

  // This class always works, but shouldn't be the default in most cases.
  static unsigned getModuleMatchQuality(const Module &M) { return 1; }
  
  virtual const TargetData *getTargetData() const { return 0; }
};

extern Target TheVBackendTarget;

} // End llvm namespace


#endif
