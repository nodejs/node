// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/v8.h"

#include "src/factory.h"
#include "src/isolate.h"
#include "src/list.h"
#include "src/objects.h"
// FIXME(mstarzinger, marja): This is weird, but required because of the missing
// (disallowed) include: src/factory.h -> src/objects-inl.h
#include "src/objects-inl.h"
// FIXME(mstarzinger, marja): This is weird, but required because of the missing
// (disallowed) include: src/type-feedback-vector.h ->
// src/type-feedback-vector-inl.h
#include "src/type-feedback-vector-inl.h"
#include "test/cctest/cctest.h"

namespace v8 {
namespace internal {

namespace {

static Handle<Code> GetDummyCode(Isolate* isolate) {
  CodeDesc desc = {nullptr,   // buffer
                   0,         // buffer_size
                   0,         // instr_size
                   0,         // reloc_size
                   0,         // constant_pool_size
                   nullptr,   // unwinding_info
                   0,         // unwinding_info_size
                   nullptr};  // origin
  Code::Flags flags =
      Code::ComputeFlags(Code::LOAD_IC, kNoExtraICState, kCacheOnReceiver);
  Handle<Code> self_ref;
  return isolate->factory()->NewCode(desc, flags, self_ref);
}

}  // namespace

TEST(CodeCache) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();
  HandleScope handle_scope(isolate);

  Handle<Map> map =
      factory->NewMap(JS_OBJECT_TYPE, JSObject::kHeaderSize, FAST_ELEMENTS);

  // This number should be large enough to cause the code cache to use its
  // hash table storage format.
  static const int kEntries = 150;

  // Prepare name/code pairs.
  List<Handle<Name>> names(kEntries);
  List<Handle<Code>> codes(kEntries);
  for (int i = 0; i < kEntries; i++) {
    names.Add(isolate->factory()->NewSymbol());
    codes.Add(GetDummyCode(isolate));
  }
  Handle<Name> bad_name = isolate->factory()->NewSymbol();
  Code::Flags bad_flags =
      Code::ComputeFlags(Code::LOAD_IC, kNoExtraICState, kCacheOnPrototype);
  DCHECK(bad_flags != codes[0]->flags());

  // Cache name/code pairs.
  for (int i = 0; i < kEntries; i++) {
    Handle<Name> name = names.at(i);
    Handle<Code> code = codes.at(i);
    Map::UpdateCodeCache(map, name, code);
    CHECK_EQ(*code, map->LookupInCodeCache(*name, code->flags()));
    CHECK_NULL(map->LookupInCodeCache(*name, bad_flags));
  }
  CHECK_NULL(map->LookupInCodeCache(*bad_name, bad_flags));

  // Check that lookup works not only right after storing.
  for (int i = 0; i < kEntries; i++) {
    Handle<Name> name = names.at(i);
    Handle<Code> code = codes.at(i);
    CHECK_EQ(*code, map->LookupInCodeCache(*name, code->flags()));
  }
}

}  // namespace internal
}  // namespace v8
