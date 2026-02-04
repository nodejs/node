// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_LITERAL_OBJECTS_H_
#define V8_OBJECTS_LITERAL_OBJECTS_H_

#include "src/base/bit-field.h"
#include "src/objects/fixed-array.h"
#include "src/objects/objects-body-descriptors.h"
#include "src/objects/struct.h"
#include "src/objects/trusted-pointer.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

class ClassLiteral;
class StructBodyDescriptor;

#include "torque-generated/src/objects/literal-objects-tq.inc"

class ObjectBoilerplateDescriptionShape final : public AllStatic {
 public:
  using ElementT = Object;
  using CompressionScheme = V8HeapCompressionScheme;
  static constexpr RootIndex kMapRootIndex =
      RootIndex::kObjectBoilerplateDescriptionMap;
  static constexpr bool kLengthEqualsCapacity = true;

  V8_ARRAY_EXTRA_FIELDS({
    TaggedMember<Smi> backing_store_size_;
    TaggedMember<Smi> flags_;
  });
};

// ObjectBoilerplateDescription is a list of properties consisting of name
// value pairs. In addition to the properties, it provides the projected number
// of properties in the backing store. This number includes properties with
// computed names that are not in the list.
class ObjectBoilerplateDescription
    : public TaggedArrayBase<ObjectBoilerplateDescription,
                             ObjectBoilerplateDescriptionShape> {
  using Super = TaggedArrayBase<ObjectBoilerplateDescription,
                                ObjectBoilerplateDescriptionShape>;
 public:
  using Shape = ObjectBoilerplateDescriptionShape;

  template <class IsolateT>
  static inline Handle<ObjectBoilerplateDescription> New(
      IsolateT* isolate, int boilerplate, int all_properties, int index_keys,
      bool has_seen_proto, AllocationType allocation = AllocationType::kYoung);

  // ObjectLiteral::Flags for nested object literals.
  inline int flags() const;
  inline void set_flags(int value);

  // Number of boilerplate properties and properties with computed names.
  inline int backing_store_size() const;
  inline void set_backing_store_size(int backing_store_size);

  inline int boilerplate_properties_count() const;

  inline Tagged<Object> name(int index) const;
  inline Tagged<Object> value(int index) const;

  inline void set_key_value(int index, Tagged<Object> key,
                            Tagged<Object> value);
  inline void set_value(int index, Tagged<Object> value);

  DECL_VERIFIER(ObjectBoilerplateDescription)
  DECL_PRINTER(ObjectBoilerplateDescription)

  class BodyDescriptor;

 private:
  using TaggedArrayBase::get;
  using TaggedArrayBase::set;

  static constexpr int kElementsPerEntry = 2;
  static constexpr int NameIndex(int i) { return i * kElementsPerEntry; }
  static constexpr int ValueIndex(int i) { return i * kElementsPerEntry + 1; }
};

