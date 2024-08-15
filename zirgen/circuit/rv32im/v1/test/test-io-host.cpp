// Copyright (c) 2024 RISC Zero, Inc.
//
// All rights reserved.

#include "risc0/core/elf.h"
#include "risc0/core/util.h"
#include "zirgen/circuit/rv32im/v1/test/runner.h"

#include <gtest/gtest.h>

namespace zirgen::rv32im_v1 {

TEST(IO, Basic) {
  std::map<uint32_t, uint32_t> image;
  auto file = risc0::loadFile("zirgen/circuit/rv32im/v1/test/test-io-guest");
  uint32_t entryPoint = risc0::loadElf(file, image);
  Runner runner(32 * 1024, image, entryPoint);
  runner.setInput({0x01020304, 0x05060708});
  runner.run();
  runner.done();
}

} // namespace zirgen::rv32im_v1
