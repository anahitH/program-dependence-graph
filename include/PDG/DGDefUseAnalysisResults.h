#pragma once

#include "PDG/DefUseResults.h"

#include "dg/llvm/analysis/PointsTo/PointerAnalysis.h"
#include "dg/llvm/analysis/ReachingDefinitions/ReachingDefinitions.h"

#include <memory>

namespace llvm {
class Module;
}

namespace pdg {

class DGDefUseAnalysisResults : public DefUseResults
{
public:
    explicit DGDefUseAnalysisResults(llvm::Module* M);

    DGDefUseAnalysisResults(const DGDefUseAnalysisResults& ) = delete;
    DGDefUseAnalysisResults(DGDefUseAnalysisResults&& ) = delete;
    DGDefUseAnalysisResults& operator =(const DGDefUseAnalysisResults& ) = delete;
    DGDefUseAnalysisResults& operator =(DGDefUseAnalysisResults&& ) = delete;


public:
    virtual DefSite getDefNode(llvm::Value* value) override;

private:
    dg::analysis::pta::PSNode* getPointsTo(llvm::Value* value);
    DefSite getPdgDefNode(llvm::Value* value, dg::analysis::pta::PSNode* pts);
    void collectValuesAndBlocks(const dg::analysis::pta::Pointer& ptr,
                                dg::analysis::rd::RDNode* rdNode,
                                unsigned size,
                                std::vector<llvm::Value*>& values,
                                std::vector<llvm::BasicBlock*>& blocks);

private:
    llvm::Module* m_module;
    std::unique_ptr<dg::LLVMPointerAnalysis> m_pta;
    std::unique_ptr<dg::analysis::rd::LLVMReachingDefinitions> m_rd;
    std::unordered_map<llvm::Value*, DefSite> m_valueDefSite;
}; // class DGDefUseAnalysisResults

} // namespace pdg