V8_OBJECT class ArrayBoilerplateDescription : public StructLayout {
 public:
  inline ElementsKind elements_kind() const;
  inline void set_elements_kind(ElementsKind kind);

  inline bool is_empty() const;

  // Dispatched behavior.
  DECL_PRINTER(ArrayBoilerplateDescription)
  DECL_VERIFIER(ArrayBoilerplateDescription)
  void BriefPrintDetails(std::ostream& os);

  using BodyDescriptor = StructBodyDescriptor;

  inline Tagged<Smi> flags() const;
  inline void set_flags(Tagged<Smi> value,
                        WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline Tagged<FixedArrayBase> constant_elements() const;
  inline void set_constant_elements(
      Tagged<FixedArrayBase> value,
      WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

 private:
  friend class Factory;
  friend class TorqueGeneratedArrayBoilerplateDescriptionAsserts;
  friend class V8HeapExplorer;

  TaggedMember<Smi> flags_;
  TaggedMember<FixedArrayBase> constant_elements_;
} V8_OBJECT_END;

V8_OBJECT class RegExpBoilerplateDescription : public StructLayout {
 public:
  // Dispatched behavior.
  void BriefPrintDetails(std::ostream& os);

  inline Tagged<RegExpData> data(IsolateForSandbox isolate) const;
  inline void set_data(Tagged<RegExpData> value,
                       WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline Tagged<String> source() const;
  inline void set_source(Tagged<String> value,
                         WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline int flags() const;
  inline void set_flags(int value);

  DECL_PRINTER(RegExpBoilerplateDescription)
  DECL_VERIFIER(RegExpBoilerplateDescription)

 private:
  friend class Factory;
  friend class TorqueGeneratedRegExpBoilerplateDescriptionAsserts;
  friend class CodeStubAssembler;
  friend class ConstructorBuiltinsAssembler;
  friend struct ObjectTraits<RegExpBoilerplateDescription>;

  TrustedPointerMember<RegExpData, kRegExpDataIndirectPointerTag> data_;
  TaggedMember<String> source_;
  TaggedMember<Smi> flags_;
} V8_OBJECT_END;

template <>
struct ObjectTraits<RegExpBoilerplateDescription> {
  using BodyDescriptor = StackedBodyDescriptor<
      FixedBodyDescriptor<offsetof(RegExpBoilerplateDescription, source_),
                          sizeof(RegExpBoilerplateDescription),
                          sizeof(RegExpBoilerplateDescription)>,
      WithStrongTrustedPointer<offsetof(RegExpBoilerplateDescription, data_),
                               kRegExpDataIndirectPointerTag>>;
};

V8_OBJECT class ClassBoilerplate : public StructLayout {
 public:
  enum ValueKind { kData, kGetter, kSetter, kAutoAccessor };

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

  template <typename IsolateT>
  static Handle<ClassBoilerplate> New(
      IsolateT* isolate, ClassLiteral* expr,
      AllocationType allocation = AllocationType::kYoung);

  inline int arguments_count() const;
  inline void set_arguments_count(int value);

  inline Tagged<Object> static_properties_template() const;
  inline void set_static_properties_template(
      Tagged<Object> value, WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline Tagged<Object> static_elements_template() const;
  inline void set_static_elements_template(
      Tagged<Object> value, WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline Tagged<FixedArray> static_computed_properties() const;
  inline void set_static_computed_properties(
      Tagged<FixedArray> value, WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline Tagged<Object> instance_properties_template() const;
  inline void set_instance_properties_template(
      Tagged<Object> value, WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline Tagged<Object> instance_elements_template() const;
  inline void set_instance_elements_template(
      Tagged<Object> value, WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline Tagged<FixedArray> instance_computed_properties() const;
  inline void set_instance_computed_properties(
      Tagged<FixedArray> value, WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  template <typename IsolateT, typename Dictionary>
  static void AddToPropertiesTemplate(IsolateT* isolate,
                                      Handle<Dictionary> dictionary,
                                      Handle<Name> name, int key_index,
                                      ValueKind value_kind, Tagged<Smi> value);

  template <typename IsolateT>
  static void AddToElementsTemplate(IsolateT* isolate,
                                    Handle<NumberDictionary> dictionary,
                                    uint32_t key, int key_index,
                                    ValueKind value_kind, Tagged<Smi> value);

  DECL_PRINTER(ClassBoilerplate)
  DECL_VERIFIER(ClassBoilerplate)

  using BodyDescriptor = StructBodyDescriptor;

 private:
  friend class Factory;
  friend class TorqueGeneratedClassBoilerplateAsserts;

  TaggedMember<Smi> arguments_count_;
  TaggedMember<Object> static_properties_template_;
  TaggedMember<Object> static_elements_template_;
  TaggedMember<FixedArray> static_computed_properties_;
  TaggedMember<Object> instance_properties_template_;
  TaggedMember<Object> instance_elements_template_;
  TaggedMember<FixedArray> instance_computed_properties_;
} V8_OBJECT_END;

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_LITERAL_OBJECTS_H_
