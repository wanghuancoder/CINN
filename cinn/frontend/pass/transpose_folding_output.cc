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

#include <absl/container/flat_hash_map.h>
#include <absl/types/variant.h>

#include <string>
#include <unordered_set>

#include "cinn/frontend/pass/transpose_folding_base.h"
#include "cinn/frontend/program_pass.h"
#include "cinn/frontend/syntax.h"

namespace cinn::frontend::pass {

class TransposeFoldingOutputPass : public TransposeFoldingBase {
 public:
  using TransposeFoldingBase::TransposeFoldingBase;

 protected:
  void set_target_instrs() override { TransposeFoldingBase::target_instrs_ = {"cublas_matmul"}; }

  void DoMatmulFoldOptimize(Instruction* dot,
                            const Out2InstrType& out2instr,
                            const In2InstrType& in2instr,
                            const std::unordered_set<std::string>& fetch_ids,
                            absl::flat_hash_set<Instruction*>* remove_instrs) const override {
    const auto& gemm_out_name = (*dot)->outputs[0]->id;

    auto debug_info = [](const std::vector<Instruction*>& instrs) {
      std::stringstream ss;
      for (auto instr : instrs) {
        ss << (*instr)->op_type << ", ";
      }
      return ss.str();
    };

    if (in2instr.contains(gemm_out_name) && in2instr.at(gemm_out_name).size() == 1) {
      // for example: dot -> x -> scale -> y -> transpose -> z
      // fold_instrs = {"scale", "transpose"}
      // ensure the foldiong structions's output only link to one op
      const auto& fold_instrs = GetFoldInstruction(*in2instr.at(gemm_out_name).begin(), out2instr, in2instr, false);

      VLOG(4) << "Fold Instruction: [" << debug_info(fold_instrs) << "]"
              << " into output of matmul: " << *dot;

      if (fold_instrs.empty()) {
        return;
      }

      auto nex_instr = dot;
      for (int i = 0; i < fold_instrs.size(); ++i) {
        auto instr = fold_instrs[i];

        if (IsValidTranspose(*instr)) {
          // As for cublas_matmul, we can continue to set the `trans_out` attr.
          bool trans_out = (*dot)->attrs.count("trans_out") ? absl::get<bool>((*dot)->attrs.at("trans_out")) : false;
          dot->SetAttr("trans_out", static_cast<bool>(trans_out ^ true));
        } else if (IsValidScale(*instr)) {
          // assume C = alpha * A * B + beta * C
          // fold scale into alpha/beta
          float scale = (*instr)->attrs.count("scale") ? absl::get<float>((*instr)->attrs.at("scale")) : 1.0f;

          float alpha = (*dot)->attrs.count("alpha") ? absl::get<float>((*dot)->attrs.at("alpha")) : 1.0f;
          float beta  = (*dot)->attrs.count("beta") ? absl::get<float>((*dot)->attrs.at("beta")) : 0.0f;

          dot->SetAttr("alpha", alpha * scale);
          dot->SetAttr("beta", beta * scale);
        } else {
          nex_instr = instr;
          continue;
        }

        // relink input: dot -> x -> scale -> y ==> dot -> y
        (*nex_instr)->outputs[0] = (*instr)->outputs[0];

        // remove useless instruction, the `GetFoldInstruction` ensure this
        remove_instrs->insert(instr);
      }
    }
  }
};

}  // namespace cinn::frontend::pass

CINN_REGISTER_HELPER(TransposeFoldingOutput) {
  CINN_REGISTER_PROGRAM_PASS(TransposeFoldingOutput, ::cinn::frontend::pass::TransposeFoldingOutputPass);

  return true;
}
