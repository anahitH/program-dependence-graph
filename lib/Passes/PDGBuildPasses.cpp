#include "Passes/PDGBuildPasses.h"

#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/Analysis/AssumptionCache.h"
#include "llvm/Analysis/BasicAliasAnalysis.h"
#include "llvm/Analysis/MemorySSA.h"
#include "llvm/Analysis/PostDominators.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Module.h"
#include "llvm/PassRegistry.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/GraphWriter.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"

#include "PDG/SVFGDefUseAnalysisResults.h"
#include "PDG/LLVMMemorySSADefUseAnalysisResults.h"
#include "PDG/LLVMDominanceTree.h"
#include "PDG/PDGBuilder.h"
#include "PDG/PDGGraphTraits.h"
#include "PDG/SVFGIndirectCallSiteResults.h"

#include "SVF/MSSA/SVFG.h"
#include "SVF/MSSA/SVFGBuilder.h"
#include "SVF/Util/SVFModule.h"
#include "SVF/MemoryModel/PointerAnalysis.h"
#include "SVF/WPA/Andersen.h"
#include "SVF/PDG/PDGPointerAnalysis.h"

#include <fstream>

namespace pdg {

char SVFGPDGBuilder::ID = 0;
static llvm::RegisterPass<SVFGPDGBuilder> X("svfg-pdg","build pdg using svfg");

void SVFGPDGBuilder::getAnalysisUsage(llvm::AnalysisUsage& AU) const
{
    AU.addRequired<llvm::PostDominatorTreeWrapperPass>();
    AU.addRequired<llvm::DominatorTreeWrapperPass>();
    AU.setPreservesAll();
}

bool SVFGPDGBuilder::runOnModule(llvm::Module& M)
{
    auto domTreeGetter = [&] (llvm::Function* F) {
        return &this->getAnalysis<llvm::DominatorTreeWrapperPass>(*F).getDomTree();
    };
    auto postdomTreeGetter = [&] (llvm::Function* F) {
        return &this->getAnalysis<llvm::PostDominatorTreeWrapperPass>(*F).getPostDomTree();
    };

    SVFModule svfM(M);
    AndersenWaveDiff* ander = new svfg::PDGAndersenWaveDiff();
    ander->disablePrintStat();
    ander->analyze(svfM);
    SVFGBuilder memSSA(true);
    SVFG *svfg = memSSA.buildSVFG((BVDataPTAImpl*)ander);


    using DefUseResultsTy = PDGBuilder::DefUseResultsTy;
    using IndCSResultsTy = PDGBuilder::IndCSResultsTy;
    using DominanceResultsTy = PDGBuilder::DominanceResultsTy;
    DefUseResultsTy defUse = DefUseResultsTy(new SVFGDefUseAnalysisResults(svfg));
    IndCSResultsTy indCSRes = IndCSResultsTy(new
            pdg::SVFGIndirectCallSiteResults(ander->getPTACallGraph()));
    DominanceResultsTy domResults = DominanceResultsTy(new LLVMDominanceTree(domTreeGetter,
                postdomTreeGetter));

    pdg::PDGBuilder pdgBuilder(&M);
    pdgBuilder.setDesUseResults(defUse);
    pdgBuilder.setIndirectCallSitesResults(indCSRes);
    pdgBuilder.setDominanceResults(domResults);
    pdgBuilder.build();

    m_pdg = pdgBuilder.getPDG();
    return false;
}

char LLVMPDGBuilder::ID = 0;
static llvm::RegisterPass<LLVMPDGBuilder> Z("llvm-pdg","build pdg using dg");

void LLVMPDGBuilder::getAnalysisUsage(llvm::AnalysisUsage& AU) const
{
    AU.addRequired<llvm::AssumptionCacheTracker>(); // otherwise run-time error
    llvm::getAAResultsAnalysisUsage(AU);
    AU.addRequiredTransitive<llvm::MemorySSAWrapperPass>();
    AU.addRequired<llvm::PostDominatorTreeWrapperPass>();
    AU.addRequired<llvm::DominatorTreeWrapperPass>();
    AU.setPreservesAll();
}

bool LLVMPDGBuilder::runOnModule(llvm::Module& M)
{
    auto memSSAGetter = [this] (llvm::Function* F) -> llvm::MemorySSA* {
        return &this->getAnalysis<llvm::MemorySSAWrapperPass>(*F).getMSSA();
    };
    llvm::Optional<llvm::BasicAAResult> BAR;
    llvm::Optional<llvm::AAResults> AAR;
    auto AARGetter = [&](llvm::Function* F) -> llvm::AAResults* {
        BAR.emplace(llvm::createLegacyPMBasicAAResult(*this, *F));
        AAR.emplace(llvm::createLegacyPMAAResults(*this, *F, *BAR));
        return &*AAR;
    };
    std::unordered_map<llvm::Function*, llvm::AAResults*> functionAAResults;
    for (auto& F : M) {
        if (!F.isDeclaration()) {
            functionAAResults.insert(std::make_pair(&F, AARGetter(&F)));
        }
    }

    auto domTreeGetter = [&] (llvm::Function* F) {
        return &this->getAnalysis<llvm::DominatorTreeWrapperPass>(*F).getDomTree();
    };
    auto postdomTreeGetter = [&] (llvm::Function* F) {
        return &this->getAnalysis<llvm::PostDominatorTreeWrapperPass>(*F).getPostDomTree();
    };

    auto aliasAnalysisResGetter = [&functionAAResults] (llvm::Function* F) {
            return functionAAResults[F];
    };

    // TODO: consider not using SVF here at all
    SVFModule svfM(M);
    AndersenWaveDiff* ander = new svfg::PDGAndersenWaveDiff();
    ander->disablePrintStat();
    ander->analyze(svfM);

    using DefUseResultsTy = PDGBuilder::DefUseResultsTy;
    using IndCSResultsTy = PDGBuilder::IndCSResultsTy;
    using DominanceResultsTy = PDGBuilder::DominanceResultsTy;
    DefUseResultsTy defUse = DefUseResultsTy(new LLVMMemorySSADefUseAnalysisResults(memSSAGetter, aliasAnalysisResGetter));
    IndCSResultsTy indCSRes = IndCSResultsTy(new
            pdg::SVFGIndirectCallSiteResults(ander->getPTACallGraph()));
    DominanceResultsTy domResults = DominanceResultsTy(new LLVMDominanceTree(domTreeGetter,
                postdomTreeGetter));

    pdg::PDGBuilder pdgBuilder(&M);
    pdgBuilder.setDesUseResults(defUse);
    pdgBuilder.setIndirectCallSitesResults(indCSRes);
    pdgBuilder.setDominanceResults(domResults);
    pdgBuilder.build();

    m_pdg = pdgBuilder.getPDG();
    return false;
}

}

