#pragma once

#include <memory>
#include <unordered_map>

#include "PDGLLVMNode.h"

namespace llvm {

class Module;
class Function;
class GlobalVariable;
} // namespace llvm

namespace pdg {

class FunctionPDG;
class PDGLLVMGlobalVariableNode;
class PDGLLVMFunctionNode;

/// Program Dependence Graph
class PDG
{
public:
    // TODO: consider collecting all nodes in one map with Value key
    using PDGNodeTy = std::shared_ptr<PDGNode>;
    using PDGFunctionNodeTy = std::shared_ptr<PDGLLVMFunctionNode>;
    using GlobalVariableNodes = std::unordered_map<llvm::GlobalVariable*, PDGNodeTy>;
    using FunctionNodes = std::unordered_map<llvm::Function*, PDGNodeTy>;
    using FunctionPDGTy = std::shared_ptr<FunctionPDG>;
    using FunctionPDGs = std::unordered_map<llvm::Function*, FunctionPDGTy>;

public:
    explicit PDG(llvm::Module* M)
        : m_module(M)
    {
    }
    
    ~PDG() = default;
    PDG(const PDG& ) = delete;
    PDG(PDG&& ) = delete;
    PDG& operator =(const PDG& ) = delete;
    PDG& operator =(PDG&& ) = delete;
    
public:
    const llvm::Module* getModule() const
    {
        return m_module;
    }

    const GlobalVariableNodes& getGlobalVariableNodes() const
    {
        return m_globalVariableNodes;
    }

    GlobalVariableNodes& getGlobalVariableNodes()
    {
        return m_globalVariableNodes;
    }

    const FunctionNodes& getFunctionNodes() const
    {
        return m_functionNodes;
    }

    FunctionNodes& getFunctionNodes()
    {
        return m_functionNodes;
    }

    const FunctionPDGs& getFunctionPDGs() const
    {
        return m_functionPDGs;
    }

    FunctionPDGs& getFunctionPDGs()
    {
        return m_functionPDGs;
    }

    bool hasGlobalVariableNode(llvm::GlobalVariable* variable) const
    {
        return m_globalVariableNodes.find(variable) != m_globalVariableNodes.end();
    }

    bool hasFunctionNode(llvm::Function* function) const
    {
        return m_functionNodes.find(function) != m_functionNodes.end();
    }

    bool hasFunctionPDG(llvm::Function* F) const
    {
        return m_functionPDGs.find(F) != m_functionPDGs.end();
    }

    PDGNodeTy getGlobalVariableNode(llvm::GlobalVariable* variable);
    PDGNodeTy getFunctionNode(llvm::Function* function) const;

    const PDGNodeTy getGlobalVariableNode(llvm::GlobalVariable* variable) const
    {
        return const_cast<PDG*>(this)->getGlobalVariableNode(variable);
    }

    FunctionPDGTy getFunctionPDG(llvm::Function* F);
    const FunctionPDGTy getFunctionPDG(llvm::Function* F) const
    {
        return const_cast<PDG*>(this)->getFunctionPDG(F);
    }

    bool addGlobalVariableNode(llvm::GlobalVariable* variable, PDGNodeTy node)
    {
        return m_globalVariableNodes.insert(std::make_pair(variable, node)).second;
    }

    bool addGlobalVariableNode(llvm::GlobalVariable* variable);
    bool addFunctionNode(llvm::Function* function, PDGNodeTy node)
    {
        return m_functionNodes.insert(std::make_pair(function, node)).second;
    }

    bool addFunctionNode(llvm::Function* function);
    
    bool addFunctionPDG(llvm::Function* F, FunctionPDGTy functionPDG)
    {
        return m_functionPDGs.insert(std::make_pair(F, functionPDG)).second;
    }

private:
    llvm::Module* m_module;
    GlobalVariableNodes m_globalVariableNodes;
    FunctionNodes m_functionNodes;
    FunctionPDGs m_functionPDGs;
};

} // namespace pdg

