#pragma once

#include "llvm/IR/InstVisitor.h"

#include <memory>
#include <unordered_set>
#include <functional>

namespace llvm {

class CallSite;
class MemorySSA;
class Module;
class Function;
class Value;

}


namespace pdg {

class PDG;
class PDGNode;
class FunctionPDG;
class DefUseResults;
class DominanceResults;
class IndirectCallSiteResults;

class PDGBuilder : public llvm::InstVisitor<PDGBuilder>
{
public:
    using PDGType = std::shared_ptr<PDG>;
    using FunctionPDGTy = std::shared_ptr<FunctionPDG>;
    using DefUseResultsTy = std::shared_ptr<DefUseResults>;
    using IndCSResultsTy = std::shared_ptr<IndirectCallSiteResults>;
    using DominanceResultsTy = std::shared_ptr<DominanceResults>;
    using PDGNodeTy = std::shared_ptr<PDGNode>;
    using FunctionSet = std::unordered_set<llvm::Function*>;

public:
    explicit PDGBuilder(llvm::Module* M);

    virtual ~PDGBuilder() = default;
    PDGBuilder(const PDGBuilder& ) = delete;
    PDGBuilder(PDGBuilder&& ) = delete;
    PDGBuilder& operator =(const PDGBuilder& ) = delete;
    PDGBuilder& operator =(PDGBuilder&& ) = delete;
 
    void build();

public:
    void setDesUseResults(DefUseResultsTy defUse);
    void setIndirectCallSitesResults(IndCSResultsTy indCSResults);
    void setDominanceResults(DominanceResultsTy domResults);

    PDGType getPDG()
    {
        return std::move(m_pdg);
    }

public:
    /// visit overrides
    // TODO: those are instructions that seems to be interesting to handle separately. 
    // Add more if necessary
    void visitBranchInst(llvm::BranchInst& I);
    void visitLoadInst(llvm::LoadInst& I);
    void visitStoreInst(llvm::StoreInst& I);
    void visitGetElementPtrInst(llvm::GetElementPtrInst& I);
    void visitPhiNode(llvm::PHINode& I);
    void visitMemSetInst(llvm::MemSetInst& I);
    void visitMemCpyInst(llvm::MemCpyInst& I);
    void visitMemMoveInst(llvm::MemMoveInst &I);
    void visitMemTransferInst(llvm::MemTransferInst &I);
    void visitMemIntrinsic(llvm::MemIntrinsic &I);
    void visitCallInst(llvm::CallInst& I);
    void visitInvokeInst(llvm::InvokeInst& I);
    void visitTerminatorInst(llvm::Instruction& I);
    void visitReturnInst(llvm::ReturnInst& I);

    // all instructions not handled individually will get here
    void visitInstruction(llvm::Instruction& I);

protected:
    virtual PDGNodeTy createInstructionNodeFor(llvm::Instruction* instr);
    virtual PDGNodeTy createBasicBlockNodeFor(llvm::BasicBlock* block);
    virtual PDGNodeTy createGlobalNodeFor(llvm::GlobalVariable* global);
    virtual PDGNodeTy createFormalArgNodeFor(llvm::Argument* arg);
    virtual PDGNodeTy createNullNode();
    virtual PDGNodeTy createConstantNodeFor(llvm::Constant* constant);

private:
    void buildFunctionPDG(llvm::Function* F);
    void buildFunctionDefinition(llvm::Function* F);
    void visitGlobals();
    void visitFormalArguments(FunctionPDG* functionPDG, llvm::Function* F);
    void visitBlock(llvm::BasicBlock& B);
    void visitBlockInstructions(llvm::BasicBlock& B);
    PDGNodeTy getInstructionNodeFor(llvm::Instruction* instr);
    PDGNodeTy getNodeFor(llvm::Value* value);
    PDGNodeTy getNodeFor(llvm::BasicBlock* block);
    void addControlEdgesForBlock(llvm::BasicBlock& B);
    void selfVisitCallSite(llvm::CallSite& callSite);
    void addDataEdge(PDGNodeTy source, PDGNodeTy dest);
    void addControlEdge(PDGNodeTy source, PDGNodeTy dest);
    void connectToDefSite(llvm::Value* value, PDGNodeTy valueNode);
    void addActualArgumentNodeConnections(PDGNodeTy actualArgNode,
                                          unsigned argIdx,
                                          const llvm::CallSite& cs,
                                          const FunctionSet& callees);
    void addPhiNodeConnections(PDGNodeTy node);

protected:
    PDGType m_pdg;
    FunctionPDGTy m_currentFPDG;

private:
    llvm::Module* m_module;
    DefUseResultsTy m_defUse;
    IndCSResultsTy m_indCSResults;
    DominanceResultsTy m_domResults;
}; // class PDGBuilder

} // namespace pdg

