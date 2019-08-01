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
    options.output_directory = "";
    options.collect_language_server_data = true;
    options.force_assert_statements = true;

    TorqueCompilerResult result = CompileTorque(source, options);
    SourceFileMap::Get() = result.source_file_map;
    LanguageServerData::Get() = std::move(result.language_server_data);
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

TEST(LanguageServer, GotoTypeDefinitionNoDataForFile) {
  LanguageServerData::Scope server_data_scope;
  SourceFileMap::Scope file_scope;
  SourceId test_id = SourceFileMap::AddSource("test.tq");

  // Regression test, this step should not crash.
  EXPECT_FALSE(LanguageServerData::FindDefinition(test_id, {0, 0}));
}

TEST(LanguageServer, GotoLabelDefinitionInSignature) {
  const std::string source =
      "type void;\n"
      "type never;\n"
      "macro Foo(): never labels Fail {\n"
      "  goto Fail;\n"
      "}\n"
      "macro Bar() labels Bailout {\n"
      "  Foo() otherwise Bailout;\n"
      "}\n";

  TestCompiler compiler;
  compiler.Compile(source);

  // Find the definition for 'Bailout' of the otherwise clause on line 6.
  const SourceId id = SourceFileMap::GetSourceId("<torque>");
  auto maybe_position = LanguageServerData::FindDefinition(id, {6, 18});
  ASSERT_TRUE(maybe_position.has_value());
  EXPECT_EQ(*maybe_position, (SourcePosition{id, {5, 19}, {5, 26}}));
}

TEST(LanguageServer, GotoLabelDefinitionInTryBlock) {
  const std::string source =
      "type void;\n"
      "type never;\n"
      "macro Foo(): never labels Fail {\n"
      "  goto Fail;\n"
      "}\n"
      "macro Bar() {\n"
      "  try { Foo() otherwise Bailout; }\n"
      "  label Bailout {}\n"
      "}\n";

  TestCompiler compiler;
  compiler.Compile(source);

  // Find the definition for 'Bailout' of the otherwise clause on line 6.
  const SourceId id = SourceFileMap::GetSourceId("<torque>");
  auto maybe_position = LanguageServerData::FindDefinition(id, {6, 25});
  ASSERT_TRUE(maybe_position.has_value());
  EXPECT_EQ(*maybe_position, (SourcePosition{id, {7, 8}, {7, 15}}));
}

TEST(LanguageServer, GotoDefinitionClassSuperType) {
  const std::string source =
      "type void;\n"
      "type never;\n"
      "type Tagged generates 'TNode<Object>' constexpr 'ObjectPtr';\n"
      "extern class HeapObject extends Tagged {}";

  TestCompiler compiler;
  compiler.Compile(source);

  // Find the definition for 'Tagged' of the 'extends' on line 3.
  const SourceId id = SourceFileMap::GetSourceId("<torque>");
  auto maybe_position = LanguageServerData::FindDefinition(id, {3, 33});
  ASSERT_TRUE(maybe_position.has_value());
  EXPECT_EQ(*maybe_position, (SourcePosition{id, {2, 5}, {2, 11}}));
}

TEST(LanguageServer, GotoLabelDefinitionInSignatureGotoStmt) {
  const std::string source =
      "type void;\n"
      "type never;\n"
      "macro Foo(): never labels Fail {\n"
      "  goto Fail;\n"
      "}\n";

  TestCompiler compiler;
  compiler.Compile(source);

  // Find the definition for 'Fail' of the goto statement on line 3.
  const SourceId id = SourceFileMap::GetSourceId("<torque>");
  auto maybe_position = LanguageServerData::FindDefinition(id, {3, 7});
  ASSERT_TRUE(maybe_position.has_value());
  EXPECT_EQ(*maybe_position, (SourcePosition{id, {2, 26}, {2, 30}}));
}

TEST(LanguageServer, GotoLabelDefinitionInTryBlockGoto) {
  const std::string source =
      "type void;\n"
      "type never;\n"
      "macro Bar() {\n"
      "  try { goto Bailout; }\n"
      "  label Bailout {}\n"
      "}\n";

  TestCompiler compiler;
  compiler.Compile(source);

  // Find the definition for 'Bailout' of the goto statement on line 3.
  const SourceId id = SourceFileMap::GetSourceId("<torque>");
  auto maybe_position = LanguageServerData::FindDefinition(id, {3, 13});
  ASSERT_TRUE(maybe_position.has_value());
  EXPECT_EQ(*maybe_position, (SourcePosition{id, {4, 8}, {4, 15}}));
}

TEST(LanguageServer, GotoLabelDefinitionGotoInOtherwise) {
  const std::string source =
      "type void;\n"
      "type never;\n"
      "macro Foo(): never labels Fail {\n"
      "  goto Fail;\n"
      "}\n"
      "macro Bar() {\n"
      "  try { Foo() otherwise goto Bailout; }\n"
      "  label Bailout {}\n"
      "}\n";

  TestCompiler compiler;
  compiler.Compile(source);

  // Find the definition for 'Bailout' of the otherwise clause on line 6.
  const SourceId id = SourceFileMap::GetSourceId("<torque>");
  auto maybe_position = LanguageServerData::FindDefinition(id, {6, 30});
  ASSERT_TRUE(maybe_position.has_value());
  EXPECT_EQ(*maybe_position, (SourcePosition{id, {7, 8}, {7, 15}}));
}

TEST(LanguageServer, SymbolsArePopulated) {
  // Small test to ensure that the GlobalContext is correctly set in
  // the LanguageServerData class and declarables are sorted into the
  // SymbolsMap.
  const std::string source = R"(
      type void;
      type never;

      macro Foo(): never labels Fail {
        goto Fail;
      }
  )";

  TestCompiler compiler;
  compiler.Compile(source);

  const SourceId id = SourceFileMap::GetSourceId("<torque>");
  const auto& symbols = LanguageServerData::SymbolsForSourceId(id);
  ASSERT_FALSE(symbols.empty());
}

}  // namespace torque
}  // namespace internal
}  // namespace v8
