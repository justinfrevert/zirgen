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

#include <fstream>
#include <iostream>

#include "mlir/Pass/PassManager.h"
#include "zirgen/Dialect/Zll/Transforms/Passes.h"
#include "zirgen/circuit/recursion/code.h"
#include "zirgen/circuit/verify/circom/circom.h"
#include "zirgen/circuit/verify/verify.h"
#include "zirgen/circuit/verify/wrap_recursion.h"

using namespace zirgen;
using namespace zirgen::snark;
using namespace zirgen::Zll;
namespace cl = llvm::cl;

static cl::opt<std::string>
    outputDir("output-dir", cl::desc("Output directory"), cl::value_desc("dir"), cl::Required);

int main(int argc, char* argv[]) {
  using zirgen::recursion::kOutSize;

  llvm::InitLLVM y(argc, argv);
  mlir::registerAsmPrinterCLOptions();
  mlir::registerMLIRContextCLOptions();
  mlir::registerPassManagerCLOptions();
  mlir::registerDefaultTimingManagerCLOptions();
  cl::ParseCommandLineOptions(argc, argv, "zirgen compiler\n");

  // Make MLIR for the function
  Module module;
  module.addFunc<3>("circom_verify",
                    {gbuf(kOutSize), gbuf(1), ioparg()},
                    [&](Buffer out, Buffer codeRoot, ReadIopVal iop) {
                      auto circuit = verify::getInterfaceRecursion();
                      auto info = zirgen::verify::verify(iop, 18, *circuit);
                      for (size_t i = 0; i < kOutSize; i++) {
                        out[i] = info.out[i];
                      }
                      codeRoot.setDigest(0, info.codeRoot, "codeRoot");
                    });
  module.optimize();

  // Convert it to the form required for circom
  mlir::PassManager pm(module.getCtx());
  mlir::OpPassManager& opm = pm.nest<mlir::func::FuncOp>();
  opm.addPass(Zll::createInlineFpExtPass());
  opm.addPass(Zll::createAddReductionsPass());
  if (failed(pm.run(module.getModule()))) {
    throw std::runtime_error("Failed to apply basic optimization passes");
  }

  // Get the function
  auto func = module.getModule().lookupSymbol<mlir::func::FuncOp>("circom_verify");

  // Generate `witgen.circom`
  std::ofstream circom_writer;
  circom_writer.exceptions(std::ofstream::failbit | std::ofstream::badbit);
  circom_writer.open(outputDir + "/stark_verify.circom");
  CircomGenerator gen(circom_writer);
  gen.emit(func, true);
  circom_writer.close();

  size_t words = 0;
  size_t elems = 0;
  std::vector<std::string> body;
  func.front().walk([&](Iop::ReadOp op) {
    if (auto type = op.getOuts()[0].getType().dyn_cast<ValType>()) {
      for (size_t i = 0; i < op.getOuts().size(); i++) {
        body.push_back("    IopType::Fp,\n");
        words++;
        elems++;
      }
    } else {
      for (size_t i = 0; i < op.getOuts().size(); i++) {
        body.push_back("    IopType::Digest,\n");
        words += 8;
        elems++;
      }
    }
  });

  // Generate `seal_format.rs`
  std::ofstream header_writer;
  header_writer.exceptions(std::ofstream::failbit | std::ofstream::badbit);
  header_writer.open(outputDir + "/seal_format.rs");

  header_writer << "// Copyright 2024 RISC Zero, Inc.\n";
  header_writer << "//\n";
  header_writer << "// Licensed under the Apache License, Version 2.0 (the \"License\");\n";
  header_writer << "// you may not use this file except in compliance with the License.\n";
  header_writer << "// You may obtain a copy of the License at\n";
  header_writer << "//\n";
  header_writer << "//     http://www.apache.org/licenses/LICENSE-2.0\n";
  header_writer << "//\n";
  header_writer << "// Unless required by applicable law or agreed to in writing, software\n";
  header_writer << "// distributed under the License is distributed on an \"AS IS\" BASIS,\n";
  header_writer << "// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.\n";
  header_writer << "// See the License for the specific language governing permissions and\n";
  header_writer << "// limitations under the License.\n";
  header_writer << "//\n";
  header_writer << "// This file is autogenerated\n";
  header_writer << "\n";
  header_writer << "pub(crate) const K_SEAL_WORDS: usize = " << words << ";\n";
  header_writer << "pub(crate) const K_SEAL_ELEMS: usize = " << elems << ";\n";
  header_writer << "\n";
  header_writer << "pub(crate) enum IopType {\n";
  header_writer << "    Fp,\n";
  header_writer << "    Digest,\n";
  header_writer << "}\n";
  header_writer << "\n";
  header_writer << "pub(crate) const K_SEAL_TYPES: [IopType; K_SEAL_ELEMS] = [\n";
  for (std::string line : body) {
    header_writer << line;
  }
  header_writer << "];\n";
  header_writer.close();
}
