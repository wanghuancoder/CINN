// Copyright (c) 2022 CINN Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <gflags/gflags.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/types/variant.h"
#include "cinn/common/cas.h"
#include "cinn/common/cinn_value.h"
#include "cinn/common/common.h"
#include "cinn/common/context.h"
#include "cinn/common/ir_util.h"
#include "cinn/common/macros.h"
#include "cinn/common/target.h"
#include "cinn/hlir/framework/node.h"
#include "cinn/hlir/framework/op.h"
#include "cinn/hlir/framework/op_strategy.h"
#include "cinn/hlir/op/op_util.h"
#include "cinn/hlir/pe/elementwise.h"
#include "cinn/hlir/pe/ir_schedule_pe.h"
#include "cinn/hlir/pe/nn.h"
#include "cinn/hlir/pe/schedule.h"
#include "cinn/ir/ir.h"
#include "cinn/ir/ir_base.h"
#include "cinn/ir/ir_operators.h"
#include "cinn/ir/tensor.h"
#include "cinn/lang/builtin.h"
#include "cinn/lang/compute.h"
#include "cinn/lang/packed_func.h"
#include "cinn/poly/stage.h"
#include "glog/logging.h"

namespace cinn {
namespace hlir {
namespace op {

using common::CINNValue;
using common::CINNValuePack;

std::shared_ptr<framework::OpStrategy> StrategyForCholesky(const framework::NodeAttr &attrs,
                                                           const std::vector<ir::Tensor> &inputs,
                                                           const std::vector<Type> &out_type,
                                                           const std::vector<std::vector<int>> &output_shapes,
                                                           const Target &target) {
  framework::CINNCompute cholesky_compute([=](lang::Args args, lang::RetValue *ret) {
    CINNValuePack pack_args = args[0];
    CHECK(!pack_args.empty()) << "at least one input tensor for cholesky compute\n";
    Expr x_expr             = pack_args[0];
    ir::Tensor x            = x_expr.as_tensor_ref();
    std::string tensor_name = "cholesky_out";
    auto out                = pe::Identity(x, tensor_name).front();
    auto stages             = CreateStages({out});
    std::vector<CINNValue> res{CINNValue(out), CINNValue(stages)};
    *ret = CINNValuePack{res};
  });
  auto strategy = std::make_shared<framework::OpStrategy>();
  strategy->AddImpl(cholesky_compute, GetInjectiveScheduleFunc(output_shapes, target), "strategy.cholesky.x86", 1);
  return strategy;
}

std::vector<framework::shape_t> InferShapeForCholesky(const std::vector<framework::shape_t> &inputs_shape,
                                                      const framework::AttrMapType &attrs) {
  CHECK_EQ(inputs_shape.size(), 1U) << "The input's shape size should be 1! Please check again.";
  framework::shape_t x_shape = inputs_shape[0];
  int x_shape_size           = x_shape.size();
  CHECK_GE(x_shape_size, 2U) << "The input x shape size should >= 2! Please check again.";
  CHECK_EQ(x_shape[x_shape_size - 2], x_shape[x_shape_size - 1])
      << "The last two dimensions of the input x must be the same!";
  return inputs_shape;
}

std::vector<Type> InferDtypeForCholesky(const std::vector<Type> &inputs_type, const framework::AttrMapType &attrs) {
  CHECK_EQ(inputs_type.size(), 1U) << "The input's shape size should be 1! Please check again.";
  CHECK(inputs_type[0].is_float()) << "The input's dtype should be float32! Please check again.";
  return inputs_type;
}

}  // namespace op
}  // namespace hlir
}  // namespace cinn

CINN_REGISTER_HELPER(cholesky_ops) {
  CINN_REGISTER_OP(cholesky)
      .describe("Cholesky")
      .set_num_inputs(1)
      .set_num_outputs(1)
      .set_attr<cinn::hlir::framework::StrategyFunction>("CINNStrategy", cinn::hlir::op::StrategyForCholesky)
      .set_attr("infershape", MakeOpFunction(cinn::hlir::op::InferShapeForCholesky))
      .set_attr("inferdtype", MakeOpFunction(cinn::hlir::op::InferDtypeForCholesky))
      .set_attr<cinn::hlir::framework::OpPatternKind>("OpPattern", cinn::hlir::framework::OpPatternKind::kElementWise)
      .set_support_level(4);

  return true;
}
