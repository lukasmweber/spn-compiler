//==============================================================================
// This file is part of the SPNC project under the Apache License v2.0 by the
// Embedded Systems and Applications Group, TU Darmstadt.
// For the full copyright and license information, please view the LICENSE
// file that was distributed with this source code.
// SPDX-License-Identifier: Apache-2.0
//==============================================================================

#ifndef SPNC_MLIR_INCLUDE_CONVERSION_HISPNTOLOSPN_HISPNTYPECONVERTER_H
#define SPNC_MLIR_INCLUDE_CONVERSION_HISPNTOLOSPN_HISPNTYPECONVERTER_H

#include <mlir/Transforms/DialectConversion.h>
#include "mlir/IR/BuiltinTypes.h"
#include "HiSPN/HiSPNDialect.h"

namespace mlir {
  namespace spn {

    ///
    /// TypeConverter for the SPN dialects, turning Tensors into equivalent MemRefs.
    class HiSPNTypeConverter : public TypeConverter {

    public:

      ///
      /// Constructor populating the TypeConverter.
      explicit HiSPNTypeConverter(mlir::Type spnComputeType) {
        addConversion([](TensorType tensorType) -> Optional<Type> {
          if (!tensorType.hasRank()) {
            return llvm::None;
          }
          return tensorType;
        });
        addConversion([](FloatType floatType) -> Optional<Type> {
          // Allow all floating-point bit-widths.
          return floatType;
        });
        addConversion([](IntegerType intType) -> Optional<Type> {
          // Allow all integer bit-widths.
          return intType;
        });
        addConversion([spnComputeType](mlir::spn::high::ProbabilityType probType) -> Optional<Type> {
          return spnComputeType;
        });
      }

    };
  }
}

#endif //SPNC_MLIR_INCLUDE_CONVERSION_HISPNTOLOSPN_HISPNTYPECONVERTER_H
