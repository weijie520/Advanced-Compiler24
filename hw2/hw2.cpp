#include "llvm/IR/PassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"

#include <set>

using namespace llvm;

namespace
{

  // struct for the statement
  struct Stmt
  {
    std::set<std::string> TREF;
    std::set<std::string> TGEN;
    std::vector<struct Dependency> FLOW_DEP;
    std::vector<struct Dependency> OUT_DEP;
    std::map<std::string, int> TDEF;
    std::map<std::string, std::string> TEQUIV;
  };

  struct Dependency
  {
    std::string var;
    int src;
    int dst;
  };

  int isSubtree(std::string s1, std::string s2)
  {
    if (s1.size() > s2.size())
      return -1;
    if(s1 == s2)
      return 0;
    for (int i = 1; i < s2.size(); i++)
    {
      if (s2[i - 1] == '*'){
        if (s2.substr(i) == s1)
          return i;
      }
      else
        break;
    }
    return -1;
  }

  struct HW2Pass : public PassInfoMixin<HW2Pass>
  {
    PreservedAnalyses run(Function &F, FunctionAnalysisManager &FAM);
  };

  PreservedAnalyses HW2Pass::run(Function &F, FunctionAnalysisManager &FAM)
  {
    // errs() << "[HW2]: " << F.getName() << '\n';

    llvm::SmallVector<struct Stmt *, 5> stmts;
    stmts.push_back(new struct Stmt());
    int statment_id = 0;

    std::map<std::string, int> TDEF; // global TDEF
    std::map<std::string, std::string> TEQUIV_IN; // TEQUIV_IN
    std::vector<std::string> varNames;

    for (BasicBlock &BB : F)
    {
      for (Instruction &I : BB)
      {
        // errs() << I << '\n';
        if (auto *LI = dyn_cast<LoadInst>(&I)) // int:13 ptr:15
        {
          Value *V = LI->getPointerOperand();
          
          // LI->get;
          if (V->hasName())
          {
            varNames.push_back(V->getName().str());
          }
          else
          {
            std::string name = "*" + varNames.back();
            varNames.push_back(name);
          }
        }
        else if (auto *SI = dyn_cast<StoreInst>(&I))
        {
          Value *V = SI->getPointerOperand();
          if (V->hasName())
          {
            varNames.push_back(V->getName().str());
          }
          else
          {
            std::string name = "*" + varNames.back();
            varNames.push_back(name);
          }

          // Add RHS variable and expression tree, proper subtree of LHS into TREF
          for(std::vector<std::string>::iterator it = varNames.begin(); it != varNames.end() - 1; ++it)
          {
            stmts[statment_id]->TREF.insert(*it);
          }

          // TREF run through TEQUIV_IN
          for(auto &name : stmts[statment_id]->TREF){
            // if(TEQUIV_IN.find(name) != TEQUIV_IN.end()){
            //   stmts[statment_id]->TREF.insert(TEQUIV_IN[name]);
            // }
            for(auto &eq: TEQUIV_IN){
              if(eq.second == name){
                stmts[statment_id]->TREF.insert(eq.first);
              }
              else if(eq.first == name){
                stmts[statment_id]->TREF.insert(eq.second);
              }
            }
          }

          // Add LHS variable expression tree (no subtree) into TGEN
          stmts[statment_id]->TGEN.insert(varNames.back());

          // (skip) If the RHS may modify the variable value, add the variable to TGEN. E.g., t = *p++; *p++ in RHS

          // Searching EQUIV_IN to find the alias of the LHS variable and add it to TGEN
          for(auto &name : stmts[statment_id]->TGEN){
            if(TEQUIV_IN.find(name) != TEQUIV_IN.end()){
              stmts[statment_id]->TGEN.insert(TEQUIV_IN[name]);
            }
            // for(auto &eq: TEQUIV_IN){
            //   if(eq.second == name){
            //     stmts[statment_id]->TGEN.insert(eq.first);
            //   }
            //   else if(eq.first == name){
            //     stmts[statment_id]->TGEN.insert(eq.second);
            //   }
            // }
          }

          // Dependency analysis
          // FLOW_DEP = TREF(Si) ∩ TDEF(Si) 
          for(auto &name : stmts[statment_id]->TREF){
            if(TDEF.find(name) != TDEF.end()){
              stmts[statment_id]->FLOW_DEP.push_back({name, TDEF[name], statment_id});
            }
          }

          // OUT_DEP = TGEN(Si) ∩ TDEF(Si)
          for(auto &name : stmts[statment_id]->TGEN){
            if(TDEF.find(name) != TDEF.end()){
              stmts[statment_id]->OUT_DEP.push_back({name, TDEF[name], statment_id});
            }
          }

          // New definition
          for(auto& name : stmts[statment_id]->TGEN){
            TDEF[name] = statment_id;
          }

          // TKILL
          // (1) TGEN(Si) ∩ TDEF(Si)
          // (2) If any element of TGEN(Si) is a proper subtree of any of the element in TDEF(Si), the element is removed
          for(auto &name : stmts[statment_id]->TGEN){
            for(auto it = TDEF.begin(); it != TDEF.end(); /* no increment here */){
              auto &alias = *it;
              if(isSubtree(name, alias.first) > 0){
                it = TDEF.erase(it);
              } else {
                ++it;
              }
            }
          }

          stmts[statment_id]->TDEF.insert(TDEF.begin(), TDEF.end());

          // EQUIV_OUT = (EQUIV_IN - EQUIV_KILL) U EQUIV_GEN
          // EQUIV_KILL = If any element of TGEN(Si) 
          // is a proper subtree of any of the element in 
          // EQuiv_IN(Si), the  pair is removed
          for(auto &name : stmts[statment_id]->TGEN){
            for(auto it = TEQUIV_IN.begin(); it != TEQUIV_IN.end(); /* no increment here */){
              auto &alias = *it;
              if(isSubtree(name, alias.first) > 0){
                it = TEQUIV_IN.erase(it);
              } else {
                ++it;
              }
            }
          }

          // EQUIV_GEN
          Value *V2 = SI->getValueOperand();
          if(V2->getType()->isPointerTy()){
            if(V2->hasName()){ // for pointer assignment: a = &b;
              TEQUIV_IN[("*" + varNames.back())] = V2->getName().str();
            }
            else
            { // for pointer assignment: a = b; (assume there is only one variable in RHS)
              TEQUIV_IN[("*" + varNames.back())] = "*" + varNames.front();
            }
          }

          // transitve closure of TEQUIV_OUT (the TEQUIV_IN of the next statement)
          for(auto &name : TEQUIV_IN){
            for(auto &name2 : TEQUIV_IN){
              if(name == name2)
                continue;
              int pos = isSubtree(name.first, name2.first);
              if(pos != -1){
                TEQUIV_IN[name2.first.substr(0, pos) + name.second] = name2.second;
              }
              pos = isSubtree(name.first, name2.second);
              if(pos != -1){
                TEQUIV_IN[name2.first] = name2.second.substr(0, pos) + name.second;
              }
              pos = isSubtree(name.second, name2.first);
              if(pos != -1){
                TEQUIV_IN[name2.first.substr(0, pos) + name.first] = name2.second;
              }
              // pos = isSubtree(name.second, name2.second);
              // if(pos != -1){
              //   TEQUIV_IN[name2.first] = name2.second.substr(0, pos) + name.first;
              // }
            }
            // if(TEQUIV_IN.find(("*" + name.second)) != TEQUIV_IN.end()){
            //   TEQUIV_IN["*" + name.first] = TEQUIV_IN[("*" + name.second)];
            // }
          }

          stmts[statment_id]->TEQUIV.insert(TEQUIV_IN.begin(), TEQUIV_IN.end());

          // new statement
          varNames.clear();
          stmts.push_back(new struct Stmt());
          statment_id++;
        }
      }
    }
    stmts.pop_back();

    int i = 0;
    for (auto &stmt : stmts)
    {
      errs() << "S" << (i++)+1 << '\n'<< "====================\n";
      errs() << "TREF: ";
      for(auto &name : stmt->TREF){
        errs() << name << ' ';
      }
      errs() << '\n';
      errs() << "TGEN: ";
      for(auto &name : stmt->TGEN){
        errs() << name << ' ';
      }
      errs() << '\n';
      errs() << "FLOW_DEP: ";
      for(auto &dep : stmt->FLOW_DEP){
        errs() << '(' << dep.var << ", S" << dep.src+1 << ", S" << dep.dst+1 << ") ";
      }
      errs() << '\n';
      errs() << "OUT_DEP: ";
      for(auto &dep : stmt->OUT_DEP){
        errs() << '(' << dep.var << ", S" << dep.src+1 << ", S" << dep.dst+1 << ") ";
      }
      errs() << '\n';
      errs() << "TDEF: ";
      for(auto &name : stmt->TDEF){
        errs() << '(' << name.first << ", S" << name.second+1 << ") ";
      }
      errs() << '\n';
      errs() << "TEQUIV: ";
      for(auto &name : stmt->TEQUIV){
        errs() << '(' << name.first << ", " << name.second << ") ";
      }
      errs() << "\n====================\n";
    }

    return PreservedAnalyses::all();
  }

} // end anonymous namespace

extern "C" ::llvm::PassPluginLibraryInfo LLVM_ATTRIBUTE_WEAK
llvmGetPassPluginInfo()
{
  return {LLVM_PLUGIN_API_VERSION, "HW2Pass", LLVM_VERSION_STRING,
          [](PassBuilder &PB)
          {
            PB.registerPipelineParsingCallback(
                [](StringRef Name, FunctionPassManager &FPM,
                   ArrayRef<PassBuilder::PipelineElement>)
                {
                  if (Name == "hw2")
                  {
                    FPM.addPass(HW2Pass());
                    return true;
                  }
                  return false;
                });
          }};
}
