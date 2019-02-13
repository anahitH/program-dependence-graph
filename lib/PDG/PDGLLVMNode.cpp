#include "PDG/PDGLLVMNode.h"

#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

namespace pdg {

std::string getNodeTypeAsString(PDGLLVMNode::NodeType type)
{
    switch(type) {
    case PDGLLVMNode::InstructionNode:
        return "InstructionNode";
    case PDGLLVMNode::FormalArgumentNode:
        return "FormalArgumentNode";
    case PDGLLVMNode::VaArgumentNode:
        return "VaArgumentNode";
    case PDGLLVMNode::ActualArgumentNode:
        return "ActualArgumentNode";
    case PDGLLVMNode::GlobalVariableNode:
        return "GlobalVariableNode";
    case PDGLLVMNode::ConstantExprNode:
        return "ConstantExprNode";
    case PDGLLVMNode::ConstantNode:
        return "ConstantNode";
    case PDGLLVMNode::BasicBlockNode:
        return "BasicBlockNode";
    case PDGLLVMNode::FunctionNode:
        return "FunctionNode";
    case PDGLLVMNode::NullNode:
        return "NullNode";
    case PDGLLVMNode::PhiNode:
        return "PhiNode";
    default:
        break;
    }
    return "UnknownNode";
}

std::string PDGLLVMNode::getNodeAsString() const
{
    std::string str;
    llvm::raw_string_ostream rawstr(str);
    rawstr << getNodeTypeAsString(m_type) << " " << *m_value;
    return rawstr.str();
}

std::string PDGPhiNode::getNodeAsString() const
{
    std::string str;
    llvm::raw_string_ostream rawstr(str);
    rawstr << "PhiNode ";
    for (unsigned i = 0; i < m_values.size(); ++i) {
        rawstr << " [ ";
        rawstr << *m_values[i];
        rawstr << m_blocks[i]->getName();
        rawstr << "] ";
    }
    return rawstr.str();
}

std::string PDGLLVMVaArgNode::getNodeAsString() const
{
    std::string str;
    llvm::raw_string_ostream rawstr(str);
    rawstr << "VAArgNode ";
    rawstr << m_function->getName();
    return rawstr.str();
}

} // namespace pdg

