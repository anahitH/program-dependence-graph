#include "PDG/SVFGDefUseAnalysisResults.h"

#include "PDG/PDGLLVMNode.h"

#include "SVF/MSSA/SVFG.h"
#include "SVF/MSSA/SVFGNode.h"

namespace pdg {

namespace {

void printNodeType(SVFGNode* node)
{
    /*Addr, Copy, Gep, Store,
      Load, TPhi, TIntraPhi, TInterPhi,
      MPhi, MIntraPhi, MInterPhi, FRet,
      ARet, AParm, APIN, APOUT,
      FParm, FPIN, FPOUT, NPtr*/
    llvm::dbgs() << " Node type ";
    if (node->getNodeKind() == SVFGNode::Addr) {
        llvm::dbgs() << "Addr\n";
    } else if (node->getNodeKind() == SVFGNode::Copy) {
        llvm::dbgs() << "Copy\n";
    } else if (node->getNodeKind() == SVFGNode::Gep) {
        llvm::dbgs() << "Gep\n";
    } else if (node->getNodeKind() == SVFGNode::Store) {
        llvm::dbgs() << "Store\n";
    } else if (node->getNodeKind() == SVFGNode::Load) {
        llvm::dbgs() << "Load\n";
    } else if (node->getNodeKind() == SVFGNode::TPhi) {
        llvm::dbgs() << "TPhi\n";
    } else if (node->getNodeKind() == SVFGNode::TIntraPhi) {
        llvm::dbgs() << "TIntraPhi\n";
    } else if (node->getNodeKind() == SVFGNode::TInterPhi) {
        llvm::dbgs() << "TInterPhi\n";
    } else if (node->getNodeKind() == SVFGNode::MPhi) {
        llvm::dbgs() << "MPhi\n";
    } else if (node->getNodeKind() == SVFGNode::MIntraPhi) {
        llvm::dbgs() << "MIntraPhi\n";
    } else if (node->getNodeKind() == SVFGNode::MInterPhi) {
        llvm::dbgs() << "MInterPhi\n";
    } else if (node->getNodeKind() == SVFGNode::FRet) {
        llvm::dbgs() << "FRet\n";
    } else if (node->getNodeKind() == SVFGNode::ARet) {
        llvm::dbgs() << "ARet\n";
    } else if (node->getNodeKind() == SVFGNode::AParm) {
        llvm::dbgs() << "AParm\n";
    } else if (node->getNodeKind() == SVFGNode::APIN) {
        llvm::dbgs() << "APIN\n";
    } else if (node->getNodeKind() == SVFGNode::APOUT) {
        llvm::dbgs() << "APOUT\n";
    } else if (node->getNodeKind() == SVFGNode::FParm) {
        llvm::dbgs() << "FParm\n";
    } else if (node->getNodeKind() == SVFGNode::FPIN) {
        llvm::dbgs() << "FPIN\n";
    } else if (node->getNodeKind() == SVFGNode::FPOUT) {
        llvm::dbgs() << "FPOUT\n";
    } else if (node->getNodeKind() == SVFGNode::NPtr) {
        llvm::dbgs() << "NPtr\n";
    } else {
        llvm::dbgs() << "Unknown SVFG Node kind\n";
    }
}

bool hasIncomingEdges(PAGNode* pagNode)
{
    //Addr, Copy, Store, Load, Call, Ret, NormalGep, VariantGep, ThreadFork, ThreadJoin
    if (pagNode->hasIncomingEdges(PAGEdge::Addr)
        || pagNode->hasIncomingEdges(PAGEdge::Copy)
        || pagNode->hasIncomingEdges(PAGEdge::Store)
        || pagNode->hasIncomingEdges(PAGEdge::Load)
        || pagNode->hasIncomingEdges(PAGEdge::Call)
        || pagNode->hasIncomingEdges(PAGEdge::Ret)
        || pagNode->hasIncomingEdges(PAGEdge::NormalGep)
        || pagNode->hasIncomingEdges(PAGEdge::VariantGep)
        || pagNode->hasIncomingEdges(PAGEdge::ThreadFork)
        || pagNode->hasIncomingEdges(PAGEdge::ThreadJoin)) {
        return true;
    }
    return false;
}

void getValuesAndBlocks(MSSADEF* def,
                        std::unordered_set<MSSADEF*>& processed_defs,
                        std::vector<llvm::Value*>& values,
                        std::vector<llvm::BasicBlock*>& blocks)
{
    if (!processed_defs.insert(def).second) {
        return;
    }
    if (def->getType() == MSSADEF::CallMSSACHI) {
        // TODO:
    } else if (def->getType() == MSSADEF::StoreMSSACHI) {
        auto* storeChi = llvm::dyn_cast<SVFG::STORECHI>(def);
        if (auto* instr = storeChi->getStoreInst()->getInst()) {
            values.push_back(const_cast<llvm::Instruction*>(instr));
        }
        if (auto* block = storeChi->getBasicBlock()) {
            blocks.push_back(const_cast<llvm::BasicBlock*>(block));
        }
    } else if (def->getType() == MSSADEF::EntryMSSACHI) {
    } else if (def->getType() == MSSADEF::SSAPHI) {
        auto* phi = llvm::dyn_cast<MemSSA::PHI>(def);
        for (auto it = phi->opVerBegin(); it != phi->opVerEnd(); ++it) {
            getValuesAndBlocks(it->second->getDef(), processed_defs, values, blocks);
        }
    }
}

template <typename PHIType>
void getValuesAndBlocks(PHIType* node,
                        std::unordered_set<MSSADEF*>& processed_defs,
                        std::vector<llvm::Value*>& values,
                        std::vector<llvm::BasicBlock*>& blocks)
{
    for (auto it = node->opVerBegin(); it != node->opVerEnd(); ++it) {
        auto* def = it->second->getDef();
        getValuesAndBlocks(def, processed_defs, values, blocks);
    }
}

void getValuesAndBlocks(IntraMSSAPHISVFGNode* svfgNode,
                        std::unordered_set<MSSADEF*>& processed_defs,
                        std::vector<llvm::Value*>& values,
                        std::vector<llvm::BasicBlock*>& blocks)
{
    getValuesAndBlocks<IntraMSSAPHISVFGNode>(svfgNode, processed_defs, values, blocks);
}

void getValuesAndBlocks(InterMSSAPHISVFGNode* svfgNode,
                        std::unordered_set<MSSADEF*>& processed_defs,
                        std::vector<llvm::Value*>& values,
                        std::vector<llvm::BasicBlock*>& blocks)
{
    getValuesAndBlocks<InterMSSAPHISVFGNode>(svfgNode, processed_defs, values, blocks);
}

void getValuesAndBlocks(PHISVFGNode* svfgNode,
                        std::unordered_set<MSSADEF*>& processed_defs,
                        std::vector<llvm::Value*>& values,
                        std::vector<llvm::BasicBlock*>& blocks)
{
    // TODO: can not take basic block from pag node
    //for (auto it = svfgNode->opVerBegin(); it != svfgNode->opVerEnd(); ++it) {
    //    auto* pagNode = it->second;
    //    if (pagNode->hasValue()) {
    //        values.push_back(const_cast<llvm::Value*>(pagNode->getValue()));
    //        blocks.push_back(const_cast<llvm::BasicBlock*>(pagNode->getBB()));
    //    }
    //}
}


void getValuesAndBlocks(SVFGNode* svfgNode,
                        std::vector<llvm::Value*>& values,
                        std::vector<llvm::BasicBlock*>& blocks)
{
    std::unordered_set<MSSADEF*> processed_defs;
    if (auto* stmtNode = llvm::dyn_cast<StmtSVFGNode>(svfgNode)) {
        if (auto* instr = stmtNode->getInst()) {
            values.push_back(const_cast<llvm::Instruction*>(instr));
        }
        if (auto* block = svfgNode->getBB()) {
            blocks.push_back(const_cast<llvm::BasicBlock*>(block));
        }
    } else if (auto* actualParam = llvm::dyn_cast<ActualParmSVFGNode>(svfgNode)) {
        auto* param = actualParam->getParam();
        if (param->hasValue()) {
            if (auto* instr = param->getValue()) {
                values.push_back(const_cast<llvm::Value*>(instr));
            }
            if (auto* block = svfgNode->getBB()) {
                blocks.push_back(const_cast<llvm::BasicBlock*>(block));
            }
        }
    } else if (auto* actualRet = llvm::dyn_cast<ActualRetSVFGNode>(svfgNode)) {
        auto* ret = actualRet->getRev();
        if (ret->hasValue()) {
            if (auto* instr = ret->getValue()) {
                values.push_back(const_cast<llvm::Value*>(instr));
            }
            if (auto* block = svfgNode->getBB()) {
                blocks.push_back(const_cast<llvm::BasicBlock*>(block));
            }
        }
    } else if (auto* formalParam = llvm::dyn_cast<FormalParmSVFGNode>(svfgNode)) {
        auto* param = formalParam->getParam();
        if (param->hasValue()) {
            if (auto* instr = param->getValue()) {
                values.push_back(const_cast<llvm::Value*>(instr));
            }
            if (auto* block = svfgNode->getBB()) {
                blocks.push_back(const_cast<llvm::BasicBlock*>(block));
            }
        }
    } else  if (auto* formalRet = llvm::dyn_cast<FormalRetSVFGNode>(svfgNode)) {
        auto* ret = formalRet->getRet();
        if (ret->hasValue()) {
            if (auto* instr = ret->getValue()) {
                values.push_back(const_cast<llvm::Value*>(instr));
            }
            if (auto* block = svfgNode->getBB()) {
                blocks.push_back(const_cast<llvm::BasicBlock*>(block));
            }
        }
    } else if (auto* formalInNode = llvm::dyn_cast<FormalINSVFGNode>(svfgNode)) {
        getValuesAndBlocks(formalInNode->getEntryChi()->getOpVer()->getDef(), processed_defs, values, blocks);
        // TODO:
    } else if (auto* formalOutNode = llvm::dyn_cast<FormalOUTSVFGNode>(svfgNode)) {
        getValuesAndBlocks(formalOutNode->getRetMU()->getVer()->getDef(), processed_defs, values, blocks);
    } else if (auto* actualInNode = llvm::dyn_cast<ActualINSVFGNode>(svfgNode)) {
        // TODO:
    } else if (auto* actualOutNode = llvm::dyn_cast<ActualOUTSVFGNode>(svfgNode)) {
        // TODO:
    } else if (auto* intraMssaPhiNode = llvm::dyn_cast<IntraMSSAPHISVFGNode>(svfgNode)) {
        getValuesAndBlocks(intraMssaPhiNode, processed_defs, values, blocks);
    } else if (auto* interMssaPhiNode = llvm::dyn_cast<InterMSSAPHISVFGNode>(svfgNode)) {
        getValuesAndBlocks(interMssaPhiNode, processed_defs, values, blocks);
    } else if (auto* null = llvm::dyn_cast<NullPtrSVFGNode>(svfgNode)) {
    } else if (auto* phiNode = llvm::dyn_cast<PHISVFGNode>(svfgNode)) {
        getValuesAndBlocks(phiNode, processed_defs, values, blocks);
    }
}

}

SVFGDefUseAnalysisResults::SVFGDefUseAnalysisResults(SVFG* svfg)
    : m_svfg(svfg)
{
}

DefUseResults::DefSite SVFGDefUseAnalysisResults::getDefNode(llvm::Value* value)
{
    auto pos = m_valueDefSite.find(value);
    if (pos != m_valueDefSite.end()) {
        return pos->second;
    }
    PDGNodeTy node;
    SVFGNode* valueSvfgNode = getSVFGNode(value);
    if (!valueSvfgNode) {
        DefSite nulldefSite(nullptr, PDGNodeTy());
        m_valueDefSite.insert(std::make_pair(value, nulldefSite));
        return nulldefSite;
    }
    std::unordered_set<SVFGNode*> processedNodes;
    const auto& svfgDefNodes = getSVFGDefNodes(valueSvfgNode, processedNodes);
    processedNodes.clear();
    auto defNode = getPdgDefNode(svfgDefNodes);
    m_valueDefSite.insert(std::make_pair(value, defNode));
    return defNode;
}

SVFGNode* SVFGDefUseAnalysisResults::getSVFGNode(llvm::Value* value)
{
    llvm::Instruction* instr = llvm::dyn_cast<llvm::Instruction>(value);
    if (!instr) {
        return nullptr;
    }
    auto* pag = m_svfg->getPAG();
    if (!pag->hasValueNode(instr)) {
        return nullptr;
    }
    auto nodeId = pag->getValueNode(instr);
    auto* pagNode = pag->getPAGNode(nodeId);
    if (!pagNode) {
        return nullptr;
    }
    if (!m_svfg->hasDef(pagNode)) {
        return nullptr;
    }
    return const_cast<SVFGNode*>(m_svfg->getDefSVFGNode(pagNode));
}

std::unordered_set<SVFGNode*> SVFGDefUseAnalysisResults::getSVFGDefNodes(SVFGNode* svfgNode, std::unordered_set<SVFGNode*>& processedNodes)
{
    std::unordered_set<SVFGNode*> defNodes;
    if (!processedNodes.insert(svfgNode).second) {
        return defNodes;
    }
    for (auto inedge_it = svfgNode->InEdgeBegin(); inedge_it != svfgNode->InEdgeEnd(); ++inedge_it) {
        SVFGNode* srcNode = (*inedge_it)->getSrcNode();
        if (auto* fin = llvm::dyn_cast<FormalINSVFGNode>(srcNode)) {
            const auto& formaInDefs = getSVFGDefNodes(srcNode, processedNodes);
            defNodes.insert(formaInDefs.begin(), formaInDefs.end());
            continue;
        }
        if (srcNode->getNodeKind() == SVFGNode::Copy
            || srcNode->getNodeKind() == SVFGNode::Store
            || srcNode->getNodeKind() == SVFGNode::MPhi
            || srcNode->getNodeKind() == SVFGNode::MIntraPhi
            || srcNode->getNodeKind() == SVFGNode::MInterPhi
            || srcNode->getNodeKind() == SVFGNode::TPhi
            || srcNode->getNodeKind() == SVFGNode::TIntraPhi
            || srcNode->getNodeKind() == SVFGNode::TInterPhi) {
            defNodes.insert(srcNode);
            //printNodeType(srcNode);
            // TODO: what other node kinds can be added here?
        } else {
            //printNodeType(srcNode);
        }
    }
    return defNodes;
}

DefUseResults::DefSite SVFGDefUseAnalysisResults::getPdgDefNode(const std::unordered_set<SVFGNode*>& svfgDefNodes)
{
    llvm::Value* defValue = nullptr;
    PDGNodeTy defNode;
    std::vector<llvm::Value*> values;
    std::vector<llvm::BasicBlock*> blocks;
    for (const auto& svfgDefNode : svfgDefNodes) {
        getValuesAndBlocks(svfgDefNode, values, blocks);
    }
    assert(values.size() == blocks.size());
    if (values.empty()) {
        return DefSite(defValue, defNode);
    }
    if (values.size() == 1) {
        defNode = getNode(*svfgDefNodes.begin());
        defValue = values[0];
    } else {
        defNode.reset(new PDGPhiNode(values, blocks));
    }
    return DefSite(defValue, defNode);
}

DefUseResults::PDGNodeTy SVFGDefUseAnalysisResults::getNode(SVFGNode* svfgNode)
{
    if (auto* stmtNode = llvm::dyn_cast<StmtSVFGNode>(svfgNode)) {
        llvm::Instruction* instr = const_cast<llvm::Instruction*>(stmtNode->getInst());
        return PDGNodeTy(new PDGLLVMInstructionNode(instr));
    }
    return PDGNodeTy();
}

} // namespace pdg

