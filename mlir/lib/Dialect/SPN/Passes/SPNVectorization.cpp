//
// This file is part of the SPNC project.
// Copyright (c) 2020 Embedded Systems and Applications Group, TU Darmstadt. All rights reserved.
//

#include "mlir/IR/Matchers.h"
#include "mlir/IR/PatternMatch.h"
#include "SPN/SPNPasses.h"
#include "SPNPassDetails.h"
#include <iostream>
#include <SPN/SPNInterfaces.h>
#include <mlir/Pass/PassManager.h>
#include "SPN/Analysis/SLP/SLPTree.h"
#include "SPN/Analysis/SLP/SLPSeeding.h"

using namespace mlir;
using namespace mlir::spn;

namespace {

  struct SPNVectorization : public SPNVectorizationBase<SPNVectorization> {

  protected:
    void runOnOperation() override {
      std::cout << "Starting SPN vectorization..." << std::endl;
      auto func = getOperation();

      func.walk([&](Operation* topLevelOp) {
        if (auto query = dyn_cast<QueryInterface>(topLevelOp)) {
          for (auto root : query.getRootNodes()) {

            // ============ TREE CHECK ============
            llvm::StringMap<std::vector<Operation*>> operationsByOpCode;
            for (auto& op : root->getBlock()->getOperations()) {
              operationsByOpCode[op.getName().getStringRef().str()].emplace_back(&op);
              auto const& uses = std::distance(op.getUses().begin(), op.getUses().end());
              if (uses > 1) {
                std::cerr << "SPN is not a tree!" << std::endl;
              }
            }
            // ====================================

            auto& seedAnalysis = getAnalysis<slp::SeedAnalysis>();

            auto const& seeds = seedAnalysis.getSeeds(4);

            if (!seeds.empty()) {
              // Run simplification on seeds to binarize them into trees.
              OpPassManager binarizer("operation", mlir::OpPassManager::Nesting::Explicit);
              binarizer.addPass(createSPNOpSimplifierPass());
              runPipeline(binarizer, seeds.front().front());
              std::cout << "binarized a seed!" << std::endl;
            }

            std::cout << "seeds computed" << std::endl;

            slp::SLPTree graph(root, 4, 3);
          }
        }
      });
    }
  };

}

std::unique_ptr<OperationPass<ModuleOp>> mlir::spn::createSPNVectorizationPass() {
  return std::make_unique<SPNVectorization>();
}

