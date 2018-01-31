// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/runtime/runtime.h"

#include "src/assembler.h"
#include "src/base/hashmap.h"
#include "src/contexts.h"
#include "src/handles-inl.h"
#include "src/heap/heap.h"
#include "src/isolate.h"
#include "src/objects-inl.h"
#include "src/runtime/runtime-utils.h"

namespace v8 {
namespace internal {

// Header of runtime functions.
#define F(name, number_of_args, result_size)                    \
  Object* Runtime_##name(int args_length, Object** args_object, \
                         Isolate* isolate);
FOR_EACH_INTRINSIC_RETURN_OBJECT(F)
#undef F

#define P(name, number_of_args, result_size)                       \
  ObjectPair Runtime_##name(int args_length, Object** args_object, \
                            Isolate* isolate);
FOR_EACH_INTRINSIC_RETURN_PAIR(P)
#undef P

#define F(name, number_of_args, result_size)                                  \
  {                                                                           \
    Runtime::k##name, Runtime::RUNTIME, #name, FUNCTION_ADDR(Runtime_##name), \
        number_of_args, result_size                                           \
  }                                                                           \
  ,


#define I(name, number_of_args, result_size)                       \
  {                                                                \
    Runtime::kInline##name, Runtime::INLINE, "_" #name,            \
        FUNCTION_ADDR(Runtime_##name), number_of_args, result_size \
  }                                                                \
  ,

static const Runtime::Function kIntrinsicFunctions[] = {
  FOR_EACH_INTRINSIC(F)
  FOR_EACH_INTRINSIC(I)
};

#undef I
#undef F

namespace {

V8_DECLARE_ONCE(initialize_function_name_map_once);
static const base::CustomMatcherHashMap* kRuntimeFunctionNameMap;

struct IntrinsicFunctionIdentifier {
  IntrinsicFunctionIdentifier(const unsigned char* data, const int length)
      : data_(data), length_(length) {}

  static bool Match(void* key1, void* key2) {
    const IntrinsicFunctionIdentifier* lhs =
        static_cast<IntrinsicFunctionIdentifier*>(key1);
    const IntrinsicFunctionIdentifier* rhs =
        static_cast<IntrinsicFunctionIdentifier*>(key2);
    if (lhs->length_ != rhs->length_) return false;
    return CompareCharsUnsigned(reinterpret_cast<const uint8_t*>(lhs->data_),
                                reinterpret_cast<const uint8_t*>(rhs->data_),
                                rhs->length_) == 0;
  }

  uint32_t Hash() {
    return StringHasher::HashSequentialString<uint8_t>(
        data_, length_, v8::internal::kZeroHashSeed);
  }

  const unsigned char* data_;
  const int length_;
};

void InitializeIntrinsicFunctionNames() {
  base::CustomMatcherHashMap* function_name_map =
      new base::CustomMatcherHashMap(IntrinsicFunctionIdentifier::Match);
  for (size_t i = 0; i < arraysize(kIntrinsicFunctions); ++i) {
    const Runtime::Function* function = &kIntrinsicFunctions[i];
    IntrinsicFunctionIdentifier* identifier = new IntrinsicFunctionIdentifier(
        reinterpret_cast<const unsigned char*>(function->name),
        static_cast<int>(strlen(function->name)));
    base::HashMap::Entry* entry =
        function_name_map->InsertNew(identifier, identifier->Hash());
    entry->value = const_cast<Runtime::Function*>(function);
  }
  kRuntimeFunctionNameMap = function_name_map;
}

}  // namespace

const Runtime::Function* Runtime::FunctionForName(const unsigned char* name,
                                                  int length) {
  base::CallOnce(&initialize_function_name_map_once,
                 &InitializeIntrinsicFunctionNames);
  IntrinsicFunctionIdentifier identifier(name, length);
  base::HashMap::Entry* entry =
      kRuntimeFunctionNameMap->Lookup(&identifier, identifier.Hash());
  if (entry) {
    return reinterpret_cast<Function*>(entry->value);
  }
  return nullptr;
}


const Runtime::Function* Runtime::FunctionForEntry(Address entry) {
  for (size_t i = 0; i < arraysize(kIntrinsicFunctions); ++i) {
    if (entry == kIntrinsicFunctions[i].entry) {
      return &(kIntrinsicFunctions[i]);
    }
  }
  return nullptr;
}


const Runtime::Function* Runtime::FunctionForId(Runtime::FunctionId id) {
  return &(kIntrinsicFunctions[static_cast<int>(id)]);
}


const Runtime::Function* Runtime::RuntimeFunctionTable(Isolate* isolate) {
  if (isolate->external_reference_redirector()) {
    // When running with the simulator we need to provide a table which has
    // redirected runtime entry addresses.
    if (!isolate->runtime_state()->redirected_intrinsic_functions()) {
      size_t function_count = arraysize(kIntrinsicFunctions);
      Function* redirected_functions = new Function[function_count];
      memcpy(redirected_functions, kIntrinsicFunctions,
             sizeof(kIntrinsicFunctions));
      for (size_t i = 0; i < function_count; i++) {
        ExternalReference redirected_entry(static_cast<Runtime::FunctionId>(i),
                                           isolate);
        redirected_functions[i].entry = redirected_entry.address();
      }
      isolate->runtime_state()->set_redirected_intrinsic_functions(
          redirected_functions);
    }

    return isolate->runtime_state()->redirected_intrinsic_functions();
  } else {
    return kIntrinsicFunctions;
  }
}


std::ostream& operator<<(std::ostream& os, Runtime::FunctionId id) {
  return os << Runtime::FunctionForId(id)->name;
}


}  // namespace internal
}  // namespace v8
