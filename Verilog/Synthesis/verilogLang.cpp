/* Nadav Rotem  - C-to-Verilog.com */
#include "llvm/DerivedTypes.h"
#include "llvm/Instructions.h"
#include "llvm/Support/CallSite.h"
#include "llvm/Support/InstIterator.h"
#include "llvm/Support/GetElementPtrTypeIterator.h"

#include "verilogLang.h"
#include "intrinsics.h"
#include <algorithm> //JAWAD

namespace xVerilog {

    string assignPartBuilder::toString(verilogLanguage* vl) {
        stringstream sb;

        sb << "wire [31:0] "<<m_name<<"_in_a"<<";\n";
        sb << "wire [31:0] "<<m_name<<"_in_b"<<";\n";
        sb <<" assign " <<m_name<<"_in_a"<<" = ";
        for (vector<assignPartEntry*>::iterator it=m_parts.begin(); it!=m_parts.end();++it) {
            assignPartEntry *part = *it;
            sb <<"\n (eip == "<<part->getState()<<") ? "<<
                vl->evalValue(part->getLeft())<<" :";
        }   
        sb <<"0;\n";

        sb <<" assign " <<m_name<<"_in_b"<<" = ";
        for (vector<assignPartEntry*>::iterator it=m_parts.begin(); it!=m_parts.end();++it) {
            assignPartEntry *part = *it;
            sb <<"\n (eip == "<<part->getState()<<") ? "<<
                vl->evalValue(part->getRight())<<" :";
        }   
        sb <<"0;\n\n";

        sb<<"wire [31:0] out_"<<m_name<<";\n";
        sb<<m_op<<"  "<<m_name<<"_instance (.clk(clk), .a("<<
            m_name<<"_in_a)"<<", .b("<<m_name<<"_in_b), .p(out_"<<m_name<<"));\n\n";
        return sb.str();
    }


    /// Verilog printer below

    string verilogLanguage::printBasicBlockDatapath(listScheduler *ls) {
        stringstream ss;
        // for each cycle in this basic block
        for (unsigned int cycle=0; cycle<ls->length();cycle++) {
            vector<Instruction*> inst = ls->getInstructionForCycle(cycle);
            // for each instruction in cycle, print it ...
            for (vector<Instruction*>::iterator ii = inst.begin(); ii != inst.end(); ++ii) {
                if (isInstructionDatapath(*ii)) {
                    ss<<printInstruction(*ii, 0);
                }
            }
        }// for each cycle      
        return ss.str();
    }

    string verilogLanguage::printBasicBlockControl(listScheduler *ls) {
        stringstream ss;
        const string space("\t");
        string name = toPrintable(ls->getBB()->getName());
        // for each cycle in this basic block
        for (unsigned int cycle=0; cycle<ls->length();cycle++) {
            ss<<""<<name<<cycle<<":\n"; //header
            ss<<"begin\n";
            vector<Instruction*> inst = ls->getInstructionForCycle(cycle);
            // for each instruction in cycle, print it ...
            for (vector<Instruction*>::iterator ii = inst.begin(); ii != inst.end(); ++ii) {
                unsigned int id = ls->getResourceIdForInstruction(*ii);
                if (!isInstructionDatapath(*ii)) {
                    ss<<space<<printInstruction(*ii, id);
                }
            }

            if (cycle+1 != ls->length()) { 
                ss<<"\teip <= "<<name<<cycle+1<<";\n"; //header
            }
            ss<<"end\n";
        }// for each cycle      

        return ss.str();
    }


    string verilogLanguage::printLoadInst(Instruction* inst, int unitNum, int cycleNum) {
        LoadInst* load = (LoadInst*) inst; // make the cast
        /*
         * If this is a regular load/store command then we just print it
         * however, if this is a memory port then we need to assign a port
         * number to it
         * */
        stringstream ss;
        ss<<GetValueName(load)<<" <= "<<evalValue(load->getOperand(0))<<unitNum;
        return ss.str();
    }


    string verilogLanguage::printStoreInst(Instruction* inst, int unitNum, int cycleNum) {
        stringstream ss;
        StoreInst* store = (StoreInst*) inst; // make the cast
        string first = evalValue(store->getOperand(0));
        string second = evalValue(store->getOperand(1)); 
        ss << second<<unitNum<< " <= " << first;
        return ss.str();
    }



