#include "PDG/LLVMMemorySSADefUseAnalysisResults.h"
#include "PDG/IndirectCallSitesAnalysis.h"

#include "PDG/PDGLLVMNode.h"

#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/Analysis/AssumptionCache.h"
#include "llvm/Analysis/BasicAliasAnalysis.h"
#include "llvm/Analysis/MemorySSA.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

namespace pdg {

LLVMMemorySSADefUseAnalysisResults::LLVMMemorySSADefUseAnalysisResults(
                        const MemorySSAGetter& mssaGetter,
                        const AARGetter& aarGetter)
    : m_memorySSAGetter(mssaGetter)
    , m_aarGetter(aarGetter)
{
}

DefUseResults::DefSite LLVMMemorySSADefUseAnalysisResults::getDefNode(llvm::Value* value)
{
    auto pos = m_valueDefSite.find(value);
    if (pos != m_valueDefSite.end()) {
        return pos->second;
    }
    DefSite nullDefSite(nullptr, PDGNodeTy());
    llvm::Instruction* instr = llvm::dyn_cast<llvm::Instruction>(value);
    if (!instr) {
        m_valueDefSite.insert(std::make_pair(value, nullDefSite));
        return nullDefSite;
    }
    auto* memorySSA = m_memorySSAGetter(instr->getParent()->getParent());
    llvm::Function* F = instr->getFunction();
    auto* aa = m_aarGetter(F);

    auto* memDefAccess = getMemoryDefAccess(instr, memorySSA);
    if (!memDefAccess) {
        m_valueDefSite.insert(std::make_pair(value, nullDefSite));
        return nullDefSite;
    }
    if (auto* memDef = llvm::dyn_cast<llvm::MemoryDef>(memDefAccess)) {
        auto* memInst = memDef->getMemoryInst();
        if (!memInst) {
            m_valueDefSite.insert(std::make_pair(value, nullDefSite));
            return nullDefSite;
        }
        return DefSite(memInst, PDGNodeTy(new PDGLLVMInstructionNode(memInst)));
    } else if (auto* memPhi = llvm::dyn_cast<llvm::MemoryPhi>(memDefAccess)) {
        std::unordered_set<llvm::MemoryAccess*> processedAccesses;
        const auto& defSites = getDefSites(value, memPhi, memorySSA, aa, processedAccesses);
        PDGNodeTy phiNode = PDGNodeTy(new PDGPhiNode(defSites.values, defSites.blocks));
        auto res = m_valueDefSite.insert(std::make_pair(value,  DefSite(nullptr, phiNode)));
        return res.first->second;
    }
    assert(false);
    m_valueDefSite.insert(std::make_pair(value, nullDefSite));
    return nullDefSite;
}

llvm::MemoryAccess* LLVMMemorySSADefUseAnalysisResults::getMemoryDefAccess(llvm::Instruction* instr,
                                                                           llvm::MemorySSA* memorySSA)
{
    llvm::MemoryAccess* memAccess = memorySSA->getMemoryAccess(instr);
    if (!memAccess) {
        return nullptr;
    }
    auto* memUse = llvm::dyn_cast<llvm::MemoryUse>(memAccess);
    if (!memUse) {
        return nullptr;
    }
    return memUse->getDefiningAccess();
}

LLVMMemorySSADefUseAnalysisResults::PHI
LLVMMemorySSADefUseAnalysisResults::getDefSites(llvm::Value* value,
                                                llvm::MemoryAccess* access,
                                                llvm::MemorySSA* memorySSA,
                                                llvm::AAResults* aa,
                                                std::unordered_set<llvm::MemoryAccess*>& processedAccesses)
{
    PHI phi;
    if (!processedAccesses.insert(access).second) {
        return phi;
    }
    if (!access || !value) {
        return phi;
    }
    if (memorySSA->isLiveOnEntryDef(access)) {
        return phi;
    }
    if (auto* def = llvm::dyn_cast<llvm::MemoryDef>(access)) {
        const auto& DL = def->getBlock()->getModule()->getDataLayout();
        llvm::ModRefInfo modRef;
        if (auto* load = llvm::dyn_cast<llvm::LoadInst>(value)) {
            modRef = aa->getModRefInfo(def->getMemoryInst(),
				       llvm::MemoryLocation::get(load));
        } else if (value->getType()->isSized() && llvm::dyn_cast<llvm::Instruction>(value)) {
		modRef = aa->getModRefInfo(def->getMemoryInst(),
				       llvm::MemoryLocation::get(def->getMemoryInst()));
        } else {
            return phi;
        }
        if (modRef == llvm::ModRefInfo::MustMod || modRef == llvm::ModRefInfo::Mod) {
            phi.values.push_back(def->getMemoryInst());
            phi.blocks.push_back(def->getBlock());
            return phi;
        }
        return getDefSites(value, def->getDefiningAccess(), memorySSA, aa, processedAccesses);
    }
    if (auto* memphi = llvm::dyn_cast<llvm::MemoryPhi>(access)) {
        for (auto def_it = memphi->defs_begin(); def_it != memphi->defs_end(); ++def_it) {
            auto accesses = getDefSites(value, *def_it, memorySSA, aa, processedAccesses);
            if (!accesses.empty()) {
                phi.values.insert(phi.values.end(), accesses.values.begin(), accesses.values.end());
                phi.blocks.insert(phi.blocks.end(), accesses.blocks.begin(), accesses.blocks.end());
            }
        }
    }
    return phi;
}

} // namespace pdg

