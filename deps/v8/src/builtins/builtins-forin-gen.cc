// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins-forin-gen.h"

#include "src/builtins/builtins-utils-gen.h"
#include "src/builtins/builtins.h"
#include "src/code-factory.h"
#include "src/code-stub-assembler.h"
#include "src/counters.h"
#include "src/keys.h"
#include "src/lookup.h"
#include "src/objects-inl.h"
#include "src/property-descriptor.h"

namespace v8 {
namespace internal {

typedef compiler::Node Node;

Node* ForInBuiltinsAssembler::ForInFilter(Node* key, Node* object,
                                          Node* context) {
  CSA_ASSERT(this, IsName(key));

  VARIABLE(var_result, MachineRepresentation::kTagged, key);

  Node* has_property =
      HasProperty(object, key, context, Runtime::kForInHasProperty);

  Label end(this);
  GotoIf(WordEqual(has_property, BooleanConstant(true)), &end);

  var_result.Bind(UndefinedConstant());
  Goto(&end);

  BIND(&end);
  return var_result.value();
}

std::tuple<Node*, Node*, Node*> ForInBuiltinsAssembler::EmitForInPrepare(
    Node* object, Node* context, Label* call_runtime,
    Label* nothing_to_iterate) {
  Label use_cache(this);
  CSA_ASSERT(this, IsJSReceiver(object));

  CheckEnumCache(object, &use_cache, nothing_to_iterate, call_runtime);

  BIND(&use_cache);
  Node* map = LoadMap(object);
  Node* enum_length = EnumLength(map);
  GotoIf(WordEqual(enum_length, SmiConstant(0)), nothing_to_iterate);
  Node* descriptors = LoadMapDescriptors(map);
  Node* cache_offset =
      LoadObjectField(descriptors, DescriptorArray::kEnumCacheBridgeOffset);
  Node* enum_cache = LoadObjectField(
      cache_offset, DescriptorArray::kEnumCacheBridgeCacheOffset);

  return std::make_tuple(map, enum_cache, enum_length);
}

Node* ForInBuiltinsAssembler::EnumLength(Node* map) {
  CSA_ASSERT(this, IsMap(map));
  Node* bitfield_3 = LoadMapBitField3(map);
  Node* enum_length = DecodeWordFromWord32<Map::EnumLengthBits>(bitfield_3);
  return SmiTag(enum_length);
}

void ForInBuiltinsAssembler::CheckPrototypeEnumCache(Node* receiver, Node* map,
                                                     Label* use_cache,
                                                     Label* use_runtime) {
  VARIABLE(current_js_object, MachineRepresentation::kTagged, receiver);
  VARIABLE(current_map, MachineRepresentation::kTagged, map);

  // These variables are updated in the loop below.
  Variable* loop_vars[2] = {&current_js_object, &current_map};
  Label loop(this, 2, loop_vars), next(this);

  Goto(&loop);
  // Check that there are no elements. |current_js_object| contains
  // the current JS object we've reached through the prototype chain.
  BIND(&loop);
  {
    Label if_elements(this), if_no_elements(this);
    Node* elements = LoadElements(current_js_object.value());
    Node* empty_fixed_array = LoadRoot(Heap::kEmptyFixedArrayRootIndex);
    // Check that there are no elements.
    Branch(WordEqual(elements, empty_fixed_array), &if_no_elements,
           &if_elements);
    BIND(&if_elements);
    {
      // Second chance, the object may be using the empty slow element
      // dictionary.
      Node* slow_empty_dictionary =
          LoadRoot(Heap::kEmptySlowElementDictionaryRootIndex);
      Branch(WordNotEqual(elements, slow_empty_dictionary), use_runtime,
             &if_no_elements);
    }

    BIND(&if_no_elements);
    {
      // Update map prototype.
      current_js_object.Bind(LoadMapPrototype(current_map.value()));
      Branch(WordEqual(current_js_object.value(), NullConstant()), use_cache,
             &next);
    }
  }

  BIND(&next);
  {
    // For all objects but the receiver, check that the cache is empty.
    current_map.Bind(LoadMap(current_js_object.value()));
    Node* enum_length = EnumLength(current_map.value());
    Node* zero_constant = SmiConstant(Smi::kZero);
    Branch(WordEqual(enum_length, zero_constant), &loop, use_runtime);
  }
}

void ForInBuiltinsAssembler::CheckEnumCache(Node* receiver, Label* use_cache,
                                            Label* nothing_to_iterate,
                                            Label* use_runtime) {
  Node* map = LoadMap(receiver);

  Label check_empty_prototype(this),
      check_dict_receiver(this, Label::kDeferred);

  // Check if the enum length field is properly initialized, indicating that
  // there is an enum cache.
  {
    Node* invalid_enum_cache_sentinel =
        SmiConstant(Smi::FromInt(kInvalidEnumCacheSentinel));
    Node* enum_length = EnumLength(map);
    Branch(WordEqual(enum_length, invalid_enum_cache_sentinel),
           &check_dict_receiver, &check_empty_prototype);
  }

  // Check that there are no elements on the fast |receiver| and its prototype
  // chain.
  BIND(&check_empty_prototype);
  CheckPrototypeEnumCache(receiver, map, use_cache, use_runtime);

  Label dict_loop(this);
  BIND(&check_dict_receiver);
  {
    // Avoid runtime-call for empty dictionary receivers.
    GotoIfNot(IsDictionaryMap(map), use_runtime);
    Node* properties = LoadProperties(receiver);
    Node* length = LoadFixedArrayElement(
        properties, NameDictionary::kNumberOfElementsIndex);
    GotoIfNot(WordEqual(length, SmiConstant(0)), use_runtime);
    // Check that there are no elements on the |receiver| and its prototype
    // chain. Given that we do not create an EnumCache for dict-mode objects,
    // directly jump to |nothing_to_iterate| if there are no elements and no
    // properties on the |receiver|.
    CheckPrototypeEnumCache(receiver, map, nothing_to_iterate, use_runtime);
  }
}

TF_BUILTIN(ForInFilter, ForInBuiltinsAssembler) {
  Node* key = Parameter(Descriptor::kKey);
  Node* object = Parameter(Descriptor::kObject);
  Node* context = Parameter(Descriptor::kContext);

  Return(ForInFilter(key, object, context));
}

TF_BUILTIN(ForInNext, ForInBuiltinsAssembler) {
  Label filter(this);
  Node* object = Parameter(Descriptor::kObject);
  Node* cache_array = Parameter(Descriptor::kCacheArray);
  Node* cache_type = Parameter(Descriptor::kCacheType);
  Node* index = Parameter(Descriptor::kIndex);
  Node* context = Parameter(Descriptor::kContext);

  Node* key = LoadFixedArrayElement(cache_array, SmiUntag(index));
  Node* map = LoadMap(object);
  GotoIfNot(WordEqual(map, cache_type), &filter);
  Return(key);
  BIND(&filter);
  Return(ForInFilter(key, object, context));
}

TF_BUILTIN(ForInPrepare, ForInBuiltinsAssembler) {
  Label call_runtime(this), nothing_to_iterate(this);
  Node* object = Parameter(Descriptor::kObject);
  Node* context = Parameter(Descriptor::kContext);

  Node* cache_type;
  Node* cache_array;
  Node* cache_length;
  std::tie(cache_type, cache_array, cache_length) =
      EmitForInPrepare(object, context, &call_runtime, &nothing_to_iterate);

  Return(cache_type, cache_array, cache_length);

  BIND(&call_runtime);
  TailCallRuntime(Runtime::kForInPrepare, context, object);

  BIND(&nothing_to_iterate);
  {
    Node* zero = SmiConstant(0);
    Return(zero, zero, zero);
  }
}
}  // namespace internal
}  // namespace v8
