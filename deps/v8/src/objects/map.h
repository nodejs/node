// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_MAP_H_
#define V8_OBJECTS_MAP_H_

#include "src/common/globals.h"
#include "src/objects/code.h"
#include "src/objects/heap-object.h"
#include "src/objects/objects.h"
#include "torque-generated/field-offsets-tq.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

enum InstanceType : uint16_t;

#define DATA_ONLY_VISITOR_ID_LIST(V) \
  V(BigInt)                          \
  V(ByteArray)                       \
  V(DataObject)                      \
  V(FixedDoubleArray)                \
  V(SeqOneByteString)                \
  V(SeqTwoByteString)

#define POINTER_VISITOR_ID_LIST(V)     \
  V(AllocationSite)                    \
  V(BytecodeArray)                     \
  V(Cell)                              \
  V(Code)                              \
  V(CodeDataContainer)                 \
  V(ConsString)                        \
  V(Context)                           \
  V(DataHandler)                       \
  V(DescriptorArray)                   \
  V(EmbedderDataArray)                 \
  V(EphemeronHashTable)                \
  V(FeedbackCell)                      \
  V(FeedbackVector)                    \
  V(FixedArray)                        \
  V(FreeSpace)                         \
  V(JSApiObject)                       \
  V(JSArrayBuffer)                     \
  V(JSDataView)                        \
  V(JSFunction)                        \
  V(JSObject)                          \
  V(JSObjectFast)                      \
  V(JSTypedArray)                      \
  V(JSWeakRef)                         \
  V(JSWeakCollection)                  \
  V(Map)                               \
  V(NativeContext)                     \
  V(Oddball)                           \
  V(PreparseData)                      \
  V(PropertyArray)                     \
  V(PropertyCell)                      \
  V(PrototypeInfo)                     \
  V(SharedFunctionInfo)                \
  V(ShortcutCandidate)                 \
  V(SlicedString)                      \
  V(SmallOrderedHashMap)               \
  V(SmallOrderedHashSet)               \
  V(SmallOrderedNameDictionary)        \
  V(Struct)                            \
  V(Symbol)                            \
  V(ThinString)                        \
  V(TransitionArray)                   \
  V(UncompiledDataWithoutPreparseData) \
  V(UncompiledDataWithPreparseData)    \
  V(WasmCapiFunctionData)              \
  V(WasmInstanceObject)                \
  V(WeakArray)                         \
  V(WeakCell)

// Objects with the same visitor id are processed in the same way by
// the heap visitors. The visitor ids for data only objects must precede
// other visitor ids. We rely on kDataOnlyVisitorIdCount for quick check
// of whether an object contains only data or may contain pointers.
enum VisitorId {
#define VISITOR_ID_ENUM_DECL(id) kVisit##id,
  DATA_ONLY_VISITOR_ID_LIST(VISITOR_ID_ENUM_DECL) kDataOnlyVisitorIdCount,
  POINTER_VISITOR_ID_LIST(VISITOR_ID_ENUM_DECL)
#undef VISITOR_ID_ENUM_DECL
      kVisitorIdCount
};

enum class ObjectFields {
  kDataOnly,
  kMaybePointers,
};

using MapHandles = std::vector<Handle<Map>>;

// All heap objects have a Map that describes their structure.
//  A Map contains information about:
//  - Size information about the object
//  - How to iterate over an object (for garbage collection)
//
// Map layout:
// +---------------+---------------------------------------------+
// |   _ Type _    | _ Description _                             |
// +---------------+---------------------------------------------+
// | TaggedPointer | map - Always a pointer to the MetaMap root  |
// +---------------+---------------------------------------------+
// | Int           | The first int field                         |
//  `---+----------+---------------------------------------------+
//      | Byte     | [instance_size]                             |
//      +----------+---------------------------------------------+
//      | Byte     | If Map for a primitive type:                |
//      |          |   native context index for constructor fn   |
//      |          | If Map for an Object type:                  |
//      |          |   inobject properties start offset in words |
//      +----------+---------------------------------------------+
//      | Byte     | [used_or_unused_instance_size_in_words]     |
//      |          | For JSObject in fast mode this byte encodes |
//      |          | the size of the object that includes only   |
//      |          | the used property fields or the slack size  |
//      |          | in properties backing store.                |
//      +----------+---------------------------------------------+
//      | Byte     | [visitor_id]                                |
// +----+----------+---------------------------------------------+
// | Int           | The second int field                        |
//  `---+----------+---------------------------------------------+
//      | Short    | [instance_type]                             |
//      +----------+---------------------------------------------+
//      | Byte     | [bit_field]                                 |
//      |          |   - has_non_instance_prototype (bit 0)      |
//      |          |   - is_callable (bit 1)                     |
//      |          |   - has_named_interceptor (bit 2)           |
//      |          |   - has_indexed_interceptor (bit 3)         |
//      |          |   - is_undetectable (bit 4)                 |
//      |          |   - is_access_check_needed (bit 5)          |
//      |          |   - is_constructor (bit 6)                  |
//      |          |   - has_prototype_slot (bit 7)              |
//      +----------+---------------------------------------------+
//      | Byte     | [bit_field2]                                |
//      |          |   - is_extensible (bit 0)                   |
//      |          |   - is_prototype_map (bit 1)                |
//      |          |   - has_hidden_prototype (bit 2)            |
//      |          |   - elements_kind (bits 3..7)               |
// +----+----------+---------------------------------------------+
// | Int           | [bit_field3]                                |
// |               |   - enum_length (bit 0..9)                  |
// |               |   - number_of_own_descriptors (bit 10..19)  |
// |               |   - is_dictionary_map (bit 20)              |
// |               |   - owns_descriptors (bit 21)               |
// |               |   - is_in_retained_map_list (bit 22)        |
// |               |   - is_deprecated (bit 23)                  |
// |               |   - is_unstable (bit 24)                    |
// |               |   - is_migration_target (bit 25)            |
// |               |   - is_immutable_proto (bit 26)             |
// |               |   - new_target_is_base (bit 27)             |
// |               |   - may_have_interesting_symbols (bit 28)   |
// |               |   - construction_counter (bit 29..31)       |
// |               |                                             |
// +*************************************************************+
// | Int           | On systems with 64bit pointer types, there  |
// |               | is an unused 32bits after bit_field3        |
// +*************************************************************+
// | TaggedPointer | [prototype]                                 |
// +---------------+---------------------------------------------+
// | TaggedPointer | [constructor_or_backpointer]                |
// +---------------+---------------------------------------------+
// | TaggedPointer | [instance_descriptors]                      |
// +*************************************************************+
// ! TaggedPointer ! [layout_descriptors]                        !
// !               ! Field is only present if compile-time flag  !
// !               ! FLAG_unbox_double_fields is enabled         !
// !               ! (basically on 64 bit architectures)         !
// +*************************************************************+
// | TaggedPointer | [dependent_code]                            |
// +---------------+---------------------------------------------+
// | TaggedPointer | [prototype_validity_cell]                   |
// +---------------+---------------------------------------------+
// | TaggedPointer | If Map is a prototype map:                  |
// |               |   [prototype_info]                          |
// |               | Else:                                       |
// |               |   [raw_transitions]                         |
// +---------------+---------------------------------------------+

