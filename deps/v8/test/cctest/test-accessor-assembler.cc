// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test/cctest/cctest.h"

#include "src/base/utils/random-number-generator.h"
#include "src/ic/accessor-assembler-impl.h"
#include "src/ic/stub-cache.h"
#include "test/cctest/compiler/code-assembler-tester.h"
#include "test/cctest/compiler/function-tester.h"

namespace v8 {
namespace internal {

using compiler::CodeAssemblerTester;
using compiler::FunctionTester;
using compiler::Node;

namespace {

void TestStubCacheOffsetCalculation(StubCache::Table table) {
  Isolate* isolate(CcTest::InitIsolateOnce());
  const int kNumParams = 2;
  CodeAssemblerTester data(isolate, kNumParams);
  AccessorAssemblerImpl m(data.state());

  {
    Node* name = m.Parameter(0);
    Node* map = m.Parameter(1);
    Node* primary_offset = m.StubCachePrimaryOffsetForTesting(name, map);
    Node* result;
    if (table == StubCache::kPrimary) {
      result = primary_offset;
    } else {
      CHECK_EQ(StubCache::kSecondary, table);
      result = m.StubCacheSecondaryOffsetForTesting(name, primary_offset);
    }
    m.Return(m.SmiTag(result));
  }

  Handle<Code> code = data.GenerateCode();
  FunctionTester ft(code, kNumParams);

  Factory* factory = isolate->factory();
  Handle<Name> names[] = {
      factory->NewSymbol(),
      factory->InternalizeUtf8String("a"),
      factory->InternalizeUtf8String("bb"),
      factory->InternalizeUtf8String("ccc"),
      factory->NewPrivateSymbol(),
      factory->InternalizeUtf8String("dddd"),
      factory->InternalizeUtf8String("eeeee"),
      factory->InternalizeUtf8String("name"),
      factory->NewSymbol(),
      factory->NewPrivateSymbol(),
  };

  Handle<Map> maps[] = {
      Handle<Map>(nullptr, isolate),
      factory->cell_map(),
      Map::Create(isolate, 0),
      factory->meta_map(),
      factory->code_map(),
      Map::Create(isolate, 0),
      factory->hash_table_map(),
      factory->symbol_map(),
      factory->string_map(),
      Map::Create(isolate, 0),
      factory->sloppy_arguments_elements_map(),
  };

  for (size_t name_index = 0; name_index < arraysize(names); name_index++) {
    Handle<Name> name = names[name_index];
    for (size_t map_index = 0; map_index < arraysize(maps); map_index++) {
      Handle<Map> map = maps[map_index];

      int expected_result;
      {
        int primary_offset = StubCache::PrimaryOffsetForTesting(*name, *map);
        if (table == StubCache::kPrimary) {
          expected_result = primary_offset;
        } else {
          expected_result =
              StubCache::SecondaryOffsetForTesting(*name, primary_offset);
        }
      }
      Handle<Object> result = ft.Call(name, map).ToHandleChecked();

      Smi* expected = Smi::FromInt(expected_result & Smi::kMaxValue);
      CHECK_EQ(expected, Smi::cast(*result));
    }
  }
}

}  // namespace

TEST(StubCachePrimaryOffset) {
  TestStubCacheOffsetCalculation(StubCache::kPrimary);
}

TEST(StubCacheSecondaryOffset) {
  TestStubCacheOffsetCalculation(StubCache::kSecondary);
}

namespace {

Handle<Code> CreateCodeWithFlags(Code::Flags flags) {
  Isolate* isolate(CcTest::InitIsolateOnce());
  CodeAssemblerTester data(isolate, flags);
  CodeStubAssembler m(data.state());
  m.Return(m.UndefinedConstant());
  return data.GenerateCodeCloseAndEscape();
}

}  // namespace

TEST(TryProbeStubCache) {
  typedef CodeStubAssembler::Label Label;
  typedef CodeStubAssembler::Variable Variable;
  Isolate* isolate(CcTest::InitIsolateOnce());
  const int kNumParams = 3;
  CodeAssemblerTester data(isolate, kNumParams);
  AccessorAssemblerImpl m(data.state());

  Code::Kind ic_kind = Code::LOAD_IC;
  StubCache stub_cache(isolate, ic_kind);
  stub_cache.Clear();

  {
    Node* receiver = m.Parameter(0);
    Node* name = m.Parameter(1);
    Node* expected_handler = m.Parameter(2);

    Label passed(&m), failed(&m);

    Variable var_handler(&m, MachineRepresentation::kTagged);
    Label if_handler(&m), if_miss(&m);

    m.TryProbeStubCache(&stub_cache, receiver, name, &if_handler, &var_handler,
                        &if_miss);
    m.Bind(&if_handler);
    m.Branch(m.WordEqual(expected_handler, var_handler.value()), &passed,
             &failed);

    m.Bind(&if_miss);
    m.Branch(m.WordEqual(expected_handler, m.IntPtrConstant(0)), &passed,
             &failed);

    m.Bind(&passed);
    m.Return(m.BooleanConstant(true));

    m.Bind(&failed);
    m.Return(m.BooleanConstant(false));
  }

  Handle<Code> code = data.GenerateCode();
  FunctionTester ft(code, kNumParams);

  std::vector<Handle<Name>> names;
  std::vector<Handle<JSObject>> receivers;
  std::vector<Handle<Code>> handlers;

  base::RandomNumberGenerator rand_gen(FLAG_random_seed);

  Factory* factory = isolate->factory();

  // Generate some number of names.
  for (int i = 0; i < StubCache::kPrimaryTableSize / 7; i++) {
    Handle<Name> name;
    switch (rand_gen.NextInt(3)) {
      case 0: {
        // Generate string.
        std::stringstream ss;
        ss << "s" << std::hex
           << (rand_gen.NextInt(Smi::kMaxValue) % StubCache::kPrimaryTableSize);
        name = factory->InternalizeUtf8String(ss.str().c_str());
        break;
      }
      case 1: {
        // Generate number string.
        std::stringstream ss;
        ss << (rand_gen.NextInt(Smi::kMaxValue) % StubCache::kPrimaryTableSize);
        name = factory->InternalizeUtf8String(ss.str().c_str());
        break;
      }
      case 2: {
        // Generate symbol.
        name = factory->NewSymbol();
        break;
      }
      default:
        UNREACHABLE();
    }
    names.push_back(name);
  }

  // Generate some number of receiver maps and receivers.
  for (int i = 0; i < StubCache::kSecondaryTableSize / 2; i++) {
    Handle<Map> map = Map::Create(isolate, 0);
    receivers.push_back(factory->NewJSObjectFromMap(map));
  }

  // Generate some number of handlers.
  for (int i = 0; i < 30; i++) {
    Code::Flags flags =
        Code::RemoveHolderFromFlags(Code::ComputeHandlerFlags(ic_kind));
    handlers.push_back(CreateCodeWithFlags(flags));
  }

  // Ensure that GC does happen because from now on we are going to fill our
  // own stub cache instance with raw values.
  DisallowHeapAllocation no_gc;

  // Populate {stub_cache}.
  const int N = StubCache::kPrimaryTableSize + StubCache::kSecondaryTableSize;
  for (int i = 0; i < N; i++) {
    int index = rand_gen.NextInt();
    Handle<Name> name = names[index % names.size()];
    Handle<JSObject> receiver = receivers[index % receivers.size()];
    Handle<Code> handler = handlers[index % handlers.size()];
    stub_cache.Set(*name, receiver->map(), *handler);
  }

  // Perform some queries.
  bool queried_existing = false;
  bool queried_non_existing = false;
  for (int i = 0; i < N; i++) {
    int index = rand_gen.NextInt();
    Handle<Name> name = names[index % names.size()];
    Handle<JSObject> receiver = receivers[index % receivers.size()];
    Object* handler = stub_cache.Get(*name, receiver->map());
    if (handler == nullptr) {
      queried_non_existing = true;
    } else {
      queried_existing = true;
    }

    Handle<Object> expected_handler(handler, isolate);
    ft.CheckTrue(receiver, name, expected_handler);
  }

  for (int i = 0; i < N; i++) {
    int index1 = rand_gen.NextInt();
    int index2 = rand_gen.NextInt();
    Handle<Name> name = names[index1 % names.size()];
    Handle<JSObject> receiver = receivers[index2 % receivers.size()];
    Object* handler = stub_cache.Get(*name, receiver->map());
    if (handler == nullptr) {
      queried_non_existing = true;
    } else {
      queried_existing = true;
    }

    Handle<Object> expected_handler(handler, isolate);
    ft.CheckTrue(receiver, name, expected_handler);
  }
  // Ensure we performed both kind of queries.
  CHECK(queried_existing && queried_non_existing);
}

}  // namespace internal
}  // namespace v8
