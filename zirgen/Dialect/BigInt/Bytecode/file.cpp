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

#include "file.h"
#include <array>
#include <stdexcept>
#include <string>

namespace zirgen::BigInt::Bytecode {

/*
BIGINT BYTECODE FILE FORMAT
All values are little-endian.
Table elements are all 64-bit aligned and therefore so are sections.

HEADER: 24 bytes
4 | 62 69 62 63   Magic "bibc"
4 | 01 00 00 00   Version 1
4 | number of inputs
4 | number of types
4 | number of constants
4 | number of operations

INPUT TABLE: 16 bytes each
8 | label
4 | bitWidth
2 | minBits
2 | isPublic

TYPE TABLE: 32 bytes each
8 | coeffs
8 | maxPos
8 | maxNeg
8 | minBits

CONSTANT TABLE: 8 bytes each
8 | word

OPERATIONS: 8 bytes each
 4 bits: opcode
 12 bits: type T
 24 bits: operand A
 24 bits: operand B

OPCODES:
  0: Eqz -  A
  1: Def - TA
  2: Con - TAB
  8: Add - TAB
  9: Sub - TAB
  A: Mul - TAB
  B: Rem - TAB
  C: Quo - TAB
  D: Inv - TAB
*/

IOException::IOException(const char* file, const char* func, int line, const char* msg)
    : std::runtime_error::runtime_error(std::string(file) + ":" + std::to_string(line) +
                                        ", check failed in " + std::string(func) + ": " +
                                        std::string(msg)) {}

#define check(cond)                                                                                \
  if (cond)                                                                                        \
  throw IOException(__FILE__, __FUNCTION__, __LINE__, #cond)

#define MAGIC 0x63626962

namespace {

void writeU16(uint16_t value, FILE* stream) {
  std::array<uint8_t, 2> buf;
  buf[0] = (value >> 0x00) & 0xFFU;
  buf[1] = (value >> 0x08) & 0xFFU;
  size_t writ = fwrite(buf.data(), buf.size(), 1, stream);
  check(ferror(stream) || !writ);
}

void writeU32(uint32_t value, FILE* stream) {
  std::array<uint8_t, 4> buf;
  buf[0] = (value >> 0x00) & 0xFFU;
  buf[1] = (value >> 0x08) & 0xFFU;
  buf[2] = (value >> 0x10) & 0xFFU;
  buf[3] = (value >> 0x18) & 0xFFU;
  size_t writ = fwrite(buf.data(), buf.size(), 1, stream);
  check(ferror(stream) || !writ);
}

void writeU64(uint64_t value, FILE* stream) {
  std::array<uint8_t, 8> buf;
  buf[0] = (value >> 0x00) & 0xFFU;
  buf[1] = (value >> 0x08) & 0xFFU;
  buf[2] = (value >> 0x10) & 0xFFU;
  buf[3] = (value >> 0x18) & 0xFFU;
  buf[4] = (value >> 0x20) & 0xFFU;
  buf[5] = (value >> 0x28) & 0xFFU;
  buf[6] = (value >> 0x30) & 0xFFU;
  buf[7] = (value >> 0x38) & 0xFFU;
  size_t writ = fwrite(buf.data(), buf.size(), 1, stream);
  check(ferror(stream) || !writ);
}

void writeHeader(const Program& p, FILE* stream) {
  writeU32(MAGIC, stream);
  writeU32(1, stream);
  writeU32(p.inputs.size(), stream);
  writeU32(p.types.size(), stream);
  writeU32(p.constants.size(), stream);
  writeU32(p.ops.size(), stream);
}

void writeInput(const Input& i, FILE* stream) {
  writeU64(i.label, stream);
  writeU32(i.bitWidth, stream);
  writeU16(i.minBits, stream);
  writeU16(i.isPublic ? 1 : 0, stream);
}

void writeType(const Type& t, FILE* stream) {
  writeU64(t.coeffs, stream);
  writeU64(t.maxPos, stream);
  writeU64(t.maxNeg, stream);
  writeU64(t.minBits, stream);
}

void writeOp(const Op& o, FILE* stream) {
  // Pack operation struct fields into a single 64-bit word
  uint64_t w = 0;
  check(static_cast<uint8_t>(o.code) >= 0x10);
  w |= static_cast<uint64_t>(static_cast<uint8_t>(o.code) & 0x0F) << 0;
  check(o.type >= 0x1000);
  w |= static_cast<uint64_t>(o.type & 0x0FFF) << 4;
  check(o.operandA >= 0x01000000);
  w |= static_cast<uint64_t>(o.operandA & 0x00FFFFFF) << 16;
  check(o.operandB >= 0x01000000);
  w |= static_cast<uint64_t>(o.operandB & 0x00FFFFFF) << 40;
  writeU64(w, stream);
}

} // namespace

void write(const Program& p, FILE* stream) {
  writeHeader(p, stream);
  // inputs referenced through 24-bit operand
  check(p.inputs.size() > 0x00FFFFFF);
  for (auto& i : p.inputs) {
    writeInput(i, stream);
  }
  // types referenced through 12-bit operand
  check(p.types.size() > 0x00000FFF);
  for (auto& t : p.types) {
    writeType(t, stream);
  }
  // constants referenced through 24-bit operand
  check(p.constants.size() > 0x00FFFFFF);
  for (uint64_t c : p.constants) {
    writeU64(c, stream);
  }
  // op results referenced through 24-bit operand
  check(p.ops.size() > 0x00FFFFFF);
  for (auto& o : p.ops) {
    writeOp(o, stream);
  }
}

namespace {

uint32_t readU16(FILE* stream) {
  check(feof(stream));
  std::array<uint8_t, 2> buf;
  size_t got = fread(buf.data(), buf.size(), 1, stream);
  check(ferror(stream) || !got);
  return (static_cast<uint16_t>(buf[0]) << 0x00) | (static_cast<uint16_t>(buf[1]) << 0x08);
}

uint32_t readU32(FILE* stream) {
  check(feof(stream));
  std::array<uint8_t, 4> buf;
  size_t got = fread(buf.data(), buf.size(), 1, stream);
  check(ferror(stream) || !got);
  return (static_cast<uint32_t>(buf[0]) << 0x00) | (static_cast<uint32_t>(buf[1]) << 0x08) |
         (static_cast<uint32_t>(buf[2]) << 0x10) | (static_cast<uint32_t>(buf[3]) << 0x18);
}

uint64_t readU64(FILE* stream) {
  check(feof(stream));
  std::array<uint8_t, 8> buf;
  size_t got = fread(buf.data(), buf.size(), 1, stream);
  check(ferror(stream) || !got);
  return (static_cast<uint64_t>(buf[0]) << 0x00) | (static_cast<uint64_t>(buf[1]) << 0x08) |
         (static_cast<uint64_t>(buf[2]) << 0x10) | (static_cast<uint64_t>(buf[3]) << 0x18) |
         (static_cast<uint64_t>(buf[4]) << 0x20) | (static_cast<uint64_t>(buf[5]) << 0x28) |
         (static_cast<uint64_t>(buf[6]) << 0x30) | (static_cast<uint64_t>(buf[7]) << 0x38);
}

void readHeader(Program& p, FILE* stream) {
  check(MAGIC != readU32(stream));
  check(1 != readU32(stream));
  p.inputs.resize(readU32(stream));
  p.types.resize(readU32(stream));
  p.constants.resize(readU32(stream));
  p.ops.resize(readU32(stream));
}

void readInput(Input& wire, FILE* stream) {
  wire.label = readU64(stream);
  wire.bitWidth = readU32(stream);
  wire.minBits = readU16(stream);
  wire.isPublic = readU16(stream) != 0;
}

void readType(Type& t, FILE* stream) {
  t.coeffs = readU64(stream);
  t.maxPos = readU64(stream);
  t.maxNeg = readU64(stream);
  t.minBits = readU64(stream);
}

void readOp(Op& o, FILE* stream) {
  uint64_t bits = readU64(stream);
  o.code = (bits >> 0) & 0x0F;
  o.type = (bits >> 4) & 0x0FFF;
  o.operandA = (bits >> 16) & 0x00FFFFFF;
  o.operandB = (bits >> 40) & 0x00FFFFFF;
}

} // namespace

void read(Program& p, FILE* stream) {
  p.clear();
  readHeader(p, stream);
  for (size_t i = 0; i < p.inputs.size(); ++i) {
    readInput(p.inputs[i], stream);
  }
  for (size_t i = 0; i < p.types.size(); ++i) {
    readType(p.types[i], stream);
  }
  for (size_t i = 0; i < p.constants.size(); ++i) {
    p.constants[i] = readU64(stream);
  }
  for (size_t i = 0; i < p.ops.size(); ++i) {
    readOp(p.ops[i], stream);
  }
}

} // namespace zirgen::BigInt::Bytecode
