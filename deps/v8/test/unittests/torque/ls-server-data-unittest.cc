// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/torque/server-data.h"
#include "src/torque/torque-compiler.h"
#include "test/unittests/test-utils.h"

namespace v8 {
namespace internal {
namespace torque {

namespace {

struct TestCompiler {
  SourceFileMap::Scope file_map_scope;
  LanguageServerData::Scope server_data_scope;

  void Compile(const std::string& source) {
    TorqueCompilerOptions options;
    options.abort_on_lint_errors = false;
    options.output_directory = "";
    options.verbose = false;
    options.collect_language_server_data = true;

    const TorqueCompilerResult result = CompileTorque(source, options);
    SourceFileMap::Get() = result.source_file_map;
    LanguageServerData::Get() = result.language_server_data;
  }
};

}  // namespace

TEST(LanguageServer, GotoTypeDefinition) {
  const std::string source =
      "type void;\n"
      "type never;\n"
      "type T1 generates 'TNode<Object>';\n"
      "type T2 generates 'TNode<Object>';\n"
      "macro SomeMacro(a: T1, b: T2): T1 { return a; }";

  TestCompiler compiler;
  compiler.Compile(source);

  // Find the definition for type 'T1' of argument 'a' on line 4.
  const SourceId id = SourceFileMap::GetSourceId("<torque>");
  auto maybe_position = LanguageServerData::FindDefinition(id, {4, 19});
  ASSERT_TRUE(maybe_position.has_value());
  EXPECT_EQ(*maybe_position, (SourcePosition{id, {2, 5}, {2, 7}}));

  // Find the defintion for type 'T2' of argument 'b' on line 4.
  maybe_position = LanguageServerData::FindDefinition(id, {4, 26});
  ASSERT_TRUE(maybe_position.has_value());
  EXPECT_EQ(*maybe_position, (SourcePosition{id, {3, 5}, {3, 7}}));
}

TEST(LanguageServer, GotoTypeDefinitionExtends) {
  const std::string source =
      "type void;\n"
      "type never;\n"
      "type T1 generates 'TNode<T1>';\n"
      "type T2 extends T1 generates 'TNode<T2>';";

  TestCompiler compiler;
  compiler.Compile(source);

  // Find the definition for 'T1' of the extends clause on line 3.
  const SourceId id = SourceFileMap::GetSourceId("<torque>");
  auto maybe_position = LanguageServerData::FindDefinition(id, {3, 16});
  ASSERT_TRUE(maybe_position.has_value());
  EXPECT_EQ(*maybe_position, (SourcePosition{id, {2, 5}, {2, 7}}));
}

}  // namespace torque
}  // namespace internal
}  // namespace v8