    string verilogLanguage::printBinaryOperatorInst(Instruction* inst, int unitNum, int cycleNum) {
        stringstream ss;
        BinaryOperator* bin = (BinaryOperator*) inst;

        string rec = GetValueName(bin);
        Value* val0 = bin->getOperand(0);
        Value* val1 = bin->getOperand(1);
        ss << rec <<" <= ";
        // state, src1, src2, output
        switch (bin->getOpcode()) {
            case Instruction::Add:{ss<<evalValue(val0)<<"+"<<evalValue(val1);break;}
            case Instruction::Sub:{ss<<evalValue(val0)<<"-"<<evalValue(val1);break;}
            case Instruction::SRem:{ss<<evalValue(val0)<<"%"<<evalValue(val1);break;} // THIS IS SLOW
            case Instruction::And:{ss<<evalValue(val0)<<"&"<<evalValue(val1);break;}
            case Instruction::Or: {ss<<evalValue(val0)<<"|"<<evalValue(val1);break;}
            case Instruction::Xor:{ss<<evalValue(val0)<<"^"<<evalValue(val1);break;}
            case Instruction::Mul:{ss<<evalValue(val0)<<"*"<<evalValue(val1);break;}
            case Instruction::AShr:{ss<<evalValue(val0)<<" >> "<<evalValue(val1);break;}
            case Instruction::LShr:{ss<<evalValue(val0)<<" >> "<<evalValue(val1);break;}
            case Instruction::Shl:{ss<<evalValue(val0)<<" << "<<evalValue(val1);break;}
            default: {
                         cerr<<"Unhandaled: "<<*inst;
                         abort();
                     }
        }
        return ss.str();
    }


    string verilogLanguage::printReturnInst(Instruction* inst) {
        stringstream ss;
        ReturnInst* ret = (ReturnInst*) inst; // make the cast
        if (ret->getNumOperands()) {
            ss << " rdy <= 1;\n";
            ss << " return_value <= ";
            ss << evalValue(ret->getOperand(0))<<";\n";
            ss << " $display($time, \" Return (0x%x) \","<<evalValue(ret->getOperand(0))<<");";
            ss << "\n $finish()";
        } else  {
            // if ret void
            ss << " rdy <= 1;\n";
            ss << " return_value <= 0;";
            ss << "\n $finish()";
        }
        return ss.str();
    }


    string verilogLanguage::printSelectInst(Instruction* inst) {
        stringstream ss;
        SelectInst* sel = (SelectInst*) inst; // make the cast
        // (cond) ? i_b : _ib;
        ss << GetValueName(sel) <<" <= ";
        ss << "(" << evalValue(sel->getOperand(0)) << " ? ";
        ss << evalValue(sel->getOperand(1)) << " : ";
        ss << evalValue(sel->getOperand(2))<<")";
        return ss.str();
    }

    string verilogLanguage::printAllocaInst(Instruction* inst) {
        stringstream ss;
        AllocaInst* alca = (AllocaInst*) inst; // make the cast
        // a <= b;
        ss << GetValueName(alca) <<" <= 0";
        ss << "/* AllocaInst hack, fix this by giving a stack address */"; 
        return ss.str();
    }

    string verilogLanguage::getIntrinsic(Instruction* inst) {
        stringstream ss;
        CallInst *cl = dyn_cast<CallInst>(inst);
        assert(cl && "inst is not a CallInst Intrinsi");
        vector<string> params; 
        for(CallSite::arg_iterator I = cl->op_begin(); I != cl->op_end(); ++I) {
            string s = evalValue(*I);
            params.push_back(s); 
        }
        ss << getIntrinsicForInstruction(cl, params);
        return ss.str();
    }

    string verilogLanguage::printIntrinsic(Instruction* inst) {
        return GetValueName(inst)  + string(" <= ") + getIntrinsic(inst);
    }

    string verilogLanguage::printIntToPtrInst(Instruction* inst) {
        stringstream ss;
        IntToPtrInst* i2p = (IntToPtrInst*) inst; // make the cast
        // a <= b;
        ss << GetValueName(i2p) <<" <= ";
        ss <<  evalValue(i2p->getOperand(0));
        return ss.str();
    }

    string verilogLanguage::printZxtInst(Instruction* inst) {
        stringstream ss;
        ZExtInst* zxt = (ZExtInst*) inst; // make the cast
        // a <= b;
        ss << GetValueName(zxt) <<" <= ";
        ss <<  evalValue(zxt->getOperand(0));
        return ss.str();
    }
    string verilogLanguage::printBitCastInst(Instruction* inst) { //JAWAD
        stringstream ss;
        BitCastInst* btcst = (BitCastInst*) inst; // make the cast
        // a <= b;
        ss << GetValueName(btcst) <<" <= ";
        ss <<  evalValue(btcst->getOperand(0));
        return ss.str();
    }

