//==============================================================================
// This file is part of the SPNC project under the Apache License v2.0 by the
// Embedded Systems and Applications Group, TU Darmstadt.
// For the full copyright and license information, please view the LICENSE
// file that was distributed with this source code.
// SPDX-License-Identifier: Apache-2.0
//==============================================================================

#include <util/Command.h>
#include "RunXSPNFPGA.h"
#include "toolchain/MLIRToolchain.h"

using namespace spnc;

spnc::ExecutionResult RunXSPNFPGA::executeStep(BinarySPN *inputFile) {

    // Invoke xspn-fpga Simple CLI to compile the SPN from inputFile to a bitstream using tapasco
    // This will also load the bitstream using tapasco-load-bitstream *
    std::vector<std::string> command;
    command.emplace_back("xscli");
    command.emplace_back(inputFile->fileName());
    Command::executeExternalCommand(command);


    auto* kernelInfo = getContext()->get<KernelInfo>();
    kernel = std::make_unique<Kernel>("", "",
                                      kernelInfo->queryType, kernelInfo->target, kernelInfo->batchSize,
                                      kernelInfo->numFeatures, kernelInfo->bytesPerFeature,
                                      kernelInfo->numResults, kernelInfo->bytesPerResult,
                                      kernelInfo->dtype);
    return success();
}

Kernel* RunXSPNFPGA::result() {
    return kernel.get();
}