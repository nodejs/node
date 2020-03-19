// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cmath>
#include <iostream>
#include <limits>
#include <memory>

#include "src/ast/ast-value-factory.h"
#include "src/ast/ast.h"
#include "src/ast/scopes.h"
#include "src/common/assert-scope.h"
#include "src/common/globals.h"
#include "src/execution/off-thread-isolate.h"
#include "src/handles/handles-inl.h"
#include "src/handles/handles.h"
#include "src/heap/off-thread-factory-inl.h"
#include "src/objects/fixed-array.h"
#include "src/objects/script.h"
#include "src/objects/shared-function-info.h"
#include "src/objects/string.h"
#include "src/parsing/parse-info.h"
#include "src/parsing/parser.h"
#include "src/parsing/rewriter.h"
#include "src/parsing/scanner-character-streams.h"
#include "src/parsing/scanner.h"
#include "src/strings/unicode-inl.h"
#include "src/utils/utils.h"
#include "test/unittests/test-utils.h"

namespace v8 {
namespace internal {

class OffThreadIsolate;

namespace {

std::vector<uint16_t> DecodeUtf8(const std::string& string) {
  if (string.empty()) return {};

  auto utf8_data =
      Vector<const uint8_t>::cast(VectorOf(string.data(), string.length()));
  Utf8Decoder decoder(utf8_data);

  std::vector<uint16_t> utf16(decoder.utf16_length());
  decoder.Decode(&utf16[0], utf8_data);

  return utf16;
}

}  // namespace

class OffThreadFactoryTest : public TestWithIsolateAndZone {
 public:
  OffThreadFactoryTest()
      : TestWithIsolateAndZone(),
        parse_info_(isolate()),
        off_thread_isolate_(isolate(), parse_info_.zone()) {}

  FunctionLiteral* ParseProgram(const char* source) {
    auto utf16_source = DecodeUtf8(source);

    // Normally this would be an external string or whatever, we don't have to
    // worry about it for now.
    source_string_ =
        factory()->NewStringFromUtf8(CStrVector(source)).ToHandleChecked();

    parse_info_.set_character_stream(
        ScannerStream::ForTesting(utf16_source.data(), utf16_source.size()));
    parse_info_.set_toplevel();
    parse_info_.set_allow_lazy_parsing();

    {
      DisallowHeapAllocation no_allocation;
      DisallowHandleAllocation no_handles;
      DisallowHeapAccess no_heap_access;

      Parser parser(parse_info());
      parser.InitializeEmptyScopeChain(parse_info());
      parser.ParseOnBackground(parse_info());

      CHECK(DeclarationScope::Analyze(parse_info()));
    }

    parse_info()->ast_value_factory()->Internalize(off_thread_isolate());
    DeclarationScope::AllocateScopeInfos(parse_info(), off_thread_isolate());

    script_ = parse_info_.CreateScript(off_thread_isolate(),
                                       off_thread_factory()->empty_string(),
                                       ScriptOriginOptions());

    // Create the SFI list on the script so that SFI SetScript works.
    Handle<WeakFixedArray> infos = off_thread_factory()->NewWeakFixedArray(
        parse_info()->max_function_literal_id() + 1, AllocationType::kOld);
    script_->set_shared_function_infos(*infos);

    return parse_info()->literal();
  }

  ParseInfo* parse_info() { return &parse_info_; }

  Handle<Script> script() { return script_; }

  OffThreadIsolate* off_thread_isolate() { return &off_thread_isolate_; }
  OffThreadFactory* off_thread_factory() {
    return off_thread_isolate()->factory();
  }

  // We only internalize strings which are referred to in other slots, so create
  // a wrapper pointing at the off_thread_string.
  Handle<FixedArray> WrapString(Handle<String> string) {
    // TODO(leszeks): Replace with a different factory method (e.g. FixedArray)
    // once OffThreadFactory supports it.
    return off_thread_factory()->StringWrapperForTest(string);
  }

