#include "llvm/IR/PassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "fstream"

using namespace llvm;

namespace
{
  // struct for the statement
  struct Stmt
  {
    struct array *rhs;
    struct array *lhs;
  };

  // arr[c * i + d];
  struct array
  {
    std::string name;
    int c;
    int d;
  };

  /*
  The solution vector is represented as,

      X= X0 + XCoeffT *t;
      y= Y0 + YCoeffT *t;

  */
  struct SolEquStru
  {
    int is_there_solution;
    int X0;
    int XCoeffT;
    int Y0;
    int YCoeffT;
  };

  /*
  The TriStru is used to hold the gcd information.
    Let gcd = GCD(a,b)  ===> a*x+b*y = gcd
  */
  struct TriStru
  {
    int gcd;
    int x;
    int y;
  };

  /*

  The main subroutine, given a, b,
  return x, y, g, where a*x+b*y =g;

  */
  struct TriStru Extended_Euclid(int a, int b)
  {
    struct TriStru Tri1, Tri2;
    if (b == 0)
    {
      Tri1.gcd = a;
      Tri1.x = 1;
      Tri1.y = 0;
      return Tri1;
    }
    Tri2 = Extended_Euclid(b, (a % b));
    Tri1.gcd = Tri2.gcd;
    Tri1.x = Tri2.y;
    Tri1.y = Tri2.x - (a / b) * Tri2.y;
    return Tri1;
  }

  /*
    Solve the diophatine equation by given a,b, and c, where
      a*x+b*y =c

    return

      X= X0 + XCoeffT *t;
      y= Y0 + YCoeffT *t;

  */
  struct SolEquStru diophatine_solver(int a, int b, int c)
  {
    int k;
    struct TriStru Triple;
    struct SolEquStru s;

    Triple = Extended_Euclid(a, b);
    // printf("a,b,g, x,y= %d %d %d %d %d \n", a, b, Triple.gcd, Triple.x, Triple.y);
    if ((c % Triple.gcd) == 0)
    {
      k = c / Triple.gcd;
      s.X0 = Triple.x * k;
      s.XCoeffT = (b / Triple.gcd);
      s.Y0 = Triple.y * k;
      s.YCoeffT = ((-a) / Triple.gcd);
      s.is_there_solution = 1;
    }
    else
      s.is_there_solution = 0;

    return (s);
  }

  int Min(int a, int b)
  {
    if (a <= b)
    {
      return a;
    }
    else
    {
      return b;
    }
  }
  int Max(int a, int b)
  {
    if (a >= b)
    {
      return a;
    }
    else
    {
      return b;
    }
  }

  void swap(int *a, int *b)
  {
    int temp = *a;
    *a = *b;
    *b = temp;
  }

  struct HW1Pass : public PassInfoMixin<HW1Pass>
  {
    PreservedAnalyses run(Function &F, FunctionAnalysisManager &FAM);
  };