    string verilogLanguage::printPHICopiesForSuccessor(BasicBlock *CurBlock,BasicBlock *Successor){
        stringstream ss;

        for (BasicBlock::iterator I = Successor->begin(); isa<PHINode>(I); ++I) {
            PHINode *PN = cast<PHINode>(I);
            //Now we have to do the printing.
            Value *IV = PN->getIncomingValueForBlock(CurBlock);
            if (!isa<UndefValue>(IV)) {
                ss <<"\t\t"<< GetValueName(I) << " <= " << evalValue(IV);
                ss << ";\n";
            }
        }
        return ss.str();
    }

    string verilogLanguage::printBranchInst(Instruction* inst) {

        stringstream ss;
        BranchInst* branch = (BranchInst*) inst; // make the cast

        if (branch->isConditional()) {
            ss << "if (";
            ss << evalValue(branch->getCondition());
            ss << ") begin\n";
            ss<<printPHICopiesForSuccessor(branch->getParent(),branch->getSuccessor(0));
            // we add a zero because the first entry in the basic block is '0'
            // i.e we jump to the first state in the basic block
            ss << "\t\teip <= " << toPrintable(branch->getSuccessor(0)->getName())<<"0;\n";
            ss << "\tend else begin\n";
            ss<<printPHICopiesForSuccessor(branch->getParent(),branch->getSuccessor(1));
            // we add a zero because the first entry in the basic block is '0'
            ss << "\t\teip <= "<<toPrintable(branch->getSuccessor(1)->getName())<<"0;\n";
            ss << "\tend\n";
        } else {
            ss<<printPHICopiesForSuccessor(branch->getParent(),branch->getSuccessor(0));
            ss << "\t\teip <= " << toPrintable(branch->getSuccessor(0)->getName())<<"0;\n";
        }
        return ss.str();
    }


    string verilogLanguage::printCmpInst(Instruction* inst) {
        stringstream ss;
        CmpInst* cmp = (CmpInst*) inst; // make the cast
        ss << GetValueName(inst) << " <= ";
        ss << "(";
        ss<< evalValue(cmp->getOperand(0));

        switch (cmp->getPredicate()) {
            case ICmpInst::ICMP_EQ:  ss << " == "; break;
            case ICmpInst::ICMP_NE:  ss << " != "; break;
            case ICmpInst::ICMP_ULE:
            case ICmpInst::ICMP_SLE: ss << " <= "; break;
            case ICmpInst::ICMP_UGE:
            case ICmpInst::ICMP_SGE: ss << " >= "; break;
            case ICmpInst::ICMP_ULT:
            case ICmpInst::ICMP_SLT: ss << " < "; break;
            case ICmpInst::ICMP_UGT:
            case ICmpInst::ICMP_SGT: ss << " > "; break;
            default: cerr << "Invalid icmp predicate!"; abort();
        }

        ss << evalValue(cmp->getOperand(1));
        ss << ")";
        return ss.str();
    }

    string verilogLanguage::getGetElementPtrInst(Instruction* inst) {
        stringstream ss;
        GetElementPtrInst* get = (GetElementPtrInst *) inst;
        if (2 == get->getNumOperands()) {
            // We have a regular array
            ss << evalValue(get->getPointerOperand()); // + 
            ss << " + ";
            ss << evalValue(get->getOperand(get->getNumOperands()-1)); // get last dimention
        } else {
            // We have a struct or a multi dimentional array
            // TODO: Assume we only have two elements. This is lame ... 
            ss << evalValue(get->getPointerOperand()); // + 
            ss << " + ";		//JAWAD
            ss << evalValue(get->getOperand(get->getNumOperands()-2)); // JAWAD 
            ss << " + 2*";
            ss << evalValue(get->getOperand(get->getNumOperands()-1)); // get last dimention
            ss << "/* Struct */";

        }
        return ss.str();
    }

    string verilogLanguage::printGetElementPtrInst(Instruction* inst) {
        stringstream ss;
        ss << GetValueName(inst) <<" <= ";
        ss<< getGetElementPtrInst(inst);
        return ss.str();
    }

    string verilogLanguage::GetValueName(const Value *Operand) {
        return valueToString(m_mang,Operand);
    }

