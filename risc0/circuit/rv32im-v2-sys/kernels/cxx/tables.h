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

#include "fp.h"

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <stdexcept>
#include <vector>

namespace risc0::circuit::rv32im_v2::cpu {

struct LookupTables {
  std::vector<std::atomic<Fp>> tableU8{1 << 8};
  std::vector<std::atomic<Fp>> tableU16{1 << 16};

  void atomic_rmw(std::atomic<Fp>& atom, Fp count) {
    Fp old = atom.load(), next;
    do {
      next = old + count;
    } while (!atom.compare_exchange_weak(old, next));
  }

  void lookupDelta(Fp table, Fp index, Fp count) {
    uint32_t tableU32 = table.asUInt32();
    uint32_t indexU32 = index.asUInt32();
    if (tableU32 == 0) {
      // tableCycle[index] += count;
      return;
    }
    if (tableU32 != 8 && tableU32 != 16) {
      throw std::runtime_error("Invalid lookup table");
    }
    if (indexU32 >= (1u << tableU32)) {
      printf("LOOKUP ERROR: table = %u, index = %u\n", tableU32, indexU32);
      throw std::runtime_error("u8/16 table error");
    }
    // printf("table = %u, index = %u\n", tableU32, indexU32);
    if (tableU32 == 8) {
      atomic_rmw(tableU8[indexU32], count);
    } else {
      atomic_rmw(tableU16[indexU32], count);
    }
  }

  Fp lookupCurrent(Fp table, Fp index) {
    uint32_t tableU32 = table.asUInt32();
    if (tableU32 != 8 && tableU32 != 16) {
      throw std::runtime_error("Invalid lookup table");
    }
    uint32_t indexU32 = index.asUInt32();
    if (tableU32 == 8) {
      return tableU8[indexU32];
    } else {
      return tableU16[indexU32];
    }
  }
};

} // namespace risc0::circuit::rv32im_v2::cpu
