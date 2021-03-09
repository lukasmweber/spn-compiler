//
// This file is part of the SPNC project.
// Copyright (c) 2020 Embedded Systems and Applications Group, TU Darmstadt. All rights reserved.
//

#include <mlir/ExecutionEngine/OptUtils.h>
#include <llvm/Support/TargetRegistry.h>
#include "GPUtoLLVMConversion.h"
#include "mlir/Conversion/SCFToStandard/SCFToStandard.h"
#include "mlir/Conversion/StandardToLLVM/ConvertStandardToLLVM.h"
#include "mlir/Conversion/GPUCommon/GPUCommonPass.h"
#include "mlir/Conversion/GPUToNVVM/GPUToNVVMPass.h"
#include "mlir/Dialect/GPU/GPUDialect.h"
#include "mlir/InitAllPasses.h"
#include "mlir/Target/LLVMIR/ModuleTranslation.h"
#include "mlir/Translation.h"
#include "mlir/Dialect/LLVMIR/NVVMDialect.h"
#include "llvm/Support/InitLLVM.h"
#include "llvm/Support/TargetSelect.h"
#include "mlir/Target/NVVMIR.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/Linker/Linker.h"
#include "mlir/Dialect/StandardOps/Transforms/Passes.h"

#ifndef SPNC_LIBDEVICE_FILE
// The location of libdevice is usually auto-detected and set by CMake.
#define SPNC_LIBDEVICE_FILE "/usr/local/cuda/nvvm/libdevice/libdevice.10.bc"
#endif

#include "cuda.h"

inline void emit_cuda_error(const llvm::Twine& message, const char* buffer,
                            CUresult error, mlir::Location loc) {
  SPNC_FATAL_ERROR(message.concat(" failed with error code ")
                       .concat(llvm::Twine{error})
                       .concat("[")
                       .concat(buffer)
                       .concat("]").str());
}

#define RETURN_ON_CUDA_ERROR(expr, msg)                                        \
  {                                                                            \
    auto _cuda_error = (expr);                                                 \
    if (_cuda_error != CUDA_SUCCESS) {                                         \
      emit_cuda_error(msg, jitErrorBuffer, _cuda_error, loc);                  \
      return {};                                                               \
    }                                                                          \
  }

mlir::OwnedBlob spnc::GPUtoLLVMConversion::compilePtxToCubin(const std::string ptx, mlir::Location loc,
                                                             llvm::StringRef name) {
  // This code is mostly copy & pasta from mlir-cuda-runner.cpp

  // Text buffer to hold error messages if necessary.
  char jitErrorBuffer[4096] = {0};

  RETURN_ON_CUDA_ERROR(cuInit(0), "cuInit");

  // Linking requires a device context.
  CUdevice device;
  RETURN_ON_CUDA_ERROR(cuDeviceGet(&device, 0), "cuDeviceGet");
  CUcontext context;
  RETURN_ON_CUDA_ERROR(cuCtxCreate(&context, 0, device), "cuCtxCreate");
  CUlinkState linkState;

  CUjit_option jitOptions[] = {CU_JIT_ERROR_LOG_BUFFER,
                               CU_JIT_ERROR_LOG_BUFFER_SIZE_BYTES};
  void* jitOptionsVals[] = {jitErrorBuffer,
                            reinterpret_cast<void*>(sizeof(jitErrorBuffer))};

  RETURN_ON_CUDA_ERROR(cuLinkCreate(2,              /* number of jit options */
                                    jitOptions,     /* jit options */
                                    jitOptionsVals, /* jit option values */
                                    &linkState),
                       "cuLinkCreate");

  // Add the PTX assembly generated by LLVM's PTX backend to the link modules.
  RETURN_ON_CUDA_ERROR(
      cuLinkAddData(linkState, CUjitInputType::CU_JIT_INPUT_PTX,
                    const_cast<void*>(static_cast<const void*>(ptx.c_str())),
                    ptx.length(), name.str().data(), /* kernel name */
                    0,                               /* number of jit options */
                    nullptr,                         /* jit options */
                    nullptr                          /* jit option values */
      ),
      "cuLinkAddData");

  void* cubinData;
  size_t cubinSize;
  RETURN_ON_CUDA_ERROR(cuLinkComplete(linkState, &cubinData, &cubinSize),
                       "cuLinkComplete");

  // Turn the generated Cubin into a binary blob that we can attach to the MLIR host module.
  char* cubinAsChar = static_cast<char*>(cubinData);
  mlir::OwnedBlob result =
      std::make_unique<std::vector<char>>(cubinAsChar, cubinAsChar + cubinSize);

  // This will also destroy the cubin data.
  RETURN_ON_CUDA_ERROR(cuLinkDestroy(linkState), "cuLinkDestroy");
  RETURN_ON_CUDA_ERROR(cuCtxDestroy(context), "cuCtxDestroy");

  return result;
}

