#include "PDG/PDGBuilder.h"

#include "PDG/PDG.h"
#include "PDG/FunctionPDG.h"
#include "PDG/PDGEdge.h"
#include "PDG/DefUseResults.h"
#include "PDG/DominanceResults.h"
#include "PDG/IndirectCallSiteResults.h"

#include "llvm/Analysis/MemorySSA.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/Constants.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

namespace pdg {

PDGBuilder::PDGBuilder(llvm::Module* M)
    : m_module(M)
{
}

void PDGBuilder::setDesUseResults(DefUseResultsTy defUse)
{
    m_defUse = defUse;
}

void PDGBuilder::setIndirectCallSitesResults(IndCSResultsTy indCSResults)
{
    m_indCSResults = indCSResults;
}

void PDGBuilder::setDominanceResults(DominanceResultsTy domResults)
{
    m_domResults = domResults;
}

void PDGBuilder::build()
{
    m_pdg.reset(new PDG(m_module));
    visitGlobals();

    for (auto& F : *m_module) {
        m_pdg->addFunctionNode(&F);
        if (F.isDeclaration()) {
            buildFunctionDefinition(&F);
            continue;
        }
        buildFunctionPDG(&F);
        m_currentFPDG.reset();
    }
}

void PDGBuilder::visitGlobals()
{
    for (auto glob_it = m_module->global_begin();
            glob_it != m_module->global_end();
            ++glob_it) {
        m_pdg->addGlobalVariableNode(&*glob_it);
    }
}

void PDGBuilder::buildFunctionDefinition(llvm::Function* F)
{
    FunctionPDG* functionPDG = new FunctionPDG(F);
    visitFormalArguments(functionPDG, F);
    m_pdg->addFunctionPDG(F, FunctionPDGTy(functionPDG));
}

void PDGBuilder::buildFunctionPDG(llvm::Function* F)
{
    if (!m_pdg->hasFunctionPDG(F)) {
        m_currentFPDG.reset(new FunctionPDG(F));
        m_pdg->addFunctionPDG(F, m_currentFPDG);
    } else {
        m_currentFPDG = m_pdg->getFunctionPDG(F);
    }
    if (!m_currentFPDG->isFunctionDefBuilt()) {
        visitFormalArguments(m_currentFPDG.get(), F);
    }
    for (auto& B : *F) {
        visitBlock(B);
        visitBlockInstructions(B);
    }
}

void PDGBuilder::visitFormalArguments(FunctionPDG* functionPDG, llvm::Function* F)
{
    for (auto arg_it = F->arg_begin();
            arg_it != F->arg_end();
            ++arg_it) {
        functionPDG->addFormalArgNode(&*arg_it, createFormalArgNodeFor(&*arg_it));
    }
    functionPDG->setFunctionDefBuilt(true);
}

void PDGBuilder::visitBlock(llvm::BasicBlock& B)
{
    m_currentFPDG->addNode(llvm::dyn_cast<llvm::Value>(&B),
            PDGNodeTy(new PDGLLVMBasicBlockNode(&B)));
}

void PDGBuilder::visitBlockInstructions(llvm::BasicBlock& B)
{
    for (auto& I : B) {
        visit(I);
    }
    addControlEdgesForBlock(B);
}

void PDGBuilder::addControlEdgesForBlock(llvm::BasicBlock& B)
{
    if (!m_currentFPDG->hasNode(&B)) {
        return;
    }
    auto blockNode = m_currentFPDG->getNode(&B);
    // Don't add control edges if block is not control dependent on something
    if (blockNode->getInEdges().empty()) {
        return;
    }
    for (auto& I : B) {
        if (!m_currentFPDG->hasNode(&I)) {
            continue;
        }
        auto destNode = m_currentFPDG->getNode(&I);
        addControlEdge(blockNode, destNode);
    }
}

void PDGBuilder::visitBranchInst(llvm::BranchInst& I)
{
    // TODO: output this for debug mode only
    //llvm::dbgs() << "Branch Inst: " << I << "\n";
    if (I.isConditional()) {
        llvm::Value* cond = I.getCondition();
        if (auto sourceNode = getNodeFor(cond)) {
            auto destNode = getInstructionNodeFor(&I);
            addDataEdge(sourceNode, destNode);
        }
    }
    visitTerminatorInst(I);
}

void PDGBuilder::visitLoadInst(llvm::LoadInst& I)
{
    // TODO: output this for debug mode only
    //llvm::dbgs() << "Load Inst: " << I << "\n";
    auto destNode = PDGNodeTy(new PDGLLVMInstructionNode(&I));
    auto ptrOp = getNodeFor(I.getPointerOperand());
    addDataEdge(ptrOp, destNode);
    m_currentFPDG->addNode(&I, destNode);
    connectToDefSite(&I, destNode);
}

void PDGBuilder::visitStoreInst(llvm::StoreInst& I)
{
    // TODO: output this for debug mode only
    //llvm::dbgs() << "Store Inst: " << I << "\n";
    auto* valueOp = I.getValueOperand();
    auto sourceNode = getNodeFor(valueOp);
    if (!sourceNode) {
        return;
    }
    auto destNode = PDGNodeTy(new PDGLLVMInstructionNode(&I));
    auto ptrOp = getNodeFor(I.getPointerOperand());
    addDataEdge(sourceNode, destNode);
    addDataEdge(ptrOp, destNode);
    m_currentFPDG->addNode(&I, destNode);
}

void PDGBuilder::visitGetElementPtrInst(llvm::GetElementPtrInst& I)
{
    //llvm::dbgs() << "GetElementPtr Inst: " << I << "\n";
    // TODO: see if needs special implementation
    visitInstruction(I);
}

void PDGBuilder::visitPhiNode(llvm::PHINode& I)
{
    //llvm::dbgs() << "Phi Inst: " << I << "\n";
    // TODO: see if needs special implementation
    visitInstruction(I);
}

void PDGBuilder::visitMemSetInst(llvm::MemSetInst& I)
{
    //llvm::dbgs() << "MemSet Inst: " << I << "\n";
    // TODO: see if needs special implementation
    visitInstruction(I);
}

void PDGBuilder::visitMemCpyInst(llvm::MemCpyInst& I)
{
    //llvm::dbgs() << "MemCpy Inst: " << I << "\n";
    // TODO: see if needs special implementation
    visitInstruction(I);
}

void PDGBuilder::visitMemMoveInst(llvm::MemMoveInst &I)
{
    //llvm::dbgs() << "MemMove Inst: " << I << "\n";
    // TODO: see if needs special implementation
    visitInstruction(I);
}

void PDGBuilder::visitMemTransferInst(llvm::MemTransferInst &I)
{
    //llvm::dbgs() << "MemTransfer Inst: " << I << "\n";
    // TODO: see if needs special implementation
    visitInstruction(I);
}

void PDGBuilder::visitMemIntrinsic(llvm::MemIntrinsic &I)
{
    //llvm::dbgs() << "MemInstrinsic Inst: " << I << "\n";
    // TODO: see if needs special implementation
    visitInstruction(I);
}

void PDGBuilder::visitCallInst(llvm::CallInst& I)
{
    // TODO: think about external calls
    //llvm::dbgs() << "Call Inst: " << I << "\n";
    llvm::CallSite callSite(&I);
    selfVisitCallSite(callSite);
}

void PDGBuilder::visitInvokeInst(llvm::InvokeInst& I)
{
    //llvm::dbgs() << "Invoke Inst: " << I << "\n";
    llvm::CallSite callSite(&I);
    selfVisitCallSite(callSite);
    visitTerminatorInst(I);
}

void PDGBuilder::visitTerminatorInst(llvm::Instruction& I)
{
    assert(I.isTerminator());
    auto sourceNode = getInstructionNodeFor(&I);
    for (unsigned i = 0; i < I.getNumSuccessors(); ++i) {
        auto* block = I.getSuccessor(i);
        if (!m_domResults->posdominates(block, I.getParent())) {
            auto destNode = getNodeFor(block);
            addControlEdge(sourceNode, destNode);
        }
    }
}

void PDGBuilder::visitReturnInst(llvm::ReturnInst& I)
{
    if (!I.getReturnValue()) {
        return;
    }
    auto returnType = I.getReturnValue()->getType();
    if (returnType->isVoidTy()) {
        return;
    }
    auto sourceNode = getInstructionNodeFor(&I);
    auto destNode = m_pdg->getFunctionNode(I.getFunction());
    addDataEdge(sourceNode, destNode);
    visitInstruction(I);
}

void PDGBuilder::visitInstruction(llvm::Instruction& I)
{
    auto destNode = getInstructionNodeFor(&I);
    for (auto op_it = I.op_begin(); op_it != I.op_end(); ++op_it) {
        auto sourceNode = getNodeFor(op_it->get());
        addDataEdge(sourceNode, destNode);
    }
}

PDGBuilder::PDGNodeTy PDGBuilder::createInstructionNodeFor(llvm::Instruction* instr)
{
    return std::make_shared<PDGLLVMInstructionNode>(instr);
}

PDGBuilder::PDGNodeTy PDGBuilder::createBasicBlockNodeFor(llvm::BasicBlock* block)
{
    return std::make_shared<PDGLLVMBasicBlockNode>(block);
}

PDGBuilder::PDGNodeTy PDGBuilder::createGlobalNodeFor(llvm::GlobalVariable* global)
{
    return std::make_shared<PDGLLVMGlobalVariableNode>(global);
}

PDGBuilder::PDGNodeTy PDGBuilder::createFormalArgNodeFor(llvm::Argument* arg)
{
    return std::make_shared<PDGLLVMFormalArgumentNode>(arg);
}

PDGBuilder::PDGNodeTy PDGBuilder::createNullNode()
{
    return std::make_shared<PDGNullNode>();
}

PDGBuilder::PDGNodeTy PDGBuilder::createConstantNodeFor(llvm::Constant* constant)
{
    return std::make_shared<PDGLLVMConstantNode>(constant);
}

void PDGBuilder::selfVisitCallSite(llvm::CallSite& callSite)
{
    auto destNode = getInstructionNodeFor(callSite.getInstruction());
    bool isIndirectCall = false;
    FunctionSet callees;
    if (!m_indCSResults->hasIndCSCallees(callSite)) {
        if (auto* calledF = callSite.getCalledFunction()) {
            callees.insert(calledF);
        }
    } else {
        callees = m_indCSResults->getIndCSCallees(callSite);
        isIndirectCall = true;
    }
    for (auto callee : callees) {
        if (!m_pdg->hasFunctionNode(callee)) {
            m_pdg->addFunctionNode(callee);
        }
        if (isIndirectCall) {
            auto calleeValueNode = getNodeFor(callSite.getCalledValue());
            addDataEdge(calleeValueNode, destNode);
        }
        auto calleeNode = m_pdg->getFunctionNode(callee);
        if (!callSite.getFunctionType()->isVoidTy()) {
            addDataEdge(calleeNode, destNode);
        }
        addControlEdge(destNode, calleeNode);
    }
    for (unsigned i = 0; i < callSite.getNumArgOperands(); ++i) {
        if (auto* val = llvm::dyn_cast<llvm::Value>(callSite.getArgOperand(i))) {
            auto sourceNode = getNodeFor(val);
            if (!sourceNode) {
                continue;
            }
            if (val->getType()->isPointerTy()
                    && !llvm::isa<PDGNullNode>(sourceNode.get())
                    && !llvm::isa<llvm::Function>(val)) {
                //llvm::dbgs() << *val << "\n";
                connectToDefSite(val, sourceNode);
            }
            auto actualArgNode = PDGNodeTy(new PDGLLVMActualArgumentNode(callSite, val, i));
            addDataEdge(sourceNode, actualArgNode);
            addDataEdge(actualArgNode, destNode);
            m_currentFPDG->addNode(actualArgNode);
            // connect actual args with formal args
            addActualArgumentNodeConnections(actualArgNode, i, callSite, callees);
        }
    }
    for (auto& F : callees) {
        if (!m_pdg->hasFunctionPDG(F)) {
            buildFunctionDefinition(F);
        }
        FunctionPDGTy calleePDG = m_pdg->getFunctionPDG(F);
        calleePDG->addCallSite(callSite);
    }
}

void PDGBuilder::addDataEdge(PDGNodeTy source, PDGNodeTy dest)
{
    if (!source || !dest) {
        return;
    }
    PDGNode::PDGEdgeType edge = PDGNode::PDGEdgeType(new PDGDataEdge(source, dest));
    source->addOutEdge(edge);
    dest->addInEdge(edge);
}

void PDGBuilder::addControlEdge(PDGNodeTy source, PDGNodeTy dest)
{
    PDGNode::PDGEdgeType edge = PDGNode::PDGEdgeType(new PDGControlEdge(source, dest));
    source->addOutEdge(edge);
    dest->addInEdge(edge);
}

PDGBuilder::PDGNodeTy PDGBuilder::getInstructionNodeFor(llvm::Instruction* instr)
{
    if (m_currentFPDG->hasNode(instr)) {
        return m_currentFPDG->getNode(instr);
    }
    m_currentFPDG->addNode(instr, createInstructionNodeFor(instr));
    return m_currentFPDG->getNode(instr);
}

PDGBuilder::PDGNodeTy PDGBuilder::getNodeFor(llvm::Value* value)
{
    assert(value);
    if (!value) {
        return nullptr;
    }
    if (m_currentFPDG->hasNode(value)) {
        return m_currentFPDG->getNode(value);
    }
    if (auto* global = llvm::dyn_cast<llvm::GlobalVariable>(value)) {
        if (!m_pdg->hasGlobalVariableNode(global)) {
            m_pdg->addGlobalVariableNode(global, createGlobalNodeFor(global));
        }
        return m_pdg->getGlobalVariableNode(global);
    }
    if (auto* argument = llvm::dyn_cast<llvm::Argument>(value)) {
        assert(m_currentFPDG->hasFormalArgNode(argument));
        return m_currentFPDG->getFormalArgNode(argument);
    }
    if (auto* nullValue = llvm::dyn_cast<llvm::ConstantPointerNull>(value)) {
        m_currentFPDG->addNode(value, createNullNode());
    } else if (auto* function = llvm::dyn_cast<llvm::Function>(value)) {
        if (!m_pdg->hasFunctionNode(function)) {
            m_pdg->addFunctionNode(function);
        }
        return m_pdg->getFunctionNode(function);
    } else if (auto* constant = llvm::dyn_cast<llvm::Constant>(value)) {
        m_currentFPDG->addNode(value, createConstantNodeFor(constant));
    } else if (auto* instr = llvm::dyn_cast<llvm::Instruction>(value)) {
        m_currentFPDG->addNode(value, createInstructionNodeFor(instr));
    } else {
        // do not assert here for now to keep track of possible values to be handled here
        llvm::dbgs() << "Unhandled value " << *value << "\n";
        return PDGNodeTy();
    }
    return m_currentFPDG->getNode(value);
}

PDGBuilder::PDGNodeTy PDGBuilder::getNodeFor(llvm::BasicBlock* block)
{
    if (!m_currentFPDG->hasNode(block)) {
        m_currentFPDG->addNode(block, createBasicBlockNodeFor(block));
    }
    return m_currentFPDG->getNode(block);
}

void PDGBuilder::connectToDefSite(llvm::Value* value, PDGNodeTy valueNode)
{
    const auto& defSite = m_defUse->getDefNode(value);
    auto* defInst = defSite.first;
    auto sourceNode = defSite.second;
    if (!defInst || !m_currentFPDG->hasNode(defInst)) {
        if (sourceNode) {
            if (defInst) {
                m_currentFPDG->addNode(defInst, sourceNode);
            } else {
                addPhiNodeConnections(sourceNode);
            }
        }
    } else if (defInst) {
        sourceNode = m_currentFPDG->getNode(defInst);
    } else {
        // Ideally this should not happen
        llvm::dbgs() << "No definition found for value " << *value << "\n";
        return;
    }
    if (sourceNode) {
        addDataEdge(sourceNode, valueNode);
    }
}

void PDGBuilder::addActualArgumentNodeConnections(PDGNodeTy actualArgNode,
                                                  unsigned argIdx,
                                                  const llvm::CallSite& cs,
                                                  const FunctionSet& callees)
{
    for (auto& F : callees) {
        if (!m_pdg->hasFunctionPDG(F)) {
            buildFunctionDefinition(F);
        }
        FunctionPDGTy calleePDG = m_pdg->getFunctionPDG(F);
        PDGNodeTy formalArgNode;
        if (F->getFunctionType()->getNumParams() <= argIdx) {
            if (!calleePDG->isVarArg()) {
                continue;
            }
            formalArgNode = calleePDG->getVaArgNode();
        } else {
            llvm::Argument* formalArg = &*(F->arg_begin() + argIdx);
            if (!formalArg) {
                continue;
            }
            if (!calleePDG->hasFormalArgNode(formalArg)) {
                calleePDG->addFormalArgNode(formalArg, createFormalArgNodeFor(formalArg));
            }
            formalArgNode = calleePDG->getFormalArgNode(formalArg);
        }
        if (formalArgNode) {
            addDataEdge(actualArgNode, formalArgNode);
        }
    }
}

void PDGBuilder::addPhiNodeConnections(PDGNodeTy node)
{
    PDGPhiNode* phiNode = llvm::dyn_cast<PDGPhiNode>(node.get());
    if (!phiNode) {
        return;
    }
    m_currentFPDG->addNode(node);
    for (unsigned i = 0; i < phiNode->getNumValues(); ++i) {
        llvm::Value* value = phiNode->getValue(i);
        if (!value) {
            // TODO :check why null gets here
            continue;
        }
        auto destNode = getNodeFor(value);
        addDataEdge(destNode, node);
    }
}

} // namespace pdg

