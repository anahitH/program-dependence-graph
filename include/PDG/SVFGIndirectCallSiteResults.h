#pragma once

#include "PDG/PDG/IndirectCallSiteResults.h"

class PTACallGraph;

namespace pdg {

class SVFGIndirectCallSiteResults : public IndirectCallSiteResults
{
public:
    using FunctionSet = IndirectCallSiteResults::FunctionSet;

public:
    explicit SVFGIndirectCallSiteResults(PTACallGraph* ptaGraph);

    virtual bool hasIndCSCallees(const llvm::CallSite& callSite) const override;
    virtual FunctionSet getIndCSCallees(const llvm::CallSite& callSite) override;

private:
    PTACallGraph* m_ptaGraph;
}; // class SVFGIndirectCallSiteResults

} // namespace pdg

