/* Nadav Rotem  - C-to-Verilog.com */

#ifndef LLVM_SCHED_UTILS_H
#define LLVM_SCHED_UTILS_H

#include "llvm/Support/Mangler.h"

#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <set>

using namespace llvm;

using std::set;
using std::vector;
using std::pair;
using std::string;
using std::stringstream;

namespace {
   
    /*
     * Set the terminal to color mode. Each line from this point will
     * be printed in color (white over blue).
     */
    std::string xBlue() {
        return "\033[44;37;5m";
    }

    /*
     * Set the terminal to color mode. Each line from this point will
     * be printed in color (white over blue).
     */
    std::string xRed() {
        return "\033[22;31;5m";
    }

    /*
     * Reset the terminal to it's regular color mode
     */ 
     std::string xReset() {
        return "\033[0m";
    }

    void logPassMessage(std::string passName,int line ,std::string message, bool good=true) {
        if (good) {
            cerr<<xBlue();
        } else {
            cerr<<xRed();
        }
        cerr<<"OPT:"<<passName<<","<<line<<":"<<xReset()<<message<<std::endl;
    }


    string toPrintable(const string& in ){
        string VarName;
        VarName.reserve(in.capacity());

        for (std::string::const_iterator I = in.begin(), E = in.end();
                I != E; ++I) {
            char ch = *I;

            if (!((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
                        (ch >= '0' && ch <= '9') || ch == '_'))
                VarName += '_';
            else
                VarName += ch;
        }
        return VarName;
    }

    string valueToString(Mangler* mang ,const Value *Operand) {
        std::string Name;

        const ConstantInt* CI = dyn_cast<ConstantInt>(Operand);

        if (CI && !isa<GlobalValue>(CI)) {
            stringstream ss;
            const Type* Ty = CI->getType();
            if (Ty == Type::Int1Ty)
                ss<<((CI->getZExtValue() ? "1" : "0"));
            else {
                ss << "(";
                if (CI->isMinValue(true)) ss <<"-"<< CI->getZExtValue() <<"";
                else ss << CI->getSExtValue();
                ss << ")";
            }
            return ss.str();
        }


        if (!isa<GlobalValue>(Operand) && Operand->getName() != "") {
            std::string VarName;

            Name = Operand->getName();

            VarName = toPrintable(Name);

            const Type* tp = Operand->getType();
            if (tp->isInteger()) {
                Name = "i_" + VarName;
            } else if(tp->isSized()) {
                Name = "p_" + VarName;
            } else {
                Name = "o_" + VarName;
            }
	    return Name;//JAWAD	

        }
        if(isa<ConstantPointerNull> (Operand)){ //JAWAD
 		 return "(0) /* NULL */";
	}
	//TODO: pass mang to this method ...
        Name = mang->getValueName(Operand);
        

        return Name;
    }




} //end of namespace
#endif // h guard
