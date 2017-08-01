// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_MAP_H_
#define V8_OBJECTS_MAP_H_

#include "src/objects.h"

#include "src/globals.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

typedef std::vector<Handle<Map>> MapHandles;

// All heap objects have a Map that describes their structure.
//  A Map contains information about:
//  - Size information about the object
//  - How to iterate over an object (for garbage collection)
class Map : public HeapObject {
 public:
  // Instance size.
  // Size in bytes or kVariableSizeSentinel if instances do not have
  // a fixed size.
  inline int instance_size();
  inline void set_instance_size(int value);

  // Only to clear an unused byte, remove once byte is used.
  inline void clear_unused();

  // [inobject_properties_or_constructor_function_index]: Provides access
  // to the inobject properties in case of JSObject maps, or the constructor
  // function index in case of primitive maps.
  inline int inobject_properties_or_constructor_function_index();
  inline void set_inobject_properties_or_constructor_function_index(int value);
  // Count of properties allocated in the object (JSObject only).
  inline int GetInObjectProperties();
  inline void SetInObjectProperties(int value);
  // Index of the constructor function in the native context (primitives only),
  // or the special sentinel value to indicate that there is no object wrapper
  // for the primitive (i.e. in case of null or undefined).
  static const int kNoConstructorFunctionIndex = 0;
  inline int GetConstructorFunctionIndex();
  inline void SetConstructorFunctionIndex(int value);
  static MaybeHandle<JSFunction> GetConstructorFunction(
      Handle<Map> map, Handle<Context> native_context);

  // Retrieve interceptors.
  inline InterceptorInfo* GetNamedInterceptor();
  inline InterceptorInfo* GetIndexedInterceptor();

  // Instance type.
  inline InstanceType instance_type();
  inline void set_instance_type(InstanceType value);

  // Tells how many unused property fields are available in the
  // instance (only used for JSObject in fast mode).
  inline int unused_property_fields();
  inline void set_unused_property_fields(int value);

  // Bit field.
  inline byte bit_field() const;
  inline void set_bit_field(byte value);

  // Bit field 2.
  inline byte bit_field2() const;
  inline void set_bit_field2(byte value);

  // Bit field 3.
  inline uint32_t bit_field3() const;
  inline void set_bit_field3(uint32_t bits);

  class EnumLengthBits : public BitField<int, 0, kDescriptorIndexBitCount> {
  };  // NOLINT
  class NumberOfOwnDescriptorsBits
      : public BitField<int, kDescriptorIndexBitCount,
                        kDescriptorIndexBitCount> {};  // NOLINT
  STATIC_ASSERT(kDescriptorIndexBitCount + kDescriptorIndexBitCount == 20);
  class DictionaryMap : public BitField<bool, 20, 1> {};
  class OwnsDescriptors : public BitField<bool, 21, 1> {};
  class HasHiddenPrototype : public BitField<bool, 22, 1> {};
  class Deprecated : public BitField<bool, 23, 1> {};
  class IsUnstable : public BitField<bool, 24, 1> {};
  class IsMigrationTarget : public BitField<bool, 25, 1> {};
  class ImmutablePrototype : public BitField<bool, 26, 1> {};
  class NewTargetIsBase : public BitField<bool, 27, 1> {};
  // Bit 28 is free.

  // Keep this bit field at the very end for better code in
  // Builtins::kJSConstructStubGeneric stub.
  // This counter is used for in-object slack tracking.
  // The in-object slack tracking is considered enabled when the counter is
  // non zero. The counter only has a valid count for initial maps. For
  // transitioned maps only kNoSlackTracking has a meaning, namely that inobject
  // slack tracking already finished for the transition tree. Any other value
  // indicates that either inobject slack tracking is still in progress, or that
  // the map isn't part of the transition tree anymore.
  class ConstructionCounter : public BitField<int, 29, 3> {};
  static const int kSlackTrackingCounterStart = 7;
  static const int kSlackTrackingCounterEnd = 1;
  static const int kNoSlackTracking = 0;
  STATIC_ASSERT(kSlackTrackingCounterStart <= ConstructionCounter::kMax);

