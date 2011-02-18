#include "VTargetMachine.h"
#include "llvm/Module.h"
#include "llvm/Target/TargetRegistry.h"
using namespace llvm;
 
Target llvm::TheVBackendTarget;
 
extern "C" void LLVMInitializeVerilogTargetInfo() { 
   RegisterTarget<> X(TheVBackendTarget, "v", "Verilog backend");
}