    bool verilogLanguage::isInstructionDatapath(Instruction *inst) {
        if (dyn_cast<BinaryOperator>(inst))     return true;
        if (dyn_cast<CmpInst>(inst))            return true;
        if (dyn_cast<GetElementPtrInst>(inst))  return true;
        if (dyn_cast<SelectInst>(inst))         return true;
        if (dyn_cast<ZExtInst>(inst))           return true;
        if (dyn_cast<IntToPtrInst>(inst))       return true;
        return false;
    }
    string verilogLanguage::printInstruction(Instruction *inst, unsigned int resourceId) {
        const string colon(";\n");
        if (dyn_cast<StoreInst>(inst))      return printStoreInst(inst, resourceId, 0) + colon;
        if (dyn_cast<LoadInst>(inst))       return printLoadInst(inst, resourceId , 0) + colon;
        if (dyn_cast<ReturnInst>(inst))     return printReturnInst(inst) + colon;
        if (dyn_cast<BranchInst>(inst))     return printBranchInst(inst);
        if (dyn_cast<PHINode>(inst))        return "" ; // we do not print PHINodes 
        if (dyn_cast<BinaryOperator>(inst)) return printBinaryOperatorInst(inst, 0, 0) + colon;
        if (dyn_cast<CmpInst>(inst))        return printCmpInst(inst) + colon;
        if (dyn_cast<GetElementPtrInst>(inst)) return printGetElementPtrInst(inst)+ colon;
        if (dyn_cast<SelectInst>(inst))     return printSelectInst(inst) + colon;
        if (dyn_cast<ZExtInst>(inst))       return printZxtInst(inst) + colon;
        if (dyn_cast<BitCastInst>(inst))    return printBitCastInst(inst) + colon; //JAWAD
        if (dyn_cast<AllocaInst>(inst))     return printAllocaInst(inst) + colon;
        if (dyn_cast<IntToPtrInst>(inst))   return printIntToPtrInst(inst) + colon;
        if (dyn_cast<CallInst>(inst))       return printIntrinsic(inst) + colon;
        if (dyn_cast<Instruction>(inst)) std::cerr<<"Unable to process "<<*inst<<"\n";
        assert(0 && "Unhandaled instruction");
        abort();
        return colon;
    }


    string verilogLanguage::getArgumentListDecl(const Function &F, const string& prefix) {
        std::stringstream ss;

        Function::const_arg_iterator I = F.arg_begin(), E = F.arg_end();

        // Loop over the arguments, printing them as input variables.
        I = F.arg_begin();
        E = F.arg_end();
        for (; I != E; ++I) {
            // integers:
            if (I->getType()->getTypeID()==Type::IntegerTyID) {
                unsigned NumBits = cast<IntegerType>(I->getType())->getBitWidth();
                ss <<" "<<prefix;
                ss <<" [" <<  NumBits-1 <<":0] "<<GetValueName(I)<<";\n";
            }  // array of integers:
            else if (I->getType()->getTypeID()==Type::ArrayTyID) {
                //ArrayType *Arr = cast<ArrayType>(I->getType());
                unsigned NumBits = cast<IntegerType>(
                        cast<ArrayType>(I->getType())->getElementType())->getBitWidth();
                unsigned NumElements = cast<ArrayType>(I->getType())->getNumElements(); 
                ss << " " << prefix;
                ss << " [" <<  NumBits-1 <<":0] "<<GetValueName(I)<<"["<<NumElements<<":0];\n";
            }  else if (I->getType()->getTypeID()==Type::PointerTyID) {
                unsigned NumBits = m_pointerSize; // 32bit for pointers
                ss << " " << prefix;
                ss << " [" <<  NumBits-1 <<":0] "<<GetValueName(I)<<";\n";
            } else {
                cerr<<"Unable to accept non integer params: "<<*I<<"\n";
                cerr<<"Types:"<<I->getType()->getTypeID()<<" "<<Type::ArrayTyID <<"\n";
                abort(); }
        }

        return ss.str();
    }