 private:
  ParseInfo parse_info_;
  OffThreadIsolate off_thread_isolate_;
  Handle<String> source_string_;
  Handle<Script> script_;
};

TEST_F(OffThreadFactoryTest, OneByteInternalizedString_IsAddedToStringTable) {
  Vector<const uint8_t> string_vector = StaticCharVector("foo");
  uint32_t hash_field = StringHasher::HashSequentialString<uint8_t>(
      string_vector.begin(), string_vector.length(), HashSeed(isolate()));

  FixedArray off_thread_wrapper;
  {
    OffThreadHandleScope handle_scope(off_thread_isolate());

    Handle<String> off_thread_string =
        off_thread_factory()->NewOneByteInternalizedString(string_vector,
                                                           hash_field);

    off_thread_wrapper =
        *off_thread_factory()->StringWrapperForTest(off_thread_string);
    off_thread_factory()->FinishOffThread();
  }

  Handle<FixedArray> wrapper = handle(off_thread_wrapper, isolate());
  off_thread_factory()->Publish(isolate());

  Handle<String> string = handle(String::cast(wrapper->get(0)), isolate());

  EXPECT_TRUE(string->IsOneByteEqualTo(CStrVector("foo")));
  EXPECT_TRUE(string->IsInternalizedString());

  Handle<String> same_string = isolate()
                                   ->factory()
                                   ->NewStringFromOneByte(string_vector)
                                   .ToHandleChecked();
  EXPECT_NE(*string, *same_string);
  EXPECT_FALSE(same_string->IsInternalizedString());

  Handle<String> internalized_string =
      isolate()->factory()->InternalizeString(same_string);
  EXPECT_EQ(*string, *internalized_string);
}

TEST_F(OffThreadFactoryTest,
       OneByteInternalizedString_DuplicateIsDeduplicated) {
  Vector<const uint8_t> string_vector = StaticCharVector("foo");
  uint32_t hash_field = StringHasher::HashSequentialString<uint8_t>(
      string_vector.begin(), string_vector.length(), HashSeed(isolate()));

  FixedArray off_thread_wrapper_1;
  FixedArray off_thread_wrapper_2;
  {
    OffThreadHandleScope handle_scope(off_thread_isolate());

    Handle<String> off_thread_string_1 =
        off_thread_factory()->NewOneByteInternalizedString(string_vector,
                                                           hash_field);
    Handle<String> off_thread_string_2 =
        off_thread_factory()->NewOneByteInternalizedString(string_vector,
                                                           hash_field);

    off_thread_wrapper_1 = *WrapString(off_thread_string_1);
    off_thread_wrapper_2 = *WrapString(off_thread_string_2);
    off_thread_factory()->FinishOffThread();
  }

  Handle<FixedArray> wrapper_1 = handle(off_thread_wrapper_1, isolate());
  Handle<FixedArray> wrapper_2 = handle(off_thread_wrapper_2, isolate());
  off_thread_factory()->Publish(isolate());

  Handle<String> string_1 = handle(String::cast(wrapper_1->get(0)), isolate());
  Handle<String> string_2 = handle(String::cast(wrapper_2->get(0)), isolate());

  EXPECT_TRUE(string_1->IsOneByteEqualTo(CStrVector("foo")));
  EXPECT_TRUE(string_1->IsInternalizedString());
  EXPECT_EQ(*string_1, *string_2);
}

TEST_F(OffThreadFactoryTest, AstRawString_IsInternalized) {
  AstValueFactory ast_value_factory(zone(), isolate()->ast_string_constants(),
                                    HashSeed(isolate()));

  const AstRawString* raw_string = ast_value_factory.GetOneByteString("foo");

  FixedArray off_thread_wrapper;
  {
    OffThreadHandleScope handle_scope(off_thread_isolate());

    ast_value_factory.Internalize(off_thread_isolate());

    off_thread_wrapper = *WrapString(raw_string->string());
    off_thread_factory()->FinishOffThread();
  }

  Handle<FixedArray> wrapper = handle(off_thread_wrapper, isolate());
  off_thread_factory()->Publish(isolate());

  Handle<String> string = handle(String::cast(wrapper->get(0)), isolate());

  EXPECT_TRUE(string->IsOneByteEqualTo(CStrVector("foo")));
  EXPECT_TRUE(string->IsInternalizedString());
}

TEST_F(OffThreadFactoryTest, AstConsString_CreatesConsString) {
  AstValueFactory ast_value_factory(zone(), isolate()->ast_string_constants(),
                                    HashSeed(isolate()));

  FixedArray off_thread_wrapper;
  {
    OffThreadHandleScope handle_scope(off_thread_isolate());

    const AstRawString* foo_string = ast_value_factory.GetOneByteString("foo");
    const AstRawString* bar_string =
        ast_value_factory.GetOneByteString("bar-plus-padding-for-length");
    AstConsString* foobar_string =
        ast_value_factory.NewConsString(foo_string, bar_string);

    ast_value_factory.Internalize(off_thread_isolate());

    off_thread_wrapper =
        *WrapString(foobar_string->GetString(off_thread_isolate()));
    off_thread_factory()->FinishOffThread();
  }

  Handle<FixedArray> wrapper = handle(off_thread_wrapper, isolate());
  off_thread_factory()->Publish(isolate());

  Handle<String> string = handle(String::cast(wrapper->get(0)), isolate());

  EXPECT_TRUE(string->IsConsString());
  EXPECT_TRUE(string->Equals(*isolate()->factory()->NewStringFromStaticChars(
      "foobar-plus-padding-for-length")));
}

TEST_F(OffThreadFactoryTest, EmptyScript) {
  FunctionLiteral* program = ParseProgram("");

  SharedFunctionInfo shared;
  {
    OffThreadHandleScope handle_scope(off_thread_isolate());

    shared = *off_thread_factory()->NewSharedFunctionInfoForLiteral(
        program, script(), true);

    off_thread_factory()->FinishOffThread();
  }

  Handle<SharedFunctionInfo> root_sfi = handle(shared, isolate());
  off_thread_factory()->Publish(isolate());

  EXPECT_EQ(root_sfi->function_literal_id(), 0);
}

TEST_F(OffThreadFactoryTest, LazyFunction) {
  FunctionLiteral* program = ParseProgram("function lazy() {}");
  FunctionLiteral* lazy = program->scope()
                              ->declarations()
                              ->AtForTest(0)
                              ->AsFunctionDeclaration()
                              ->fun();

  SharedFunctionInfo shared;
  {
    OffThreadHandleScope handle_scope(off_thread_isolate());

    shared = *off_thread_factory()->NewSharedFunctionInfoForLiteral(
        lazy, script(), true);

    off_thread_factory()->FinishOffThread();
  }

  Handle<SharedFunctionInfo> lazy_sfi = handle(shared, isolate());
  off_thread_factory()->Publish(isolate());

  EXPECT_EQ(lazy_sfi->function_literal_id(), 1);
  EXPECT_TRUE(lazy_sfi->Name().IsOneByteEqualTo(CStrVector("lazy")));
  EXPECT_FALSE(lazy_sfi->is_compiled());
  EXPECT_TRUE(lazy_sfi->HasUncompiledDataWithoutPreparseData());
}

TEST_F(OffThreadFactoryTest, EagerFunction) {
  FunctionLiteral* program = ParseProgram("(function eager() {})");
  FunctionLiteral* eager = program->body()
                               ->at(0)
                               ->AsExpressionStatement()
                               ->expression()
                               ->AsFunctionLiteral();

  SharedFunctionInfo shared;
  {
    OffThreadHandleScope handle_scope(off_thread_isolate());

    shared = *off_thread_factory()->NewSharedFunctionInfoForLiteral(
        eager, script(), true);

    off_thread_factory()->FinishOffThread();
  }

  Handle<SharedFunctionInfo> eager_sfi = handle(shared, isolate());
  off_thread_factory()->Publish(isolate());

  EXPECT_EQ(eager_sfi->function_literal_id(), 1);
  EXPECT_TRUE(eager_sfi->Name().IsOneByteEqualTo(CStrVector("eager")));
  EXPECT_FALSE(eager_sfi->HasUncompiledData());
  // TODO(leszeks): Allocate bytecode and enable these checks.
  // EXPECT_TRUE(eager_sfi->is_compiled());
  // EXPECT_TRUE(eager_sfi->HasBytecodeArray());
}

TEST_F(OffThreadFactoryTest, ImplicitNameFunction) {
  FunctionLiteral* program = ParseProgram("let implicit_name = function() {}");
  FunctionLiteral* implicit_name = program->body()
                                       ->at(0)
                                       ->AsBlock()
                                       ->statements()
                                       ->at(0)
                                       ->AsExpressionStatement()
                                       ->expression()
                                       ->AsAssignment()
                                       ->value()
                                       ->AsFunctionLiteral();

  SharedFunctionInfo shared;
  {
    OffThreadHandleScope handle_scope(off_thread_isolate());

    shared = *off_thread_factory()->NewSharedFunctionInfoForLiteral(
        implicit_name, script(), true);

    off_thread_factory()->FinishOffThread();
  }

  Handle<SharedFunctionInfo> implicit_name_sfi = handle(shared, isolate());
  off_thread_factory()->Publish(isolate());

  EXPECT_EQ(implicit_name_sfi->function_literal_id(), 1);
  EXPECT_TRUE(
      implicit_name_sfi->Name().IsOneByteEqualTo(CStrVector("implicit_name")));
}

}  // namespace internal
}  // namespace v8