class Map : public HeapObject {
 public:
  // Instance size.
  // Size in bytes or kVariableSizeSentinel if instances do not have
  // a fixed size.
  DECL_INT_ACCESSORS(instance_size)
  // Size in words or kVariableSizeSentinel if instances do not have
  // a fixed size.
  DECL_INT_ACCESSORS(instance_size_in_words)

  // [inobject_properties_start_or_constructor_function_index]:
  // Provides access to the inobject properties start offset in words in case of
  // JSObject maps, or the constructor function index in case of primitive maps.
  DECL_INT_ACCESSORS(inobject_properties_start_or_constructor_function_index)

  // Get/set the in-object property area start offset in words in the object.
  inline int GetInObjectPropertiesStartInWords() const;
  inline void SetInObjectPropertiesStartInWords(int value);
  // Count of properties allocated in the object (JSObject only).
  inline int GetInObjectProperties() const;
  // Index of the constructor function in the native context (primitives only),
  // or the special sentinel value to indicate that there is no object wrapper
  // for the primitive (i.e. in case of null or undefined).
  static const int kNoConstructorFunctionIndex = 0;
  inline int GetConstructorFunctionIndex() const;
  inline void SetConstructorFunctionIndex(int value);
  static MaybeHandle<JSFunction> GetConstructorFunction(
      Handle<Map> map, Handle<Context> native_context);

  // Retrieve interceptors.
  inline InterceptorInfo GetNamedInterceptor();
  inline InterceptorInfo GetIndexedInterceptor();

  // Instance type.
  DECL_PRIMITIVE_ACCESSORS(instance_type, InstanceType)

  // Returns the size of the used in-object area including object header
  // (only used for JSObject in fast mode, for the other kinds of objects it
  // is equal to the instance size).
  inline int UsedInstanceSize() const;

  // Tells how many unused property fields (in-object or out-of object) are
  // available in the instance (only used for JSObject in fast mode).
  inline int UnusedPropertyFields() const;
  // Tells how many unused in-object property words are present.
  inline int UnusedInObjectProperties() const;
  // Updates the counters tracking unused fields in the object.
  inline void SetInObjectUnusedPropertyFields(int unused_property_fields);
  // Updates the counters tracking unused fields in the property array.
  inline void SetOutOfObjectUnusedPropertyFields(int unused_property_fields);
  inline void CopyUnusedPropertyFields(Map map);
  inline void CopyUnusedPropertyFieldsAdjustedForInstanceSize(Map map);
  inline void AccountAddedPropertyField();
  inline void AccountAddedOutOfObjectPropertyField(
      int unused_in_property_array);

  //
  // Bit field.
  //
  DECL_PRIMITIVE_ACCESSORS(bit_field, byte)
  // Atomic accessors, used for whitelisting legitimate concurrent accesses.
  DECL_PRIMITIVE_ACCESSORS(relaxed_bit_field, byte)

// Bit positions for |bit_field|.
#define MAP_BIT_FIELD_FIELDS(V, _)          \
  V(HasNonInstancePrototypeBit, bool, 1, _) \
  V(IsCallableBit, bool, 1, _)              \
  V(HasNamedInterceptorBit, bool, 1, _)     \
  V(HasIndexedInterceptorBit, bool, 1, _)   \
  V(IsUndetectableBit, bool, 1, _)          \
  V(IsAccessCheckNeededBit, bool, 1, _)     \
  V(IsConstructorBit, bool, 1, _)           \
  V(HasPrototypeSlotBit, bool, 1, _)