    string verilogLanguage::getTestBench(Function &F) {
        std::stringstream ss;
        ss<<"\n // Test Bench \n\n";

        MemportMap memports = listScheduler::getMemoryPortDeclerations(&F,TD); //JAWAD 

        ss<<"\nmodule "<<GetValueName(&F)<<"_test;\n";
        ss << " wire rdy;\n reg reset, clk;\n";

        for (MemportMap::iterator it = memports.begin(); it != memports.end(); ++it) {
            std::string name = it->first;
            int width = it->second;
            for (unsigned int i=0; i<2; i++) {
                ss<<"wire ["<<width-1<<":0] mem_"<<name<<"_out"<<i<<";\n";
                ss<<"wire ["<<width-1<<":0] mem_"<<name<<"_in"<<i<<";\n";
                ss<<"wire ["<<m_pointerSize-1<<":0] mem_"<<name<<"_addr"<<i<<";\n";
                ss<<"wire mem_"<<name<<"_mode"<<i<<";\n";
            } 

            ss<<"xram ram_"<<name<<" (mem_"<<name<<"_out0, mem_"<<name<<"_in0, mem_"<<name<<"_addr0, mem_"<<name<<"_mode0, clk,\n"<<
                "  mem_"<<name<<"_out1, mem_"<<name<<"_in1, mem_"<<name<<"_addr1, mem_"<<name<<"_mode1, clk);\n\n\n";
        }



        ss<<" always #5 clk = ~clk;\n";

        ss<<getArgumentListDecl(F, string("reg"));

        if (F.getReturnType()->getTypeID()==Type::VoidTyID) {
            //If we return void, we have a dummy one bit return val
            ss << " wire return_value;\n";
        } else if (F.getReturnType()->getTypeID()!=Type::IntegerTyID) {
            cerr<<"Unable to accept non integer return func";
            abort();
        } else {
            unsigned NumBits = cast<IntegerType>(F.getReturnType())->getBitWidth();
            ss << " wire [" << NumBits-1 << ":0] return_value;\n";
        }

        ss<<getFunctionSignature(&F,"instance1");
        ss << "initial begin\n";

        ss << " clk = 0;\n";
        ss << " $monitor(\"return = %b, 0x%x\", rdy,  return_value);\n";
	ss << "\n // Configure the values below to test the module\n";
        Function::const_arg_iterator I = F.arg_begin(), E = F.arg_end();
        I = F.arg_begin();
        E = F.arg_end();
        for (; I != E; ++I) {
            if (GetValueName(I) == "i_n") {
                ss<<" "<<GetValueName(I)<<" <= 128;// detected index variable\n";
            } else {
                ss<<" "<<GetValueName(I)<<" <= 0;\n";
            }
        }

        ss<<" #5 reset = 1; #5 reset = 0;\n";
        ss<<"end\n";   

        ss << "\nendmodule //main_test \n";
        return ss.str();
    }


    string verilogLanguage::getTypeDecl(const Type *Ty, bool isSigned, const std::string &NameSoFar) {
        assert((Ty->isPrimitiveType() || Ty->isInteger() || Ty->isSized()) && "Invalid type decl");
        std::stringstream ss;
        switch (Ty->getTypeID()) {
            case Type::VoidTyID: { 
                                     return std::string(" reg /*void*/") +  NameSoFar;
                                 }
            case Type::PointerTyID: { // define the verilog pointer type
                                        unsigned NBits = m_pointerSize; // 32bit for our pointers
                                        ss<< "reg ["<<NBits-1<<":0] "<<NameSoFar;
                                        return ss.str();
                                    }
            case Type::IntegerTyID: { // define the verilog integer type
                                        unsigned NBits = cast<IntegerType>(Ty)->getBitWidth();
                                        if (NBits == 1) return std::string("reg ")+NameSoFar;
                                        ss<< "reg ["<<NBits-1<<":0] "<<NameSoFar;
                                        return ss.str();
                                    }
            default :
                                    cerr << "Unknown primitive type: " << *Ty << "\n";
                                    abort();
        }
    }