  PreservedAnalyses HW1Pass::run(Function &F, FunctionAnalysisManager &FAM)
  {
    // errs() << F.getParent()->getSourceFileName() << '\n';
    std::string filename = F.getParent()->getSourceFileName();
    filename.replace(filename.find("."), 4, ".json");

    std::vector<struct Stmt *> stmts;
    stmts.push_back(new struct Stmt());
    struct array *arr = new struct array();
    arr->c = 1;
    arr->d = 0;
    int statment_id = 0;

    for (BasicBlock &BB : F)
    {
      // errs() << "BasicBlock: " << BB.getName() << '\n';
      if (BB.getName() == "for.body")
        for (Instruction &I : BB)
        {
          if (auto *op = dyn_cast<BinaryOperator>(&I))
          {
            switch (op->getOpcode())
            {
            case Instruction::Add:
              arr->d = dyn_cast<ConstantInt>(op->getOperand(1))->getZExtValue();
              // errs() << arr->d << "Add\n";
              break;
            case Instruction::Sub:
              arr->d = -(dyn_cast<ConstantInt>(op->getOperand(1))->getZExtValue());
              // errs() << arr->d << " Sub\n";
              break;
            case Instruction::Mul:
              arr->c = dyn_cast<ConstantInt>(op->getOperand(0))->getZExtValue();
              // errs() << "Mul\n";
              break;
            default:
              errs() << "Other\n";
            }
          }

          else if (auto *op = dyn_cast<SExtInst>(&I))
          {
          }
          else if (auto *op = dyn_cast<GetElementPtrInst>(&I))
          {
            arr->name = op->getPointerOperand()->getName();
            // errs() << op->getPointerOperand()->getName()
            //        << "GetElementPtrInst: " << *op << '\n';
          }
          else if (auto *op = dyn_cast<LoadInst>(&I))
          {
            // errs() << op->getPointerOperand()->getName() << "LoadInst: " << *op << '\n';
            stmts[statment_id]->rhs = arr;
            arr = new struct array();
            arr->c = 1;
            arr->d = 0;
          }
          else if (auto *op = dyn_cast<StoreInst>(&I))
          {
            // errs() << "StoreInst: " << *op << '\n';
            stmts[statment_id++]->lhs = arr;
            stmts.push_back(new struct Stmt());
            stmts[statment_id]->rhs = NULL;
            stmts[statment_id]->lhs = NULL;
            arr = new struct array();
            arr->c = 1;
            arr->d = 0;
          }
        }
    }
    std::free(stmts[statment_id]);
    stmts.pop_back();

    // for (int i = 0; i < statment_id; i++)
    // {
    //   errs() << "Statement " << i << '\n';
    //   if (stmts[i]->lhs != NULL)
    //   {
    //     errs() << "write: " << stmts[i]->lhs->name << " " << stmts[i]->lhs->c << " " << stmts[i]->lhs->d << '\n';
    //   }
    //   if (stmts[i]->rhs != NULL)
    //   {
    //     errs() << "read: " << stmts[i]->rhs->name << " " << stmts[i]->rhs->c << " " << stmts[i]->rhs->d << '\n';
    //   }
    // }

    auto &LI = FAM.getResult<LoopAnalysis>(F);
    auto &SE = FAM.getResult<ScalarEvolutionAnalysis>(F);
    int lb = 0, ub = 0;

    for (auto &L : LI)
    {
      auto b = L->getBounds(SE);
      lb = (dyn_cast<ConstantInt>(&b->getInitialIVValue()))->getZExtValue();
      ub = (dyn_cast<ConstantInt>(&b->getFinalIVValue()))->getZExtValue() - 1;
      // errs() << "Loop range: " << '[' << lb << ", " << ub << ")\n";
    }

    // errs() << LI << '\n';
    int xt_lb, xt_ub, yt_lb, yt_ub, t_lb, t_ub;

    std::string result = "{\n \"FlowDependence\": [";
    int flag = 0;
    for (int i = 0; i < statment_id; i++)
    {
      for (int j = 0; j < statment_id; j++)
      {
        if (stmts[i]->lhs != NULL && stmts[j]->rhs != NULL)
        {
          if (stmts[i]->lhs->name == stmts[j]->rhs->name)
          {
            struct SolEquStru f;
            f = diophatine_solver(stmts[i]->lhs->c, -stmts[j]->rhs->c, stmts[j]->rhs->d - stmts[i]->lhs->d);
            if (f.is_there_solution)
            {
              xt_lb = (lb - f.X0) / f.XCoeffT;
              xt_ub = (ub - f.X0) / f.XCoeffT;
              yt_lb = (lb - f.Y0) / f.YCoeffT;
              yt_ub = (ub - f.Y0) / f.YCoeffT;
              if (f.XCoeffT < 0)
                swap(&xt_lb, &xt_ub);
              if (f.YCoeffT < 0)
                swap(&yt_lb, &yt_ub);
              t_lb = Max(xt_lb, yt_lb);
              t_ub = Min(xt_ub, yt_ub);
              // errs() << t_lb << " " << t_ub << '\n';

              for (int k = t_lb; k <= t_ub; k++)
              {
                int x = f.X0 + f.XCoeffT * k;
                int y = f.Y0 + f.YCoeffT * k;
                // errs() << "X: " << x << " Y: " << y << '\n';
                if (x < y || (x == y && j > i))
                {
                  if (flag)
                    result += ",";
                  result += "\n   {\n    \"array\": \"" + stmts[i]->lhs->name + "\",\n";
                  result += "    \"src_stmt\": " + std::to_string(i + 1) + ",\n";
                  result += "    \"src_idx\": " + std::to_string(x) + ",\n";
                  result += "    \"dst_stmt\": " + std::to_string(j + 1) + ",\n";
                  result += "    \"dst_idx\": " + std::to_string(y) + "\n   }";
                  flag = 1;
                }
              }
            }
          }
        }
      }
    }
    if (flag)
      result += "\n ";
    flag = 0;
    result += "],\n \"AntiDependence\": [";
    for (int i = 0; i < statment_id; i++)
    {
      for (int j = 0; j < statment_id; j++)
      {
        if (stmts[i]->lhs != NULL && stmts[j]->rhs != NULL)
        {
          if (stmts[i]->rhs->name == stmts[j]->lhs->name)
          {
            struct SolEquStru f;
            f = diophatine_solver(stmts[i]->rhs->c, -stmts[j]->lhs->c, stmts[j]->lhs->d - stmts[i]->rhs->d);
            if (f.is_there_solution)
            {
              xt_lb = (lb - f.X0) / f.XCoeffT;
              xt_ub = (ub - f.X0) / f.XCoeffT;
              yt_lb = (lb - f.Y0) / f.YCoeffT;
              yt_ub = (ub - f.Y0) / f.YCoeffT;
              if (f.XCoeffT < 0)
                swap(&xt_lb, &xt_ub);
              if (f.YCoeffT < 0)
                swap(&yt_lb, &yt_ub);
              t_lb = Max(xt_lb, yt_lb);
              t_ub = Min(xt_ub, yt_ub);
              int flag = 0;
              for (int k = t_lb; k <= t_ub; k++)
              {
                int x = f.X0 + f.XCoeffT * k;
                int y = f.Y0 + f.YCoeffT * k;
                if (x < y || (x == y && i < j))
                {
                  if (flag)
                    result += ",";
                  result += "\n   {\n    \"array\": \"" + stmts[i]->lhs->name + "\",\n";
                  result += "    \"src_stmt\": " + std::to_string(i + 1) + ",\n";
                  result += "    \"src_idx\": " + std::to_string(x) + ",\n";
                  result += "    \"dst_stmt\": " + std::to_string(j + 1) + ",\n";
                  result += "    \"dst_idx\": " + std::to_string(y) + "\n   }";
                  flag = 1;
                }
              }
            }
          }
        }
      }
    }
    if (flag)
      result += "\n ";
    flag = 0;
    result += "],\n \"OutputDependence\": [";

    for (int i = 0; i < statment_id; i++)
    {
      for (int j = 0; j < statment_id; j++)
      {
        if (stmts[i]->lhs != NULL && stmts[j]->lhs != NULL)
        {
          if (stmts[i]->lhs->name == stmts[j]->lhs->name)
          {
            struct SolEquStru f;
            f = diophatine_solver(stmts[i]->lhs->c, -stmts[j]->lhs->c, stmts[j]->lhs->d - stmts[i]->lhs->d);
            if (f.is_there_solution)
            {
              xt_lb = (lb - f.X0) / f.XCoeffT;
              xt_ub = (ub - f.X0) / f.XCoeffT;
              yt_lb = (lb - f.Y0) / f.YCoeffT;
              yt_ub = (ub - f.Y0) / f.YCoeffT;
              if (f.XCoeffT < 0)
                swap(&xt_lb, &xt_ub);
              if (f.YCoeffT < 0)
                swap(&yt_lb, &yt_ub);
              t_lb = Max(xt_lb, yt_lb);
              t_ub = Min(xt_ub, yt_ub);
              int flag = 0;
              for (int k = t_lb; k <= t_ub; k++)
              {
                int x = f.X0 + f.XCoeffT * k;
                int y = f.Y0 + f.YCoeffT * k;
                if (x < y || (x == y && i < j))
                {
                  if (flag)
                    result += ",";
                  result += "\n   {\n    \"array\": \"" + stmts[i]->lhs->name + "\",\n";
                  result += "    \"src_stmt\": " + std::to_string(i + 1) + ",\n";
                  result += "    \"src_idx\": " + std::to_string(x) + ",\n";
                  result += "    \"dst_stmt\": " + std::to_string(j + 1) + ",\n";
                  result += "    \"dst_idx\": " + std::to_string(y) + "\n   }";
                  flag = 1;
                }
              }
            }
          }
        }
      }
    }
    if (flag)
      result += "\n ";
    result += "]\n}";
    // errs() << result << '\n';

    std::ofstream file(filename);
    file << result;
    file.close();

    return PreservedAnalyses::all();
  }

} // end anonymous namespace

extern "C" ::llvm::PassPluginLibraryInfo LLVM_ATTRIBUTE_WEAK
llvmGetPassPluginInfo()
{
  return {LLVM_PLUGIN_API_VERSION, "HW1Pass", LLVM_VERSION_STRING,
          [](PassBuilder &PB)
          {
            PB.registerPipelineParsingCallback(
                [](StringRef Name, FunctionPassManager &FPM,
                   ArrayRef<PassBuilder::PipelineElement>)
                {
                  if (Name == "hw1")
                  {
                    FPM.addPass(HW1Pass());
                    return true;
                  }
                  return false;
                });
          }};
}
