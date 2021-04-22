// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_LITERAL_OBJECTS_H_
#define V8_OBJECTS_LITERAL_OBJECTS_H_

#include "src/base/bit-field.h"
#include "src/objects/fixed-array.h"
#include "src/objects/struct.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

class ClassLiteral;

#include "torque-generated/src/objects/literal-objects-tq.inc"

// ObjectBoilerplateDescription is a list of properties consisting of name value
// pairs. In addition to the properties, it provides the projected number
// of properties in the backing store. This number includes properties with
// computed names that are not
// in the list.
// TODO(ishell): Don't derive from FixedArray as it already has its own map.
class ObjectBoilerplateDescription : public FixedArray {
 public:
  inline Object name(int index) const;
  inline Object name(IsolateRoot isolate, int index) const;

  inline Object value(int index) const;
  inline Object value(IsolateRoot isolate, int index) const;

  inline void set_key_value(int index, Object key, Object value);

  // The number of boilerplate properties.
  inline int size() const;

  // Number of boilerplate properties and properties with computed names.
  inline int backing_store_size() const;
  inline void set_backing_store_size(int backing_store_size);

  // Used to encode ObjectLiteral::Flags for nested object literals
  // Stored as the first element of the fixed array
  DECL_INT_ACCESSORS(flags)
  static const int kLiteralTypeOffset = 0;
  static const int kDescriptionStartIndex = 1;

  DECL_CAST(ObjectBoilerplateDescription)
  DECL_VERIFIER(ObjectBoilerplateDescription)
  DECL_PRINTER(ObjectBoilerplateDescription)

 private:
  inline bool has_number_of_properties() const;

  OBJECT_CONSTRUCTORS(ObjectBoilerplateDescription, FixedArray);
};

class ArrayBoilerplateDescription
    : public TorqueGeneratedArrayBoilerplateDescription<
          ArrayBoilerplateDescription, Struct> {
 public:
  inline ElementsKind elements_kind() const;
  inline void set_elements_kind(ElementsKind kind);

  inline bool is_empty() const;

  // Dispatched behavior.
  DECL_PRINTER(ArrayBoilerplateDescription)
  void BriefPrintDetails(std::ostream& os);

 private:
  TQ_OBJECT_CONSTRUCTORS(ArrayBoilerplateDescription)
};

class RegExpBoilerplateDescription
    : public TorqueGeneratedRegExpBoilerplateDescription<
          RegExpBoilerplateDescription, Struct> {
 public:
  // Dispatched behavior.
  DECL_PRINTER(RegExpBoilerplateDescription)
  void BriefPrintDetails(std::ostream& os);

 private:
  TQ_OBJECT_CONSTRUCTORS(RegExpBoilerplateDescription)
};

class ClassBoilerplate : public FixedArray {
 public:
  enum ValueKind { kData, kGetter, kSetter };

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

  template <typename LocalIsolate, typename Dictionary>
  static void AddToPropertiesTemplate(LocalIsolate* isolate,
                                      Handle<Dictionary> dictionary,
                                      Handle<Name> name, int key_index,
                                      ValueKind value_kind, Smi value);

  template <typename LocalIsolate>
  static void AddToElementsTemplate(LocalIsolate* isolate,
                                    Handle<NumberDictionary> dictionary,
                                    uint32_t key, int key_index,
                                    ValueKind value_kind, Smi value);

  template <typename LocalIsolate>
  static Handle<ClassBoilerplate> BuildClassBoilerplate(LocalIsolate* isolate,
                                                        ClassLiteral* expr);

  enum {
    kArgumentsCountIndex,
    kClassPropertiesTemplateIndex,
    kClassElementsTemplateIndex,
    kClassComputedPropertiesIndex,
    kPrototypePropertiesTemplateIndex,
    kPrototypeElementsTemplateIndex,
    kPrototypeComputedPropertiesIndex,
    kBoilerplateLength  // last element
  };

 private:
  DECL_INT_ACCESSORS(flags)

  OBJECT_CONSTRUCTORS(ClassBoilerplate, FixedArray);
};

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_LITERAL_OBJECTS_H_
