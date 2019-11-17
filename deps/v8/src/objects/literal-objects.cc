// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/objects/literal-objects.h"

#include "src/ast/ast.h"
#include "src/builtins/accessors.h"
#include "src/execution/isolate.h"
#include "src/heap/factory.h"
#include "src/objects/hash-table-inl.h"
#include "src/objects/literal-objects-inl.h"
#include "src/objects/objects-inl.h"
#include "src/objects/smi.h"
#include "src/objects/struct-inl.h"

namespace v8 {
namespace internal {

namespace {

inline int EncodeComputedEntry(ClassBoilerplate::ValueKind value_kind,
                               unsigned key_index) {
  using Flags = ClassBoilerplate::ComputedEntryFlags;
  int flags = Flags::ValueKindBits::encode(value_kind) |
              Flags::KeyIndexBits::encode(key_index);
  return flags;
}

void AddToDescriptorArrayTemplate(
    Isolate* isolate, Handle<DescriptorArray> descriptor_array_template,
    Handle<Name> name, ClassBoilerplate::ValueKind value_kind,
    Handle<Object> value) {
  InternalIndex entry = descriptor_array_template->Search(
      *name, descriptor_array_template->number_of_descriptors());
  // TODO(ishell): deduplicate properties at AST level, this will allow us to
  // avoid creation of closures that will be overwritten anyway.
  if (entry.is_not_found()) {
    // Entry not found, add new one.
    Descriptor d;
    if (value_kind == ClassBoilerplate::kData) {
      d = Descriptor::DataConstant(name, value, DONT_ENUM);
    } else {
      DCHECK(value_kind == ClassBoilerplate::kGetter ||
             value_kind == ClassBoilerplate::kSetter);
      Handle<AccessorPair> pair = isolate->factory()->NewAccessorPair();
      pair->set(value_kind == ClassBoilerplate::kGetter ? ACCESSOR_GETTER
                                                        : ACCESSOR_SETTER,
                *value);
      d = Descriptor::AccessorConstant(name, pair, DONT_ENUM);
    }
    descriptor_array_template->Append(&d);

  } else {
    // Entry found, update it.
    int sorted_index = descriptor_array_template->GetDetails(entry).pointer();
    if (value_kind == ClassBoilerplate::kData) {
      Descriptor d = Descriptor::DataConstant(name, value, DONT_ENUM);
      d.SetSortedKeyIndex(sorted_index);
      descriptor_array_template->Set(entry, &d);
    } else {
      DCHECK(value_kind == ClassBoilerplate::kGetter ||
             value_kind == ClassBoilerplate::kSetter);
      Object raw_accessor = descriptor_array_template->GetStrongValue(entry);
      AccessorPair pair;
      if (raw_accessor.IsAccessorPair()) {
        pair = AccessorPair::cast(raw_accessor);
      } else {
        Handle<AccessorPair> new_pair = isolate->factory()->NewAccessorPair();
        Descriptor d = Descriptor::AccessorConstant(name, new_pair, DONT_ENUM);
        d.SetSortedKeyIndex(sorted_index);
        descriptor_array_template->Set(entry, &d);
        pair = *new_pair;
      }
      pair.set(value_kind == ClassBoilerplate::kGetter ? ACCESSOR_GETTER
                                                       : ACCESSOR_SETTER,
               *value);
    }
  }
}

Handle<NameDictionary> DictionaryAddNoUpdateNextEnumerationIndex(
    Isolate* isolate, Handle<NameDictionary> dictionary, Handle<Name> name,
    Handle<Object> value, PropertyDetails details, int* entry_out = nullptr) {
  return NameDictionary::AddNoUpdateNextEnumerationIndex(
      isolate, dictionary, name, value, details, entry_out);
}

Handle<NumberDictionary> DictionaryAddNoUpdateNextEnumerationIndex(
    Isolate* isolate, Handle<NumberDictionary> dictionary, uint32_t element,
    Handle<Object> value, PropertyDetails details, int* entry_out = nullptr) {
  // NumberDictionary does not maintain the enumeration order, so it's
  // a normal Add().
  return NumberDictionary::Add(isolate, dictionary, element, value, details,
                               entry_out);
}

void DictionaryUpdateMaxNumberKey(Handle<NameDictionary> dictionary,
                                  Handle<Name> name) {
  // No-op for name dictionaries.
}

void DictionaryUpdateMaxNumberKey(Handle<NumberDictionary> dictionary,
                                  uint32_t element) {
  dictionary->UpdateMaxNumberKey(element, Handle<JSObject>());
  dictionary->set_requires_slow_elements();
}

constexpr int ComputeEnumerationIndex(int value_index) {
  // We "shift" value indices to ensure that the enumeration index for the value
  // will not overlap with minimum properties set for both class and prototype
  // objects.
  return value_index + Max(ClassBoilerplate::kMinimumClassPropertiesCount,
                           ClassBoilerplate::kMinimumPrototypePropertiesCount);
}

inline int GetExistingValueIndex(Object value) {
  return value.IsSmi() ? Smi::ToInt(value) : -1;
}

template <typename Dictionary, typename Key>
void AddToDictionaryTemplate(Isolate* isolate, Handle<Dictionary> dictionary,
                             Key key, int key_index,
                             ClassBoilerplate::ValueKind value_kind,
                             Object value) {
  int entry = dictionary->FindEntry(isolate, key);

  if (entry == kNotFound) {
    // Entry not found, add new one.
    const bool is_elements_dictionary =
        std::is_same<Dictionary, NumberDictionary>::value;
    STATIC_ASSERT(is_elements_dictionary !=
                  (std::is_same<Dictionary, NameDictionary>::value));
    int enum_order =
        is_elements_dictionary ? 0 : ComputeEnumerationIndex(key_index);
    Handle<Object> value_handle;
    PropertyDetails details(
        value_kind != ClassBoilerplate::kData ? kAccessor : kData, DONT_ENUM,
        PropertyCellType::kNoCell, enum_order);

    if (value_kind == ClassBoilerplate::kData) {
      value_handle = handle(value, isolate);
    } else {
      AccessorComponent component = value_kind == ClassBoilerplate::kGetter
                                        ? ACCESSOR_GETTER
                                        : ACCESSOR_SETTER;
      Handle<AccessorPair> pair(isolate->factory()->NewAccessorPair());
      pair->set(component, value);
      value_handle = pair;
    }

    // Add value to the dictionary without updating next enumeration index.
    Handle<Dictionary> dict = DictionaryAddNoUpdateNextEnumerationIndex(
        isolate, dictionary, key, value_handle, details, &entry);
    // It is crucial to avoid dictionary reallocations because it may remove
    // potential gaps in enumeration indices values that are necessary for
    // inserting computed properties into right places in the enumeration order.
    CHECK_EQ(*dict, *dictionary);

    DictionaryUpdateMaxNumberKey(dictionary, key);

  } else {
    // Entry found, update it.
    int enum_order = dictionary->DetailsAt(entry).dictionary_index();
    Object existing_value = dictionary->ValueAt(entry);
    if (value_kind == ClassBoilerplate::kData) {
      // Computed value is a normal method.
      if (existing_value.IsAccessorPair()) {
        AccessorPair current_pair = AccessorPair::cast(existing_value);

        int existing_getter_index =
            GetExistingValueIndex(current_pair.getter());
        int existing_setter_index =
            GetExistingValueIndex(current_pair.setter());
        // At least one of the accessors must already be defined.
        DCHECK(existing_getter_index >= 0 || existing_setter_index >= 0);
        if (existing_getter_index < key_index &&
            existing_setter_index < key_index) {
          // Either both getter and setter were defined before the computed
          // method or just one of them was defined before while the other one
          // was not defined yet, so overwrite property to kData.
          PropertyDetails details(kData, DONT_ENUM, PropertyCellType::kNoCell,
                                  enum_order);
          dictionary->DetailsAtPut(isolate, entry, details);
          dictionary->ValueAtPut(entry, value);

        } else {
          // The data property was defined "between" accessors so the one that
          // was overwritten has to be cleared.
          if (existing_getter_index < key_index) {
            DCHECK_LT(key_index, existing_setter_index);
            // Getter was defined and it was done before the computed method
            // and then it was overwritten by the current computed method which
            // in turn was later overwritten by the setter method. So we clear
            // the getter.
            current_pair.set_getter(*isolate->factory()->null_value());

          } else if (existing_setter_index < key_index) {
            DCHECK_LT(key_index, existing_getter_index);
            // Setter was defined and it was done before the computed method
            // and then it was overwritten by the current computed method which
            // in turn was later overwritten by the getter method. So we clear
            // the setter.
            current_pair.set_setter(*isolate->factory()->null_value());
          }
        }
      } else {
        // Overwrite existing value if it was defined before the computed one
        // (AccessorInfo "length" property is always defined before).
        DCHECK_IMPLIES(!existing_value.IsSmi(),
                       existing_value.IsAccessorInfo());
        DCHECK_IMPLIES(!existing_value.IsSmi(),
                       AccessorInfo::cast(existing_value).name() ==
                           *isolate->factory()->length_string());
        if (!existing_value.IsSmi() || Smi::ToInt(existing_value) < key_index) {
          PropertyDetails details(kData, DONT_ENUM, PropertyCellType::kNoCell,
                                  enum_order);
          dictionary->DetailsAtPut(isolate, entry, details);
          dictionary->ValueAtPut(entry, value);
        }
      }
    } else {
      AccessorComponent component = value_kind == ClassBoilerplate::kGetter
                                        ? ACCESSOR_GETTER
                                        : ACCESSOR_SETTER;
      if (existing_value.IsAccessorPair()) {
        // Update respective component of existing AccessorPair.
        AccessorPair current_pair = AccessorPair::cast(existing_value);

        int existing_component_index =
            GetExistingValueIndex(current_pair.get(component));
        if (existing_component_index < key_index) {
          current_pair.set(component, value);
        }

      } else {
        // Overwrite existing value with new AccessorPair.
        Handle<AccessorPair> pair(isolate->factory()->NewAccessorPair());
        pair->set(component, value);
        PropertyDetails details(kAccessor, DONT_ENUM, PropertyCellType::kNoCell,
                                enum_order);
        dictionary->DetailsAtPut(isolate, entry, details);
        dictionary->ValueAtPut(entry, *pair);
      }
    }
  }
}

}  // namespace

// Helper class that eases building of a properties, elements and computed
// properties templates.
class ObjectDescriptor {
 public:
  void IncComputedCount() { ++computed_count_; }
  void IncPropertiesCount() { ++property_count_; }
  void IncElementsCount() { ++element_count_; }

