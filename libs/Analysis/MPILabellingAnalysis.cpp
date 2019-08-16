
#include "morpheus/Analysis/MPILabellingAnalysis.hpp"

#include <cassert>


using namespace llvm;

// -------------------------------------------------------------------------- //
// MPILabellingAnalysis

MPILabelling
MPILabellingAnalysis::run (Module &m, ModuleAnalysisManager &mam) {

  CallGraph &cg = mam.getResult<CallGraphAnalysis>(m);
  return MPILabelling(cg);
}

// provide definition of the analysis Key
AnalysisKey MPILabellingAnalysis::Key;


// -------------------------------------------------------------------------- //
// MPILabelling

MPILabelling::MPILabelling(CallGraph &cg) {

  // There is no root node
  for (const auto &node : cg) {
    CallGraphNode const *cgn = node.second.get();
    if (cgn->getFunction()) { // explore only function node TODO: check whether is it enough to ignore non-function calls or the reaction should be "better"?
      explore_cgnode(cgn);
    }
  }
}

// Public API --------------------------------------------------------------- //

Instruction *MPILabelling::get_unique_call(StringRef name) const {

  auto search = mpi_calls.find(name);
  if (search == mpi_calls.end()) {
    return nullptr;
  }

  std::vector<CallSite> const &calls = search->second;
  assert(calls.size() == 1 && "Expect single call.");

  return calls[0].getInstruction();
}

std::vector<Instruction *> MPILabelling::get_calls(StringRef name) const {
  auto search = mpi_calls.find(name);
  if (search == mpi_calls.end()) {
    return {}; // empty list
  }

  std::vector<CallSite> const &calls = search->second;
  std::vector<Instruction *> instrs(calls.size());
  std::generate(
    instrs.begin(),
    instrs.end(),
    [it = calls.begin()] () mutable {
      return (it++)->getInstruction();
    });
  return instrs;
}

bool MPILabelling::is_sequential(Function const *f) const {
  return check_status<SEQUENTIAL>(f);
}

bool MPILabelling::is_mpi_involved(Function const *f) const {
  return (check_status<MPI_INVOLVED>(f) ||
          check_status<MPI_INVOLVED_MEDIATELY>(f));
}

MPILabelling::MPICheckpoints
MPILabelling::get_mpi_checkpoints(BasicBlock const *bb) const {
  auto search = bb_mpi_checkpoints.find(bb);
  if (search == bb_mpi_checkpoints.end()) {
    return MPICheckpoints();
  }

  return search->second;
}

// Private methods ---------------------------------------------------------- //

MPILabelling::ExplorationState
MPILabelling::explore_cgnode(CallGraphNode const *cgn) {
  Function *f = cgn->getFunction();
  if (!f) { // external call
    return EXTERNAL;
  }

  auto it = fn_labels.find(f);
  if (it != fn_labels.end()) {
    return it->getSecond();
  }

  if (f->hasName() && f->getName().startswith("MPI_")) {
    fn_labels[f] = MPI_CALL;
    return MPI_CALL;
  }

  // NOTE: store the info that the function is currently being processed.
  //       This helps to resolve recursive calls as it immediately returns
  //       PROCESSING status.

  // TODO: => TEST: make a test to simple function calling itself. => it has to end with sequential
  fn_labels[f] = PROCESSING;

  ExplorationState res_es = SEQUENTIAL;

  for (const CallGraphNode::CallRecord &cr : *cgn) {
    ExplorationState inner_es = SEQUENTIAL;

    if (cr.first == nullptr) { // calling external node => black (opaque) transition in the MPN
      inner_es = EXTERNAL;
    } else {
      CallSite call_site(cr.first);
      CallGraphNode *called_cgn = cr.second;

      ExplorationState const &es = explore_cgnode(called_cgn);
      switch(es) {
      case MPI_CALL:
        mpi_calls[call_site.getCalledFunction()->getName()].push_back(call_site);
        inner_es = MPI_INVOLVED;
        save_checkpoint(call_site, MPICallType::DIRECT);
        break;
      case MPI_INVOLVED:
      case MPI_INVOLVED_MEDIATELY:
        inner_es = MPI_INVOLVED_MEDIATELY;
        save_checkpoint(call_site, MPICallType::INDIRECT);
        break;
      case PROCESSING:
      case SEQUENTIAL:
      case EXTERNAL:
        // do nothing
        break;
      }
    }

    if (res_es < inner_es) {
      res_es = inner_es;
    }
  }

  fn_labels[f] = res_es; // set the resulting status
  return res_es;
}

void MPILabelling::save_checkpoint(CallSite cs, MPICallType call_type) {
  BasicBlock *bb = cs->getParent();
  assert(bb != nullptr && "Null parent of instruction.");

  bb_mpi_checkpoints[bb].emplace(cs, call_type);
}
