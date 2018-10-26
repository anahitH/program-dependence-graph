#pragma once

#include <unordered_set>

namespace llvm {

class CallSite;
class Function;
} // namespace llvm

namespace pdg {

/// Interface to query callees for indirect call sites
class IndirectCallSiteResults
{
public:
    using FunctionSet = std::unordered_set<llvm::Function*>;

public:
    virtual ~IndirectCallSiteResults() {}

    virtual bool hasIndCSCallees(const llvm::CallSite& callSite) const = 0;
    virtual FunctionSet getIndCSCallees(const llvm::CallSite& callSite) = 0;
}; // class IndirectCallSiteResults

} // namespace pdg

