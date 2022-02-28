//==============================================================================
// This file is part of the SPNC project under the Apache License v2.0 by the
// Embedded Systems and Applications Group, TU Darmstadt.
// For the full copyright and license information, please view the LICENSE
// file that was distributed with this source code.
// SPDX-License-Identifier: Apache-2.0
//==============================================================================

#include "FPGAToolchain.h"
#include "pipeline/BasicSteps.h"
#include "util/FileSystem.h"

using namespace spnc;

std::unique_ptr<Pipeline < Kernel>> spnc::FPGAToolchain::setupPipeline(const std::string &inputFile, std::unique_ptr<interface::Configuration> config) {

    std::unique_ptr<Pipeline<Kernel>> pipeline = std::make_unique<Pipeline<Kernel>>();

    auto targetMachine = createTargetMachine(0);
    auto kernelInfo = std::make_unique<KernelInfo>();
    kernelInfo->target = KernelTarget::FPGA;

    pipeline->getContext()->add(std::move(targetMachine));
    pipeline->getContext()->add(std::move(kernelInfo));

    auto& locateInput = pipeline->emplaceStep<LocateFile<FileType::SPN_BINARY >>(inputFile);


    return pipeline;
}
