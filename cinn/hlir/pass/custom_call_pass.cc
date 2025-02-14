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

#include "cinn/common/type.h"
#include "cinn/hlir/pass/fusion_helper_base.h"
namespace cinn {
namespace hlir {
namespace pass {

using framework::Graph;
using framework::Node;
using framework::NodeData;
using framework::OpPatternKind;
using framework::shape_t;

using common::GraphEdge;
using common::GraphNode;

using GroupPtr  = std::shared_ptr<Graph::Group>;
using GroupList = std::vector<GroupPtr>;

using ShapeDict         = absl::flat_hash_map<std::string, shape_t>;
using ConditionFunction = std::function<bool(const Node*, const Node*)>;

class GraphAlterHelper {
 public:
  GraphAlterHelper(Graph* graph) : graph_(graph) {}
  void MatmulToCublasCustomCall() {
    auto nodes = graph_->CollectNodes([](const common::GraphNode* graph_node) -> bool {
      if (graph_node->safe_as<Node>()) {
        auto node = graph_node->safe_as<Node>();
        if (node->op()->name == "matmul" || node->op()->name == "mul" || node->op()->name == "cublas_gemm" ||
            node->op()->name == "cublas_matmul") {
          return true;
        }
      }

      return false;
    });

    for (auto gnode : nodes) {
      auto src = gnode->safe_as<Node>();
      CHECK(src);
      src->attrs.op                        = framework::Operator::Get("custom_call");
      src->attrs.attr_store["custom_call"] = std::string("cinn_call_cublas");
    }
  }

  void ConvToCudnnCustomCall() {
    auto nodes = graph_->CollectNodes([](const common::GraphNode* graph_node) -> bool {
      if (graph_node->safe_as<Node>()) {
        auto node = graph_node->safe_as<Node>();
        if (node->op()->name == "conv2d") {
          return true;
        }
      }

      return false;
    });

    for (auto gnode : nodes) {
      auto src = gnode->safe_as<Node>();
      CHECK(src);
      src->attrs.op = framework::Operator::Get("custom_call");
      CHECK(src->attrs.attr_store.count("conv_type"));
      std::string type = src->attrs.attr_store.count("conv_type")
                             ? absl::get<std::string>(src->attrs.attr_store["conv_type"])
                             : "forward";
      if (type == "forward") {
        src->attrs.attr_store["custom_call"] = std::string("cinn_call_cudnn_conv2d_forward");
      } else if (type == "backward_data") {
        src->attrs.attr_store["custom_call"] = std::string("cinn_call_cudnn_conv2d_backward_data");
      } else if (type == "backward_filter") {
        src->attrs.attr_store["custom_call"] = std::string("cinn_call_cudnn_conv2d_backward_filter");
      } else {
        LOG(FATAL) << "conv type is unkown!";
      }
      auto out_links = src->outlinks_in_order(true);
      for (int idx = 1; idx < out_links.size(); ++idx) {
        auto link = out_links[idx];
        CHECK(link->sink()->safe_as<NodeData>());
        src->UnLinkSingleTo(link->sink());
        graph_->DropNode(link->sink());
      }
    }
  }

  void GaussianRandomToCustomCall() {
    auto nodes = graph_->CollectNodes([](const common::GraphNode* graph_node) -> bool {
      if (graph_node->safe_as<Node>()) {
        auto node = graph_node->safe_as<Node>();
        if (node->op()->name == "gaussian_random") {
          return true;
        }
      }

      return false;
    });

    for (auto gnode : nodes) {
      auto src = gnode->safe_as<Node>();
      CHECK(src);
      src->attrs.op                        = framework::Operator::Get("custom_call");
      src->attrs.attr_store["custom_call"] = std::string("cinn_call_gaussian_random");
    }
  }

  void UniformRandomToCustomCall() {
    auto nodes = graph_->CollectNodes([](const common::GraphNode* graph_node) -> bool {
      if (graph_node->safe_as<Node>()) {
        auto node = graph_node->safe_as<Node>();
        if (node->op()->name == "uniform_random") {
          return true;
        }
      }

      return false;
    });

    for (auto gnode : nodes) {
      auto src = gnode->safe_as<Node>();
      CHECK(src);
      src->attrs.op                        = framework::Operator::Get("custom_call");
      src->attrs.attr_store["custom_call"] = std::string("cinn_call_uniform_random");
    }
  }

