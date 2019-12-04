// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/api/api-inl.h"
#include "src/heap/spaces.h"
#include "test/cctest/cctest.h"
#include "tools/debug_helper/debug-helper.h"

namespace v8 {
namespace internal {

namespace {

namespace d = v8::debug_helper;

uintptr_t memory_fail_start = 0;
uintptr_t memory_fail_end = 0;

class MemoryFailureRegion {
 public:
  MemoryFailureRegion(uintptr_t start, uintptr_t end) {
    memory_fail_start = start;
    memory_fail_end = end;
  }
  ~MemoryFailureRegion() {
    memory_fail_start = 0;
    memory_fail_end = 0;
  }
};

// Implement the memory-reading callback. This one just fetches memory from the
// current process, but a real implementation for a debugging extension would
// fetch memory from the debuggee process or crash dump.
d::MemoryAccessResult ReadMemory(uintptr_t address, uint8_t* destination,
                                 size_t byte_count) {
  if (address >= memory_fail_start && address <= memory_fail_end) {
    // Simulate failure to read debuggee memory.
    return d::MemoryAccessResult::kAddressValidButInaccessible;
  }
  memcpy(destination, reinterpret_cast<void*>(address), byte_count);
  return d::MemoryAccessResult::kOk;
}

void CheckProp(const d::ObjectProperty& property, const char* expected_type,
               const char* expected_name,
               d::PropertyKind expected_kind = d::PropertyKind::kSingle,
               size_t expected_num_values = 1) {
  CHECK_EQ(property.num_values, expected_num_values);
  CHECK(property.type == std::string("v8::internal::TaggedValue") ||
        property.type == std::string(expected_type));
  CHECK(property.decompressed_type == std::string(expected_type));
  CHECK(property.kind == expected_kind);
  CHECK(property.name == std::string(expected_name));
}

template <typename TValue>
void CheckProp(const d::ObjectProperty& property, const char* expected_type,
               const char* expected_name, TValue expected_value) {
  CheckProp(property, expected_type, expected_name);
  CHECK(*reinterpret_cast<TValue*>(property.address) == expected_value);
}

}  // namespace

TEST(GetObjectProperties) {
  CcTest::InitializeVM();
  v8::Isolate* isolate = CcTest::isolate();
  v8::HandleScope scope(isolate);
  LocalContext context;
  d::Roots roots{0, 0, 0, 0};  // We don't know the heap roots.

  v8::Local<v8::Value> v = CompileRun("42");
  Handle<Object> o = v8::Utils::OpenHandle(*v);
  d::ObjectPropertiesResultPtr props =
      d::GetObjectProperties(o->ptr(), &ReadMemory, roots);
  CHECK(props->type_check_result == d::TypeCheckResult::kSmi);
  CHECK(props->brief == std::string("42 (0x2a)"));
  CHECK(props->type == std::string("v8::internal::Smi"));
  CHECK_EQ(props->num_properties, 0);

  v = CompileRun("[\"a\", \"bc\"]");
  o = v8::Utils::OpenHandle(*v);
  props = d::GetObjectProperties(o->ptr(), &ReadMemory, roots);
  CHECK(props->type_check_result == d::TypeCheckResult::kUsedMap);
  CHECK(props->type == std::string("v8::internal::JSArray"));
  CHECK_EQ(props->num_properties, 4);
  CheckProp(*props->properties[0], "v8::internal::Map", "map");
  CheckProp(*props->properties[1], "v8::internal::Object",
            "properties_or_hash");
  CheckProp(*props->properties[2], "v8::internal::FixedArrayBase", "elements");
  CheckProp(*props->properties[3], "v8::internal::Object", "length",
            static_cast<i::Tagged_t>(IntToSmi(2)));

  // We need to supply a root address for decompression before reading the
  // elements from the JSArray.
  roots.any_heap_pointer = o->ptr();

  i::Tagged_t properties_or_hash =
      *reinterpret_cast<i::Tagged_t*>(props->properties[1]->address);
  i::Tagged_t elements =
      *reinterpret_cast<i::Tagged_t*>(props->properties[2]->address);

  // The properties_or_hash_code field should be an empty fixed array. Since
  // that is at a known offset, we should be able to detect it even without
  // any ability to read memory.
  {
    MemoryFailureRegion failure(0, UINTPTR_MAX);
    props = d::GetObjectProperties(properties_or_hash, &ReadMemory, roots);
    CHECK(props->type_check_result ==
          d::TypeCheckResult::kObjectPointerValidButInaccessible);
    CHECK(props->type == std::string("v8::internal::HeapObject"));
    CHECK_EQ(props->num_properties, 1);
    CheckProp(*props->properties[0], "v8::internal::Map", "map");
    CHECK(std::string(props->brief).substr(0, 21) ==
          std::string("maybe EmptyFixedArray"));

    // Provide a heap root so the API can be more sure.
    roots.read_only_space =
        reinterpret_cast<uintptr_t>(reinterpret_cast<i::Isolate*>(isolate)
                                        ->heap()
                                        ->read_only_space()
                                        ->first_page());
    props = d::GetObjectProperties(properties_or_hash, &ReadMemory, roots);
    CHECK(props->type_check_result ==
          d::TypeCheckResult::kObjectPointerValidButInaccessible);
    CHECK(props->type == std::string("v8::internal::HeapObject"));
    CHECK_EQ(props->num_properties, 1);
    CheckProp(*props->properties[0], "v8::internal::Map", "map");
    CHECK(std::string(props->brief).substr(0, 15) ==
          std::string("EmptyFixedArray"));
  }

  props = d::GetObjectProperties(elements, &ReadMemory, roots);
  CHECK(props->type_check_result == d::TypeCheckResult::kUsedMap);
  CHECK(props->type == std::string("v8::internal::FixedArray"));
  CHECK_EQ(props->num_properties, 3);
  CheckProp(*props->properties[0], "v8::internal::Map", "map");
  CheckProp(*props->properties[1], "v8::internal::Object", "length",
            static_cast<i::Tagged_t>(IntToSmi(2)));
  CheckProp(*props->properties[2], "v8::internal::Object", "objects",
            d::PropertyKind::kArrayOfKnownSize, 2);

  // Get the second string value from the FixedArray.
  i::Tagged_t second_string_address = *reinterpret_cast<i::Tagged_t*>(
      props->properties[2]->address + sizeof(i::Tagged_t));
  props = d::GetObjectProperties(second_string_address, &ReadMemory, roots);
  CHECK(props->type_check_result == d::TypeCheckResult::kUsedMap);
  CHECK(props->type == std::string("v8::internal::SeqOneByteString"));
  CHECK_EQ(props->num_properties, 4);
  CheckProp(*props->properties[0], "v8::internal::Map", "map");
  CheckProp(*props->properties[1], "uint32_t", "hash_field");
  CheckProp(*props->properties[2], "int32_t", "length", 2);
  CheckProp(*props->properties[3], "char", "chars",
            d::PropertyKind::kArrayOfKnownSize, 2);
  CHECK_EQ(
      strncmp("bc",
              reinterpret_cast<const char*>(props->properties[3]->address), 2),
      0);

  // Read the second string again, using a type hint instead of the map. All of
  // its properties should match what we read last time.
  d::ObjectPropertiesResultPtr props2;
  {
    uintptr_t map_address =
        d::GetObjectProperties(
            *reinterpret_cast<i::Tagged_t*>(props->properties[0]->address),
            &ReadMemory, roots)
            ->properties[0]
            ->address;
    MemoryFailureRegion failure(map_address, map_address + i::Map::kSize);
    props2 = d::GetObjectProperties(second_string_address, &ReadMemory, roots,
                                    "v8::internal::String");
    CHECK(props2->type_check_result == d::TypeCheckResult::kUsedTypeHint);
    CHECK(props2->type == std::string("v8::internal::String"));
    CHECK_EQ(props2->num_properties, 3);
    CheckProp(*props2->properties[0], "v8::internal::Map", "map",
              *reinterpret_cast<i::Tagged_t*>(props->properties[0]->address));
    CheckProp(*props2->properties[1], "uint32_t", "hash_field",
              *reinterpret_cast<int32_t*>(props->properties[1]->address));
    CheckProp(*props2->properties[2], "int32_t", "length", 2);
  }

  // Try a weak reference.
  props2 = d::GetObjectProperties(second_string_address | kWeakHeapObjectMask,
                                  &ReadMemory, roots);
  std::string weak_ref_prefix = "weak ref to ";
  CHECK(weak_ref_prefix + props->brief == props2->brief);
  CHECK(props2->type_check_result == d::TypeCheckResult::kUsedMap);
  CHECK(props2->type == std::string("v8::internal::SeqOneByteString"));
  CHECK_EQ(props2->num_properties, 4);
  CheckProp(*props2->properties[0], "v8::internal::Map", "map",
            *reinterpret_cast<i::Tagged_t*>(props->properties[0]->address));
  CheckProp(*props2->properties[1], "uint32_t", "hash_field",
            *reinterpret_cast<i::Tagged_t*>(props->properties[1]->address));
  CheckProp(*props2->properties[2], "int32_t", "length", 2);

  // Build a complicated string (multi-level cons with slices inside) to test
  // string printing.
  v = CompileRun(R"(
    const alphabet = "abcdefghijklmnopqrstuvwxyz";
    alphabet.substr(3,20) + alphabet.toUpperCase().substr(5,15) + "7")");
  o = v8::Utils::OpenHandle(*v);
  props = d::GetObjectProperties(o->ptr(), &ReadMemory, roots);
  CHECK(std::string(props->brief).substr(0, 38) ==
        std::string("\"defghijklmnopqrstuvwFGHIJKLMNOPQRST7\""));

  // Cause a failure when reading the "second" pointer within the top-level
  // ConsString.
  {
    CheckProp(*props->properties[4], "v8::internal::String", "second");
    uintptr_t second_address = props->properties[4]->address;
    MemoryFailureRegion failure(second_address, second_address + 4);
    props = d::GetObjectProperties(o->ptr(), &ReadMemory, roots);
    CHECK(std::string(props->brief).substr(0, 40) ==
          std::string("\"defghijklmnopqrstuvwFGHIJKLMNOPQRST...\""));
  }

  // Build a very long string.
  v = CompileRun("'a'.repeat(1000)");
  o = v8::Utils::OpenHandle(*v);
  props = d::GetObjectProperties(o->ptr(), &ReadMemory, roots);
  CHECK(std::string(props->brief).substr(79, 7) == std::string("aa...\" "));
}

}  // namespace internal
}  // namespace v8
