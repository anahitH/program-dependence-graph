#include "PDG/PDG.h"

#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/Constants.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"


namespace pdg {

PDG::PDGNodeTy PDG::getGlobalVariableNode(llvm::GlobalVariable* variable)
{
    assert(hasGlobalVariableNode(variable));
    return m_globalVariableNodes.find(variable)->second;
}

PDG::PDGNodeTy PDG::getFunctionNode(llvm::Function* function) const
{
    assert(hasFunctionNode(function));
    return m_functionNodes.find(function)->second;
}

PDG::FunctionPDGTy PDG::getFunctionPDG(llvm::Function* F)
{
    assert(hasFunctionPDG(F));
    return m_functionPDGs.find(F)->second;
}

bool PDG::addGlobalVariableNode(llvm::GlobalVariable* variable)
{
    if (hasGlobalVariableNode(variable)) {
        return false;
    }
    m_globalVariableNodes.insert(std::make_pair(variable, PDGNodeTy(new PDGLLVMGlobalVariableNode(variable))));
    return true;
}

bool PDG::addFunctionNode(llvm::Function* function)
{
    if (hasFunctionNode(function)) {
        return false;
    }
    m_functionNodes.insert(std::make_pair(function, PDGNodeTy(new PDGLLVMFunctionNode(function))));
    return true;
}

} // namespace pdg