    string verilogLanguage::getMemDecl(Function *F) {
        stringstream ss;
        MemportMap memports = listScheduler::getMemoryPortDeclerations(F,TD);//JAWAD  
    
        // For each of the instances of each memory port)
        for (unsigned int i=0; i<m_memportNum; i++) {
            for (MemportMap::iterator it = memports.begin(); it != memports.end(); ++it) {
                std::string name = it->first;
                int width = it->second;
                ss<<"input wire ["<<width-1<<":0] mem_"<<name<<"_out"<<i<<";\n";
                ss<<"output reg ["<<width-1<<":0] mem_"<<name<<"_in"<<i<<";\n";
                ss<<"output reg ["<<m_pointerSize-1<<":0] mem_"<<name<<"_addr"<<i<<";\n";
                ss<<"output reg mem_"<<name<<"_mode"<<i<<";\n";
            } 
        }

        ss<<"\n\n";
        return ss.str();
    }
    string verilogLanguage::getClockHeader() {
        stringstream ss;
        ss<<"always @(posedge clk)\n begin\n  if (reset)\n   begin\n";
        ss<<"    $display(\"@hard reset\");\n    eip<=0;\n    rdy<=0;\n   end\n\n";
        return ss.str();
    }
    string verilogLanguage::getCaseHeader() {
        stringstream ss;
        ss<<"case (eip)\n";
        return ss.str();
    }
    string verilogLanguage::getClockFooter() {
        stringstream ss;
        ss<<"end //always @(..)\n\n";
        return ss.str();
    }
    string verilogLanguage::getCaseFooter() {
        stringstream ss;
        ss<<" endcase //eip\n";
        return ss.str();
    }
    string verilogLanguage::getModuleFooter() {
        stringstream ss;
        ss<<"endmodule\n\n";
        return ss.str();
    }

    string verilogLanguage::getFunctionLocalVariables(listSchedulerVector lsv) {

        std::stringstream ss;
        // for each listScheduler of a basic block
        for (listSchedulerVector::iterator lsi=lsv.begin(); lsi!=lsv.end();++lsi) {
            // for each cycle in each LS
            for (unsigned int cycle=0; cycle<(*lsi)->length();cycle++) {
                vector<Instruction*> inst = (*lsi)->getInstructionForCycle(cycle);
                // for each instruction in each cycle in each LS
                for (vector<Instruction*>::iterator I = inst.begin(); I!=inst.end(); ++I) {
                    // if has a return type, print it as a variable name
                    if ((*I)->getType() != Type::VoidTy) {
                        ss << " ";
                        ss << getTypeDecl((*I)->getType(), false, GetValueName(*I));
                        ss << ";   /*local var*/\n";
                    }    
                }
            }// for each cycle    

            // Print all PHINode variables as well
            BasicBlock *bb= (*lsi)->getBB(); 
            for (BasicBlock::iterator bit = bb->begin(); bit != bb->end(); bit++) { 
                if (dyn_cast<PHINode>(bit)) {
                    // if has a return type, print it as a variable name 
                    if ((bit)->getType() != Type::VoidTy) { 
                        ss << " "; 
                        ss << getTypeDecl((bit)->getType(), false, GetValueName(bit)); 
                        ss << ";   /*phi var*/\n"; 
                    }     
                }
            } 
        }// for each listScheduler

        return ss.str();
    }

    unsigned int verilogLanguage::getNumberOfStates(listSchedulerVector &lsv){
        int numberOfStates = 0;
        for (listSchedulerVector::iterator it = lsv.begin(); it!=lsv.end();it++) {
            numberOfStates += (*it)->length();
        }
        return numberOfStates;
    }

    string verilogLanguage::getStateDefs(listSchedulerVector &lsv)  {
        std::stringstream ss;

        unsigned int numberOfStates = getNumberOfStates(lsv);

        // Instruction pointer of n bits, n^2 states
        unsigned int NumOfStateBits = (int) ceil(log(numberOfStates+1)/log(2))-1;

        ss<<"\n // Number of states:"<<numberOfStates<<"\n";
        ss << " reg ["<< NumOfStateBits<<":0] eip;\n";

        int stateCounter = 0;
        // print the definitions for the values of the EIP values.
        //     // for example: 'define start 16'd0  ...
        for (listSchedulerVector::iterator it = lsv.begin(); it!=lsv.end(); it++) {
            // each cycle in the BB
            for (unsigned int i=0;i<(*it)->length();i++) {
                ss << " parameter "<<toPrintable((*it)->getBB()->getName())<<i
                    <<" = "<<NumOfStateBits+1<<"'d"<<stateCounter<<";\n";
                stateCounter++;
            }
        }
        ss<<"\n";
        return ss.str();
    }



    string verilogLanguage::printAssignPart(vector<assignPartEntry*> ass, verilogLanguage* lang) {
        stringstream sb;
        map<string,string> unitNames;

        sb <<"// Assign part ("<< ass.size() <<")\n";

        // extract all unit names from assign part
        for (vector<assignPartEntry*>::iterator it = ass.begin(); it!=ass.end(); ++it) {
            unitNames[(*it)->getUnitName()] = (*it)->getUnitType();
        }

        // for each uniqe name 
        for (map<string,string>::iterator nm = unitNames.begin(); nm!=unitNames.end(); ++nm) {
            assignPartBuilder apb(nm->first, nm->second);
            // for all assign parts with this name
            for (vector<assignPartEntry*>::iterator it = ass.begin(); it!=ass.end(); ++it) {
                if (nm->first==(*it)->getUnitName()) {
                    apb.addPart(*it);
                }
            }
            sb<<apb.toString(this);
        }
        sb << "\n\n";
        return sb.str();
    }


