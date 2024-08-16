// Copyright (c) 2024 RISC Zero, Inc.
//
// All rights reserved.

#pragma once

#include "zirgen/Dialect/Zll/IR/Interpreter.h"
#include "zirgen/compiler/edsl/component.h"

namespace zirgen {

struct TestRunner {
  using Polynomial = llvm::SmallVector<uint64_t, 4>;

  template <typename Func>
  TestRunner(size_t stages,
             size_t codeSize,
             size_t outSize,
             size_t dataSize,
             size_t mixSize,
             size_t accumSize,
             Func func)
      : steps(0)
      , codeSize(codeSize)
      , outSize(outSize)
      , dataSize(dataSize)
      , mixSize(mixSize)
      , accumSize(accumSize) {
    module.addFunc<5>(
        "test_func",
        {cbuf(codeSize), gbuf(outSize), mbuf(dataSize), gbuf(mixSize), mbuf(accumSize)},
        func);
    module.optimize();
    // module.dump();
    module.optimize(stages);
  }
  void setup(Zll::ExternHandler* handler, std::vector<uint64_t> code);
  void runStage(size_t stage);
  void setMix();
  void dump();
  void done();

  Module module;
  size_t steps;
  size_t codeSize;
  size_t outSize;
  size_t dataSize;
  size_t mixSize;
  size_t accumSize;
  std::vector<Polynomial> code;
  std::vector<Polynomial> out;
  std::vector<Polynomial> data;
  std::vector<Polynomial> mix;
  std::vector<Polynomial> accum;
  std::vector<llvm::MutableArrayRef<Polynomial>> args;
};

} // namespace zirgen
