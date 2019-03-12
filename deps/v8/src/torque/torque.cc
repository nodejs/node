// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fstream>
#include <iostream>

#include "src/torque/declarable.h"
#include "src/torque/declaration-visitor.h"
#include "src/torque/global-context.h"
#include "src/torque/implementation-visitor.h"
#include "src/torque/torque-parser.h"
#include "src/torque/type-oracle.h"
#include "src/torque/types.h"
#include "src/torque/utils.h"

namespace v8 {
namespace internal {
namespace torque {

int WrappedMain(int argc, const char** argv) {
  std::string output_directory;
  bool verbose = false;
  SourceFileMap::Scope source_file_map_scope;
  CurrentSourceFile::Scope unknown_sourcefile_scope(
      SourceFileMap::AddSource("<unknown>"));
  CurrentAst::Scope ast_scope;
  LintErrorStatus::Scope lint_error_status_scope;

  for (int i = 1; i < argc; ++i) {
    // Check for options
    if (!strcmp("-o", argv[i])) {
      output_directory = argv[++i];
      continue;
    }
    if (!strcmp("-v", argv[i])) {
      verbose = true;
      continue;
    }

    // Otherwise it's a .tq
    // file, parse it and
    // remember the syntax tree
    std::string path = argv[i];
    SourceId source_id = SourceFileMap::AddSource(path);
    CurrentSourceFile::Scope source_id_scope(source_id);
    std::ifstream file_stream(path);
    std::string file_content = {std::istreambuf_iterator<char>(file_stream),
                                std::istreambuf_iterator<char>()};
    ParseTorque(file_content);
  }

  GlobalContext::Scope global_context(std::move(CurrentAst::Get()));
  if (verbose) GlobalContext::SetVerbose();
  TypeOracle::Scope type_oracle;

  if (output_directory.length() != 0) {
    DeclarationVisitor().Visit(GlobalContext::Get().ast());

    ImplementationVisitor visitor;
    for (Namespace* n : GlobalContext::Get().GetNamespaces()) {
      visitor.BeginNamespaceFile(n);
    }

    visitor.VisitAllDeclarables();

    std::string output_header_path = output_directory;
    output_header_path += "/builtin-definitions-from-dsl.h";
    visitor.GenerateBuiltinDefinitions(output_header_path);

    output_header_path = output_directory + "/class-definitions-from-dsl.h";
    visitor.GenerateClassDefinitions(output_header_path);

    for (Namespace* n : GlobalContext::Get().GetNamespaces()) {
      visitor.EndNamespaceFile(n);
      visitor.GenerateImplementation(output_directory, n);
    }
  }

  if (LintErrorStatus::HasLintErrors()) std::abort();

  return 0;
}

}  // namespace torque
}  // namespace internal
}  // namespace v8

int main(int argc, const char** argv) {
  return v8::internal::torque::WrappedMain(argc, argv);
}
