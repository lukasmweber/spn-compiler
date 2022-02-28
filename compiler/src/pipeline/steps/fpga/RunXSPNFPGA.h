//
// Created by lw on 2/28/22.
//

#ifndef SPNC_COMPILER_SRC_PIPELINE_FPGA_RUNXSPNFPGA_H
#define SPNC_COMPILER_SRC_PIPELINE_FPGA_RUNXSPNFPGA_H

#include <pipeline/Pipeline.h>
#include <util/FileSystem.h>
#include "Kernel.h"

namespace spnc {

    class RunXSPNFPGA : public StepSingleInput<RunXSPNFPGA, BinarySPN>,
                        public StepWithResult<Kernel>{

    public:

        using StepSingleInput<RunXSPNFPGA, BinarySPN>::StepSingleInput;

        ExecutionResult executeStep(BinarySPN* inputFile);

        Kernel* result() override;

        STEP_NAME("run-xspn-fpga")

    private:

        std::unique_ptr<Kernel> kernel;


    };


}


#endif //SPNC_COMPILER_SRC_PIPELINE_FPGA_RUNXSPNFPGA_H
