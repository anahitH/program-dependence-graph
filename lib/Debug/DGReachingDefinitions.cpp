#include "llvm/Pass.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Module.h"
#include "llvm/PassRegistry.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/raw_os_ostream.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/IR/Metadata.h"

#include "dg/llvm/LLVMDependenceGraphBuilder.h"
#include "dg/llvm/LLVMDependenceGraph.h"
#include "dg/llvm/analysis/DefUse/DefUse.h"
#include "dg/llvm/analysis/PointsTo/PointerAnalysis.h"
#include "dg/llvm/analysis/ReachingDefinitions/ReachingDefinitions.h"
#include "dg/analysis/PointsTo/PointerAnalysisFS.h"
#include "dg/analysis/PointsTo/PointerAnalysisFI.h"
#include "dg/analysis/PointsTo/PointerAnalysisFSInv.h"

#include <memory>

class DGReachingDefinitionsPass : public llvm::ModulePass
{
public:
    static char ID;
    DGReachingDefinitionsPass()
        : llvm::ModulePass(ID)
    {
    }

    bool runOnModule(llvm::Module& M) override
    {
        llvm::dbgs() << "DGReachingDefinitionsPass\n";
        dg::llvmdg::LLVMDependenceGraphOptions options;
        options.PTAOptions.analysisType = dg::LLVMPointerAnalysisOptions::AnalysisType::fi;
        options.RDAOptions.analysisType = dg::analysis::LLVMReachingDefinitionsAnalysisOptions::AnalysisType::dense;
        std::unique_ptr<dg::LLVMPointerAnalysis> pta(
                new dg::LLVMPointerAnalysis(&M, options.PTAOptions));
        std::unique_ptr<dg::LLVMReachingDefinitions> rda(new
                dg::LLVMReachingDefinitions(&M, pta.get(), options.RDAOptions));

        pta->run<dg::analysis::pta::PointerAnalysisFI>();
        rda->run<dg::analysis::rd::ReachingDefinitionsAnalysis>();

        llvm::DataLayout dl(&M);
        for (auto& F : M) {
            llvm::dbgs() << "Reaching definitions in function " << F.getName() << "\n";
            for (auto& B : F) {
                for (auto& I : B) {
                    if (auto* loadI = llvm::dyn_cast<llvm::LoadInst>(&I)) {
                        llvm::dbgs() << "   " << I << "\n";
                        auto* ptrOp = loadI->getPointerOperand();
                        auto* pts = pta->getPointsTo(ptrOp);
                        if (!pts) {
                            llvm::dbgs() << "       No points to\n";
                            continue;
                        }
                        auto *mem = rda->getMapping(&I);
                        for (const auto& ptr : pts->pointsTo) {
                            if (!ptr.isValid())
                                continue;

                            if (ptr.isInvalidated())
                                continue;
                            if (llvm::Value* llvmVal = ptr.target->getUserData<llvm::Value>()) {
                                auto* val = rda->getNode(llvmVal);
                                std::set<dg::analysis::rd::RDNode *> defs;
                                auto size = dl.getTypeAllocSize(I.getType());
                                mem->getReachingDefinitions(val, ptr.offset, size, defs);
                                for (auto *rd : defs) {
                                    if (rd->isUnknown()) {
                                        continue;
                                    }
                                    if (rd->getType() != dg::analysis::rd::RDNodeType::PHI) {
                                        llvm::dbgs() << *(rd->getUserData<llvm::Value>()) << "\n";
                                    }
                                }

                            }
                        }
                    }
                }
            }
        }

        return false;
    }

}; // class DGReachingDefinitionsPass

char DGReachingDefinitionsPass::ID = 0;
static llvm::RegisterPass<DGReachingDefinitionsPass> X("dg-rd","Print dg reaching definitions");

