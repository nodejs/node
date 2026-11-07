// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_LITERAL_OBJECTS_H_
#define V8_OBJECTS_LITERAL_OBJECTS_H_

#include "src/base/bit-field.h"
#include "src/objects/contexts.h"
#include "src/objects/feedback-cell.h"
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

V8_OBJECT class PrototypeSharedClosureInfo : public Struct {
 public:
  inline PrototypeSharedClosureInfo(
      const AllocationWitness& witness,
      Tagged<ObjectBoilerplateDescription> boilerplate_description,
      Tagged<ClosureFeedbackCellArray> closure_feedback_cell_array,
      Tagged<Context> context);

  inline Tagged<ObjectBoilerplateDescription> boilerplate_description() const;
  inline Tagged<ClosureFeedbackCellArray> closure_feedback_cell_array() const;
  inline Tagged<Context> context() const;

  DECL_PRINTER(PrototypeSharedClosureInfo)
  DECL_VERIFIER(PrototypeSharedClosureInfo)

  using BodyDescriptor = StructBodyDescriptor;

 public:
  const TaggedMember<ObjectBoilerplateDescription> boilerplate_description_;
  const TaggedMember<ClosureFeedbackCellArray> closure_feedback_cell_array_;
  const TaggedMember<Context> context_;
} V8_OBJECT_END;

// ObjectBoilerplateDescription is a list of properties consisting of name
// value pairs. In addition to the properties, it provides the projected number
// of properties in the backing store. This number includes properties with
// computed names that are not in the list.
V8_OBJECT class ObjectBoilerplateDescription
    : public TaggedArrayBase<ObjectBoilerplateDescription, Object> {
  using Super = TaggedArrayBase<ObjectBoilerplateDescription, Object>;

 public:
  static constexpr RootIndex kMapRootIndex =
      RootIndex::kObjectBoilerplateDescriptionMap;
  using KeyT = UnionOf<InternalizedString, Number>;

  template <class IsolateT>
  static inline Handle<ObjectBoilerplateDescription> New(
      IsolateT* isolate, uint32_t boilerplate, uint32_t backing_store_size,
      AllocationType allocation = AllocationType::kYoung);

  // ObjectLiteral::Flags for nested object literals.
  inline int flags() const;
  inline void set_flags(int value);

  // Number of boilerplate properties and properties with computed names.
  inline int backing_store_size() const;
  inline void set_backing_store_size(int backing_store_size);

  inline int boilerplate_properties_count() const;

  inline Tagged<KeyT> name(int index) const;
  inline Tagged<Object> value(int index) const;

  inline void set_key_value(int index, Tagged<KeyT> key, Tagged<Object> value);
  inline void set_value(int index, Tagged<Object> value);

  DECL_VERIFIER(ObjectBoilerplateDescription)
  DECL_PRINTER(ObjectBoilerplateDescription)

  class BodyDescriptor;

  static constexpr uint32_t kLengthOffset = sizeof(HeapObject);
  static constexpr uint32_t kHeaderSize =
      kLengthOffset + 3 * (TAGGED_SIZE_8_BYTES ? kTaggedSize : kApiInt32Size);

 private:
  using TaggedArrayBase::get;
  using TaggedArrayBase::set;

  static constexpr int kElementsPerEntry = 2;
  static constexpr int NameIndex(int i) { return i * kElementsPerEntry; }
  static constexpr int ValueIndex(int i) { return i * kElementsPerEntry + 1; }

 public:
  // length_ / optional_padding_ live in FixedArrayBase.
  TaggedMember<Smi> backing_store_size_;
  TaggedMember<Smi> flags_;
  V8_TQ_TAIL_NAME(raw_entries);
  V8_TQ_TAIL_LENGTH(length);
  FLEXIBLE_ARRAY_MEMBER(typename Super::ElementMemberT, objects);
} V8_OBJECT_END;

