
#include "testing_pass.hpp"

using namespace llvm;

// Provide a definition for the static object used to identify passes.
AnalysisKey TestingAnalysisPass::Key;

PreservedAnalyses TestingFnPass::run(Function &F, FunctionAnalysisManager &FAM) {

  auto &res = FAM.getResult<TestingAnalysisPass>(F);
  errs() << "TFP fn: " << F.getName()
         << " result: " << res.result
         << "\n";
  return PreservedAnalyses::all();
}