    string verilogLanguage::getAssignmentString(listSchedulerVector lv) {
        stringstream sb;

        vector<assignPartEntry*> parts;
        for (listSchedulerVector::iterator it=lv.begin(); it!=lv.end(); ++it) {
            vector<assignPartEntry*> p = (*it)->getAssignParts();
            parts.insert(parts.begin(),p.begin(),p.end());
        }

        return printAssignPart(parts, this); 

    }

    string verilogLanguage::evalValue(Value* val) {
        if (Instruction* inst = dyn_cast<Instruction>(val)) {
            if (abstractHWOpcode::isInstructionOnlyWires(inst)) return printInlinedInstructions(inst);
        }
        return GetValueName(val);
    }

    string verilogLanguage::printInlinedInstructions(Instruction* inst) {
        std::stringstream ss;
        ss<<"(";

        if (BinaryOperator *calc = dyn_cast<BinaryOperator>(inst)) {
            Value *v0 = (calc->getOperand(0));
            Value *v1 = (calc->getOperand(1));
            // if second parameter is a constant (shift by constant is wiring)
            if (calc->getOpcode() == Instruction::Shl) {
                ss<<evalValue(v0)<<" << "<<evalValue(v1);
            } else if (calc->getOpcode() == Instruction::LShr || calc->getOpcode() == Instruction::AShr) {
                ss<<evalValue(v0)<<" >> "<<evalValue(v1);
            } else if (calc->getOpcode() == Instruction::Add) {
                ss<<evalValue(v0)<<" + "<<evalValue(v1);
            } else if (calc->getOpcode() == Instruction::Xor) {
                ss<<evalValue(v0)<<" ^ "<<evalValue(v1);
            } else if (calc->getOpcode() == Instruction::And) {
                ss<<evalValue(v0)<<" & "<<evalValue(v1);
            } else if (calc->getOpcode() == Instruction::Or) {
                ss<<evalValue(v0)<<" | "<<evalValue(v1);
            } else {
                cerr<<"Unknown Instruction "<<*calc;
                abort();
            }
        } else if (dyn_cast<TruncInst>(inst)) { 
            // make the cast
            ss <<  evalValue(inst->getOperand(0));
        } else if (dyn_cast<SExtInst>(inst)) { 
            // make the cast
            ss <<  evalValue(inst->getOperand(0));
        } else if (dyn_cast<ZExtInst>(inst)) { 
            // make the cast
            ss <<  evalValue(inst->getOperand(0));
        } else if (dyn_cast<IntToPtrInst>(inst)) { 
            // make the cast
            ss <<  evalValue(inst->getOperand(0));
        } else if (dyn_cast<GetElementPtrInst>(inst)) { 
            // make the cast
            ss <<  getGetElementPtrInst(inst);
        } else if (dyn_cast<CallInst>(inst)) { 
            // make the cast
            ss <<  getIntrinsic(dyn_cast<CallInst>(inst));
        } else if (dyn_cast<PtrToIntInst>(inst)) { 
            // make the cast
            ss <<  evalValue(inst->getOperand(0));
        } else {
            std::cerr<<"Unknown wire instruction "<<*inst;
            abort();
        }

        ss<<")";
        return ss.str();
    }