  // Inobject slack tracking is the way to reclaim unused inobject space.
  //
  // The instance size is initially determined by adding some slack to
  // expected_nof_properties (to allow for a few extra properties added
  // after the constructor). There is no guarantee that the extra space
  // will not be wasted.
  //
  // Here is the algorithm to reclaim the unused inobject space:
  // - Detect the first constructor call for this JSFunction.
  //   When it happens enter the "in progress" state: initialize construction
  //   counter in the initial_map.
  // - While the tracking is in progress initialize unused properties of a new
  //   object with one_pointer_filler_map instead of undefined_value (the "used"
  //   part is initialized with undefined_value as usual). This way they can
  //   be resized quickly and safely.
  // - Once enough objects have been created  compute the 'slack'
  //   (traverse the map transition tree starting from the
  //   initial_map and find the lowest value of unused_property_fields).
  // - Traverse the transition tree again and decrease the instance size
  //   of every map. Existing objects will resize automatically (they are
  //   filled with one_pointer_filler_map). All further allocations will
  //   use the adjusted instance size.
  // - SharedFunctionInfo's expected_nof_properties left unmodified since
  //   allocations made using different closures could actually create different
  //   kind of objects (see prototype inheritance pattern).
  //
  //  Important: inobject slack tracking is not attempted during the snapshot
  //  creation.

  static const int kGenerousAllocationCount =
      kSlackTrackingCounterStart - kSlackTrackingCounterEnd + 1;

  // Starts the tracking by initializing object constructions countdown counter.
  void StartInobjectSlackTracking();

  // True if the object constructions countdown counter is a range
  // [kSlackTrackingCounterEnd, kSlackTrackingCounterStart].
  inline bool IsInobjectSlackTrackingInProgress();

  // Does the tracking step.
  inline void InobjectSlackTrackingStep();

  // Completes inobject slack tracking for the transition tree starting at this
  // initial map.
  void CompleteInobjectSlackTracking();

  // Tells whether the object in the prototype property will be used
  // for instances created from this function.  If the prototype
  // property is set to a value that is not a JSObject, the prototype
  // property will not be used to create instances of the function.
  // See ECMA-262, 13.2.2.
  inline void set_non_instance_prototype(bool value);
  inline bool has_non_instance_prototype();

  // Tells whether the instance has a [[Construct]] internal method.
  // This property is implemented according to ES6, section 7.2.4.
  inline void set_is_constructor(bool value);
  inline bool is_constructor() const;

  // Tells whether the instance with this map has a hidden prototype.
  inline void set_has_hidden_prototype(bool value);
  inline bool has_hidden_prototype() const;

  // Records and queries whether the instance has a named interceptor.
  inline void set_has_named_interceptor();
  inline bool has_named_interceptor();

  // Records and queries whether the instance has an indexed interceptor.
  inline void set_has_indexed_interceptor();
  inline bool has_indexed_interceptor();

  // Tells whether the instance is undetectable.
  // An undetectable object is a special class of JSObject: 'typeof' operator
  // returns undefined, ToBoolean returns false. Otherwise it behaves like
  // a normal JS object.  It is useful for implementing undetectable
  // document.all in Firefox & Safari.
  // See https://bugzilla.mozilla.org/show_bug.cgi?id=248549.
  inline void set_is_undetectable();
  inline bool is_undetectable();

  // Tells whether the instance has a [[Call]] internal method.
  // This property is implemented according to ES6, section 7.2.3.
  inline void set_is_callable();
  inline bool is_callable() const;

  inline void set_new_target_is_base(bool value);
  inline bool new_target_is_base();
  inline void set_is_extensible(bool value);
  inline bool is_extensible();
  inline void set_is_prototype_map(bool value);
  inline bool is_prototype_map() const;

  inline void set_elements_kind(ElementsKind elements_kind);
  inline ElementsKind elements_kind();

  // Tells whether the instance has fast elements that are only Smis.
  inline bool has_fast_smi_elements();

  // Tells whether the instance has fast elements.
  inline bool has_fast_object_elements();
  inline bool has_fast_smi_or_object_elements();
  inline bool has_fast_double_elements();
  inline bool has_fast_elements();
  inline bool has_sloppy_arguments_elements();
  inline bool has_fast_sloppy_arguments_elements();
  inline bool has_fast_string_wrapper_elements();
  inline bool has_fixed_typed_array_elements();
  inline bool has_dictionary_elements();

  static bool IsValidElementsTransition(ElementsKind from_kind,
                                        ElementsKind to_kind);

  // Returns true if the current map doesn't have DICTIONARY_ELEMENTS but if a
  // map with DICTIONARY_ELEMENTS was found in the prototype chain.
  bool DictionaryElementsInPrototypeChainOnly();

  inline Map* ElementsTransitionMap();

