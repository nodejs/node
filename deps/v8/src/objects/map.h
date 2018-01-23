// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_MAP_H_
#define V8_OBJECTS_MAP_H_

#include "src/objects.h"
#include "src/objects/code.h"

#include "src/globals.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

#define VISITOR_ID_LIST(V) \
  V(AllocationSite)        \
  V(BigInt)                \
  V(ByteArray)             \
  V(BytecodeArray)         \
  V(Cell)                  \
  V(Code)                  \
  V(CodeDataContainer)     \
  V(ConsString)            \
  V(DataObject)            \
  V(FeedbackVector)        \
  V(FixedArray)            \
  V(FixedDoubleArray)      \
  V(FixedFloat64Array)     \
  V(FixedTypedArrayBase)   \
  V(FreeSpace)             \
  V(JSApiObject)           \
  V(JSArrayBuffer)         \
  V(JSFunction)            \
  V(JSObject)              \
  V(JSObjectFast)          \
  V(JSRegExp)              \
  V(JSWeakCollection)      \
  V(Map)                   \
  V(NativeContext)         \
  V(Oddball)               \
  V(PropertyArray)         \
  V(PropertyCell)          \
  V(SeqOneByteString)      \
  V(SeqTwoByteString)      \
  V(SharedFunctionInfo)    \
  V(ShortcutCandidate)     \
  V(SlicedString)          \
  V(SmallOrderedHashMap)   \
  V(SmallOrderedHashSet)   \
  V(Struct)                \
  V(Symbol)                \
  V(ThinString)            \
  V(TransitionArray)       \
  V(WeakCell)

// For data objects, JS objects and structs along with generic visitor which
// can visit object of any size we provide visitors specialized by
// object size in words.
// Ids of specialized visitors are declared in a linear order (without
// holes) starting from the id of visitor specialized for 2 words objects
// (base visitor id) and ending with the id of generic visitor.
// Method GetVisitorIdForSize depends on this ordering to calculate visitor
// id of specialized visitor from given instance size, base visitor id and
// generic visitor's id.
enum VisitorId {
#define VISITOR_ID_ENUM_DECL(id) kVisit##id,
  VISITOR_ID_LIST(VISITOR_ID_ENUM_DECL)
#undef VISITOR_ID_ENUM_DECL
      kVisitorIdCount
};

typedef std::vector<Handle<Map>> MapHandles;

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
//      |          |   - unused (bit 7)                          |
//      +----------+---------------------------------------------+
//      | Byte     | [bit_field2]                                |
//      |          |   - is_extensible (bit 0)                   |
//      |          |   - is_prototype_map (bit 2)                |
//      |          |   - elements_kind (bits 3..7)               |
// +----+----------+---------------------------------------------+
// | Int           | [bit_field3]                                |
// |               |   - number_of_own_descriptors (bit 0..19)   |
// |               |   - is_dictionary_map (bit 20)              |
// |               |   - owns_descriptors (bit 21)               |
// |               |   - has_hidden_prototype (bit 22)           |
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
// | TaggedPointer | If Map is a prototype map:                  |
// |               |   [prototype_info]                          |
// |               | Else:                                       |
// |               |   [raw_transitions]                         |
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
// | TaggedPointer | [weak_cell_cache]                           |
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
  inline InterceptorInfo* GetNamedInterceptor();
  inline InterceptorInfo* GetIndexedInterceptor();

  // Instance type.
  DECL_PRIMITIVE_ACCESSORS(instance_type, InstanceType)

  // Returns the size of the used in-object area including object header
  // (only used for JSObject in fast mode, for the other kinds of objects it
  // is equal to the instance size).
  inline int UsedInstanceSize() const;

  // Tells how many unused property fields (in-object or out-of object) are
  // available in the instance (only used for JSObject in fast mode).
  inline int UnusedPropertyFields() const;
  // Updates the counters tracking unused fields in the object.
  inline void SetInObjectUnusedPropertyFields(int unused_property_fields);
  // Updates the counters tracking unused fields in the property array.
  inline void SetOutOfObjectUnusedPropertyFields(int unused_property_fields);
  inline void CopyUnusedPropertyFields(Map* map);
  inline void AccountAddedPropertyField();
  inline void AccountAddedOutOfObjectPropertyField(
      int unused_in_property_array);

  //
  // Bit field.
  //
  DECL_PRIMITIVE_ACCESSORS(bit_field, byte)

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
#define MAP_BIT_FIELD2_FIELDS(V, _) \
  /* One bit is still free here. */ \
  V(IsExtensibleBit, bool, 1, _)    \
  V(IsPrototypeMapBit, bool, 1, _)  \
  V(ElementsKindBits, ElementsKind, 5, _)

  DEFINE_BIT_FIELDS(MAP_BIT_FIELD2_FIELDS)
