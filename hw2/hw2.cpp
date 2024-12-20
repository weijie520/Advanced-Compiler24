#include "llvm/IR/PassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/JSON.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/FileSystem.h"
#include "fstream"

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

  // check if s1 is a subtree of s2
  // if yes, return the position of the subtree
  // if no, return -1
  static int isSubtree(std::string s1, std::string s2)
  {
    if (s1.size() > s2.size())
      return -1;
    if (s1 == s2)
      return 0;
    for (int i = 1; i < s2.size(); i++)
    {
      if (s2[i - 1] == '*')
      {
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
    std::string filename = F.getParent()->getSourceFileName();
    filename.replace(filename.rfind("."), 2, ".json");

    llvm::SmallVector<struct Stmt *, 5> stmts;
    stmts.push_back(new struct Stmt());
    int statment_id = 0;

    std::unordered_map<std::string, int> TDEF;              // global TDEF
    std::unordered_map<std::string, std::string> TEQUIV_IN; // TEQUIV_IN
    std::vector<std::string> varNames;

    for (BasicBlock &BB : F)
    {
      for (Instruction &I : BB)
      {
        // errs() << I << '\n';
        if (auto *LI = dyn_cast<LoadInst>(&I)) // int:13 ptr:15
        {
          Value *V = LI->getPointerOperand();

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

          // Add RHS variable and expression tree and the proper subtree of LHS into TREF
          stmts[statment_id]->TREF.insert(varNames.begin(), varNames.end() - 1);

          // TREF run through TEQUIV_IN
          std::set<std::string> TREF_Derived; // Inorder to avoid the multiple alias of the same variable
          for (auto &name : stmts[statment_id]->TREF)
          {
            for (auto &eq : TEQUIV_IN)
            {
              if (eq.second == name)
              {
                TREF_Derived.insert(eq.first);
              }
              else if (eq.first == name)
              {
                TREF_Derived.insert(eq.second);
              }
            }
          }
          stmts[statment_id]->TREF.insert(TREF_Derived.begin(), TREF_Derived.end());

          // Add LHS variable expression tree (no subtree) into TGEN
          stmts[statment_id]->TGEN.insert(varNames.back());

          // (skip) If the RHS may modify the variable value, add the variable to TGEN. E.g., t = *p++; *p++ in RHS

          // Searching EQUIV_IN to find the alias of the LHS variable and add it to TGEN
          std::set<std::string> TGEN_Derived; // Inorder to avoid the multiple alias of the same variable
          for (auto &name : stmts[statment_id]->TGEN)
          {
            for (auto &eq : TEQUIV_IN)
            {
              if (eq.second == name)
              {
                TGEN_Derived.insert(eq.first);
              }
              else if (eq.first == name)
              {
                TGEN_Derived.insert(eq.second);
              }
            }
          }
          stmts[statment_id]->TGEN.insert(TGEN_Derived.begin(), TGEN_Derived.end());

          // Dependency analysis
          // FLOW_DEP = TREF(Si) ∩ TDEF(Si)
          for (auto &name : stmts[statment_id]->TREF)
          {
            if (TDEF.find(name) != TDEF.end())
            {
              stmts[statment_id]->FLOW_DEP.push_back({name, TDEF[name], statment_id});
            }
          }

          // OUT_DEP = TGEN(Si) ∩ TDEF(Si)
          for (auto &name : stmts[statment_id]->TGEN)
          {
            if (TDEF.find(name) != TDEF.end())
            {
              stmts[statment_id]->OUT_DEP.push_back({name, TDEF[name], statment_id});
            }
          }

          // TKILL
          // (1) TGEN(Si) ∩ TDEF(Si)
          // (2) If any element of TGEN(Si) is a proper subtree of any of the element in TDEF(Si), the element is removed
          for (auto &name : stmts[statment_id]->TGEN)
          {
            for (auto it = TDEF.begin(); it != TDEF.end(); /* no increment here */)
            {
              auto &alias = *it;
              if (isSubtree(name, alias.first) > 0)
              {
                it = TDEF.erase(it);
              }
              else
              {
                ++it;
              }
            }
          }

          // New definition
          for (auto &name : stmts[statment_id]->TGEN)
          {
            TDEF[name] = statment_id;
          }

          stmts[statment_id]->TDEF.insert(TDEF.begin(), TDEF.end());

          // EQUIV_OUT = (EQUIV_IN - EQUIV_KILL) U EQUIV_GEN
          // EQUIV_KILL = If any element of TGEN(Si)
          // is a proper subtree of any of the element in
          // EQuiv_IN(Si), the  pair is removed
          for (auto &name : stmts[statment_id]->TGEN)
          {
            for (auto it = TEQUIV_IN.begin(); it != TEQUIV_IN.end(); /* no increment here */)
            {
              auto &alias = *it;
              if (isSubtree(name, alias.first) > 0 || isSubtree(name, alias.second) > 0)
              {
                it = TEQUIV_IN.erase(it);
              }
              else
              {
                ++it;
              }
            }
          }

          // EQUIV_GEN
          Value *V2 = SI->getValueOperand();
          if (V2->getType()->isPointerTy())
          {
            if (V2->hasName())
            { // for pointer assignment: a = &b;
              TEQUIV_IN[("*" + varNames.back())] = V2->getName().str();
            }
            else
            { // for pointer assignment: a = b; (assume there is only one variable in RHS)
              TEQUIV_IN[("*" + varNames.back())] = "*" + varNames.front();
            }
          }

          // transitve closure of TEQUIV_OUT (the TEQUIV_IN of the next statement)
          for (auto &name : TEQUIV_IN)
          {
            for (auto &name2 : TEQUIV_IN)
            {
              if (name == name2)
                continue;
              int pos = isSubtree(name.first, name2.first);
              if (pos > 0)
              {
                TEQUIV_IN[name2.first.substr(0, pos) + name.second] = name2.second;
              }
              pos = isSubtree(name.second, name2.first);
              if (pos != -1)
              {
                TEQUIV_IN[name2.first.substr(0, pos) + name.first] = name2.second;
              }
              pos = isSubtree(name.second, name2.second);
              if (pos > 0)
              {
                TEQUIV_IN[name2.first] = name2.second.substr(0, pos) + name.first;
              }
            }
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

    llvm::json::Object output;

    for (int i = 0; i < stmts.size(); i++)
    {
      llvm::json::Object stmt;

      llvm::json::Array tref;
      for (auto &name : stmts[i]->TREF)
      {
        tref.push_back(name);
      }
      stmt["TREF"] = llvm::json::Value(std::move(tref));

      llvm::json::Array tgen;
      for (auto &name : stmts[i]->TGEN)
      {
        tgen.push_back(name);
      }
      stmt["TGEN"] = llvm::json::Value(std::move(tgen));

      llvm::json::Array DEP;
      for (auto &dep : stmts[i]->FLOW_DEP)
      {
        llvm::json::Object dep_obj;
        dep_obj["var"] = dep.var;
        dep_obj["src"] = dep.src + 1;
        dep_obj["dst"] = dep.dst + 1;
        dep_obj["type"] = "flow";
        DEP.push_back(std::move(dep_obj));
      }

      for (auto &dep : stmts[i]->OUT_DEP)
      {
        llvm::json::Object dep_obj;
        dep_obj["var"] = dep.var;
        dep_obj["src"] = dep.src + 1;
        dep_obj["dst"] = dep.dst + 1;
        dep_obj["type"] = "output";
        DEP.push_back(std::move(dep_obj));
      }

      stmt["DEP"] = llvm::json::Value(std::move(DEP));

      llvm::json::Object tdef;
      for (auto &name : stmts[i]->TDEF)
      {
        llvm::json::Object def_obj;
        tdef[name.first] = name.second + 1;
      }
      stmt["TDEF"] = llvm::json::Value(std::move(tdef));

      llvm::json::Array tequiv;
      for (auto &name : stmts[i]->TEQUIV)
      {
        llvm::json::Array equiv_obj;
        equiv_obj.push_back(name.first);
        equiv_obj.push_back(name.second);
        tequiv.push_back(std::move(equiv_obj));
      }
      stmt["TEQUIV"] = llvm::json::Value(std::move(tequiv));

      output["S" + std::to_string(i + 1)] = llvm::json::Value(std::move(stmt));
    }

    std::error_code ec;
    raw_fd_ostream file(filename, ec, sys::fs::FileAccess::FA_Write);
    file << llvm::json::Value(std::move(output));
    file.close();
    // int i = 0;
    // for (auto &stmt : stmts)
    // {
    //   errs() << "S" << (i++) + 1 << ":--------------------\n";
    //   errs() << "TREF: {";
    //   for (auto &name : stmt->TREF)
    //   {
    //     errs() << name;
    //     if (name != *stmt->TREF.rbegin() && stmt->TREF.size() > 1)
    //     {
    //       errs() << ", ";
    //     }
    //   }
    //   errs() << "}\n";
    //   errs() << "TGEN: {";
    //   for (auto &name : stmt->TGEN)
    //   {
    //     errs() << name;
    //     if (name != *stmt->TGEN.rbegin() && stmt->TGEN.size() > 1)
    //     {
    //       errs() << ", ";
    //     }
    //   }
    //   errs() << "}\n";
    //   errs() << "DEP: {";
    //   int first = 1;
    //   for (auto &dep : stmt->FLOW_DEP)
    //   {
    //     if (first)
    //     {
    //       first = 0;
    //       errs() << "\n";
    //     }
    //     errs() << "   " << dep.var << ": S" << dep.src + 1 << "--->S" << dep.dst + 1 << "\n";
    //   }
    //   for (auto &dep : stmt->OUT_DEP)
    //   {
    //     if (first)
    //     {
    //       first = 0;
    //       errs() << "\n";
    //     }
    //     errs() << "   " << dep.var << ": S" << dep.src + 1 << "-O->S" << dep.dst + 1 << "\n";
    //   }
    //   errs() << "}\n";
    //   errs() << "TDEF: {";
    //   for (auto &name : stmt->TDEF)
    //   {
    //     errs() << '(' << name.first << ", S" << name.second + 1 << ")";
    //     if (name != *stmt->TDEF.rbegin() && stmt->TDEF.size() > 1)
    //     {
    //       errs() << ", ";
    //     }
    //   }
    //   errs() << "}\n";
    //   errs() << "TEQUIV: {";
    //   for (auto &name : stmt->TEQUIV)
    //   {
    //     errs() << '(' << name.first << ", " << name.second << ")";
    //     if (name != *stmt->TEQUIV.rbegin() && stmt->TEQUIV.size() > 1)
    //     {
    //       errs() << ", ";
    //     }
    //   }
    //   errs() << "}\n\n";
    // }

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