std::unique_ptr<llvm::Module> spnc::GPUtoLLVMConversion::translateAndLinkGPUModule(mlir::Operation* gpuModule,
                                                                                   llvm::LLVMContext& llvmContext,
                                                                                   llvm::StringRef name) {
  // Translate the input MLIR GPU module to NVVM IR (LLVM IR + some extension.
  auto llvmModule = mlir::translateModuleToNVVMIR(gpuModule, llvmContext, name);
  // The kernel might use some optimized device functions from libdevice (__nv_*, e.g. __nv_exp).
  // libdevice is shipped as LLVM bitcode by Nvidia, so we load the bitcode file and link it
  // with the translated NVVM IR module.
  llvm::SMDiagnostic Err;
  auto libdevice = llvm::parseIRFile(SPNC_LIBDEVICE_FILE, Err, llvmContext);
  if (!libdevice) {
    SPNC_FATAL_ERROR("Failed to load libdevice: {}", Err.getMessage().str());
  }
  llvm::Linker::linkModules(*llvmModule, std::move(libdevice));

  // Apply optimizations to the module after linking.
  auto gpuOptPipeline = mlir::makeOptimizingTransformer(3, 0, nullptr);
  if (auto err = gpuOptPipeline(llvmModule.get())) {
    SPNC_FATAL_ERROR("Optimization of converted GPU LLVM IR failed");
  }
  llvm::dbgs() << "LLVM GPU module after optimization:\n";
  llvmModule->dump();
  return llvmModule;
}

mlir::ModuleOp& spnc::GPUtoLLVMConversion::execute() {
  if (!cached) {
    // Initialize LLVM NVPTX backend, as we will lower the
    // content of the GPU module to PTX and compile it to cubin.
    LLVMInitializeNVPTXTarget();
    LLVMInitializeNVPTXTargetInfo();
    LLVMInitializeNVPTXTargetMC();
    LLVMInitializeNVPTXAsmPrinter();
    //
    // The lowering of the GPU-part of the input module involves the
    // following steps:
    // 1. Outline the GPU parts from the host part of the module.
    // 2. Run a bunch of transformation passes on the GPU-portion of the module.
    // 3. Convert the GPU kernels to a binary blob. For this purpose, the
    //    GPU portion of the module is translated to NVVM IR (essentially LLVM IR with some extensions)
    //    and compiled to PTX assembly using LLVM's PTX backend. The generated PTX is then compiled and
    //    linked into CUBIN using the CUDA runtime library API.
    //    The binary representation of the CUBIN is attached to the MLIR module as a
    //    string attribute and will be included as binary blob in the compiler output.
    //    At runtime, the binary blob is loaded with the CUDA API and executed.
    // 4. Apply the conversion to async calls to all the GPU calls on the host side.
    // 5. Replace the calls to GPU management functions on the host side with calls
    //    to a very thin runtime wrapper library around the CUDA API. This step also
    //    lowers the remaining code from standard to LLVM dialect.
    // 6. Lower the newly generated calls to LLVM dialect.
    // The result of this transformation is a MLIR module with only the host-part remaining as MLIR
    // code (the GPU portion is a binary blob attribute) in LLVM dialect that can then be lowered to LLVM IR.
    // Enable IR printing if requested via CLI

    // Setup the pass manager.
    mlir::PassManager pm{mlirContext.get()};
    if (spnc::option::dumpIR.get(*this->config)) {
      pm.enableIRPrinting(/* Print before every pass*/ [](mlir::Pass*, mlir::Operation*) { return false; },
          /* Print after every pass*/ [](mlir::Pass*, mlir::Operation*) { return true; },
          /* Print module scope*/ true,
          /* Print only after change*/ false);
    }
    auto inputModule = input.execute();
    // Clone the module to keep the original module available
    // for actions using the same input module.
    module = std::make_unique<mlir::ModuleOp>(inputModule.clone());

    // Outline the GPU kernels from the host-part of the code.
    pm.addPass(mlir::createGpuKernelOutliningPass());
    // Nested pass manager operating only on the GPU-part of the code.
    auto& kernelPm = pm.nest<mlir::gpu::GPUModuleOp>();
    kernelPm.addPass(mlir::createStripDebugInfoPass());
    kernelPm.addPass(mlir::createLowerGpuOpsToNVVMOpsPass());
    // Convert the GPU-part to a binary blob and annotate it as an atttribute to the MLIR module.
    // translateAndLinkModule and compilePtxToCubin are call-backs.
    // TODO Retrieve and use information about target GPU from CUDA.
    const char gpuBinaryAnnotation[] = "nvvm.cubin";
    kernelPm.addPass(mlir::createConvertGPUKernelToBlobPass(
        GPUtoLLVMConversion::translateAndLinkGPUModule,
        GPUtoLLVMConversion::compilePtxToCubin,
        "nvptx64-nvidia-cuda", "sm_35", "+ptx60",
        gpuBinaryAnnotation));
    auto& funcPm = pm.nest<mlir::FuncOp>();
    funcPm.addPass(mlir::createStdExpandOpsPass());
    funcPm.addPass(mlir::createGpuAsyncRegionPass());
    funcPm.addPass(mlir::createAsyncRefCountingPass());
    // Convert the host-side GPU operations into runtime library calls.
    // This also lowers Standard-dialect operations to LLVM dialect.
    pm.addPass(mlir::createGpuToLLVMConversionPass(gpuBinaryAnnotation));
    pm.addPass(mlir::createAsyncToAsyncRuntimePass());
    pm.addPass(mlir::createConvertAsyncToLLVMPass());
    pm.addPass(mlir::createLowerToLLVMPass());

    auto result = pm.run(*module);
    if (failed(result)) {
      SPNC_FATAL_ERROR("Converting the GPU module failed");
    }
    auto verificationResult = module->verify();
    if (failed(verificationResult)) {
      SPNC_FATAL_ERROR("Module failed verification after conversion of GPU code");
    }
    cached = true;
  }
  return *module;
}

spnc::GPUtoLLVMConversion::GPUtoLLVMConversion(ActionWithOutput<mlir::ModuleOp>& input,
                                               std::shared_ptr<mlir::MLIRContext> ctx) :
    ActionSingleInput<mlir::ModuleOp, mlir::ModuleOp>(input),
    mlirContext{std::move(ctx)} {}