#include "PDG/DGDefUseAnalysisResults.h"
#include "PDG/PDGLLVMNode.h"

#include "llvm/IR/Module.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/raw_os_ostream.h"

#include "dg/llvm/LLVMDependenceGraphBuilder.h"
#include "dg/llvm/analysis/DefUse/DefUse.h"
#include "dg/llvm/analysis/PointsTo/PointerAnalysis.h"
#include "dg/llvm/analysis/ReachingDefinitions/ReachingDefinitions.h"
#include "dg/analysis/PointsTo/PointerAnalysisFS.h"
#include "dg/analysis/PointsTo/PointerAnalysisFI.h"
#include "dg/analysis/PointsTo/PointerAnalysisFSInv.h"

namespace pdg {

DGDefUseAnalysisResults::DGDefUseAnalysisResults(llvm::Module* M)
    : m_module(M)
{
    dg::llvmdg::LLVMDependenceGraphOptions options;
    options.PTAOptions.analysisType = dg::LLVMPointerAnalysisOptions::AnalysisType::fi;
    options.RDAOptions.analysisType = dg::analysis::LLVMReachingDefinitionsAnalysisOptions::AnalysisType::dense;
    m_pta.reset(new dg::LLVMPointerAnalysis(M, options.PTAOptions));
    m_rd.reset(new dg::LLVMReachingDefinitions(M, m_pta.get(), options.RDAOptions));

    m_pta->run<dg::analysis::pta::PointerAnalysisFI>();
    m_rd->run<dg::analysis::rd::ReachingDefinitionsAnalysis>();
}

DefUseResults::DefSite DGDefUseAnalysisResults::getDefNode(llvm::Value* value)
{
    auto pos = m_valueDefSite.find(value);
    if (pos != m_valueDefSite.end()) {
        return pos->second;
    }
    DefSite nulldefSite(nullptr, PDGNodeTy());
    auto* pts = getPointsTo(value);
    if (!pts) {
        m_valueDefSite.insert(std::make_pair(value, nulldefSite));
        return nulldefSite;
    }
    m_valueDefSite.insert(std::make_pair(value, nulldefSite));
    auto defNode = getPdgDefNode(value, pts);
    m_valueDefSite.insert(std::make_pair(value, defNode));
    return defNode;
}

dg::analysis::pta::PSNode* DGDefUseAnalysisResults::getPointsTo(llvm::Value* value)
{
    if (auto* load = llvm::dyn_cast<llvm::LoadInst>(value)) {
        return m_pta->getPointsTo(load->getPointerOperand());
    }
    // TODO: check if there are instructions that need to be handled separately
    return m_pta->getPointsTo(value);
}

DefUseResults::DefSite DGDefUseAnalysisResults::getPdgDefNode(llvm::Value* value, dg::analysis::pta::PSNode* pts)
{
    llvm::DataLayout dl(m_module);
    const auto size = dl.getTypeAllocSize(value->getType());
    llvm::Value* defValue = nullptr;
    PDGNodeTy defNode;
    std::vector<llvm::Value*> values;
    std::vector<llvm::BasicBlock*> blocks;

    auto *mem = m_rd->getMapping(value);
    for (const auto& ptr : pts->pointsTo) {
        if (!ptr.isValid() || ptr.isInvalidated()) {
            continue;
        }
        collectValuesAndBlocks(ptr, mem, size, values, blocks);
    }
    assert(values.size() == blocks.size());
    if (values.size() == 1) {
        if (auto* instr = llvm::dyn_cast<llvm::Instruction>(values[0])) {
            defNode = PDGNodeTy(new PDGLLVMInstructionNode(instr));
            defValue = values[0];
        }
    } else {
        defNode.reset(new PDGPhiNode(values, blocks));
    }
    return DefSite(defValue, defNode);
}

void DGDefUseAnalysisResults::collectValuesAndBlocks(const dg::analysis::pta::Pointer& ptr,
                                                     dg::analysis::rd::RDNode* rdNode,
                                                     unsigned size,
                                                     std::vector<llvm::Value*>& values,
                                                     std::vector<llvm::BasicBlock*>& blocks)
{
    llvm::Value* llvmVal = ptr.target->getUserData<llvm::Value>();
    if (!llvmVal) {
        return;
    }
    auto* val = m_rd->getNode(llvmVal);
    std::set<dg::analysis::rd::RDNode *> defs;
    rdNode->getReachingDefinitions(val, ptr.offset, size, defs);
    for (auto *rd : defs) {
        if (rd->isUnknown()) {
            continue;
        }
        if (rd->getType() != dg::analysis::rd::RDNodeType::PHI) {
            auto* value = rd->getUserData<llvm::Value>();
            if (auto* inst = llvm::dyn_cast<llvm::Instruction>(value)) {
                values.push_back(value);
                blocks.push_back(inst->getParent());
            } else {
                llvm::dbgs() << "No parent block for value " << *value << " definitions of " << *llvmVal << "\n";
            }
        }
    }
}


} // namespace pdg

