//==============================================================================
// This file is part of the SPNC project under the Apache License v2.0 by the
// Embedded Systems and Applications Group, TU Darmstadt.
// For the full copyright and license information, please view the LICENSE
// file that was distributed with this source code.
// SPDX-License-Identifier: Apache-2.0
//==============================================================================

#ifndef SPNC_MLIR_INCLUDE_CONVERSION_LOSPNTOCPU_STRUCTUREPATTERNS_H
#define SPNC_MLIR_INCLUDE_CONVERSION_LOSPNTOCPU_STRUCTUREPATTERNS_H

#include "mlir/Transforms/DialectConversion.h"
#include "LoSPN/LoSPNDialect.h"
#include "LoSPN/LoSPNOps.h"
#include "llvm/Support/Debug.h"

namespace mlir {
  namespace spn {

    struct KernelLowering : OpConversionPattern<low::SPNKernel> {

      using OpConversionPattern<low::SPNKernel>::OpConversionPattern;

      LogicalResult matchAndRewrite(low::SPNKernel op,
                                    ArrayRef<Value> operands,
                                    ConversionPatternRewriter& rewriter) const override;
    };

    struct BatchTaskLowering : OpConversionPattern<low::SPNTask> {

      using OpConversionPattern<low::SPNTask>::OpConversionPattern;

      LogicalResult matchAndRewrite(low::SPNTask op,
                                    ArrayRef<Value> operands,
                                    ConversionPatternRewriter& rewriter) const override;
    };

    struct SingleTaskLowering : OpConversionPattern<low::SPNTask> {

      using OpConversionPattern<low::SPNTask>::OpConversionPattern;

      LogicalResult matchAndRewrite(low::SPNTask op,
                                    ArrayRef<Value> operands,
                                    ConversionPatternRewriter& rewriter) const override;
    };

    struct BodyLowering : OpConversionPattern<low::SPNBody> {

      using OpConversionPattern<low::SPNBody>::OpConversionPattern;

      LogicalResult matchAndRewrite(low::SPNBody op,
                                    ArrayRef<Value> operands,
                                    ConversionPatternRewriter& rewriter) const override;
    };

    static inline void populateLoSPNtoCPUStructurePatterns(OwningRewritePatternList& patterns, MLIRContext* context,
                                                           TypeConverter& typeConverter) {
      patterns.insert<KernelLowering>(typeConverter, context);
      patterns.insert<BodyLowering>(typeConverter, context);
    }

    static inline void populateLoSPNtoCPUTaskPatterns(OwningRewritePatternList& patterns,
                                                      MLIRContext* context,
                                                      TypeConverter& typeConverter) {
      patterns.insert<BatchTaskLowering, SingleTaskLowering>(typeConverter, context, 1);
    }
  }
}

#endif //SPNC_MLIR_INCLUDE_CONVERSION_LOSPNTOCPU_STRUCTUREPATTERNS_H
