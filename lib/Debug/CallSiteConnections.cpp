#include "llvm/Pass.h"

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

#include "PDG/PDG/PDG.h"
#include "PDG/PDG/FunctionPDG.h"
#include "PDG/SVFGDefUseAnalysisResults.h"
#include "PDG/LLVMMemorySSADefUseAnalysisResults.h"
//#include "PDG/DGDefUseAnalysisResults.h"
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
#include <memory>

extern llvm::cl::opt<std::string> def_use;

class CallSiteConnectionsPrinter : public llvm::ModulePass
{
public:
    static char ID;
    CallSiteConnectionsPrinter()
        : llvm::ModulePass(ID)
    {
    }

    void getAnalysisUsage(llvm::AnalysisUsage& AU) const override
    {
        AU.addRequired<llvm::AssumptionCacheTracker>(); // otherwise run-time error
        llvm::getAAResultsAnalysisUsage(AU);
        AU.addRequiredTransitive<llvm::MemorySSAWrapperPass>();
        AU.addRequired<llvm::PostDominatorTreeWrapperPass>();
        AU.addRequired<llvm::DominatorTreeWrapperPass>();
        AU.setPreservesAll();
    }

    bool runOnModule(llvm::Module& M) override
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

        // Passing AARGetter to LLVMMemorySSADefUseAnalysisResults causes segmentation fault when requesting AAR
        // use following functional as a workaround before the problem with AARGetter is found
        auto aliasAnalysisResGetter = [&functionAAResults] (llvm::Function* F) {
            return functionAAResults[F];
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
        DefUseResultsTy defUse;
        if (def_use == "llvm") {
            llvm::dbgs() << "Using llvm for def-use information\n";
            defUse = DefUseResultsTy(new LLVMMemorySSADefUseAnalysisResults(memSSAGetter, aliasAnalysisResGetter));
        } else {
            llvm::dbgs() << "Using (default) svfg for def-use information\n";
            defUse = DefUseResultsTy(new SVFGDefUseAnalysisResults(svfg));
        }
        IndCSResultsTy indCSRes = IndCSResultsTy(new
                pdg::SVFGIndirectCallSiteResults(ander->getPTACallGraph()));
        DominanceResultsTy domResults = DominanceResultsTy(new LLVMDominanceTree(domTreeGetter,
                    postdomTreeGetter));

        pdg::PDGBuilder pdgBuilder(&M);
        pdgBuilder.setDesUseResults(defUse);
        pdgBuilder.setIndirectCallSitesResults(indCSRes);
        pdgBuilder.setDominanceResults(domResults);
        pdgBuilder.build();

        auto pdg = pdgBuilder.getPDG();

        dumpCallSitesConnections(M, pdg);
    }

private:
    void dumpCallSitesConnections(llvm::Module& M, PDGBuilder::PDGType pdg)
    {
        for (auto& F : M) {
            llvm::dbgs() << "Function: " << F.getName() << "\n";
            if (!pdg->hasFunctionPDG(&F)) {
                llvm::dbgs() << "   No PDG built.\n"; 
                continue;
            }
            auto F_pdg = pdg->getFunctionPDG(&F);
            const auto& callSites = F_pdg->getCallSites();
            if (callSites.empty()) {
                llvm::dbgs() << "   No caller\n";
            }
            for (auto& callSite : callSites) {
                dumpCallSiteConnections(&F, callSite, F_pdg);
            }
        }
    }

    void dumpCallSiteConnections(llvm::Function* callee,
                                 const llvm::CallSite& cs,
                                 PDG::FunctionPDGTy F_pdg)
    {
        auto* call_inst = cs.getInstruction();
        llvm::dbgs() << "   Call Site: " << *call_inst << "\n";
        llvm::Function* caller = cs.getInstruction()->getFunction();
        llvm::dbgs() << "   Caller: " << caller->getName() << "\n";
        for (auto arg_it = callee->arg_begin(); arg_it != callee->arg_end(); ++arg_it) {
            llvm::Argument* arg = &*arg_it;
            llvm::dbgs() << "   Arg: " << *arg << "\n";
            assert(F_pdg->hasFormalArgNode(arg));
            auto arg_node = F_pdg->getFormalArgNode(arg);
            for (auto edge_it = arg_node->inEdgesBegin();
                    edge_it != arg_node->inEdgesEnd();
                    ++edge_it) {
                auto src = (*edge_it)->getSource();
                if (auto* actual_arg = llvm::dyn_cast<pdg::PDGLLVMActualArgumentNode>(src.get())) {
                    if (cs == actual_arg->getCallSite()) {
                        llvm::dbgs() << "       conn: " << src->getNodeAsString() << "\n";
                    }
                } else {
                    llvm::dbgs() << "       conn: " << src->getNodeAsString() << "\n";
                }
            }

        }

        //assert(pdg->hasFunctionPDG(caller));
        //auto caller_pdg = pdg->getFunctionPDG(caller);
        //assert(caller_pdg->hasNode(call_inst));
        //auto* node = caller_pdg->getNode(call_inst);
    }
};

char CallSiteConnectionsPrinter::ID = 0;
static llvm::RegisterPass<CallSiteConnectionsPrinter> X("dump-cs-info","Dump Call site information");