  inline FixedArrayBase* GetInitialElements();

  // [raw_transitions]: Provides access to the transitions storage field.
  // Don't call set_raw_transitions() directly to overwrite transitions, use
  // the TransitionArray::ReplaceTransitions() wrapper instead!
  DECL_ACCESSORS(raw_transitions, Object)
  // [prototype_info]: Per-prototype metadata. Aliased with transitions
  // (which prototype maps don't have).
  DECL_ACCESSORS(prototype_info, Object)
  // PrototypeInfo is created lazily using this helper (which installs it on
  // the given prototype's map).
  static Handle<PrototypeInfo> GetOrCreatePrototypeInfo(
      Handle<JSObject> prototype, Isolate* isolate);
  static Handle<PrototypeInfo> GetOrCreatePrototypeInfo(
      Handle<Map> prototype_map, Isolate* isolate);
  inline bool should_be_fast_prototype_map() const;
  static void SetShouldBeFastPrototypeMap(Handle<Map> map, bool value,
                                          Isolate* isolate);

  // [prototype chain validity cell]: Associated with a prototype object,
  // stored in that object's map's PrototypeInfo, indicates that prototype
  // chains through this object are currently valid. The cell will be
  // invalidated and replaced when the prototype chain changes.
  static Handle<Cell> GetOrCreatePrototypeChainValidityCell(Handle<Map> map,
                                                            Isolate* isolate);
  static const int kPrototypeChainValid = 0;
  static const int kPrototypeChainInvalid = 1;

  // Return the map of the root of object's prototype chain.
  Map* GetPrototypeChainRootMap(Isolate* isolate);

  // Returns a WeakCell object containing given prototype. The cell is cached
  // in PrototypeInfo which is created lazily.
  static Handle<WeakCell> GetOrCreatePrototypeWeakCell(
      Handle<JSObject> prototype, Isolate* isolate);

  Map* FindRootMap();
  Map* FindFieldOwner(int descriptor);

  inline int GetInObjectPropertyOffset(int index);

  int NumberOfFields();

  // Returns true if transition to the given map requires special
  // synchronization with the concurrent marker.
  bool TransitionRequiresSynchronizationWithGC(Map* target);
  // Returns true if transition to the given map removes a tagged in-object
  // field.
  bool TransitionRemovesTaggedField(Map* target);
  // Returns true if transition to the given map replaces a tagged in-object
  // field with an untagged in-object field.
  bool TransitionChangesTaggedFieldToUntaggedField(Map* target);

  // TODO(ishell): candidate with JSObject::MigrateToMap().
  bool InstancesNeedRewriting(Map* target);
  bool InstancesNeedRewriting(Map* target, int target_number_of_fields,
                              int target_inobject, int target_unused,
                              int* old_number_of_fields);
  // TODO(ishell): moveit!
  static Handle<Map> GeneralizeAllFields(Handle<Map> map);
  MUST_USE_RESULT static Handle<FieldType> GeneralizeFieldType(
      Representation rep1, Handle<FieldType> type1, Representation rep2,
      Handle<FieldType> type2, Isolate* isolate);
  static void GeneralizeField(Handle<Map> map, int modify_index,
                              PropertyConstness new_constness,
                              Representation new_representation,
                              Handle<FieldType> new_field_type);
  // Returns true if |descriptor|'th property is a field that may be generalized
  // by just updating current map.
  static inline bool IsInplaceGeneralizableField(PropertyConstness constness,
                                                 Representation representation,
                                                 FieldType* field_type);

  static Handle<Map> ReconfigureProperty(Handle<Map> map, int modify_index,
                                         PropertyKind new_kind,
                                         PropertyAttributes new_attributes,
                                         Representation new_representation,
                                         Handle<FieldType> new_field_type);

  static Handle<Map> ReconfigureElementsKind(Handle<Map> map,
                                             ElementsKind new_elements_kind);

  static Handle<Map> PrepareForDataProperty(Handle<Map> old_map,
                                            int descriptor_number,
                                            PropertyConstness constness,
                                            Handle<Object> value);

  static Handle<Map> Normalize(Handle<Map> map, PropertyNormalizationMode mode,
                               const char* reason);

  // Tells whether the map is used for JSObjects in dictionary mode (ie
  // normalized objects, ie objects for which HasFastProperties returns false).
  // A map can never be used for both dictionary mode and fast mode JSObjects.
  // False by default and for HeapObjects that are not JSObjects.
  inline void set_dictionary_map(bool value);
  inline bool is_dictionary_map();

