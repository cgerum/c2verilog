/* Nadav Rotem  - C-to-Verilog.com */
#include "llvm/Pass.h"
#include "llvm/Function.h"
#include "llvm/Instructions.h"
#include "llvm/Constants.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/Support/Streams.h"
#include "llvm/Module.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/Support/CFG.h"
#include "llvm/Analysis/FindUsedTypes.h" 
#include "llvm/DerivedTypes.h"
#include "llvm/Support/InstIterator.h"

#include <algorithm>
#include <sstream>

#include "../utils.h"
#include "subscriptDetect.h" 

namespace xVerilog {
    
    void subscriptDetect::incrementAccessIndex(Value* inst, int offset) {
        Value* exp; 
        Value* ld = isLoadSubscript(inst);
        Value* st = isStoreSubscript(inst);
        exp = (ld? ld : (st? st: NULL ));
        assert(exp && "Inst is nither Load or Store array subscript");
        GetElementPtrInst *ptr = dyn_cast<GetElementPtrInst>(exp);
        assert(ptr && "ptr is not of GetElementPtrInst type");
        // Pointer to the value that the GetElementPtrInst uses as index of the array
        Value* index = ptr->getOperand(1);
        cerr<<"DBG: working on index"<<*index;  
       
        // In this case, change the 'ADD' instruction to reflect the offset
        if (BinaryOperator* bin = dyn_cast<BinaryOperator>(index)) {
            unsigned int bitWidth = cast<IntegerType>(index->getType())->getBitWidth();

            // if this index is subtracted then we need to negate it first. A[i-(constant+offset)];
            if (bin->getOpcode() == Instruction::Sub) {
                offset=-offset;
            }

            if (bin->getOpcode() == Instruction::Add || bin->getOpcode() == Instruction::Sub) {
                for (int param_num=0; param_num<2;param_num++) {
                    if (ConstantInt *con = dyn_cast<ConstantInt>(bin->getOperand(param_num))) {
                        unsigned int current_off = con->getValue().getZExtValue();
                        // create a new constant
                        ConstantInt *ncon = ConstantInt::get(APInt(bitWidth, offset + current_off));
                        bin->setOperand(param_num,ncon);
                    }
                }
            } else {
                // we can't modify an existing node. Create a new add.
                addOffsetToGetElementPtr(ptr,offset);
            }

        } else if (isInductionVariable(index)) {
            // Else, we add an 'ADD' instruction for the PHI variable
            addOffsetToGetElementPtr(ptr,offset);
        } else assert(0 && "unknown type of index for array");


    }

    void subscriptDetect::addOffsetToGetElementPtr(Value *inst, int offset) {
        GetElementPtrInst* ptr = dyn_cast<GetElementPtrInst>(inst);
        assert(ptr && "inst is not of GetElementPtrInst type");
        Value* index = ptr->getOperand(1);
        // what's the bitwidth of our index?
        unsigned int bitWidth = cast<IntegerType>(index->getType())->getBitWidth();
        // New add instruction, place it before the GetElementPtrInst 
        BinaryOperator* newIndex = BinaryOperator::Create(Instruction::Add, 
                ConstantInt::get(APInt(bitWidth, offset)) , index, "i_offset", ptr);
        // Tell GetElementPtrInst to use it instead of the normal PHI
        ptr->setOperand(1,newIndex); 
    }

    bool subscriptDetect::isInductionVariable(Value *inst) {
        return ( m_inductionVariables.find(inst) != m_inductionVariables.end() );
    }

    bool subscriptDetect::isModifiedInductionVariable(Value *inst) {
        if (BinaryOperator *calc = dyn_cast<BinaryOperator>(inst)) {
            Value *v0 = (calc->getOperand(0));
            Value *v1 = (calc->getOperand(1));
            if ( isInductionVariable(v0) || isInductionVariable(v1)) {
                return true;
            }
        }
        return false;
    }

    Value* subscriptDetect::isAddressCalculation(Value *inst) {
        if (GetElementPtrInst *ptr = dyn_cast<GetElementPtrInst>(inst)) {
            Value *v = (ptr->getOperand(1));
            if (isModifiedInductionVariable(v) || isInductionVariable(v)) {
                return ptr;
            }
        }
        return NULL;
    }

    Value* subscriptDetect::isLoadSubscript(Value *inst) {
        if (LoadInst *ptr = dyn_cast<LoadInst>(inst)) {
            Value *v = ptr->getOperand(0);
            return isAddressCalculation(v);
        }
        return NULL;
    }

    Value* subscriptDetect::isStoreSubscript(Value *inst) {
        if (StoreInst *ptr = dyn_cast<StoreInst>(inst)) {
            Value *v = ptr->getOperand(1);
            return isAddressCalculation(v);         
        }
        return NULL;
    }

    bool subscriptDetect::runOnFunction(Function &F) {
        LoopInfo *LInfo = &getAnalysis<LoopInfo>();

        // for each BB, find it's loop
        for (Function::const_iterator I = F.begin(), E = F.end(); I!=E; ++I) {
            if (Loop *L = LInfo->getLoopFor(I)) {
                PHINode* inductionVar = L->getCanonicalInductionVariable();
                if (inductionVar) { 
                    m_inductionVariables.insert(inductionVar);
                }
            }
        }

        // F is a pointer to a Function instance
        for (inst_iterator i = inst_begin(F), e = inst_end(F); i != e; ++i) {
            Instruction *inst = &*i;
            if (isStoreSubscript(inst)) {
                cerr<<"Found Store "<<*inst;
                m_StoreSubscripts.insert(inst);
            } else if (isLoadSubscript(inst)) {
                cerr<<"Found Load "<<*inst;
                m_LoadSubscripts.insert(inst);
            }
        }

        return false;
    }

    char subscriptDetect::ID = 0;
    RegisterPass<subscriptDetect> ADXX("detect_arrays", "Detect arrays in the code");

} //namespace