  DEFINE_BIT_FIELDS(MAP_BIT_FIELD_FIELDS)
#undef MAP_BIT_FIELD_FIELDS

  //
  // Bit field 2.
  //
  DECL_PRIMITIVE_ACCESSORS(bit_field2, byte)

// Bit positions for |bit_field2|.
#define MAP_BIT_FIELD2_FIELDS(V, _)    \
  V(IsExtensibleBit, bool, 1, _)       \
  V(IsPrototypeMapBit, bool, 1, _)     \
  V(HasHiddenPrototypeBit, bool, 1, _) \
  V(ElementsKindBits, ElementsKind, 5, _)

  DEFINE_BIT_FIELDS(MAP_BIT_FIELD2_FIELDS)
#undef MAP_BIT_FIELD2_FIELDS

  //
  // Bit field 3.
  //
  DECL_PRIMITIVE_ACCESSORS(bit_field3, uint32_t)

  // Clear uninitialized padding space. This ensures that the snapshot content
  // is deterministic. Depending on the V8 build mode there could be no padding.
  V8_INLINE void clear_padding();

// Bit positions for |bit_field3|.
#define MAP_BIT_FIELD3_FIELDS(V, _)                               \
  V(EnumLengthBits, int, kDescriptorIndexBitCount, _)             \
  V(NumberOfOwnDescriptorsBits, int, kDescriptorIndexBitCount, _) \
  V(IsDictionaryMapBit, bool, 1, _)                               \
  V(OwnsDescriptorsBit, bool, 1, _)                               \
  V(IsInRetainedMapListBit, bool, 1, _)                           \
  V(IsDeprecatedBit, bool, 1, _)                                  \
  V(IsUnstableBit, bool, 1, _)                                    \
  V(IsMigrationTargetBit, bool, 1, _)                             \
  V(IsImmutablePrototypeBit, bool, 1, _)                          \
  V(NewTargetIsBaseBit, bool, 1, _)                               \
  V(MayHaveInterestingSymbolsBit, bool, 1, _)                     \
  V(ConstructionCounterBits, int, 3, _)

  DEFINE_BIT_FIELDS(MAP_BIT_FIELD3_FIELDS)
#undef MAP_BIT_FIELD3_FIELDS

  STATIC_ASSERT(NumberOfOwnDescriptorsBits::kMax >= kMaxNumberOfDescriptors);

  static const int kSlackTrackingCounterStart = 7;
  static const int kSlackTrackingCounterEnd = 1;
  static const int kNoSlackTracking = 0;
  STATIC_ASSERT(kSlackTrackingCounterStart <= ConstructionCounterBits::kMax);

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
  inline bool IsInobjectSlackTrackingInProgress() const;

  // Does the tracking step.
  inline void InobjectSlackTrackingStep(Isolate* isolate);

  // Computes inobject slack for the transition tree starting at this initial
  // map.
  int ComputeMinObjectSlack(Isolate* isolate);
  inline int InstanceSizeFromSlack(int slack) const;

  // Completes inobject slack tracking for the transition tree starting at this
  // initial map.
  V8_EXPORT_PRIVATE void CompleteInobjectSlackTracking(Isolate* isolate);

  // Tells whether the object in the prototype property will be used
  // for instances created from this function.  If the prototype
  // property is set to a value that is not a JSObject, the prototype
  // property will not be used to create instances of the function.
  // See ECMA-262, 13.2.2.
  DECL_BOOLEAN_ACCESSORS(has_non_instance_prototype)

  // Tells whether the instance has a [[Construct]] internal method.
  // This property is implemented according to ES6, section 7.2.4.
  DECL_BOOLEAN_ACCESSORS(is_constructor)

  // Tells whether the instance with this map may have properties for
  // interesting symbols on it.
  // An "interesting symbol" is one for which Name::IsInterestingSymbol()
  // returns true, i.e. a well-known symbol like @@toStringTag.
  DECL_BOOLEAN_ACCESSORS(may_have_interesting_symbols)

  DECL_BOOLEAN_ACCESSORS(has_prototype_slot)

  // Tells whether the instance with this map has a hidden prototype.
  DECL_BOOLEAN_ACCESSORS(has_hidden_prototype)

  // Records and queries whether the instance has a named interceptor.
  DECL_BOOLEAN_ACCESSORS(has_named_interceptor)

  // Records and queries whether the instance has an indexed interceptor.
  DECL_BOOLEAN_ACCESSORS(has_indexed_interceptor)

  // Tells whether the instance is undetectable.
  // An undetectable object is a special class of JSObject: 'typeof' operator
  // returns undefined, ToBoolean returns false. Otherwise it behaves like
  // a normal JS object.  It is useful for implementing undetectable
  // document.all in Firefox & Safari.
  // See https://bugzilla.mozilla.org/show_bug.cgi?id=248549.
  DECL_BOOLEAN_ACCESSORS(is_undetectable)

  // Tells whether the instance has a [[Call]] internal method.
  // This property is implemented according to ES6, section 7.2.3.
  DECL_BOOLEAN_ACCESSORS(is_callable)

  DECL_BOOLEAN_ACCESSORS(new_target_is_base)
  DECL_BOOLEAN_ACCESSORS(is_extensible)
  DECL_BOOLEAN_ACCESSORS(is_prototype_map)
  inline bool is_abandoned_prototype_map() const;

  // Whether the instance has been added to the retained map list by
  // Heap::AddRetainedMap.
  DECL_BOOLEAN_ACCESSORS(is_in_retained_map_list)