  explicit ObjectDescriptor(int property_slack)
      : property_slack_(property_slack) {}

  bool HasDictionaryProperties() const {
    return computed_count_ > 0 ||
           (property_count_ + property_slack_) > kMaxNumberOfDescriptors;
  }

  Handle<Object> properties_template() const {
    return HasDictionaryProperties()
               ? Handle<Object>::cast(properties_dictionary_template_)
               : Handle<Object>::cast(descriptor_array_template_);
  }

  Handle<NumberDictionary> elements_template() const {
    return elements_dictionary_template_;
  }

  Handle<FixedArray> computed_properties() const {
    return computed_properties_;
  }

  void CreateTemplates(Isolate* isolate) {
    Factory* factory = isolate->factory();
    descriptor_array_template_ = factory->empty_descriptor_array();
    properties_dictionary_template_ = factory->empty_property_dictionary();
    if (property_count_ || computed_count_ || property_slack_) {
      if (HasDictionaryProperties()) {
        properties_dictionary_template_ = NameDictionary::New(
            isolate, property_count_ + computed_count_ + property_slack_);
      } else {
        descriptor_array_template_ = DescriptorArray::Allocate(
            isolate, 0, property_count_ + property_slack_);
      }
    }
    elements_dictionary_template_ =
        element_count_ || computed_count_
            ? NumberDictionary::New(isolate, element_count_ + computed_count_)
            : factory->empty_slow_element_dictionary();

    computed_properties_ =
        computed_count_
            ? factory->NewFixedArray(computed_count_ *
                                     ClassBoilerplate::kFullComputedEntrySize)
            : factory->empty_fixed_array();

    temp_handle_ = handle(Smi::kZero, isolate);
  }

