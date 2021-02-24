// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/codegen/external-reference-table.h"

#include "src/builtins/accessors.h"
#include "src/codegen/external-reference.h"
#include "src/ic/stub-cache.h"
#include "src/logging/counters.h"

#if defined(DEBUG) && defined(V8_OS_LINUX) && !defined(V8_OS_ANDROID)
#define SYMBOLIZE_FUNCTION
#include <execinfo.h>

#include <vector>

#include "src/base/platform/wrappers.h"
#endif  // DEBUG && V8_OS_LINUX && !V8_OS_ANDROID

namespace v8 {
namespace internal {

#define ADD_EXT_REF_NAME(name, desc) desc,
#define ADD_BUILTIN_NAME(Name, ...) "Builtin_" #Name,
#define ADD_RUNTIME_FUNCTION(name, ...) "Runtime::" #name,
#define ADD_ISOLATE_ADDR(Name, name) "Isolate::" #name "_address",
#define ADD_ACCESSOR_INFO_NAME(_, __, AccessorName, ...) \
  "Accessors::" #AccessorName "Getter",
#define ADD_ACCESSOR_SETTER_NAME(name) "Accessors::" #name,
#define ADD_STATS_COUNTER_NAME(name, ...) "StatsCounter::" #name,
// static
// clang-format off
const char* const
    ExternalReferenceTable::ref_name_[ExternalReferenceTable::kSize] = {
        // Special references:
        "nullptr",
        // External references:
        EXTERNAL_REFERENCE_LIST(ADD_EXT_REF_NAME)
        EXTERNAL_REFERENCE_LIST_WITH_ISOLATE(ADD_EXT_REF_NAME)
        // Builtins:
        BUILTIN_LIST_C(ADD_BUILTIN_NAME)
        // Runtime functions:
        FOR_EACH_INTRINSIC(ADD_RUNTIME_FUNCTION)
        // Isolate addresses:
        FOR_EACH_ISOLATE_ADDRESS_NAME(ADD_ISOLATE_ADDR)
        // Accessors:
        ACCESSOR_INFO_LIST_GENERATOR(ADD_ACCESSOR_INFO_NAME, /* not used */)
        ACCESSOR_SETTER_LIST(ADD_ACCESSOR_SETTER_NAME)
        // Stub cache:
        "Load StubCache::primary_->key",
        "Load StubCache::primary_->value",
        "Load StubCache::primary_->map",
        "Load StubCache::secondary_->key",
        "Load StubCache::secondary_->value",
        "Load StubCache::secondary_->map",
        "Store StubCache::primary_->key",
        "Store StubCache::primary_->value",
        "Store StubCache::primary_->map",
        "Store StubCache::secondary_->key",
        "Store StubCache::secondary_->value",
        "Store StubCache::secondary_->map",
        // Native code counters:
        STATS_COUNTER_NATIVE_CODE_LIST(ADD_STATS_COUNTER_NAME)
};
// clang-format on
#undef ADD_EXT_REF_NAME
#undef ADD_BUILTIN_NAME
#undef ADD_RUNTIME_FUNCTION
#undef ADD_ISOLATE_ADDR
#undef ADD_ACCESSOR_INFO_NAME
#undef ADD_ACCESSOR_SETTER_NAME
#undef ADD_STATS_COUNTER_NAME

// Forward declarations for C++ builtins.
#define FORWARD_DECLARE(Name) \
  Address Builtin_##Name(int argc, Address* args, Isolate* isolate);
BUILTIN_LIST_C(FORWARD_DECLARE)
#undef FORWARD_DECLARE

void ExternalReferenceTable::Init(Isolate* isolate) {
  int index = 0;

  // kNullAddress is preserved through serialization/deserialization.
  Add(kNullAddress, &index);
  AddReferences(isolate, &index);
  AddBuiltins(&index);
  AddRuntimeFunctions(&index);
  AddIsolateAddresses(isolate, &index);
  AddAccessors(&index);
  AddStubCache(isolate, &index);
  AddNativeCodeStatsCounters(isolate, &index);
  is_initialized_ = static_cast<uint32_t>(true);

  CHECK_EQ(kSize, index);
}

const char* ExternalReferenceTable::ResolveSymbol(void* address) {
#ifdef SYMBOLIZE_FUNCTION
  char** names = backtrace_symbols(&address, 1);
  const char* name = names[0];
  // The array of names is malloc'ed. However, each name string is static
  // and do not need to be freed.
  base::Free(names);
  return name;
#else
  return "<unresolved>";
#endif  // SYMBOLIZE_FUNCTION
}

void ExternalReferenceTable::Add(Address address, int* index) {
  ref_addr_[(*index)++] = address;
}

void ExternalReferenceTable::AddReferences(Isolate* isolate, int* index) {
  CHECK_EQ(kSpecialReferenceCount, *index);

#define ADD_EXTERNAL_REFERENCE(name, desc) \
  Add(ExternalReference::name().address(), index);
  EXTERNAL_REFERENCE_LIST(ADD_EXTERNAL_REFERENCE)
#undef ADD_EXTERNAL_REFERENCE

#define ADD_EXTERNAL_REFERENCE(name, desc) \
  Add(ExternalReference::name(isolate).address(), index);
  EXTERNAL_REFERENCE_LIST_WITH_ISOLATE(ADD_EXTERNAL_REFERENCE)
#undef ADD_EXTERNAL_REFERENCE

  CHECK_EQ(kSpecialReferenceCount + kExternalReferenceCount, *index);
}

void ExternalReferenceTable::AddBuiltins(int* index) {
  CHECK_EQ(kSpecialReferenceCount + kExternalReferenceCount, *index);

  static const Address c_builtins[] = {
#define DEF_ENTRY(Name, ...) FUNCTION_ADDR(&Builtin_##Name),
      BUILTIN_LIST_C(DEF_ENTRY)
#undef DEF_ENTRY
  };
  for (Address addr : c_builtins) {
    Add(ExternalReference::Create(addr).address(), index);
  }

  CHECK_EQ(kSpecialReferenceCount + kExternalReferenceCount +
               kBuiltinsReferenceCount,
           *index);
}

void ExternalReferenceTable::AddRuntimeFunctions(int* index) {
  CHECK_EQ(kSpecialReferenceCount + kExternalReferenceCount +
               kBuiltinsReferenceCount,
           *index);

  static constexpr Runtime::FunctionId runtime_functions[] = {
#define RUNTIME_ENTRY(name, ...) Runtime::k##name,
      FOR_EACH_INTRINSIC(RUNTIME_ENTRY)
#undef RUNTIME_ENTRY
  };

  for (Runtime::FunctionId fId : runtime_functions) {
    Add(ExternalReference::Create(fId).address(), index);
  }

  CHECK_EQ(kSpecialReferenceCount + kExternalReferenceCount +
               kBuiltinsReferenceCount + kRuntimeReferenceCount,
           *index);
}

void ExternalReferenceTable::AddIsolateAddresses(Isolate* isolate, int* index) {
  CHECK_EQ(kSpecialReferenceCount + kExternalReferenceCount +
               kBuiltinsReferenceCount + kRuntimeReferenceCount,
           *index);

  for (int i = 0; i < IsolateAddressId::kIsolateAddressCount; ++i) {
    Add(isolate->get_address_from_id(static_cast<IsolateAddressId>(i)), index);
  }

  CHECK_EQ(kSpecialReferenceCount + kExternalReferenceCount +
               kBuiltinsReferenceCount + kRuntimeReferenceCount +
               kIsolateAddressReferenceCount,
           *index);
}

void ExternalReferenceTable::AddAccessors(int* index) {
  CHECK_EQ(kSpecialReferenceCount + kExternalReferenceCount +
               kBuiltinsReferenceCount + kRuntimeReferenceCount +
               kIsolateAddressReferenceCount,
           *index);

  static const Address accessors[] = {
  // Getters:
#define ACCESSOR_INFO_DECLARATION(_, __, AccessorName, ...) \
  FUNCTION_ADDR(&Accessors::AccessorName##Getter),
      ACCESSOR_INFO_LIST_GENERATOR(ACCESSOR_INFO_DECLARATION, /* not used */)
#undef ACCESSOR_INFO_DECLARATION
  // Setters:
#define ACCESSOR_SETTER_DECLARATION(name) FUNCTION_ADDR(&Accessors::name),
          ACCESSOR_SETTER_LIST(ACCESSOR_SETTER_DECLARATION)
#undef ACCESSOR_SETTER_DECLARATION
  };

  for (Address addr : accessors) {
    Add(addr, index);
  }

  CHECK_EQ(kSpecialReferenceCount + kExternalReferenceCount +
               kBuiltinsReferenceCount + kRuntimeReferenceCount +
               kIsolateAddressReferenceCount + kAccessorReferenceCount,
           *index);
}

void ExternalReferenceTable::AddStubCache(Isolate* isolate, int* index) {
  CHECK_EQ(kSpecialReferenceCount + kExternalReferenceCount +
               kBuiltinsReferenceCount + kRuntimeReferenceCount +
               kIsolateAddressReferenceCount + kAccessorReferenceCount,
           *index);

  StubCache* load_stub_cache = isolate->load_stub_cache();

  // Stub cache tables
  Add(load_stub_cache->key_reference(StubCache::kPrimary).address(), index);
  Add(load_stub_cache->value_reference(StubCache::kPrimary).address(), index);
  Add(load_stub_cache->map_reference(StubCache::kPrimary).address(), index);
  Add(load_stub_cache->key_reference(StubCache::kSecondary).address(), index);
  Add(load_stub_cache->value_reference(StubCache::kSecondary).address(), index);
  Add(load_stub_cache->map_reference(StubCache::kSecondary).address(), index);

  StubCache* store_stub_cache = isolate->store_stub_cache();

  // Stub cache tables
  Add(store_stub_cache->key_reference(StubCache::kPrimary).address(), index);
  Add(store_stub_cache->value_reference(StubCache::kPrimary).address(), index);
  Add(store_stub_cache->map_reference(StubCache::kPrimary).address(), index);
  Add(store_stub_cache->key_reference(StubCache::kSecondary).address(), index);
  Add(store_stub_cache->value_reference(StubCache::kSecondary).address(),
      index);
  Add(store_stub_cache->map_reference(StubCache::kSecondary).address(), index);

  CHECK_EQ(kSpecialReferenceCount + kExternalReferenceCount +
               kBuiltinsReferenceCount + kRuntimeReferenceCount +
               kIsolateAddressReferenceCount + kAccessorReferenceCount +
               kStubCacheReferenceCount,
           *index);
}

Address ExternalReferenceTable::GetStatsCounterAddress(StatsCounter* counter) {
  int* address = counter->Enabled()
                     ? counter->GetInternalPointer()
                     : reinterpret_cast<int*>(&dummy_stats_counter_);
  return reinterpret_cast<Address>(address);
}

void ExternalReferenceTable::AddNativeCodeStatsCounters(Isolate* isolate,
                                                        int* index) {
  CHECK_EQ(kSpecialReferenceCount + kExternalReferenceCount +
               kBuiltinsReferenceCount + kRuntimeReferenceCount +
               kIsolateAddressReferenceCount + kAccessorReferenceCount +
               kStubCacheReferenceCount,
           *index);

  Counters* counters = isolate->counters();

#define SC(name, caption) Add(GetStatsCounterAddress(counters->name()), index);
  STATS_COUNTER_NATIVE_CODE_LIST(SC)
#undef SC

  CHECK_EQ(kSpecialReferenceCount + kExternalReferenceCount +
               kBuiltinsReferenceCount + kRuntimeReferenceCount +
               kIsolateAddressReferenceCount + kAccessorReferenceCount +
               kStubCacheReferenceCount + kStatsCountersReferenceCount,
           *index);
  CHECK_EQ(kSize, *index);
}

}  // namespace internal
}  // namespace v8

#undef SYMBOLIZE_FUNCTION
