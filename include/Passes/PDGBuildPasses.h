#pragma once

#include "llvm/Pass.h"

#include <memory>

namespace pdg {

class PDG;

/// LLVM pass to build PDG from SVFG
class SVFGPDGBuilder : public llvm::ModulePass
{
public:
    using PDGType = std::shared_ptr<PDG>;

public:
    static char ID;
    SVFGPDGBuilder()
        : llvm::ModulePass(ID)
    {
    }

    void getAnalysisUsage(llvm::AnalysisUsage& AU) const override;
    bool runOnModule(llvm::Module& M) override;

    PDGType getPDG()
    {
        return m_pdg;
    }

private:
    PDGType m_pdg;
};

/// LLVM pass to build PDG from DG
class LLVMPDGBuilder : public llvm::ModulePass
{
public:
    using PDGType = std::shared_ptr<PDG>;

public:
    static char ID;
    LLVMPDGBuilder()
        : llvm::ModulePass(ID)
    {
    }

    void getAnalysisUsage(llvm::AnalysisUsage& AU) const override;
    bool runOnModule(llvm::Module& M) override;

    PDGType getPDG()
    {
        return m_pdg;
    }

private:
    PDGType m_pdg;

};

}

