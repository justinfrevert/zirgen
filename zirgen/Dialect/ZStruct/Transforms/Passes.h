// Copyright 2024 RISC Zero, Inc.
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

#pragma once

#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/PatternMatch.h"
#include "mlir/Pass/Pass.h"

namespace zirgen::ZStruct {

// Pass constructors
std::unique_ptr<mlir::OperationPass<mlir::ModuleOp>> createOptimizeLayoutPass();
std::unique_ptr<mlir::Pass> createUnrollPass();
std::unique_ptr<mlir::OperationPass<mlir::ModuleOp>> createExpandLayoutPass();
std::unique_ptr<mlir::Pass> createInlineLayoutPass();

// Generate the code for registering passes.
#define GEN_PASS_REGISTRATION
#include "zirgen/Dialect/ZStruct/Transforms/Passes.h.inc"

} // namespace zirgen::ZStruct
