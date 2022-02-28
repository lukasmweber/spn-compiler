# ==============================================================================
#  This file is part of the SPNC project under the Apache License v2.0 by the
#  Embedded Systems and Applications Group, TU Darmstadt.
#  For the full copyright and license information, please view the LICENSE
#  file that was distributed with this source code.
#  SPDX-License-Identifier: Apache-2.0
# ==============================================================================

import numpy as np
import tempfile
import os

from xspn.serialization.binary.BinarySerialization import BinarySerializer
from xspn.structure.Model import SPNModel
from xspn.structure.Query import JointProbability, ErrorModel
import spnc.spncpy as spncpy

class FPGACompiler:

    def compile_ll(self, spn, inputDataType = "float64", errorModel = ErrorModel(), batchSize = 64, name = "spn_fpga"):
        model = SPNModel(spn, inputDataType, name)
        query = JointProbability(model, batchSize = batchSize, supportMarginal = False, rootError = errorModel)

        tmpfile = tempfile.NamedTemporaryFile()
        if self.verbose:
            print(f"Serializing SPN to {tmpfile}")
        BinarySerializer(tmpfile.name).serialize_to_file(query)
        # Check that the serialization worked.
        if not os.path.isfile(tmpfile.name):
            raise RuntimeError("Serialization of the SPN failed")

        options = dict({"target": "FPGA"})

        kernel = spncpy.SPNCompiler.compileQuery(tmpfile.name, options)

        return kernel

    def execute(self, kernel, inputs):
        if type(inputs) is not np.ndarray:
            raise RuntimeError("Input is not an numpy array")
        if inputs.ndim != 2:
            raise RuntimeError("Input must be a two-dimensional array")

        numSamples = inputs.shape[0]
        results = kernel.execute(numSamples, inputs)
        return results

        def log_likelihood(self, spn, inputs, errorModel = ErrorModel(),
                           batchSize = 64):
            if type(inputs) is not np.ndarray:
                raise RuntimeError("Input is not an numpy array")
            if inputs.ndim != 2:
                raise RuntimeError("Input must be a two-dimensional array")

            dataType = inputs.dtype
            kernel = self.compile_ll(spn, str(dataType), errorModel = errorModel,
                             batchSize = batchSize, supportMarginal = supportMarginal,
                             name = "spn_gpu")
            results = self.execute(kernel, inputs)
            return results