#undef MAP_BIT_FIELD2_FIELDS

  //
  // Bit field 3.
  //
  DECL_PRIMITIVE_ACCESSORS(bit_field3, uint32_t)

// Bit positions for |bit_field3|.
#define MAP_BIT_FIELD3_FIELDS(V, _)                               \
  V(EnumLengthBits, int, kDescriptorIndexBitCount, _)             \
  V(NumberOfOwnDescriptorsBits, int, kDescriptorIndexBitCount, _) \
  V(IsDictionaryMapBit, bool, 1, _)                               \
  V(OwnsDescriptorsBit, bool, 1, _)                               \
  V(HasHiddenPrototypeBit, bool, 1, _)                            \
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
  inline void InobjectSlackTrackingStep();

  // Completes inobject slack tracking for the transition tree starting at this
  // initial map.
  void CompleteInobjectSlackTracking();

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
  inline bool has_fixed_typed_array_elements() const;
  inline bool has_dictionary_elements() const;

  static bool IsValidElementsTransition(ElementsKind from_kind,
                                        ElementsKind to_kind);

  // Returns true if the current map doesn't have DICTIONARY_ELEMENTS but if a
  // map with DICTIONARY_ELEMENTS was found in the prototype chain.
  bool DictionaryElementsInPrototypeChainOnly();

  inline Map* ElementsTransitionMap();

  inline FixedArrayBase* GetInitialElements() const;

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

  static bool IsPrototypeChainInvalidated(Map* map);

  // Return the map of the root of object's prototype chain.
  Map* GetPrototypeChainRootMap(Isolate* isolate) const;

  // Returns a WeakCell object containing given prototype. The cell is cached
  // in PrototypeInfo which is created lazily.
  static Handle<WeakCell> GetOrCreatePrototypeWeakCell(
      Handle<JSReceiver> prototype, Isolate* isolate);

  Map* FindRootMap() const;
  Map* FindFieldOwner(int descriptor) const;

  inline int GetInObjectPropertyOffset(int index) const;

  int NumberOfFields() const;

  bool HasOutOfObjectProperties() const;

  // Returns true if transition to the given map requires special
  // synchronization with the concurrent marker.
  bool TransitionRequiresSynchronizationWithGC(Map* target) const;
  // Returns true if transition to the given map removes a tagged in-object
  // field.
  bool TransitionRemovesTaggedField(Map* target) const;
  // Returns true if transition to the given map replaces a tagged in-object
  // field with an untagged in-object field.
  bool TransitionChangesTaggedFieldToUntaggedField(Map* target) const;

  // TODO(ishell): candidate with JSObject::MigrateToMap().
  bool InstancesNeedRewriting(Map* target) const;
  bool InstancesNeedRewriting(Map* target, int target_number_of_fields,
                              int target_inobject, int target_unused,
                              int* old_number_of_fields) const;
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

  // Generalizes constness, representation and field_type if objects with given
  // instance type can have fast elements that can be transitioned by stubs or
  // optimized code to more general elements kind.
  // This generalization is necessary in order to ensure that elements kind
  // transitions performed by stubs / optimized code don't silently transition
  // kMutable fields back to kConst state or fields with HeapObject
  // representation and "Any" type back to "Class" type.
  static inline void GeneralizeIfCanHaveTransitionableFastElementsKind(
      Isolate* isolate, InstanceType instance_type,
      PropertyConstness* constness, Representation* representation,
      Handle<FieldType>* field_type);

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
  DECL_BOOLEAN_ACCESSORS(is_dictionary_map)

  // Tells whether the instance needs security checks when accessing its
  // properties.
  DECL_BOOLEAN_ACCESSORS(is_access_check_needed)

  // [prototype]: implicit prototype object.
  DECL_ACCESSORS(prototype, Object)
  // TODO(jkummerow): make set_prototype private.
  static void SetPrototype(Handle<Map> map, Handle<Object> prototype,
                           bool enable_prototype_setup_mode = true);

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
  inline Object* GetBackPointer() const;
  inline void SetBackPointer(Object* value,
                             WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  // [instance descriptors]: describes the object.
  DECL_ACCESSORS(instance_descriptors, DescriptorArray)

  // [layout descriptor]: describes the object layout.
  DECL_ACCESSORS(layout_descriptor, LayoutDescriptor)
  // |layout descriptor| accessor which can be used from GC.
  inline LayoutDescriptor* layout_descriptor_gc_safe() const;
  inline bool HasFastPointerLayout() const;

  // |layout descriptor| accessor that is safe to call even when
  // FLAG_unbox_double_fields is disabled (in this case Map does not contain
  // |layout_descriptor| field at all).
  inline LayoutDescriptor* GetLayoutDescriptor() const;

  inline void UpdateDescriptors(DescriptorArray* descriptors,
                                LayoutDescriptor* layout_descriptor);
  inline void InitializeDescriptors(DescriptorArray* descriptors,
                                    LayoutDescriptor* layout_descriptor);

  // [dependent code]: list of optimized codes that weakly embed this map.
  DECL_ACCESSORS(dependent_code, DependentCode)

  // [weak cell cache]: cache that stores a weak cell pointing to this map.
  DECL_ACCESSORS(weak_cell_cache, Object)

  inline PropertyDetails GetLastDescriptorDetails() const;

  inline int LastAdded() const;

  inline int NumberOfOwnDescriptors() const;
  inline void SetNumberOfOwnDescriptors(int number);

  inline Cell* RetrieveDescriptorsPointer();

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
                                    Handle<SharedFunctionInfo> shared_info);

  static Handle<Map> CopyForPreventExtensions(Handle<Map> map,
                                              PropertyAttributes attrs_to_add,
                                              Handle<Symbol> transition_marker,
                                              const char* reason);

  static Handle<Map> FixProxy(Handle<Map> map, InstanceType type, int size);

  // Maximal number of fast properties. Used to restrict the number of map
  // transitions to avoid an explosion in the number of maps for objects used as
  // dictionaries.
  inline bool TooManyFastProperties(StoreFromKeyed store_mode) const;
  static Handle<Map> TransitionToDataProperty(
      Handle<Map> map, Handle<Name> name, Handle<Object> value,
      PropertyAttributes attributes, PropertyConstness constness,
      StoreFromKeyed store_mode, bool* created_new_map);
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
  int NextFreePropertyIndex() const;

  // Returns the number of enumerable properties.
  int NumberOfEnumerableProperties() const;

  DECL_CAST(Map)

  static inline int SlackForArraySize(int old_size, int size_limit);

  static void EnsureDescriptorSlack(Handle<Map> map, int slack);

  // Returns the map to be used for instances when the given {prototype} is
  // passed to an Object.create call. Might transition the given {prototype}.
  static Handle<Map> GetObjectCreateMap(Handle<HeapObject> prototype);

  // Similar to {GetObjectCreateMap} but does not transition {prototype} and
  // fails gracefully by returning an empty handle instead.
  static MaybeHandle<Map> TryGetObjectCreateMap(Handle<HeapObject> prototype);

  // Computes a hash value for this map, to be used in HashTables and such.
  int Hash();

  // Returns the transitioned map for this map with the most generic
  // elements_kind that's found in |candidates|, or |nullptr| if no match is
  // found at all.
  Map* FindElementsKindTransitionedMap(MapHandles const& candidates);

  inline bool CanTransition() const;

  inline bool IsBooleanMap() const;
  inline bool IsPrimitiveMap() const;
  inline bool IsJSReceiverMap() const;
  inline bool IsJSObjectMap() const;
  inline bool IsJSArrayMap() const;
  inline bool IsJSFunctionMap() const;
  inline bool IsStringMap() const;
  inline bool IsJSProxyMap() const;
  inline bool IsModuleMap() const;
  inline bool IsJSGlobalProxyMap() const;
  inline bool IsJSGlobalObjectMap() const;
  inline bool IsJSTypedArrayMap() const;
  inline bool IsJSDataViewMap() const;

  inline bool IsSpecialReceiverMap() const;

  static void AddDependentCode(Handle<Map> map,
                               DependentCode::DependencyGroup group,
                               Handle<Code> code);

  bool IsMapInArrayPrototypeChain() const;

  static Handle<WeakCell> WeakCellForMap(Handle<Map> map);

  // Dispatched behavior.
  DECL_PRINTER(Map)
  DECL_VERIFIER(Map)

