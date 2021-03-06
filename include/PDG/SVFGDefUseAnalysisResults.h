#pragma once

#include "PDG/PDG/DefUseResults.h"

#include <unordered_map>
#include <unordered_set>

class SVFG;
class SVFGNode;

namespace pdg {

class SVFGDefUseAnalysisResults : public DefUseResults
{
public:
    explicit SVFGDefUseAnalysisResults(SVFG* svfg);
    
    SVFGDefUseAnalysisResults(const SVFGDefUseAnalysisResults& ) = delete;
    SVFGDefUseAnalysisResults(SVFGDefUseAnalysisResults&& ) = delete;
    SVFGDefUseAnalysisResults& operator =(const SVFGDefUseAnalysisResults& ) = delete;
    SVFGDefUseAnalysisResults& operator =(SVFGDefUseAnalysisResults&& ) = delete;

public:
    virtual DefSite getDefNode(llvm::Value* value) override;

private:
    SVFGNode* getSVFGNode(llvm::Value* value);
    std::unordered_set<SVFGNode*> getSVFGDefNodes(SVFGNode* svfgNode, std::unordered_set<SVFGNode*>& processedNodes);
    DefSite getPdgDefNode(const std::unordered_set<SVFGNode*>& svfgDefNodes);
    PDGNodeTy getNode(SVFGNode* svfgNode);

private:
    SVFG* m_svfg;
    std::unordered_map<llvm::Value*, DefSite> m_valueDefSite;
}; // class SVFGDefUseAnalysisResults

} // namespace pdg