V8_OBJECT class ArrayBoilerplateDescription : public Struct {
 public:
  inline ArrayBoilerplateDescription(const AllocationWitness& witness,
                                     ElementsKind elements_kind,
                                     Tagged<FixedArrayBase> constant_values);

  inline ElementsKind elements_kind() const;

  inline bool is_empty() const;

  // Dispatched behavior.
  DECL_PRINTER(ArrayBoilerplateDescription)
  DECL_VERIFIER(ArrayBoilerplateDescription)
  void BriefPrintDetails(std::ostream& os);

  using BodyDescriptor = StructBodyDescriptor;

  inline Tagged<Smi> flags() const;
  inline Tagged<FixedArrayBase> constant_elements() const;

 private:
  friend class Factory;
  friend class TorqueGeneratedArrayBoilerplateDescriptionAsserts;
  friend class V8HeapExplorer;

  const TaggedMember<Smi> flags_;
  const TaggedMember<FixedArrayBase> constant_elements_;
} V8_OBJECT_END;

V8_OBJECT class RegExpBoilerplateDescription : public Struct {
 public:
  inline RegExpBoilerplateDescription(const AllocationWitness& witness,
                                      Tagged<RegExpData> data,
                                      Tagged<Smi> flags);

  // Dispatched behavior.
  void BriefPrintDetails(std::ostream& os);

  inline Tagged<RegExpData> data(IsolateForSandbox isolate) const;
  inline int flags() const;

  DECL_PRINTER(RegExpBoilerplateDescription)
  DECL_VERIFIER(RegExpBoilerplateDescription)

 private:
  friend class Factory;
  friend class TorqueGeneratedRegExpBoilerplateDescriptionAsserts;
  friend class CodeStubAssembler;
  friend class ConstructorBuiltinsAssembler;
  friend struct ObjectTraits<RegExpBoilerplateDescription>;

  const TrustedPointerMember<RegExpData, kRegExpDataIndirectPointerTag> data_;
  const TaggedMember<Smi> flags_ V8_TQ_TYPE(SmiTagged<JSRegExpFlags>);
} V8_OBJECT_END;

template <>
struct ObjectTraits<RegExpBoilerplateDescription> {
  using BodyDescriptor = StackedBodyDescriptor<
      FixedBodyDescriptor<offsetof(RegExpBoilerplateDescription, flags_),
                          sizeof(RegExpBoilerplateDescription),
                          sizeof(RegExpBoilerplateDescription)>,
      WithStrongTrustedPointer<offsetof(RegExpBoilerplateDescription, data_),
                               kRegExpDataIndirectPointerTag>>;
};

V8_OBJECT class ClassBoilerplate : public Struct {
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

  inline ClassBoilerplate(const AllocationWitness& witness, int arguments_count,
                          Tagged<Object> static_properties_template,
                          Tagged<Object> static_elements_template,
                          Tagged<FixedArray> static_computed_properties,
                          Tagged<Object> instance_properties_template,
                          Tagged<Object> instance_elements_template,
                          Tagged<FixedArray> instance_computed_properties);

  template <typename IsolateT>
  static Handle<ClassBoilerplate> New(
      IsolateT* isolate, ClassLiteral* expr,
      AllocationType allocation = AllocationType::kYoung);

  inline int arguments_count() const;
  inline Tagged<Object> static_properties_template() const;
  inline Tagged<Object> static_elements_template() const;
  inline Tagged<FixedArray> static_computed_properties() const;
  inline Tagged<Object> instance_properties_template() const;
  inline Tagged<Object> instance_elements_template() const;
  inline Tagged<FixedArray> instance_computed_properties() const;

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

  const TaggedMember<Smi> arguments_count_;
  const TaggedMember<Object> static_properties_template_;
  const TaggedMember<Object> static_elements_template_;
  const TaggedMember<FixedArray> static_computed_properties_;
  const TaggedMember<Object> instance_properties_template_;
  const TaggedMember<Object> instance_elements_template_;
  const TaggedMember<FixedArray> instance_computed_properties_;
} V8_OBJECT_END;

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_LITERAL_OBJECTS_H_
