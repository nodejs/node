// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_TEMPLATE_OBJECTS_H_
#define V8_OBJECTS_TEMPLATE_OBJECTS_H_

#include "src/objects.h"
#include "src/objects/hash-table.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

// TemplateObjectDescription is a triple of hash, raw strings and cooked
// strings for tagged template literals. Used to communicate with the runtime
// for template object creation within the {Runtime_GetTemplateObject} method.
class TemplateObjectDescription final : public Tuple3 {
 public:
  DECL_INT_ACCESSORS(hash)
  DECL_ACCESSORS(raw_strings, FixedArray)
  DECL_ACCESSORS(cooked_strings, FixedArray)

  bool Equals(TemplateObjectDescription const* that) const;

  static Handle<JSArray> GetTemplateObject(
      Handle<TemplateObjectDescription> description,
      Handle<Context> native_context);

  DECL_CAST(TemplateObjectDescription)

  static constexpr int kHashOffset = kValue1Offset;
  static constexpr int kRawStringsOffset = kValue2Offset;
  static constexpr int kCookedStringsOffset = kValue3Offset;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(TemplateObjectDescription);
};

class TemplateMapShape final : public BaseShape<TemplateObjectDescription*> {
 public:
  static bool IsMatch(TemplateObjectDescription* key, Object* value);
  static uint32_t Hash(Isolate* isolate, TemplateObjectDescription* key);
  static uint32_t HashForObject(Isolate* isolate, Object* object);

  static constexpr int kPrefixSize = 0;
  static constexpr int kEntrySize = 2;
};

class TemplateMap final : public HashTable<TemplateMap, TemplateMapShape> {
 public:
  static Handle<TemplateMap> New(Isolate* isolate);

  // Tries to lookup the given {key} in the {template_map}. Returns the
  // value if it's found, otherwise returns an empty MaybeHandle.
  WARN_UNUSED_RESULT static MaybeHandle<JSArray> Lookup(
      Handle<TemplateMap> template_map, Handle<TemplateObjectDescription> key);

  // Adds the {key} / {value} pair to the {template_map} and returns the
  // new TemplateMap (we might need to re-allocate). This assumes that
  // there's no entry for {key} in the {template_map} already.
  static Handle<TemplateMap> Add(Handle<TemplateMap> template_map,
                                 Handle<TemplateObjectDescription> key,
                                 Handle<JSArray> value);

  DECL_CAST(TemplateMap)

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(TemplateMap);
};

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_TEMPLATE_OBJECTS_H_
