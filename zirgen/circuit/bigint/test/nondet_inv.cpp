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

#include "zirgen/circuit/bigint/op_tests.h"
#include "zirgen/circuit/bigint/rsa.h"
#include "zirgen/circuit/bigint/test/bibc.h"

#include <gtest/gtest.h>

using namespace zirgen;
using namespace zirgen::BigInt::test;

TEST_F(BibcTest, NondetInv8) {
  mlir::OpBuilder builder(ctx);
  auto func = makeFunc("nondet_inv_8", builder);
  BigInt::makeNondetInvTest(builder, func.getLoc(), 8);
  lower();

  auto inputs = apints({"4", "3", "1"});
  ZType a, b;
  AB(func, inputs, a, b);
  EXPECT_EQ(a, b);
}

TEST_F(BibcTest, NondetInv128) {
  mlir::OpBuilder builder(ctx);
  auto func = makeFunc("nondet_inv_128", builder);
  BigInt::makeNondetInvTest(builder, func.getLoc(), 128);
  lower();

  auto inputs = apints({"100E", "0BB9", "03D9"});
  ZType a, b;
  AB(func, inputs, a, b);
  EXPECT_EQ(a, b);
}