  DECL_PRIMITIVE_ACCESSORS(elements_kind, ElementsKind)

  // Tells whether the instance has fast elements that are only Smis.
  inline bool has_fast_smi_elements() const;

  // Tells whether the instance has fast elements.
  inline bool has_fast_object_elements() const;
  inline bool has_fast_smi_or_object_elements() const;
  inline bool has_fast_double_elements() const;
  inline bool has_fast_elements() const;
  inline bool has_sloppy_arguments_elements() const;
  inline bool has_fast_sloppy_arguments_elements() const;
  inline bool has_fast_string_wrapper_elements() const;
  inline bool has_typed_array_elements() const;
  inline bool has_dictionary_elements() const;
  inline bool has_frozen_or_sealed_elements() const;
  inline bool has_sealed_elements() const;
  inline bool has_frozen_elements() const;

  // Returns true if the current map doesn't have DICTIONARY_ELEMENTS but if a
  // map with DICTIONARY_ELEMENTS was found in the prototype chain.
  bool DictionaryElementsInPrototypeChainOnly(Isolate* isolate);

  inline Map ElementsTransitionMap();

  inline FixedArrayBase GetInitialElements() const;

  // [raw_transitions]: Provides access to the transitions storage field.
  // Don't call set_raw_transitions() directly to overwrite transitions, use
  // the TransitionArray::ReplaceTransitions() wrapper instead!
  DECL_ACCESSORS(raw_transitions, MaybeObject)
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
  // stored in that object's map, indicates that prototype chains through this
  // object are currently valid. The cell will be invalidated and replaced when
  // the prototype chain changes. When there's nothing to guard (for example,
  // when direct prototype is null or Proxy) this function returns Smi with
  // |kPrototypeChainValid| sentinel value.
  static Handle<Object> GetOrCreatePrototypeChainValidityCell(Handle<Map> map,
                                                              Isolate* isolate);
  static const int kPrototypeChainValid = 0;
  static const int kPrototypeChainInvalid = 1;

  static bool IsPrototypeChainInvalidated(Map map);

  // Return the map of the root of object's prototype chain.
  Map GetPrototypeChainRootMap(Isolate* isolate) const;

  V8_EXPORT_PRIVATE Map FindRootMap(Isolate* isolate) const;
  V8_EXPORT_PRIVATE Map FindFieldOwner(Isolate* isolate, int descriptor) const;

  inline int GetInObjectPropertyOffset(int index) const;

  class FieldCounts {
   public:
    FieldCounts(int mutable_count, int const_count)
        : mutable_count_(mutable_count), const_count_(const_count) {}

    int GetTotal() const { return mutable_count() + const_count(); }

    int mutable_count() const { return mutable_count_; }
    int const_count() const { return const_count_; }

   private:
    int mutable_count_;
    int const_count_;
  };

  FieldCounts GetFieldCounts() const;
  int NumberOfFields() const;

  bool HasOutOfObjectProperties() const;

  // Returns true if transition to the given map requires special
  // synchronization with the concurrent marker.
  bool TransitionRequiresSynchronizationWithGC(Map target) const;
  // Returns true if transition to the given map removes a tagged in-object
  // field.
  bool TransitionRemovesTaggedField(Map target) const;
  // Returns true if transition to the given map replaces a tagged in-object
  // field with an untagged in-object field.
  bool TransitionChangesTaggedFieldToUntaggedField(Map target) const;

  // TODO(ishell): candidate with JSObject::MigrateToMap().
  bool InstancesNeedRewriting(Map target) const;
  bool InstancesNeedRewriting(Map target, int target_number_of_fields,
                              int target_inobject, int target_unused,
                              int* old_number_of_fields) const;
  V8_WARN_UNUSED_RESULT static Handle<FieldType> GeneralizeFieldType(
      Representation rep1, Handle<FieldType> type1, Representation rep2,
      Handle<FieldType> type2, Isolate* isolate);
  static void GeneralizeField(Isolate* isolate, Handle<Map> map,
                              int modify_index, PropertyConstness new_constness,
                              Representation new_representation,
                              Handle<FieldType> new_field_type);
  // Returns true if the |field_type| is the most general one for
  // given |representation|.
  static inline bool IsMostGeneralFieldType(Representation representation,
                                            FieldType field_type);

  // Generalizes representation and field_type if objects with given
  // instance type can have fast elements that can be transitioned by
  // stubs or optimized code to more general elements kind.
  // This generalization is necessary in order to ensure that elements kind
  // transitions performed by stubs / optimized code don't silently transition
  // fields with representation "Tagged" back to "Smi" or "HeapObject" or
  // fields with HeapObject representation and "Any" type back to "Class" type.
  static inline void GeneralizeIfCanHaveTransitionableFastElementsKind(
      Isolate* isolate, InstanceType instance_type,
      Representation* representation, Handle<FieldType>* field_type);

  V8_EXPORT_PRIVATE static Handle<Map> ReconfigureProperty(
      Isolate* isolate, Handle<Map> map, int modify_index,
      PropertyKind new_kind, PropertyAttributes new_attributes,
      Representation new_representation, Handle<FieldType> new_field_type);

  V8_EXPORT_PRIVATE static Handle<Map> ReconfigureElementsKind(
      Isolate* isolate, Handle<Map> map, ElementsKind new_elements_kind);

