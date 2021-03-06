#pragma once

#include <memory>
#include <vector>

namespace llvm {

class Value;
} // namespace llvm

namespace pdg {

class PDGNode;

/// Interface to query def-use results
class DefUseResults
{
public:
    using PDGNodeTy = std::shared_ptr<PDGNode>;
    using PDGNodes = std::vector<PDGNodeTy>;
    using DefSite = std::pair<llvm::Value*, PDGNodeTy>;

public:
    virtual ~DefUseResults() {}

    virtual DefSite getDefNode(llvm::Value* value) = 0;
}; // class DefUseResults

} // namespace pdg