    string verilogLanguage::getFunctionSignature(const Function *F, const std::string Instance) {
        std::stringstream ss;

        MemportMap memports = listScheduler::getMemoryPortDeclerations(F,TD);//JAWAD  

        if (Instance=="") ss << "module ";

        // Print out the name...
        ss << GetValueName(F)<<" "<< Instance << " (clk, reset, rdy,// control \n\t";

        // For each of the instances of each memory port)
        for (unsigned int i=0; i<m_memportNum; i++) {
            // print memory port decl
            for (MemportMap::iterator it = memports.begin(); it != memports.end(); ++it) {
                std::string name = it->first;
                ss<<"mem_"<<name<<"_out"<<i
                <<", mem_"<<name<<"_in"<<i
                <<", mem_"<<name<<"_addr"<<i
                <<", mem_"<<name<<"_mode"<<i
                <<", // memport for: "<<name<<" \n\t";
            }
        }
        // Loop over the arguments, printing them.
        for (Function::const_arg_iterator I=F->arg_begin(),E = F->arg_end(); I!=E; ++I) {
            if (I->hasName()) { ss << GetValueName(I); } else { ss << "NoName"; }
            ss << ", ";
        }

        ss << "return_value); // params \n";

        if (Instance!="") // this is an instance, no need to declare vars
            return ss.str();

        ss << " input wire clk;\n";
        ss << " input wire reset;\n";
        ss << " output rdy;\n";
        ss << " reg rdy;\n";

        if (F->getReturnType()->getTypeID()==Type::VoidTyID) {
            //If we return void, we have a dummy one bit return val
            ss << " output return_value;\n";
            ss << " reg return_value;\n";
        } else if (F->getReturnType()->getTypeID()!=Type::IntegerTyID) {
            cerr<<"Unable to accept non integer return func";
            abort();
        } else {
            unsigned NumBits = cast<IntegerType>(F->getReturnType())->getBitWidth();
            ss << " output [" << NumBits-1 << ":0] return_value;\n";
            ss << " reg [" << NumBits-1 << ":0] return_value;\n";
        }

        ss << getArgumentListDecl(*F,string("input"));
        return ss.str();
    }

    string verilogLanguage::getBRAMDefinition(unsigned int wordBits, unsigned int addressBits) {
       std::stringstream ss;

       ss<<
           "// Dual port memory block\n"\
           "module xram (out0, din0, addr0, we0, clk0,\n"\
           "           out1, din1, addr1, we1, clk1);\n"\
           "  parameter ADDRESS_WIDTH = "<<addressBits<<";\n";
       ss<<"  parameter WORD_WIDTH = "<<wordBits<<";\n";
           ss<<
           "  output [WORD_WIDTH-1:0] out0;\n"\
           "  input [WORD_WIDTH-1:0] din0;\n"\
           "  input [ADDRESS_WIDTH-1:0] addr0;\n"\
           "  input we0;\n"\
           "  input clk0;\n"\
           "  output [WORD_WIDTH-1:0] out1;\n"\
           "  input [WORD_WIDTH-1:0] din1;\n"\
           "  input [ADDRESS_WIDTH-1:0] addr1;\n"\
           "  input we1;\n"\
           "  input clk1;\n"\
           "  reg [WORD_WIDTH-1:0] mem[1<<ADDRESS_WIDTH-1:0];\n"\
           "   integer i;\n"\
           "   initial begin\n"\
           "       for (i = 0; i < (1<<(ADDRESS_WIDTH-1)); i = i + 1) begin\n"\
           "       mem[i] <= i;\n"\
           "     end\n"\
           "   end\n"\
           "  assign out0 = mem[addr0];\n"\
           "  assign out1 = mem[addr1];\n"\
           "  always @(posedge clk0)begin\n"\
           "      if (we0) begin\n"\
           "          mem[addr0] = din0;\n"\
           "          $display($time,\"w mem[%d] == %d; in=%d\",addr0, mem[addr0],din0);\n"\
           "      end\n"\
           "  end\n"\
           "  always @(posedge clk1)begin\n"\
           "      if (we1) begin\n"\
           "          mem[addr1] = din1;\n"\
           "          $display($time,\"w mem[%d] == %d; in=%d\",addr0, mem[addr0],din0);\n"\
           "      end \n"\
           "  end\n"\
           "endmodule\n";
       return ss.str();
    }

    string verilogLanguage::createBinOpModule(string opName, string symbol, unsigned int stages) {

        std::stringstream ss;

        ss<<"\nmodule "<<opName<<" (clk, a, b, p);\n";
        ss<<"output reg [31:0] p;\ninput [31:0] a;\ninput [31:0] b;\ninput clk;";
        for (unsigned int i=0; i<stages-1; i++) {
            ss<<"reg [31:0] t"<<i<<";\n";
        }
        ss<<"always @(posedge clk)begin\n";
        ss<<"t0 <= a "<<symbol<<" b;\n";
        for (unsigned int i=1; i<stages-1; i++) {
            ss<<"t"<<(i)<<" <= t"<<(i-1)<<";\n";
        }
        ss<<"p <=t"<<stages-2<<";\nend\nendmodule\n\n";   
        return ss.str();
    }




}//namespace