#ifdef VERIFY_HEAP
  void DictionaryMapVerify();
#endif

  DECL_PRIMITIVE_ACCESSORS(visitor_id, VisitorId)

  static Handle<Map> TransitionToPrototype(Handle<Map> map,
                                           Handle<Object> prototype);

  static Handle<Map> TransitionToImmutableProto(Handle<Map> map);

  static const int kMaxPreAllocatedPropertyFields = 255;

  // Layout description.
#define MAP_FIELDS(V)                                                       \
  /* Raw data fields. */                                                    \
  V(kInstanceSizeInWordsOffset, kUInt8Size)                                 \
  V(kInObjectPropertiesStartOrConstructorFunctionIndexOffset, kUInt8Size)   \
  V(kUsedOrUnusedInstanceSizeInWordsOffset, kUInt8Size)                     \
  V(kVisitorIdOffset, kUInt8Size)                                           \
  V(kInstanceTypeOffset, kUInt16Size)                                       \
  V(kBitFieldOffset, kUInt8Size)                                            \
  V(kBitField2Offset, kUInt8Size)                                           \
  V(kBitField3Offset, kUInt32Size)                                          \
  V(k64BitArchPaddingOffset, kPointerSize == kUInt32Size ? 0 : kUInt32Size) \
  /* Pointer fields. */                                                     \
  V(kPointerFieldsBeginOffset, 0)                                           \
  V(kPrototypeOffset, kPointerSize)                                         \
  V(kConstructorOrBackPointerOffset, kPointerSize)                          \
  V(kTransitionsOrPrototypeInfoOffset, kPointerSize)                        \
  V(kDescriptorsOffset, kPointerSize)                                       \
  V(kLayoutDescriptorOffset, FLAG_unbox_double_fields ? kPointerSize : 0)   \
  V(kDependentCodeOffset, kPointerSize)                                     \
  V(kWeakCellCacheOffset, kPointerSize)                                     \
  V(kPointerFieldsEndOffset, 0)                                             \
  /* Total size. */                                                         \
  V(kSize, 0)

  DEFINE_FIELD_OFFSET_CONSTANTS(HeapObject::kHeaderSize, MAP_FIELDS)