  // Tells whether the instance needs security checks when accessing its
  // properties.
  inline void set_is_access_check_needed(bool access_check_needed);
  inline bool is_access_check_needed();

  // Returns true if map has a non-empty stub code cache.
  inline bool has_code_cache();

  // [prototype]: implicit prototype object.
  DECL_ACCESSORS(prototype, Object)
  // TODO(jkummerow): make set_prototype private.
  static void SetPrototype(
      Handle<Map> map, Handle<Object> prototype,
      PrototypeOptimizationMode proto_mode = FAST_PROTOTYPE);

  // [constructor]: points back to the function or FunctionTemplateInfo
  // responsible for this map.
  // The field overlaps with the back pointer. All maps in a transition tree
  // have the same constructor, so maps with back pointers can walk the
  // back pointer chain until they find the map holding their constructor.
  // Returns null_value if there's neither a constructor function nor a
  // FunctionTemplateInfo available.
  DECL_ACCESSORS(constructor_or_backpointer, Object)
  inline Object* GetConstructor() const;
  inline FunctionTemplateInfo* GetFunctionTemplateInfo() const;
  inline void SetConstructor(Object* constructor,
                             WriteBarrierMode mode = UPDATE_WRITE_BARRIER);
  // [back pointer]: points back to the parent map from which a transition
  // leads to this map. The field overlaps with the constructor (see above).
  inline Object* GetBackPointer();
  inline void SetBackPointer(Object* value,
                             WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  // [instance descriptors]: describes the object.
  DECL_ACCESSORS(instance_descriptors, DescriptorArray)

  // [layout descriptor]: describes the object layout.
  DECL_ACCESSORS(layout_descriptor, LayoutDescriptor)
  // |layout descriptor| accessor which can be used from GC.
  inline LayoutDescriptor* layout_descriptor_gc_safe();
  inline bool HasFastPointerLayout() const;

  // |layout descriptor| accessor that is safe to call even when
  // FLAG_unbox_double_fields is disabled (in this case Map does not contain
  // |layout_descriptor| field at all).
  inline LayoutDescriptor* GetLayoutDescriptor();

  inline void UpdateDescriptors(DescriptorArray* descriptors,
                                LayoutDescriptor* layout_descriptor);
  inline void InitializeDescriptors(DescriptorArray* descriptors,
                                    LayoutDescriptor* layout_descriptor);

  // [stub cache]: contains stubs compiled for this map.
  DECL_ACCESSORS(code_cache, FixedArray)

  // [dependent code]: list of optimized codes that weakly embed this map.
  DECL_ACCESSORS(dependent_code, DependentCode)

  // [weak cell cache]: cache that stores a weak cell pointing to this map.
  DECL_ACCESSORS(weak_cell_cache, Object)

  inline PropertyDetails GetLastDescriptorDetails();

  inline int LastAdded();

  inline int NumberOfOwnDescriptors();
  inline void SetNumberOfOwnDescriptors(int number);

  inline Cell* RetrieveDescriptorsPointer();

  // Checks whether all properties are stored either in the map or on the object
  // (inobject, properties, or elements backing store), requiring no special
  // checks.
  bool OnlyHasSimpleProperties();
  inline int EnumLength();
  inline void SetEnumLength(int length);

  inline bool owns_descriptors();
  inline void set_owns_descriptors(bool owns_descriptors);
  inline void mark_unstable();
  inline bool is_stable();
  inline void set_migration_target(bool value);
  inline bool is_migration_target();
  inline void set_immutable_proto(bool value);
  inline bool is_immutable_proto();
  inline void set_construction_counter(int value);
  inline int construction_counter();
  inline void deprecate();
  inline bool is_deprecated();
  inline bool CanBeDeprecated();
  // Returns a non-deprecated version of the input. If the input was not
  // deprecated, it is directly returned. Otherwise, the non-deprecated version
  // is found by re-transitioning from the root of the transition tree using the
  // descriptor array of the map. Returns MaybeHandle<Map>() if no updated map
  // is found.
  static MaybeHandle<Map> TryUpdate(Handle<Map> map) WARN_UNUSED_RESULT;

  // Returns a non-deprecated version of the input. This method may deprecate
  // existing maps along the way if encodings conflict. Not for use while
  // gathering type feedback. Use TryUpdate in those cases instead.
  static Handle<Map> Update(Handle<Map> map);

  static inline Handle<Map> CopyInitialMap(Handle<Map> map);
  static Handle<Map> CopyInitialMap(Handle<Map> map, int instance_size,
                                    int in_object_properties,
                                    int unused_property_fields);
  static Handle<Map> CopyInitialMapNormalized(
      Handle<Map> map,
      PropertyNormalizationMode mode = CLEAR_INOBJECT_PROPERTIES);
  static Handle<Map> CopyDropDescriptors(Handle<Map> map);
  static Handle<Map> CopyInsertDescriptor(Handle<Map> map,
                                          Descriptor* descriptor,
                                          TransitionFlag flag);

  static Handle<Object> WrapFieldType(Handle<FieldType> type);
  static FieldType* UnwrapFieldType(Object* wrapped_type);

  MUST_USE_RESULT static MaybeHandle<Map> CopyWithField(
      Handle<Map> map, Handle<Name> name, Handle<FieldType> type,
      PropertyAttributes attributes, PropertyConstness constness,
      Representation representation, TransitionFlag flag);

  MUST_USE_RESULT static MaybeHandle<Map> CopyWithConstant(
      Handle<Map> map, Handle<Name> name, Handle<Object> constant,
      PropertyAttributes attributes, TransitionFlag flag);

  // Returns a new map with all transitions dropped from the given map and
  // the ElementsKind set.
  static Handle<Map> TransitionElementsTo(Handle<Map> map,
                                          ElementsKind to_kind);

  static Handle<Map> AsElementsKind(Handle<Map> map, ElementsKind kind);

  static Handle<Map> CopyAsElementsKind(Handle<Map> map, ElementsKind kind,
                                        TransitionFlag flag);

  static Handle<Map> AsLanguageMode(Handle<Map> initial_map,
                                    LanguageMode language_mode,
                                    FunctionKind kind);

  static Handle<Map> CopyForPreventExtensions(Handle<Map> map,
                                              PropertyAttributes attrs_to_add,
                                              Handle<Symbol> transition_marker,
                                              const char* reason);

  static Handle<Map> FixProxy(Handle<Map> map, InstanceType type, int size);

  // Maximal number of fast properties. Used to restrict the number of map
  // transitions to avoid an explosion in the number of maps for objects used as
  // dictionaries.
  inline bool TooManyFastProperties(StoreFromKeyed store_mode);
  static Handle<Map> TransitionToDataProperty(Handle<Map> map,
                                              Handle<Name> name,
                                              Handle<Object> value,
                                              PropertyAttributes attributes,
                                              PropertyConstness constness,
                                              StoreFromKeyed store_mode);
  static Handle<Map> TransitionToAccessorProperty(
      Isolate* isolate, Handle<Map> map, Handle<Name> name, int descriptor,
      Handle<Object> getter, Handle<Object> setter,
      PropertyAttributes attributes);
  static Handle<Map> ReconfigureExistingProperty(Handle<Map> map,
                                                 int descriptor,
                                                 PropertyKind kind,
                                                 PropertyAttributes attributes);

  inline void AppendDescriptor(Descriptor* desc);

  // Returns a copy of the map, prepared for inserting into the transition
  // tree (if the |map| owns descriptors then the new one will share
  // descriptors with |map|).
  static Handle<Map> CopyForTransition(Handle<Map> map, const char* reason);

  // Returns a copy of the map, with all transitions dropped from the
  // instance descriptors.
  static Handle<Map> Copy(Handle<Map> map, const char* reason);
  static Handle<Map> Create(Isolate* isolate, int inobject_properties);

  // Returns the next free property index (only valid for FAST MODE).
  int NextFreePropertyIndex();

  // Returns the number of properties described in instance_descriptors
  // filtering out properties with the specified attributes.
  int NumberOfDescribedProperties(DescriptorFlag which = OWN_DESCRIPTORS,
                                  PropertyFilter filter = ALL_PROPERTIES);

  DECLARE_CAST(Map)

  // Code cache operations.

  // Clears the code cache.
  inline void ClearCodeCache(Heap* heap);

  // Update code cache.
  static void UpdateCodeCache(Handle<Map> map, Handle<Name> name,
                              Handle<Code> code);

  // Extend the descriptor array of the map with the list of descriptors.
  // In case of duplicates, the latest descriptor is used.
  static void AppendCallbackDescriptors(Handle<Map> map,
                                        Handle<Object> descriptors);

  static inline int SlackForArraySize(int old_size, int size_limit);

  static void EnsureDescriptorSlack(Handle<Map> map, int slack);

  Code* LookupInCodeCache(Name* name, Code::Flags code);

  static Handle<Map> GetObjectCreateMap(Handle<HeapObject> prototype);

  // Computes a hash value for this map, to be used in HashTables and such.
  int Hash();

  // Returns the transitioned map for this map with the most generic
  // elements_kind that's found in |candidates|, or |nullptr| if no match is
  // found at all.
  Map* FindElementsKindTransitionedMap(MapHandles const& candidates);

  inline bool CanTransition();

  inline bool IsBooleanMap();
  inline bool IsPrimitiveMap();
  inline bool IsJSReceiverMap();
  inline bool IsJSObjectMap();
  inline bool IsJSArrayMap();
  inline bool IsJSFunctionMap();
  inline bool IsStringMap();
  inline bool IsJSProxyMap();
  inline bool IsModuleMap();
  inline bool IsJSGlobalProxyMap();
  inline bool IsJSGlobalObjectMap();
  inline bool IsJSTypedArrayMap();
  inline bool IsJSDataViewMap();

  inline bool IsSpecialReceiverMap();

  inline bool CanOmitMapChecks();

  static void AddDependentCode(Handle<Map> map,
                               DependentCode::DependencyGroup group,
                               Handle<Code> code);

  bool IsMapInArrayPrototypeChain();

  static Handle<WeakCell> WeakCellForMap(Handle<Map> map);

  // Dispatched behavior.
  DECLARE_PRINTER(Map)
  DECLARE_VERIFIER(Map)

#ifdef VERIFY_HEAP
  void DictionaryMapVerify();
  void VerifyOmittedMapChecks();
#endif

  inline int visitor_id();
  inline void set_visitor_id(int visitor_id);

  static Handle<Map> TransitionToPrototype(Handle<Map> map,
                                           Handle<Object> prototype,
                                           PrototypeOptimizationMode mode);

  static Handle<Map> TransitionToImmutableProto(Handle<Map> map);

  static const int kMaxPreAllocatedPropertyFields = 255;

  // Layout description.
  static const int kInstanceSizesOffset = HeapObject::kHeaderSize;
  static const int kInstanceAttributesOffset = kInstanceSizesOffset + kIntSize;
  static const int kBitField3Offset = kInstanceAttributesOffset + kIntSize;
  static const int kPrototypeOffset = kBitField3Offset + kPointerSize;
  static const int kConstructorOrBackPointerOffset =
      kPrototypeOffset + kPointerSize;
  // When there is only one transition, it is stored directly in this field;
  // otherwise a transition array is used.
  // For prototype maps, this slot is used to store this map's PrototypeInfo
  // struct.
  static const int kTransitionsOrPrototypeInfoOffset =
      kConstructorOrBackPointerOffset + kPointerSize;
  static const int kDescriptorsOffset =
      kTransitionsOrPrototypeInfoOffset + kPointerSize;
#if V8_DOUBLE_FIELDS_UNBOXING
  static const int kLayoutDescriptorOffset = kDescriptorsOffset + kPointerSize;
  static const int kCodeCacheOffset = kLayoutDescriptorOffset + kPointerSize;
#else
  static const int kLayoutDescriptorOffset = 1;  // Must not be ever accessed.
  static const int kCodeCacheOffset = kDescriptorsOffset + kPointerSize;
#endif
  static const int kDependentCodeOffset = kCodeCacheOffset + kPointerSize;
  static const int kWeakCellCacheOffset = kDependentCodeOffset + kPointerSize;
  static const int kSize = kWeakCellCacheOffset + kPointerSize;

  // Layout of pointer fields. Heap iteration code relies on them
  // being continuously allocated.
  static const int kPointerFieldsBeginOffset = Map::kPrototypeOffset;
  static const int kPointerFieldsEndOffset = kSize;

  // Byte offsets within kInstanceSizesOffset.
  static const int kInstanceSizeOffset = kInstanceSizesOffset + 0;
  static const int kInObjectPropertiesOrConstructorFunctionIndexByte = 1;
  static const int kInObjectPropertiesOrConstructorFunctionIndexOffset =
      kInstanceSizesOffset + kInObjectPropertiesOrConstructorFunctionIndexByte;
  // Note there is one byte available for use here.
  static const int kUnusedByte = 2;
  static const int kUnusedOffset = kInstanceSizesOffset + kUnusedByte;
  static const int kVisitorIdByte = 3;
  static const int kVisitorIdOffset = kInstanceSizesOffset + kVisitorIdByte;

// Byte offsets within kInstanceAttributesOffset attributes.
#if V8_TARGET_LITTLE_ENDIAN
  // Order instance type and bit field together such that they can be loaded
  // together as a 16-bit word with instance type in the lower 8 bits regardless
  // of endianess. Also provide endian-independent offset to that 16-bit word.
  static const int kInstanceTypeOffset = kInstanceAttributesOffset + 0;
  static const int kBitFieldOffset = kInstanceAttributesOffset + 1;
#else
  static const int kBitFieldOffset = kInstanceAttributesOffset + 0;
  static const int kInstanceTypeOffset = kInstanceAttributesOffset + 1;
#endif
  static const int kInstanceTypeAndBitFieldOffset =
      kInstanceAttributesOffset + 0;
  static const int kBitField2Offset = kInstanceAttributesOffset + 2;
  static const int kUnusedPropertyFieldsOffset = kInstanceAttributesOffset + 3;

  STATIC_ASSERT(kInstanceTypeAndBitFieldOffset ==
                Internals::kMapInstanceTypeAndBitFieldOffset);

  // Bit positions for bit field.
  static const int kHasNonInstancePrototype = 0;
  static const int kIsCallable = 1;
  static const int kHasNamedInterceptor = 2;
  static const int kHasIndexedInterceptor = 3;
  static const int kIsUndetectable = 4;
  static const int kIsAccessCheckNeeded = 5;
  static const int kIsConstructor = 6;
  // Bit 7 is free.

  // Bit positions for bit field 2
  static const int kIsExtensible = 0;
  // Bit 1 is free.
  class IsPrototypeMapBits : public BitField<bool, 2, 1> {};
  class ElementsKindBits : public BitField<ElementsKind, 3, 5> {};

  // Derived values from bit field 2
  static const int8_t kMaximumBitField2FastElementValue =
      static_cast<int8_t>((FAST_ELEMENTS + 1)
                          << Map::ElementsKindBits::kShift) -
      1;
  static const int8_t kMaximumBitField2FastSmiElementValue =
      static_cast<int8_t>((FAST_SMI_ELEMENTS + 1)
                          << Map::ElementsKindBits::kShift) -
      1;
  static const int8_t kMaximumBitField2FastHoleyElementValue =
      static_cast<int8_t>((FAST_HOLEY_ELEMENTS + 1)
                          << Map::ElementsKindBits::kShift) -
      1;
  static const int8_t kMaximumBitField2FastHoleySmiElementValue =
      static_cast<int8_t>((FAST_HOLEY_SMI_ELEMENTS + 1)
                          << Map::ElementsKindBits::kShift) -
      1;

  typedef FixedBodyDescriptor<kPointerFieldsBeginOffset,
                              kPointerFieldsEndOffset, kSize>
      BodyDescriptor;

  // Compares this map to another to see if they describe equivalent objects.
  // If |mode| is set to CLEAR_INOBJECT_PROPERTIES, |other| is treated as if
  // it had exactly zero inobject properties.
  // The "shared" flags of both this map and |other| are ignored.
  bool EquivalentToForNormalization(Map* other, PropertyNormalizationMode mode);

  // Returns true if given field is unboxed double.
  inline bool IsUnboxedDoubleField(FieldIndex index);

#if V8_TRACE_MAPS
  static void TraceTransition(const char* what, Map* from, Map* to, Name* name);
  static void TraceAllTransitions(Map* map);
#endif

  static inline Handle<Map> AddMissingTransitionsForTesting(
      Handle<Map> split_map, Handle<DescriptorArray> descriptors,
      Handle<LayoutDescriptor> full_layout_descriptor);

  // Fires when the layout of an object with a leaf map changes.
  // This includes adding transitions to the leaf map or changing
  // the descriptor array.
  inline void NotifyLeafMapLayoutChange();

 private:
  // Returns the map that this (root) map transitions to if its elements_kind
  // is changed to |elements_kind|, or |nullptr| if no such map is cached yet.
  Map* LookupElementsTransitionMap(ElementsKind elements_kind);

  // Tries to replay property transitions starting from this (root) map using
  // the descriptor array of the |map|. The |root_map| is expected to have
  // proper elements kind and therefore elements kinds transitions are not
  // taken by this function. Returns |nullptr| if matching transition map is
  // not found.
  Map* TryReplayPropertyTransitions(Map* map);

  static void ConnectTransition(Handle<Map> parent, Handle<Map> child,
                                Handle<Name> name, SimpleTransitionFlag flag);

  bool EquivalentToForTransition(Map* other);
  bool EquivalentToForElementsKindTransition(Map* other);
  static Handle<Map> RawCopy(Handle<Map> map, int instance_size);
  static Handle<Map> ShareDescriptor(Handle<Map> map,
                                     Handle<DescriptorArray> descriptors,
                                     Descriptor* descriptor);
  static Handle<Map> AddMissingTransitions(
      Handle<Map> map, Handle<DescriptorArray> descriptors,
      Handle<LayoutDescriptor> full_layout_descriptor);
  static void InstallDescriptors(
      Handle<Map> parent_map, Handle<Map> child_map, int new_descriptor,
      Handle<DescriptorArray> descriptors,
      Handle<LayoutDescriptor> full_layout_descriptor);
  static Handle<Map> CopyAddDescriptor(Handle<Map> map, Descriptor* descriptor,
                                       TransitionFlag flag);
  static Handle<Map> CopyReplaceDescriptors(
      Handle<Map> map, Handle<DescriptorArray> descriptors,
      Handle<LayoutDescriptor> layout_descriptor, TransitionFlag flag,
      MaybeHandle<Name> maybe_name, const char* reason,
      SimpleTransitionFlag simple_flag);

  static Handle<Map> CopyReplaceDescriptor(Handle<Map> map,
                                           Handle<DescriptorArray> descriptors,
                                           Descriptor* descriptor, int index,
                                           TransitionFlag flag);
  static MUST_USE_RESULT MaybeHandle<Map> TryReconfigureExistingProperty(
      Handle<Map> map, int descriptor, PropertyKind kind,
      PropertyAttributes attributes, const char** reason);

  static Handle<Map> CopyNormalized(Handle<Map> map,
                                    PropertyNormalizationMode mode);

  // TODO(ishell): Move to MapUpdater.
  static Handle<Map> CopyGeneralizeAllFields(
      Handle<Map> map, ElementsKind elements_kind, int modify_index,
      PropertyKind kind, PropertyAttributes attributes, const char* reason);

  void DeprecateTransitionTree();

  void ReplaceDescriptors(DescriptorArray* new_descriptors,
                          LayoutDescriptor* new_layout_descriptor);

  // Update field type of the given descriptor to new representation and new
  // type. The type must be prepared for storing in descriptor array:
  // it must be either a simple type or a map wrapped in a weak cell.
  void UpdateFieldType(int descriptor_number, Handle<Name> name,
                       PropertyConstness new_constness,
                       Representation new_representation,
                       Handle<Object> new_wrapped_type);

  // TODO(ishell): Move to MapUpdater.
  void PrintReconfiguration(FILE* file, int modify_index, PropertyKind kind,
                            PropertyAttributes attributes);
  // TODO(ishell): Move to MapUpdater.
  void PrintGeneralization(FILE* file, const char* reason, int modify_index,
                           int split, int descriptors, bool constant_to_field,
                           Representation old_representation,
                           Representation new_representation,
                           MaybeHandle<FieldType> old_field_type,
                           MaybeHandle<Object> old_value,
                           MaybeHandle<FieldType> new_field_type,
                           MaybeHandle<Object> new_value);
  static const int kFastPropertiesSoftLimit = 12;
  static const int kMaxFastProperties = 128;

  friend class MapUpdater;

  DISALLOW_IMPLICIT_CONSTRUCTORS(Map);
};

// The cache for maps used by normalized (dictionary mode) objects.
// Such maps do not have property descriptors, so a typical program
// needs very limited number of distinct normalized maps.
class NormalizedMapCache : public FixedArray {
 public:
  static Handle<NormalizedMapCache> New(Isolate* isolate);

  MUST_USE_RESULT MaybeHandle<Map> Get(Handle<Map> fast_map,
                                       PropertyNormalizationMode mode);
  void Set(Handle<Map> fast_map, Handle<Map> normalized_map);

  void Clear();

  DECLARE_CAST(NormalizedMapCache)

  static inline bool IsNormalizedMapCache(const HeapObject* obj);

  DECLARE_VERIFIER(NormalizedMapCache)
 private:
  static const int kEntries = 64;

  static inline int GetIndex(Handle<Map> map);

  // The following declarations hide base class methods.
  Object* get(int index);
  void set(int index, Object* value);
};

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_MAP_H_