  V8_EXPORT_PRIVATE static Handle<Map> PrepareForDataProperty(
      Isolate* isolate, Handle<Map> old_map, int descriptor_number,
      PropertyConstness constness, Handle<Object> value);

  V8_EXPORT_PRIVATE static Handle<Map> Normalize(Isolate* isolate,
                                                 Handle<Map> map,
                                                 PropertyNormalizationMode mode,
                                                 const char* reason);

  // Tells whether the map is used for JSObjects in dictionary mode (ie
  // normalized objects, ie objects for which HasFastProperties returns false).
  // A map can never be used for both dictionary mode and fast mode JSObjects.
  // False by default and for HeapObjects that are not JSObjects.
  DECL_BOOLEAN_ACCESSORS(is_dictionary_map)

  // Tells whether the instance needs security checks when accessing its
  // properties.
  DECL_BOOLEAN_ACCESSORS(is_access_check_needed)

  // [prototype]: implicit prototype object.
  DECL_ACCESSORS(prototype, HeapObject)
  // TODO(jkummerow): make set_prototype private.
  V8_EXPORT_PRIVATE static void SetPrototype(
      Isolate* isolate, Handle<Map> map, Handle<HeapObject> prototype,
      bool enable_prototype_setup_mode = true);

  // [constructor]: points back to the function or FunctionTemplateInfo
  // responsible for this map.
  // The field overlaps with the back pointer. All maps in a transition tree
  // have the same constructor, so maps with back pointers can walk the
  // back pointer chain until they find the map holding their constructor.
  // Returns null_value if there's neither a constructor function nor a
  // FunctionTemplateInfo available.
  DECL_ACCESSORS(constructor_or_backpointer, Object)
  inline Object GetConstructor() const;
  inline FunctionTemplateInfo GetFunctionTemplateInfo() const;
  inline void SetConstructor(Object constructor,
                             WriteBarrierMode mode = UPDATE_WRITE_BARRIER);
  // [back pointer]: points back to the parent map from which a transition
  // leads to this map. The field overlaps with the constructor (see above).
  inline HeapObject GetBackPointer() const;
  inline void SetBackPointer(Object value,
                             WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  // [instance descriptors]: describes the object.
  inline DescriptorArray instance_descriptors() const;
  inline DescriptorArray synchronized_instance_descriptors() const;
  V8_EXPORT_PRIVATE void SetInstanceDescriptors(Isolate* isolate,
                                                DescriptorArray descriptors,
                                                int number_of_own_descriptors);

  // [layout descriptor]: describes the object layout.
  DECL_ACCESSORS(layout_descriptor, LayoutDescriptor)
  // |layout descriptor| accessor which can be used from GC.
  inline LayoutDescriptor layout_descriptor_gc_safe() const;
  inline bool HasFastPointerLayout() const;

  // |layout descriptor| accessor that is safe to call even when
  // FLAG_unbox_double_fields is disabled (in this case Map does not contain
  // |layout_descriptor| field at all).
  inline LayoutDescriptor GetLayoutDescriptor() const;

  inline void UpdateDescriptors(Isolate* isolate, DescriptorArray descriptors,
                                LayoutDescriptor layout_descriptor,
                                int number_of_own_descriptors);
  inline void InitializeDescriptors(Isolate* isolate,
                                    DescriptorArray descriptors,
                                    LayoutDescriptor layout_descriptor);

  // [dependent code]: list of optimized codes that weakly embed this map.
  DECL_ACCESSORS(dependent_code, DependentCode)

  // [prototype_validity_cell]: Cell containing the validity bit for prototype
  // chains or Smi(0) if uninitialized.
  // The meaning of this validity cell is different for prototype maps and
  // non-prototype maps.
  // For prototype maps the validity bit "guards" modifications of prototype
  // chains going through this object. When a prototype object changes, both its
  // own validity cell and those of all "downstream" prototypes are invalidated;
  // handlers for a given receiver embed the currently valid cell for that
  // receiver's prototype during their creation and check it on execution.
  // For non-prototype maps which are used as transitioning store handlers this
  // field contains the validity cell which guards modifications of this map's
  // prototype.
  DECL_ACCESSORS(prototype_validity_cell, Object)

  // Returns true if prototype validity cell value represents "valid" prototype
  // chain state.
  inline bool IsPrototypeValidityCellValid() const;

  inline PropertyDetails GetLastDescriptorDetails() const;

  inline int LastAdded() const;

  inline int NumberOfOwnDescriptors() const;
  inline void SetNumberOfOwnDescriptors(int number);

  inline Cell RetrieveDescriptorsPointer();

  // Checks whether all properties are stored either in the map or on the object
  // (inobject, properties, or elements backing store), requiring no special
  // checks.
  bool OnlyHasSimpleProperties() const;
  inline int EnumLength() const;
  inline void SetEnumLength(int length);

  DECL_BOOLEAN_ACCESSORS(owns_descriptors)

  inline void mark_unstable();
  inline bool is_stable() const;

  DECL_BOOLEAN_ACCESSORS(is_migration_target)

  DECL_BOOLEAN_ACCESSORS(is_immutable_proto)

  // This counter is used for in-object slack tracking.
  // The in-object slack tracking is considered enabled when the counter is
  // non zero. The counter only has a valid count for initial maps. For
  // transitioned maps only kNoSlackTracking has a meaning, namely that inobject
  // slack tracking already finished for the transition tree. Any other value
  // indicates that either inobject slack tracking is still in progress, or that
  // the map isn't part of the transition tree anymore.
  DECL_INT_ACCESSORS(construction_counter)

  DECL_BOOLEAN_ACCESSORS(is_deprecated)
  inline bool CanBeDeprecated() const;
  // Returns a non-deprecated version of the input. If the input was not
  // deprecated, it is directly returned. Otherwise, the non-deprecated version
  // is found by re-transitioning from the root of the transition tree using the
  // descriptor array of the map. Returns MaybeHandle<Map>() if no updated map
  // is found.
  V8_EXPORT_PRIVATE static MaybeHandle<Map> TryUpdate(
      Isolate* isolate, Handle<Map> map) V8_WARN_UNUSED_RESULT;
  V8_EXPORT_PRIVATE static Map TryUpdateSlow(Isolate* isolate,
                                             Map map) V8_WARN_UNUSED_RESULT;

  // Returns a non-deprecated version of the input. This method may deprecate
  // existing maps along the way if encodings conflict. Not for use while
  // gathering type feedback. Use TryUpdate in those cases instead.
  V8_EXPORT_PRIVATE static Handle<Map> Update(Isolate* isolate,
                                              Handle<Map> map);

  static inline Handle<Map> CopyInitialMap(Isolate* isolate, Handle<Map> map);
  V8_EXPORT_PRIVATE static Handle<Map> CopyInitialMap(
      Isolate* isolate, Handle<Map> map, int instance_size,
      int in_object_properties, int unused_property_fields);
  static Handle<Map> CopyInitialMapNormalized(
      Isolate* isolate, Handle<Map> map,
      PropertyNormalizationMode mode = CLEAR_INOBJECT_PROPERTIES);
  static Handle<Map> CopyDropDescriptors(Isolate* isolate, Handle<Map> map);
  V8_EXPORT_PRIVATE static Handle<Map> CopyInsertDescriptor(
      Isolate* isolate, Handle<Map> map, Descriptor* descriptor,
      TransitionFlag flag);

  static MaybeObjectHandle WrapFieldType(Isolate* isolate,
                                         Handle<FieldType> type);
  V8_EXPORT_PRIVATE static FieldType UnwrapFieldType(MaybeObject wrapped_type);

  V8_EXPORT_PRIVATE V8_WARN_UNUSED_RESULT static MaybeHandle<Map> CopyWithField(
      Isolate* isolate, Handle<Map> map, Handle<Name> name,
      Handle<FieldType> type, PropertyAttributes attributes,
      PropertyConstness constness, Representation representation,
      TransitionFlag flag);

  V8_EXPORT_PRIVATE V8_WARN_UNUSED_RESULT static MaybeHandle<Map>
  CopyWithConstant(Isolate* isolate, Handle<Map> map, Handle<Name> name,
                   Handle<Object> constant, PropertyAttributes attributes,
                   TransitionFlag flag);

  // Returns a new map with all transitions dropped from the given map and
  // the ElementsKind set.
  static Handle<Map> TransitionElementsTo(Isolate* isolate, Handle<Map> map,
                                          ElementsKind to_kind);

  V8_EXPORT_PRIVATE static Handle<Map> AsElementsKind(Isolate* isolate,
                                                      Handle<Map> map,
                                                      ElementsKind kind);

  static Handle<Map> CopyAsElementsKind(Isolate* isolate, Handle<Map> map,
                                        ElementsKind kind, TransitionFlag flag);

  static Handle<Map> AsLanguageMode(Isolate* isolate, Handle<Map> initial_map,
                                    Handle<SharedFunctionInfo> shared_info);

  V8_EXPORT_PRIVATE static Handle<Map> CopyForPreventExtensions(
      Isolate* isolate, Handle<Map> map, PropertyAttributes attrs_to_add,
      Handle<Symbol> transition_marker, const char* reason,
      bool old_map_is_dictionary_elements_kind = false);

  // Maximal number of fast properties. Used to restrict the number of map
  // transitions to avoid an explosion in the number of maps for objects used as
  // dictionaries.
  inline bool TooManyFastProperties(StoreOrigin store_origin) const;
  V8_EXPORT_PRIVATE static Handle<Map> TransitionToDataProperty(
      Isolate* isolate, Handle<Map> map, Handle<Name> name,
      Handle<Object> value, PropertyAttributes attributes,
      PropertyConstness constness, StoreOrigin store_origin);
  V8_EXPORT_PRIVATE static Handle<Map> TransitionToAccessorProperty(
      Isolate* isolate, Handle<Map> map, Handle<Name> name, int descriptor,
      Handle<Object> getter, Handle<Object> setter,
      PropertyAttributes attributes);
  V8_EXPORT_PRIVATE static Handle<Map> ReconfigureExistingProperty(
      Isolate* isolate, Handle<Map> map, int descriptor, PropertyKind kind,
      PropertyAttributes attributes);

  inline void AppendDescriptor(Isolate* isolate, Descriptor* desc);

  // Returns a copy of the map, prepared for inserting into the transition
  // tree (if the |map| owns descriptors then the new one will share
  // descriptors with |map|).
  static Handle<Map> CopyForElementsTransition(Isolate* isolate,
                                               Handle<Map> map);

  // Returns a copy of the map, with all transitions dropped from the
  // instance descriptors.
  static Handle<Map> Copy(Isolate* isolate, Handle<Map> map,
                          const char* reason);
  V8_EXPORT_PRIVATE static Handle<Map> Create(Isolate* isolate,
                                              int inobject_properties);

  // Returns the next free property index (only valid for FAST MODE).
  int NextFreePropertyIndex() const;

  // Returns the number of enumerable properties.
  int NumberOfEnumerableProperties() const;

  DECL_CAST(Map)

  static inline int SlackForArraySize(int old_size, int size_limit);

  V8_EXPORT_PRIVATE static void EnsureDescriptorSlack(Isolate* isolate,
                                                      Handle<Map> map,
                                                      int slack);

  // Returns the map to be used for instances when the given {prototype} is
  // passed to an Object.create call. Might transition the given {prototype}.
  static Handle<Map> GetObjectCreateMap(Isolate* isolate,
                                        Handle<HeapObject> prototype);

  // Similar to {GetObjectCreateMap} but does not transition {prototype} and
  // fails gracefully by returning an empty handle instead.
  static MaybeHandle<Map> TryGetObjectCreateMap(Isolate* isolate,
                                                Handle<HeapObject> prototype);

  // Computes a hash value for this map, to be used in HashTables and such.
  int Hash();

  // Returns the transitioned map for this map with the most generic
  // elements_kind that's found in |candidates|, or |nullptr| if no match is
  // found at all.
  V8_EXPORT_PRIVATE Map FindElementsKindTransitionedMap(
      Isolate* isolate, MapHandles const& candidates);

  inline bool CanTransition() const;

#define DECL_TESTER(Type, ...) inline bool Is##Type##Map() const;
  INSTANCE_TYPE_CHECKERS(DECL_TESTER)
#undef DECL_TESTER
  inline bool IsBooleanMap() const;
  inline bool IsNullOrUndefinedMap() const;
  inline bool IsPrimitiveMap() const;
  inline bool IsSpecialReceiverMap() const;
  inline bool IsCustomElementsReceiverMap() const;

  bool IsMapInArrayPrototypeChain(Isolate* isolate) const;

  // Dispatched behavior.
  void MapPrint(std::ostream& os);
  DECL_VERIFIER(Map)

#ifdef VERIFY_HEAP
  void DictionaryMapVerify(Isolate* isolate);
#endif

  DECL_PRIMITIVE_ACCESSORS(visitor_id, VisitorId)

  static ObjectFields ObjectFieldsFrom(VisitorId visitor_id) {
    return (visitor_id < kDataOnlyVisitorIdCount)
               ? ObjectFields::kDataOnly
               : ObjectFields::kMaybePointers;
  }

  V8_EXPORT_PRIVATE static Handle<Map> TransitionToPrototype(
      Isolate* isolate, Handle<Map> map, Handle<HeapObject> prototype);

  static Handle<Map> TransitionToImmutableProto(Isolate* isolate,
                                                Handle<Map> map);

  static const int kMaxPreAllocatedPropertyFields = 255;

  DEFINE_FIELD_OFFSET_CONSTANTS(HeapObject::kHeaderSize,
                                TORQUE_GENERATED_MAP_FIELDS)

  STATIC_ASSERT(kInstanceTypeOffset == Internals::kMapInstanceTypeOffset);

  class BodyDescriptor;

  // Compares this map to another to see if they describe equivalent objects.
  // If |mode| is set to CLEAR_INOBJECT_PROPERTIES, |other| is treated as if
  // it had exactly zero inobject properties.
  // The "shared" flags of both this map and |other| are ignored.
  bool EquivalentToForNormalization(const Map other,
                                    PropertyNormalizationMode mode) const;

  // Returns true if given field is unboxed double.
  inline bool IsUnboxedDoubleField(FieldIndex index) const;

  void PrintMapDetails(std::ostream& os);

  static inline Handle<Map> AddMissingTransitionsForTesting(
      Isolate* isolate, Handle<Map> split_map,
      Handle<DescriptorArray> descriptors,
      Handle<LayoutDescriptor> full_layout_descriptor);

  // Fires when the layout of an object with a leaf map changes.
  // This includes adding transitions to the leaf map or changing
  // the descriptor array.
  inline void NotifyLeafMapLayoutChange(Isolate* isolate);

  V8_EXPORT_PRIVATE static VisitorId GetVisitorId(Map map);

  // Returns true if objects with given instance type are allowed to have
  // fast transitionable elements kinds. This predicate is used to ensure
  // that objects that can have transitionable fast elements kind will not
  // get in-place generalizable fields because the elements kind transition
  // performed by stubs or optimized code can't properly generalize such
  // fields.
  static inline bool CanHaveFastTransitionableElementsKind(
      InstanceType instance_type);
  inline bool CanHaveFastTransitionableElementsKind() const;

  // Whether this is the map of the given native context's global proxy.
  bool IsMapOfGlobalProxy(Handle<NativeContext> native_context) const;

 private:
  // This byte encodes either the instance size without the in-object slack or
  // the slack size in properties backing store.
  // Let H be JSObject::kHeaderSize / kTaggedSize.
  // If value >= H then:
  //     - all field properties are stored in the object.
  //     - there is no property array.
  //     - value * kTaggedSize is the actual object size without the slack.
  // Otherwise:
  //     - there is no slack in the object.
  //     - the property array has value slack slots.
  // Note that this encoding requires that H = JSObject::kFieldsAdded.
  DECL_INT_ACCESSORS(used_or_unused_instance_size_in_words)

  // Returns the map that this (root) map transitions to if its elements_kind
  // is changed to |elements_kind|, or |nullptr| if no such map is cached yet.
  Map LookupElementsTransitionMap(Isolate* isolate, ElementsKind elements_kind);

  // Tries to replay property transitions starting from this (root) map using
  // the descriptor array of the |map|. The |root_map| is expected to have
  // proper elements kind and therefore elements kinds transitions are not
  // taken by this function. Returns |nullptr| if matching transition map is
  // not found.
  Map TryReplayPropertyTransitions(Isolate* isolate, Map map);

  static void ConnectTransition(Isolate* isolate, Handle<Map> parent,
                                Handle<Map> child, Handle<Name> name,
                                SimpleTransitionFlag flag);

  bool EquivalentToForTransition(const Map other) const;
  bool EquivalentToForElementsKindTransition(const Map other) const;
  static Handle<Map> RawCopy(Isolate* isolate, Handle<Map> map,
                             int instance_size, int inobject_properties);
  static Handle<Map> ShareDescriptor(Isolate* isolate, Handle<Map> map,
                                     Handle<DescriptorArray> descriptors,
                                     Descriptor* descriptor);
  V8_EXPORT_PRIVATE static Handle<Map> AddMissingTransitions(
      Isolate* isolate, Handle<Map> map, Handle<DescriptorArray> descriptors,
      Handle<LayoutDescriptor> full_layout_descriptor);
  static void InstallDescriptors(
      Isolate* isolate, Handle<Map> parent_map, Handle<Map> child_map,
      int new_descriptor, Handle<DescriptorArray> descriptors,
      Handle<LayoutDescriptor> full_layout_descriptor);
  static Handle<Map> CopyAddDescriptor(Isolate* isolate, Handle<Map> map,
                                       Descriptor* descriptor,
                                       TransitionFlag flag);
  static Handle<Map> CopyReplaceDescriptors(
      Isolate* isolate, Handle<Map> map, Handle<DescriptorArray> descriptors,
      Handle<LayoutDescriptor> layout_descriptor, TransitionFlag flag,
      MaybeHandle<Name> maybe_name, const char* reason,
      SimpleTransitionFlag simple_flag);

  static Handle<Map> CopyReplaceDescriptor(Isolate* isolate, Handle<Map> map,
                                           Handle<DescriptorArray> descriptors,
                                           Descriptor* descriptor, int index,
                                           TransitionFlag flag);
  static Handle<Map> CopyNormalized(Isolate* isolate, Handle<Map> map,
                                    PropertyNormalizationMode mode);

  // TODO(ishell): Move to MapUpdater.
  static Handle<Map> CopyGeneralizeAllFields(Isolate* isolate, Handle<Map> map,
                                             ElementsKind elements_kind,
                                             int modify_index,
                                             PropertyKind kind,
                                             PropertyAttributes attributes,
                                             const char* reason);

  void DeprecateTransitionTree(Isolate* isolate);

  void ReplaceDescriptors(Isolate* isolate, DescriptorArray new_descriptors,
                          LayoutDescriptor new_layout_descriptor);

  // Update field type of the given descriptor to new representation and new
  // type. The type must be prepared for storing in descriptor array:
  // it must be either a simple type or a map wrapped in a weak cell.
  void UpdateFieldType(Isolate* isolate, int descriptor_number,
                       Handle<Name> name, PropertyConstness new_constness,
                       Representation new_representation,
                       const MaybeObjectHandle& new_wrapped_type);

  // TODO(ishell): Move to MapUpdater.
  void PrintReconfiguration(Isolate* isolate, FILE* file, int modify_index,
                            PropertyKind kind, PropertyAttributes attributes);
  // TODO(ishell): Move to MapUpdater.
  void PrintGeneralization(
      Isolate* isolate, FILE* file, const char* reason, int modify_index,
      int split, int descriptors, bool constant_to_field,
      Representation old_representation, Representation new_representation,
      PropertyConstness old_constness, PropertyConstness new_constness,
      MaybeHandle<FieldType> old_field_type, MaybeHandle<Object> old_value,
      MaybeHandle<FieldType> new_field_type, MaybeHandle<Object> new_value);

  // Use the high-level instance_descriptors/SetInstanceDescriptors instead.
  inline void set_synchronized_instance_descriptors(
      DescriptorArray array, WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  static const int kFastPropertiesSoftLimit = 12;
  static const int kMaxFastProperties = 128;

  friend class MapUpdater;

  OBJECT_CONSTRUCTORS(Map, HeapObject);
};

// The cache for maps used by normalized (dictionary mode) objects.
// Such maps do not have property descriptors, so a typical program
// needs very limited number of distinct normalized maps.
class NormalizedMapCache : public WeakFixedArray {
 public:
  NEVER_READ_ONLY_SPACE
  static Handle<NormalizedMapCache> New(Isolate* isolate);

  V8_WARN_UNUSED_RESULT MaybeHandle<Map> Get(Handle<Map> fast_map,
                                             PropertyNormalizationMode mode);
  void Set(Handle<Map> fast_map, Handle<Map> normalized_map);

  DECL_CAST(NormalizedMapCache)
  DECL_VERIFIER(NormalizedMapCache)

 private:
  friend bool HeapObject::IsNormalizedMapCache() const;

  static const int kEntries = 64;

  static inline int GetIndex(Handle<Map> map);

  // The following declarations hide base class methods.
  Object get(int index);
  void set(int index, Object value);

  OBJECT_CONSTRUCTORS(NormalizedMapCache, WeakFixedArray);
};

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_MAP_H_