  void AddConstant(Isolate* isolate, Handle<Name> name, Handle<Object> value,
                   PropertyAttributes attribs) {
    bool is_accessor = value->IsAccessorInfo();
    DCHECK(!value->IsAccessorPair());
    if (HasDictionaryProperties()) {
      PropertyKind kind = is_accessor ? i::kAccessor : i::kData;
      PropertyDetails details(kind, attribs, PropertyCellType::kNoCell,
                              next_enumeration_index_++);
      properties_dictionary_template_ =
          DictionaryAddNoUpdateNextEnumerationIndex(
              isolate, properties_dictionary_template_, name, value, details);
    } else {
      Descriptor d = is_accessor
                         ? Descriptor::AccessorConstant(name, value, attribs)
                         : Descriptor::DataConstant(name, value, attribs);
      descriptor_array_template_->Append(&d);
    }
  }

  void AddNamedProperty(Isolate* isolate, Handle<Name> name,
                        ClassBoilerplate::ValueKind value_kind,
                        int value_index) {
    Smi value = Smi::FromInt(value_index);
    if (HasDictionaryProperties()) {
      UpdateNextEnumerationIndex(value_index);
      AddToDictionaryTemplate(isolate, properties_dictionary_template_, name,
                              value_index, value_kind, value);
    } else {
      *temp_handle_.location() = value.ptr();
      AddToDescriptorArrayTemplate(isolate, descriptor_array_template_, name,
                                   value_kind, temp_handle_);
    }
  }