#undef MAP_FIELDS

  STATIC_ASSERT(kInstanceTypeOffset == Internals::kMapInstanceTypeOffset);

  typedef FixedBodyDescriptor<kPointerFieldsBeginOffset,
                              kPointerFieldsEndOffset, kSize>
      BodyDescriptor;

  // Compares this map to another to see if they describe equivalent objects.
  // If |mode| is set to CLEAR_INOBJECT_PROPERTIES, |other| is treated as if
  // it had exactly zero inobject properties.
  // The "shared" flags of both this map and |other| are ignored.
  bool EquivalentToForNormalization(const Map* other,
                                    PropertyNormalizationMode mode) const;

  // Returns true if given field is unboxed double.
  inline bool IsUnboxedDoubleField(FieldIndex index) const;

  void PrintMapDetails(std::ostream& os, JSObject* holder = nullptr);

  static inline Handle<Map> AddMissingTransitionsForTesting(
      Handle<Map> split_map, Handle<DescriptorArray> descriptors,
      Handle<LayoutDescriptor> full_layout_descriptor);

  // Fires when the layout of an object with a leaf map changes.
  // This includes adding transitions to the leaf map or changing
  // the descriptor array.
  inline void NotifyLeafMapLayoutChange();

  static VisitorId GetVisitorId(Map* map);

  // Returns true if objects with given instance type are allowed to have
  // fast transitionable elements kinds. This predicate is used to ensure
  // that objects that can have transitionable fast elements kind will not
  // get in-place generalizable fields because the elements kind transition
  // performed by stubs or optimized code can't properly generalize such
  // fields.
  static inline bool CanHaveFastTransitionableElementsKind(
      InstanceType instance_type);
  inline bool CanHaveFastTransitionableElementsKind() const;

 private:
  // This byte encodes either the instance size without the in-object slack or
  // the slack size in properties backing store.
  // Let H be JSObject::kHeaderSize / kPointerSize.
  // If value >= H then:
  //     - all field properties are stored in the object.
  //     - there is no property array.
  //     - value * kPointerSize is the actual object size without the slack.
  // Otherwise:
  //     - there is no slack in the object.
  //     - the property array has value slack slots.
  // Note that this encoding requires that H = JSObject::kFieldsAdded.
  DECL_INT_ACCESSORS(used_or_unused_instance_size_in_words)

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

  bool EquivalentToForTransition(const Map* other) const;
  bool EquivalentToForElementsKindTransition(const Map* other) const;
  static Handle<Map> RawCopy(Handle<Map> map, int instance_size,
                             int inobject_properties);
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
  void Set(Handle<Map> fast_map, Handle<Map> normalized_map,
           Handle<WeakCell> normalized_map_weak_cell);

  void Clear();

  DECL_CAST(NormalizedMapCache)

  static inline bool IsNormalizedMapCache(const HeapObject* obj);

  DECL_VERIFIER(NormalizedMapCache)
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
