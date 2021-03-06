/* Nadav Rotem  - C-to-Verilog.com */
#include "globalVarsRegistry.h"

namespace xVerilog {

    /// static variables
    Module* globalVarRegistry::m_module;
    map<string, GlobalVariable*> globalVarRegistry::m_map;
    vector<Instruction*> globalVarRegistry::m_garbage;

    /// initial values
    Value* Zero1 = 0;
    Value* One1 = 0; 
    Value* Zero32 = 0; 
    Value* One32 = 0; 

   
    void globalVarRegistry::destroy() {
        // destroy all variables that should be destroied
        // We do that so that we don't have dead values with users
        // flying around

        // destroy all keys in hash table (all global variables)
        for( map<string, GlobalVariable*>::iterator it = m_map.begin();
                it != m_map.end(); ++it ) {
            // Replace all used of this variable with a null pointer
            it->second->replaceAllUsesWith(ConstantPointerNull::get(it->second->getType()));
            // Remove this global variable from its parent module
            it->second->removeFromParent();
            // Delete it
            delete it->second;
        }

        // For each load instruction that we have modified
        for (vector<Instruction*>::iterator it = m_garbage.begin(); it!=m_garbage.end();it++) {
            // Replace the users of this dummy instruction with zero;
            if (!(*it)->hasNUses(0)) (*it)->replaceAllUsesWith(ConstantInt::get((*it)->getType(),0));
            // And delete it
            delete *it;
        }
    }

    const Type* globalVarRegistry::bitNumToType(int bitnum){
        if (bitnum==64) return Type::getInt64Ty(m_module->getContext());
        if (bitnum==32) return Type::getInt32Ty(m_module->getContext());
        if (bitnum==16) return Type::getInt16Ty(m_module->getContext());
        if (bitnum==8) return Type::getInt8Ty(m_module->getContext());
        if (bitnum==1) return Type::getInt1Ty(m_module->getContext());
        std::cerr<<"Unsupported bit addressing mode; "<<bitnum<<"\n";
        abort();
    }

    GlobalVariable* globalVarRegistry::getGlobalVariableByName(string varName, 
            int bits, bool pointer ,int val) {
        if (m_map[varName] != NULL) return m_map[varName];
        GlobalVariable *glob;  
        if (pointer) {
            glob = new GlobalVariable(llvm::PointerType::get((bitNumToType(bits)),0),false,
                    GlobalValue::ExternalLinkage,0,varName,m_module);
            //glob = new GlobalVariable(llvm::PointerType::get((bitNumToType(bits))),false,
            //        GlobalValue::ExternalLinkage,0,varName,m_module);
        } else { /* int */
            const Type *t = bitNumToType(bits); 
            glob = new GlobalVariable(t, false,
                    GlobalValue::ExternalLinkage,0,varName,m_module);
        }

        m_map[varName] = glob;
        return glob;
    } 

    GlobalVariable* globalVarRegistry::getGlobalVariableByName(string varName, const Type* type) {
        if (m_map[varName] != NULL) { 
                return m_map[varName];
        }
        GlobalVariable *glob = new GlobalVariable(type, false, GlobalValue::ExternalLinkage,0,varName,m_module);
        m_map[varName] = glob;
        return glob;
    } 

} // namespace
