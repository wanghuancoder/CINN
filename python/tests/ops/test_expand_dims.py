#!/usr/bin/env python3

# Copyright (c) 2021 CINN Authors. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import unittest
import numpy as np
from op_test import OpTest, OpTestTool
import paddle
import cinn
from cinn.frontend import *
from cinn.common import *


@OpTestTool.skip_if(not is_compiled_with_cuda(),
                    "x86 test will be skipped due to timeout.")
class TestExpandDimsOp(OpTest):
    def setUp(self):
        self.init_case()

    def init_case(self):
        self.inputs = {
            "x": np.random.random([
                32,
                64,
            ]).astype("float32")
        }
        self.axes = [0]

    def build_paddle_program(self, target):
        x = paddle.to_tensor(self.inputs["x"], stop_gradient=True)
        out = paddle.unsqueeze(x, self.axes)

        self.paddle_outputs = [out]

    # Note: If the forward and backward operators are run in the same program,
    # the forward result will be incorrect.
    def build_cinn_program(self, target):
        builder = NetBuilder("expand_dims")
        x = builder.create_input(Float(32), self.inputs["x"].shape, "x")
        out = builder.expand_dims(x, self.axes)

        prog = builder.build()
        res = self.get_cinn_output(prog, target, [x], [self.inputs["x"]],
                                   [out])

        self.cinn_outputs = res

    def test_check_results(self):
        self.check_outputs_and_grads()


class TestExpandDimsCase1(TestExpandDimsOp):
    def init_case(self):
        self.inputs = {"x": np.random.random([2, 3, 4]).astype("float32")}
        self.axes = [0, 2, 4]


class TestExpandDimsCase2(TestExpandDimsOp):
    def init_case(self):
        self.inputs = {"x": np.random.random([2, 3, 4]).astype("float32")}
        self.axes = [3, 4, 5]


if __name__ == "__main__":
    unittest.main()
