// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_ARGUMENTS_H_
#define V8_OBJECTS_ARGUMENTS_H_

#include "src/objects/fixed-array.h"
#include "src/objects/js-objects.h"
#include "src/objects/struct.h"
#include "torque-generated/field-offsets-tq.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

// Superclass for all objects with instance type {JS_ARGUMENTS_TYPE}
class JSArgumentsObject : public JSObject {
 public:
  DECL_VERIFIER(JSArgumentsObject)
  DECL_CAST(JSArgumentsObject)
  OBJECT_CONSTRUCTORS(JSArgumentsObject, JSObject);
};

// Common superclass for JSSloppyArgumentsObject and JSStrictArgumentsObject.
// Note that the instance type {JS_ARGUMENTS_TYPE} does _not_ guarantee the
// below layout, the in-object properties might have transitioned to dictionary
// mode already. Only use the below layout with the specific initial maps.
class JSArgumentsObjectWithLength : public JSArgumentsObject {
 public:
  // Layout description.
  DEFINE_FIELD_OFFSET_CONSTANTS(
      JSObject::kHeaderSize,
      TORQUE_GENERATED_JSARGUMENTS_OBJECT_WITH_LENGTH_FIELDS)

  // Indices of in-object properties.
  static const int kLengthIndex = 0;

  DECL_VERIFIER(JSArgumentsObjectWithLength)

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(JSArgumentsObjectWithLength);
};

// JSSloppyArgumentsObject is just a JSObject with specific initial map.
// This initial map adds in-object properties for "length" and "callee".
class JSSloppyArgumentsObject : public JSArgumentsObjectWithLength {
 public:
  DEFINE_FIELD_OFFSET_CONSTANTS(
      JSArgumentsObjectWithLength::kSize,
      TORQUE_GENERATED_JSSLOPPY_ARGUMENTS_OBJECT_FIELDS)

  // Indices of in-object properties.
  static const int kCalleeIndex = kLengthIndex + 1;

  inline static bool GetSloppyArgumentsLength(Isolate* isolate,
                                              Handle<JSObject> object,
                                              int* out);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(JSSloppyArgumentsObject);
};

// JSStrictArgumentsObject is just a JSObject with specific initial map.
// This initial map adds an in-object property for "length".
class JSStrictArgumentsObject : public JSArgumentsObjectWithLength {
 public:
  // Layout description.
  static const int kSize = JSArgumentsObjectWithLength::kSize;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(JSStrictArgumentsObject);
};

// Helper class to access FAST_ and SLOW_SLOPPY_ARGUMENTS_ELEMENTS
//
// +---+-----------------------+
// | 0 | Context  context      |
// +---------------------------+
// | 1 | FixedArray arguments  +----+ HOLEY_ELEMENTS
// +---------------------------+    v-----+-----------+
// | 2 | Object param_1_map    |    |  0  | the_hole  |
// |...| ...                   |    | ... | ...       |
// |n+1| Object param_n_map    |    | n-1 | the_hole  |
// +---------------------------+    |  n  | element_1 |
//                                  | ... | ...       |
//                                  |n+m-1| element_m |
//                                  +-----------------+
//
// Parameter maps give the index into the provided context. If a map entry is
// the_hole it means that the given entry has been deleted from the arguments
// object.
// The arguments backing store kind depends on the ElementsKind of the outer
// JSArgumentsObject:
// - FAST_SLOPPY_ARGUMENTS_ELEMENTS: HOLEY_ELEMENTS
// - SLOW_SLOPPY_ARGUMENTS_ELEMENTS: DICTIONARY_ELEMENTS
class SloppyArgumentsElements : public FixedArray {
 public:
  static const int kContextIndex = 0;
  static const int kArgumentsIndex = 1;
  static const uint32_t kParameterMapStart = 2;

  DECL_GETTER(context, Context)
  DECL_GETTER(arguments, FixedArray)
  inline void set_arguments(FixedArray arguments);
  inline uint32_t parameter_map_length();
  inline Object get_mapped_entry(uint32_t entry);
  inline void set_mapped_entry(uint32_t entry, Object object);

  DECL_CAST(SloppyArgumentsElements)
#ifdef VERIFY_HEAP
  void SloppyArgumentsElementsVerify(Isolate* isolate, JSObject holder);
#endif

  OBJECT_CONSTRUCTORS(SloppyArgumentsElements, FixedArray);
};

// Representation of a slow alias as part of a sloppy arguments objects.
// For fast aliases (if HasSloppyArgumentsElements()):
// - the parameter map contains an index into the context
// - all attributes of the element have default values
// For slow aliases (if HasDictionaryArgumentsElements()):
// - the parameter map contains no fast alias mapping (i.e. the hole)
// - this struct (in the slow backing store) contains an index into the context
// - all attributes are available as part if the property details
class AliasedArgumentsEntry : public Struct {
 public:
  inline int aliased_context_slot() const;
  inline void set_aliased_context_slot(int count);

  DECL_CAST(AliasedArgumentsEntry)

  // Dispatched behavior.
  DECL_PRINTER(AliasedArgumentsEntry)
  DECL_VERIFIER(AliasedArgumentsEntry)

  DEFINE_FIELD_OFFSET_CONSTANTS(HeapObject::kHeaderSize,
                                TORQUE_GENERATED_ALIASED_ARGUMENTS_ENTRY_FIELDS)

  OBJECT_CONSTRUCTORS(AliasedArgumentsEntry, Struct);
};

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_ARGUMENTS_H_