  void AddIndexedProperty(Isolate* isolate, uint32_t element,
                          ClassBoilerplate::ValueKind value_kind,
                          int value_index) {
    Smi value = Smi::FromInt(value_index);
    AddToDictionaryTemplate(isolate, elements_dictionary_template_, element,
                            value_index, value_kind, value);
  }

  void AddComputed(ClassBoilerplate::ValueKind value_kind, int key_index) {
    int value_index = key_index + 1;
    UpdateNextEnumerationIndex(value_index);

    int flags = EncodeComputedEntry(value_kind, key_index);
    computed_properties_->set(current_computed_index_++, Smi::FromInt(flags));
  }

  void UpdateNextEnumerationIndex(int value_index) {
    int next_index = ComputeEnumerationIndex(value_index);
    DCHECK_LT(next_enumeration_index_, next_index);
    next_enumeration_index_ = next_index;
  }

  void Finalize(Isolate* isolate) {
    if (HasDictionaryProperties()) {
      properties_dictionary_template_->SetNextEnumerationIndex(
          next_enumeration_index_);
      computed_properties_ = FixedArray::ShrinkOrEmpty(
          isolate, computed_properties_, current_computed_index_);
    } else {
      DCHECK(descriptor_array_template_->IsSortedNoDuplicates());
    }
  }

 private:
  const int property_slack_;
  int property_count_ = 0;
  int next_enumeration_index_ = PropertyDetails::kInitialIndex;
  int element_count_ = 0;
  int computed_count_ = 0;
  int current_computed_index_ = 0;

