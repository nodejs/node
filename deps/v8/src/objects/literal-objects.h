// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_LITERAL_OBJECTS_H_
#define V8_OBJECTS_LITERAL_OBJECTS_H_

#include "src/objects.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

class ClassLiteral;

// BoilerplateDescription is a list of properties consisting of name value
// pairs. In addition to the properties, it provides the projected number
// of properties in the backing store. This number includes properties with
// computed names that are not
// in the list.
class BoilerplateDescription : public FixedArray {
 public:
  Object* name(int index) const;
  Object* value(int index) const;

  // The number of boilerplate properties.
  int size() const;

  // Number of boilerplate properties and properties with computed names.
  int backing_store_size() const;

  void set_backing_store_size(Isolate* isolate, int backing_store_size);

  DECL_CAST(BoilerplateDescription)

 private:
  bool has_number_of_properties() const;
};

// Pair of {ElementsKind} and an array of constant values for {ArrayLiteral}
// expressions. Used to communicate with the runtime for literal boilerplate
// creation within the {Runtime_CreateArrayLiteral} method.
class ConstantElementsPair : public Tuple2 {
 public:
  DECL_INT_ACCESSORS(elements_kind)
  DECL_ACCESSORS(constant_values, FixedArrayBase)

  inline bool is_empty() const;

  DECL_CAST(ConstantElementsPair)

  static const int kElementsKindOffset = kValue1Offset;
  static const int kConstantValuesOffset = kValue2Offset;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(ConstantElementsPair);
};

class ClassBoilerplate : public FixedArray {
 public:
  enum ValueKind { kData, kGetter, kSetter };

  struct Flags {
#define FLAGS_BIT_FIELDS(V, _)               \
  V(InstallClassNameAccessorBit, bool, 1, _) \
  V(ArgumentsCountBits, int, 30, _)
    DEFINE_BIT_FIELDS(FLAGS_BIT_FIELDS)
#undef FLAGS_BIT_FIELDS
  };

  struct ComputedEntryFlags {
#define COMPUTED_ENTRY_BIT_FIELDS(V, _) \
  V(ValueKindBits, ValueKind, 2, _)     \
  V(KeyIndexBits, unsigned, 29, _)
    DEFINE_BIT_FIELDS(COMPUTED_ENTRY_BIT_FIELDS)
#undef COMPUTED_ENTRY_BIT_FIELDS
  };

  enum DefineClassArgumentsIndices {
    kConstructorArgumentIndex = 1,
    kPrototypeArgumentIndex = 2,
    // The index of a first dynamic argument passed to Runtime::kDefineClass
    // function. The dynamic arguments are consist of method closures and
    // computed property names.
    kFirstDynamicArgumentIndex = 3,
  };

  static const int kMinimumClassPropertiesCount = 6;
  static const int kMinimumPrototypePropertiesCount = 1;

  DECL_CAST(ClassBoilerplate)

  DECL_BOOLEAN_ACCESSORS(install_class_name_accessor)
  DECL_INT_ACCESSORS(arguments_count)
  DECL_ACCESSORS(static_properties_template, Object)
  DECL_ACCESSORS(static_elements_template, Object)
  DECL_ACCESSORS(static_computed_properties, FixedArray)
  DECL_ACCESSORS(instance_properties_template, Object)
  DECL_ACCESSORS(instance_elements_template, Object)
  DECL_ACCESSORS(instance_computed_properties, FixedArray)

  static void AddToPropertiesTemplate(Isolate* isolate,
                                      Handle<NameDictionary> dictionary,
                                      Handle<Name> name, int key_index,
                                      ValueKind value_kind, Object* value);

  static void AddToElementsTemplate(Isolate* isolate,
                                    Handle<NumberDictionary> dictionary,
                                    uint32_t key, int key_index,
                                    ValueKind value_kind, Object* value);

  static Handle<ClassBoilerplate> BuildClassBoilerplate(Isolate* isolate,
                                                        ClassLiteral* expr);

  enum {
    kFlagsIndex,
    kClassPropertiesTemplateIndex,
    kClassElementsTemplateIndex,
    kClassComputedPropertiesIndex,
    kPrototypePropertiesTemplateIndex,
    kPrototypeElementsTemplateIndex,
    kPrototypeComputedPropertiesIndex,
    kBoileplateLength  // last element
  };

  static const int kFullComputedEntrySize = 2;

 private:
  DECL_INT_ACCESSORS(flags)
};

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_LITERAL_OBJECTS_H_