  void CholeskyToCustomCall() {
    auto nodes = graph_->CollectNodes([](const common::GraphNode* graph_node) -> bool {
      if (graph_node->safe_as<Node>()) {
        auto node = graph_node->safe_as<Node>();
        if (node->op()->name == "cholesky") {
          return true;
        }
      }

      return false;
    });

    Target target = graph_->target_;

    for (auto gnode : nodes) {
      auto src = gnode->safe_as<Node>();
      CHECK(src);
      src->attrs.op = framework::Operator::Get("custom_call");
      if (target.arch == Target::Arch::X86) {
        src->attrs.attr_store["custom_call"] = std::string("cinn_call_cholesky_host");
      } else if (target.arch == Target::Arch::NVGPU) {
        src->attrs.attr_store["custom_call"] = std::string("cinn_call_cholesky_nvgpu");
      }
    }
  }

 private:
  Graph* graph_;
};

void MatmulToCublasCustomCallPassInternal(Graph* graph) {
  VLOG(3) << "MatmulToCublasCustomCallPass...!";
  GraphAlterHelper(graph).MatmulToCublasCustomCall();
  VLOG(3) << "MatmulToCublasCustomCallPass Finish...!";
}

void ConvToCudnnCustomCallPassInternal(Graph* graph) {
  VLOG(3) << "ConvToCudnnCustomCallPass...!";
  GraphAlterHelper(graph).ConvToCudnnCustomCall();
  VLOG(3) << "ConvToCudnnCustomCallPass Finish...!";
}

void GaussianRandomToCustomCallInternal(Graph* graph) {
  VLOG(3) << "GaussianRandomToCustomCall...!";
  GraphAlterHelper(graph).GaussianRandomToCustomCall();
  VLOG(3) << "GaussianRandomToCustomCall Finish...!";
}

void UniformRandomToCustomCallInternal(Graph* graph) {
  VLOG(3) << "UniformRandomToCustomCall...!";
  GraphAlterHelper(graph).UniformRandomToCustomCall();
  VLOG(3) << "UniformRandomToCustomCall Finish...!";
}

void CholeskyToCustomCallPassInternal(Graph* graph) {
  VLOG(3) << "CholeskyToCustomCallPass...!";
  GraphAlterHelper(graph).CholeskyToCustomCall();
  VLOG(3) << "CholeskyToCustomCallPass Finish...!";
}

}  // namespace pass
}  // namespace hlir
}  // namespace cinn

CINN_REGISTER_HELPER(CustomCallPass) {
#ifdef CINN_WITH_CUDA
  CINN_REGISTER_PASS(MatmulToCublasCustomCallPass)
      .describe("This pass which convert matmul op to custom call pass.")
      .set_change_structure(false)
      .set_body(cinn::hlir::pass::MatmulToCublasCustomCallPassInternal);
  CINN_REGISTER_PASS(GaussianRandomToCustomCallPass)
      .describe("This pass which convert gaussian random op to custom call pass.")
      .set_change_structure(false)
      .set_body(cinn::hlir::pass::GaussianRandomToCustomCallInternal);
  CINN_REGISTER_PASS(UniformRandomToCustomCallPass)
      .describe("This pass which convert uniform random op to custom call pass.")
      .set_change_structure(false)
      .set_body(cinn::hlir::pass::UniformRandomToCustomCallInternal);
#endif
#ifdef CINN_WITH_CUDNN
  CINN_REGISTER_PASS(ConvToCudnnCustomCallPass)
      .describe("This pass which convert conv op to custom call pass.")
      .set_change_structure(false)
      .set_body(cinn::hlir::pass::ConvToCudnnCustomCallPassInternal);
#endif
  CINN_REGISTER_PASS(CholeskyToCustomCallPass)
      .describe("This pass which convert cholesky op to custom call pass.")
      .set_change_structure(false)
      .set_body(cinn::hlir::pass::CholeskyToCustomCallPassInternal);

  return true;
}