  Handle<DescriptorArray> descriptor_array_template_;
  Handle<NameDictionary> properties_dictionary_template_;
  Handle<NumberDictionary> elements_dictionary_template_;
  Handle<FixedArray> computed_properties_;
  // This temporary handle is used for storing to descriptor array.
  Handle<Object> temp_handle_;
};

void ClassBoilerplate::AddToPropertiesTemplate(
    Isolate* isolate, Handle<NameDictionary> dictionary, Handle<Name> name,
    int key_index, ClassBoilerplate::ValueKind value_kind, Object value) {
  AddToDictionaryTemplate(isolate, dictionary, name, key_index, value_kind,
                          value);
}

void ClassBoilerplate::AddToElementsTemplate(
    Isolate* isolate, Handle<NumberDictionary> dictionary, uint32_t key,
    int key_index, ClassBoilerplate::ValueKind value_kind, Object value) {
  AddToDictionaryTemplate(isolate, dictionary, key, key_index, value_kind,
                          value);
}

Handle<ClassBoilerplate> ClassBoilerplate::BuildClassBoilerplate(
    Isolate* isolate, ClassLiteral* expr) {
  // Create a non-caching handle scope to ensure that the temporary handle used
  // by ObjectDescriptor for passing Smis around does not corrupt handle cache
  // in CanonicalHandleScope.
  HandleScope scope(isolate);
  Factory* factory = isolate->factory();
  ObjectDescriptor static_desc(kMinimumClassPropertiesCount);
  ObjectDescriptor instance_desc(kMinimumPrototypePropertiesCount);

  for (int i = 0; i < expr->public_members()->length(); i++) {
    ClassLiteral::Property* property = expr->public_members()->at(i);
    ObjectDescriptor& desc =
        property->is_static() ? static_desc : instance_desc;
    if (property->is_computed_name()) {
      desc.IncComputedCount();
    } else {
      if (property->key()->AsLiteral()->IsPropertyName()) {
        desc.IncPropertiesCount();
      } else {
        desc.IncElementsCount();
      }
    }
  }

  //
  // Initialize class object template.
  //
  static_desc.CreateTemplates(isolate);
  STATIC_ASSERT(JSFunction::kLengthDescriptorIndex == 0);
  {
    // Add length_accessor.
    PropertyAttributes attribs =
        static_cast<PropertyAttributes>(DONT_ENUM | READ_ONLY);
    static_desc.AddConstant(isolate, factory->length_string(),
                            factory->function_length_accessor(), attribs);
  }
  {
    // Add prototype_accessor.
    PropertyAttributes attribs =
        static_cast<PropertyAttributes>(DONT_ENUM | DONT_DELETE | READ_ONLY);
    static_desc.AddConstant(isolate, factory->prototype_string(),
                            factory->function_prototype_accessor(), attribs);
  }
  if (FunctionLiteral::NeedsHomeObject(expr->constructor())) {
    PropertyAttributes attribs =
        static_cast<PropertyAttributes>(DONT_ENUM | DONT_DELETE | READ_ONLY);
    Handle<Object> value(
        Smi::FromInt(ClassBoilerplate::kPrototypeArgumentIndex), isolate);
    static_desc.AddConstant(isolate, factory->home_object_symbol(), value,
                            attribs);
  }
  {
    Handle<ClassPositions> class_positions = factory->NewClassPositions(
        expr->start_position(), expr->end_position());
    static_desc.AddConstant(isolate, factory->class_positions_symbol(),
                            class_positions, DONT_ENUM);
  }

  //
  // Initialize prototype object template.
  //
  instance_desc.CreateTemplates(isolate);
  {
    Handle<Object> value(
        Smi::FromInt(ClassBoilerplate::kConstructorArgumentIndex), isolate);
    instance_desc.AddConstant(isolate, factory->constructor_string(), value,
                              DONT_ENUM);
  }

  //
  // Fill in class boilerplate.
  //
  int dynamic_argument_index = ClassBoilerplate::kFirstDynamicArgumentIndex;

  for (int i = 0; i < expr->public_members()->length(); i++) {
    ClassLiteral::Property* property = expr->public_members()->at(i);
    ClassBoilerplate::ValueKind value_kind;
    switch (property->kind()) {
      case ClassLiteral::Property::METHOD:
        value_kind = ClassBoilerplate::kData;
        break;
      case ClassLiteral::Property::GETTER:
        value_kind = ClassBoilerplate::kGetter;
        break;
      case ClassLiteral::Property::SETTER:
        value_kind = ClassBoilerplate::kSetter;
        break;
      case ClassLiteral::Property::FIELD:
        DCHECK_IMPLIES(property->is_computed_name(), !property->is_private());
        if (property->is_computed_name()) {
          ++dynamic_argument_index;
        }
        continue;
    }

    ObjectDescriptor& desc =
        property->is_static() ? static_desc : instance_desc;
    if (property->is_computed_name()) {
      int computed_name_index = dynamic_argument_index;
      dynamic_argument_index += 2;  // Computed name and value indices.
      desc.AddComputed(value_kind, computed_name_index);
      continue;
    }
    int value_index = dynamic_argument_index++;

    Literal* key_literal = property->key()->AsLiteral();
    uint32_t index;
    if (key_literal->AsArrayIndex(&index)) {
      desc.AddIndexedProperty(isolate, index, value_kind, value_index);

    } else {
      Handle<String> name = key_literal->AsRawPropertyName()->string();
      DCHECK(name->IsInternalizedString());
      desc.AddNamedProperty(isolate, name, value_kind, value_index);
    }
  }

  // Add name accessor to the class object if necessary.
  bool install_class_name_accessor = false;
  if (!expr->has_name_static_property()) {
    if (static_desc.HasDictionaryProperties()) {
      // Install class name accessor if necessary during class literal
      // instantiation.
      install_class_name_accessor = true;
    } else {
      // Set class name accessor if the "name" method was not added yet.
      PropertyAttributes attribs =
          static_cast<PropertyAttributes>(DONT_ENUM | READ_ONLY);
      static_desc.AddConstant(isolate, factory->name_string(),
                              factory->function_name_accessor(), attribs);
    }
  }

  static_desc.Finalize(isolate);
  instance_desc.Finalize(isolate);

  Handle<ClassBoilerplate> class_boilerplate =
      Handle<ClassBoilerplate>::cast(factory->NewFixedArray(kBoileplateLength));

  class_boilerplate->set_flags(0);
  class_boilerplate->set_install_class_name_accessor(
      install_class_name_accessor);
  class_boilerplate->set_arguments_count(dynamic_argument_index);

  class_boilerplate->set_static_properties_template(
      *static_desc.properties_template());
  class_boilerplate->set_static_elements_template(
      *static_desc.elements_template());
  class_boilerplate->set_static_computed_properties(
      *static_desc.computed_properties());

  class_boilerplate->set_instance_properties_template(
      *instance_desc.properties_template());
  class_boilerplate->set_instance_elements_template(
      *instance_desc.elements_template());
  class_boilerplate->set_instance_computed_properties(
      *instance_desc.computed_properties());

  return scope.CloseAndEscape(class_boilerplate);
}

}  // namespace internal
}  // namespace v8
