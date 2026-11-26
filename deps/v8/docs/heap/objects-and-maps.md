# Objects and Maps in V8

Every heap-allocated value in V8 derives from **`HeapObject`** (`src/objects/heap-object.h`), whose first field (`offset 0`) is always a tagged pointer to a **`Map`** (`src/objects/map.h`). The `Map` (also known as a **Hidden Class** or **Shape**) defines the object's `InstanceType`, size, prototype, property descriptors, and GC visitation layout.

---

## 1. Defining Heap Objects in C++ (`V8_OBJECT` & Member Wrappers)

Historically, V8 heap objects were bare C++ classes with zero-size bodies (`sizeof(HeapObject) == 0`) that accessed fields via manual byte-offset macros (`READ_FIELD(this, kFooOffset)`). Modern V8 defines heap object layouts directly as standard C++ classes with real data members using `V8_OBJECT` (`src/objects/object-macros.h`) and typed member wrappers (`src/objects/tagged-field.h`, `src/objects/trusted-pointer.h`).

### `V8_OBJECT` and `V8_ABSTRACT_OBJECT` (`src/objects/object-macros.h`)
```cpp
V8_OBJECT class JSRegExp : public JSObject {
 public:
  V8_IT_OWN_TYPE;
  // ... accessors and methods ...

  TaggedMember<UnionOf<String, Undefined>> source_;
  TaggedMember<Smi> flags_;
  TrustedPointerMember<RegExpData, kRegExpDataTrustedPointerTag> data_;
} V8_OBJECT_END;
```
* **`V8_OBJECT ... V8_OBJECT_END`**: Wraps the class in `#pragma pack(push, 4)` and enables `-Werror=padded`. Because `Tagged_t` is 4 bytes under pointer compression, `#pragma pack(4)` ensures 8-byte fields or base classes do not introduce implicit 8-byte alignment padding inside heap objects, and the compiler rejects any declaration with unintended internal padding.
* **`V8_ABSTRACT_OBJECT ... V8_OBJECT_END`**: Wraps an abstract base class in `#pragma pack(push, 1)` so derived classes can pack narrow fields (`uint8_t`, `uint16_t`) into trailing bytes of the base class.
* **`HeapObject` Semantics**: Inside a `V8_OBJECT` method, `this` is a standard C++ pointer to the **untagged** start address of the object in memory (`reinterpret_cast<Address>(this) == address()`). `operator new` and `operator delete` are deleted on `HeapObject` (except placement `new (witness)` taking an `AllocationWitness`), and copy/move constructors are deleted so C++ code never copies heap objects by value.

### Typed Field Member Wrappers (`src/objects/tagged-field.h`, `src/objects/trusted-pointer.h`)
Heap object fields are declared using member wrapper templates that automatically handle pointer compression, atomic memory ordering, sandbox indirection, and GC write barriers:

| Member Type | Size | Underlying Storage & Write Barrier | Typical Methods |
| :--- | :--- | :--- | :--- |
| **`TaggedMember<T>`** | `kTaggedSize` (4B / 8B) | Compressed pointer in `V8HeapCompressionScheme`. Calls `WriteBarrier::ForValue`. | `load()`, `store(this, val, mode)`, `Relaxed_Load()`, `Relaxed_Store(...)`, `Acquire_Load()`, `Release_Store(...)`, `SeqCst_Swap(...)`, `SeqCst_CompareAndSwap(...)` |
| **`ProtectedTaggedMember<T>`** | `kTaggedSize` (4B / 8B) | Alias for `TaggedMember<T, TrustedSpaceCompressionScheme>`. Used inside `TrustedObject`s for references to other `TrustedObject`s. Calls `WriteBarrier::ForProtectedPointer`. | Same as `TaggedMember<T>` (`load()`, `store(this, val, mode)`, `Acquire_Load()`, `Release_Store(...)`) |
| **`TrustedPointerMember<T, Tag>`** | 4B (`TrustedPointerHandle`) | Indirect pointer handle into `TrustedPointerTable` under `V8_ENABLE_SANDBOX` (falls back to `TaggedMember<T>` when sandbox is disabled). Calls `WriteBarrier::ForTrustedPointer`. | `load(isolate)`, `load_maybe_empty(isolate)`, `store(this, val, mode)`, `Acquire_Load(isolate)`, `Release_Store(this, val, mode)`, `is_empty()`, `is_unpublished(isolate)`, `clear(this)` |
| **`JSDispatchHandleMember`** | 4B (`JSDispatchHandle`) | Atomic handle into `JSDispatchTable` (`src/sandbox/js-dispatch-table.h`). Calls `WriteBarrier::ForJSDispatchHandle`. | `Relaxed_Load()`, `Acquire_Load()`, `Relaxed_Store(this, handle, mode)`, `Release_Store(this, handle, mode)`, `store_no_write_barrier(handle)` |
| **`UnalignedDoubleMember`** | 8B (`double`) | Alias for `UnalignedValueMember<Float64>`. Stores a 64-bit IEEE-754 `double` with 4-byte (`alignof(Tagged_t)`) alignment. | `value()`, `set_value(double)`, `value_as_bits()`, `set_value_as_bits(uint64_t)` |
| **`SmiBitFieldMember<Group>`** | `kTaggedSize` (4B / 8B) | Stores a `BitFieldGroup<Group, uint32_t>` encoded as a `Tagged<Smi>` (`TaggedMember<Smi>`). | `load()`, `store(Group)` |
| **`FLEXIBLE_ARRAY_MEMBER(T, name)`** | 0B in `sizeof(Class)` | Declares a trailing zero-length array (`T flexible_array_member_data_[0]`) at the end of a variable-sized object. | `name()` (returns `T*` or `const T*`), `OFFSET_OF_DATA_START(Class)` |

### `tools/metagen` and Layout / InstanceType Generation (`tools/metagen/metagen.py`)
C++ `V8_OBJECT` classes are the single source of truth for object layouts and `InstanceType` enum values:
* **`tools/metagen/metagen.py`** uses `libclang` to parse C++ headers under `V8_METAGEN_GENERATION_PASS`.
* **Instance-Type Annotations (`V8_IT_*`)**:
  * Pure leaf `V8_OBJECT` classes automatically receive an `InstanceType` value.
  * `V8_IT_ABSTRACT`: Marks an abstract intermediate class (emits `FIRST_*_TYPE` / `LAST_*_TYPE` range markers, no own value).
  * `V8_IT_OWN_TYPE`: Marks a concrete base class that also has concrete subclasses (e.g., `Map`, `JSArray`, `FixedArray`, `HashTable`) so it receives its own `_TYPE` value in addition to range bounds.
  * `V8_IT_REUSE_PARENT`: Shares the parent class's `InstanceType` value.
  * `V8_IT_FIXED_VALUE(N)`, `V8_IT_FLAG_BITS(N)`, `V8_IT_ORDER(POS)`: Controls exact numeric assignment for fast range/bitmask checks.
* **Torque Export Annotations (`V8_TQ_*`)**: Annotations such as `V8_TQ_TAIL_LENGTH(...)`, `V8_TQ_NO_TAIL`, `V8_TQ_CONST`, `V8_TQ_RELAXED`, `V8_TQ_ACQ_REL`, and `V8_TQ_CUSTOM_WEAK` export C++ field layouts into `layouts.json` so the Torque compiler (`src/torque/`) and CodeStubAssembler can generate type-safe field accessors without duplicating `.tq` layout declarations.

---

## 2. Layout of a `Map` (`src/objects/map.h`)

Every `HeapObject` starts with `TaggedMember<Map> map_` at offset `0`. Because `Map` is itself a `HeapObject`, its own `map_` points to the immortal **`meta_map`** in `ReadOnlySpace`.

```
Map Layout (src/objects/map.h)
+------------------------------------------------------------------+
| map_                       (TaggedMember<Map> -> meta_map)       |
| instance_size_in_words_    (uint8_t: size / kTaggedSize, or 0)   |
| inobject_properties_start_or_constructor_function_index_ (uint8) |
| used_or_unused_instance_size_in_words_                   (uint8) |
| visitor_id_                (uint8_t: VisitorId for GC dispatch)  |
| instance_type_             (InstanceType : uint16_t)             |
| bit_field_                 (uint8_t: callable, constructor, ...) |
| bit_field2_                (uint8_t: ElementsKind, ...)          |
| bit_field3_                (uint32_t: NumberOfOwnDescriptors,    |
|                             IsDictionaryMap, IsExtensible, ...)  |
| [4B padding on 64-bit uncompressed builds]                       |
| prototype_                 (TaggedMember<JSPrototype>)           |
| constructor_or_back_pointer_or_native_context_ (TaggedMember)    |
| instance_descriptors_      (DescriptorArray | WasmStruct)*       |
| dependent_code_            (DependentCode | Map)*                |
| prototype_validity_cell_   (TaggedMember<Smi|Cell>)              |
| transitions_or_prototype_info_ (TaggedMember<MaybeWeak<...>>)    |
+------------------------------------------------------------------+
```
*\* On Wasm builds (`V8_ENABLE_WEBASSEMBLY`), `WasmStruct` maps with custom descriptors reuse `instance_descriptors_` to store their `custom_descriptor` (`WasmStruct`) and `dependent_code_` to store their `immediate_supertype_map` (`Map`). Code loading `instance_descriptors_` (C++ `Map::instance_descriptors()` or CSA `LoadMapDescriptors`) must therefore verify the map is not a `WasmStruct` map first (e.g., via map identity, `IsJSObjectMap`, or `!IsSpecialReceiverMap`).*

### Key `Map` Fields
1. **`instance_size_in_words_` (`uint8_t`)**:
   * Stores the object's byte size divided by `kTaggedSize` (`instance_size() == instance_size_in_words_ * kTaggedSize`), allowing fixed object sizes up to `255 * kTaggedSize` (`1020` bytes under pointer compression) to fit in a single byte.
   * For variable-length objects (`FixedArray`, `SeqString`, `ByteArray`, `FeedbackVector`, etc.), `instance_size_in_words_` is set to **`kVariableSizeSentinel` (`0`)**, instructing `HeapObject::SizeFromMap(map)` to read the object's length field.
2. **`inobject_properties_start_or_constructor_function_index_` (`uint8_t`)**:
   * For `JSObject` maps: offset (in words) where in-object properties start.
   * For primitive maps (`String`, `Symbol`, `HeapNumber`, `BigInt`): index of the corresponding constructor function in the `NativeContext`.
3. **`used_or_unused_instance_size_in_words_` (`uint8_t`)**:
   * For `JSObject` maps: tracks unused in-object property slots remaining before the backing `PropertyArray` must be grown.
   * For `JSFunction` maps (`JSFunction` with slack tracking in progress): tracks the slack size.
4. **`visitor_id_` (`uint8_t`, `VisitorId`)**:
   * Precomputed dispatch index used by `HeapVisitor` (`src/heap/objects-visiting.h`) so the garbage collector can jump directly to the appropriate `BodyDescriptor` visitor without switching on `InstanceType`.
5. **`instance_type_` (`InstanceType`, `uint16_t`)**:
   * 16-bit tag generated by `tools/metagen` identifying the exact C++ class or category (`FIRST_JS_RECEIVER_TYPE`, `JS_OBJECT_TYPE`, `FIXED_ARRAY_TYPE`, etc.). String instance types pack encoding/representation bitflags in the low bits (`kStringRepresentationMask`, `kStringEncodingMask`, `kIsNotStringMask`).
6. **`bit_field_`, `bit_field2_`, `bit_field3_`**:
   * **`bit_field_` (`Map::Bits1`)**: `IsCallableBit`, `IsUndetectableBit`, `HasNamedInterceptorBit`, `IsAccessCheckNeededBit`, `HasIndexedInterceptorBit`, `IsConstructorBit`, and `IsExtendedMapBit` (bit 6).
   * **`bit_field2_` (`Map::Bits2`)**: `NewTargetIsBaseBit`, `IsImmutablePrototypeBit`, and `ElementsKindBits` (6 bits encoding `PACKED_SMI_ELEMENTS`, `HOLEY_DOUBLE_ELEMENTS`, `DICTIONARY_ELEMENTS`, etc.).
   * **`bit_field3_` (`Map::Bits3`)**: `EnumLengthBits` (10 bits), `NumberOfOwnDescriptorsBits` (10 bits, up to `kMaxNumberOfDescriptors = 1023`), `IsPrototypeMapBit`, `IsDictionaryMapBit`, `OwnsDescriptorsBit`, `IsInRetainedMapListBit`, `IsDeprecatedBit`, `IsUnstableBit`, `IsMigrationTargetBit`, `IsExtensibleBit` (bit 27), `MayHaveInterestingPropertiesBit`, and `ConstructionCounterBits` (3 bits for in-object slack tracking).

### `ExtendedMap` (`src/objects/map.h`)
Some specialized maps need additional metadata fields beyond `Map::kSize`:
* **`ExtendedMap`** (`V8_ABSTRACT_OBJECT class ExtendedMap : public Map`) sets `Map::Bits1::IsExtendedMapBit` (bit 6 of `bit_field_`) to `true` and appends a `uint8_t bit_field_ex_` right after `transitions_or_prototype_info_`.
* `bit_field_ex_` (`ExtendedMap::BitsEx`) encodes two bitfields:
  * `MapKindBits` (3 bits, `ExtendedMapKind` enum: e.g., `kContextfulMetaMap`, `kPrototypeSharedClosureInfoMap`), and
  * `MapSizeInWordsBits` (5 bits), storing the extended map's own size in words so the GC and snapshot deserializer (`kExtendedMapBitFieldEx`) know the exact size of the `ExtendedMap` instance.

---

## 3. JavaScript Object Layout (`JSObject`)

A standard `JSObject` (`src/objects/js-objects.h`) consists of three 1-word header fields followed immediately by zero or more **In-Object Properties**:

```
JSObject Memory Layout
+----------------------------------------------------+  offset 0
| map_          (TaggedMember<Map>)                  |
+----------------------------------------------------+  offset kTaggedSize
| properties_or_hash_ (PropertyArray | SwissNameDict |
|                      | NameDictionary | Smi hash)  |
+----------------------------------------------------+  offset 2 * kTaggedSize
| elements_     (FixedArrayBase: FixedArray |        |
|                FixedDoubleArray | NumberDictionary)|
+----------------------------------------------------+  offset JSObject::kHeaderSize
| In-Object Property 0                               |
| In-Object Property 1                               |
| ...                                                |
+----------------------------------------------------+  offset instance_size()
```

### Fast vs. Dictionary Properties
1. **Fast Mode (`!map->is_dictionary_map()`)**:
   * Property names, attributes (`READ_ONLY`, `DONT_ENUM`, `DONT_DELETE`), `PropertyConstness` (`kConst` vs. `kMutable`), `Representation` (`Smi`, `Double`, `HeapObject`, `Tagged`), and field indices are stored linearly in the `Map`'s **`DescriptorArray`** (`instance_descriptors_`).
   * **In-Object Properties** are stored directly inside the `JSObject` body starting at `GetInObjectPropertyOffset(0)`.
   * **Out-of-Object Properties** (when in-object slots fill up) overflow into the `PropertyArray` referenced by `properties_or_hash_`.
   * **Map Transitions & `MapUpdater` (`src/objects/map-updater.h`)**: Adding a property transitions the `JSObject`'s `Map` along the **Transition Tree** (`transitions_or_prototype_info_`), sharing `DescriptorArray` prefixes across sibling/child maps as long as descriptor additions remain linear. When an existing property's attributes, constness, or field `Representation` / `FieldType` must be generalized in place (or when migrating an instance of a deprecated map), **`MapUpdater`** walks back to the root map and replays the transition tree forward with the generalized descriptors, deprecating outdated intermediate maps (`IsDeprecatedBit`) and setting `IsMigrationTargetBit` on the canonical target map.
2. **Dictionary Mode (`map->is_dictionary_map()`)**:
   * When an object has too many properties (`> kMaxNumberOfDescriptors`), deletes properties frequently, or undergoes non-standard shape changes, it converts to a dictionary map.
   * `properties_or_hash_` points to a **`SwissNameDictionary`** (or **`NameDictionary`**), mapping property names directly to values and `PropertyDetails` without mutating the `Map`.
3. **Global Objects (`JSGlobalObject` & `GlobalDictionary`)**:
   * Global objects always use a **`GlobalDictionary`** (`src/objects/dictionary.h`) for their properties.
   * Unlike `NameDictionary` (which stores property values inline in the table), each entry in a **`GlobalDictionary` stores a `PropertyCell`** (`src/objects/property-cell.h`).
   * Because a `PropertyCell` has a stable heap address and tracks its own `PropertyCellType` (`kConstant`, `kConstantType`, `kMutable`, `kUndefined`), baseline and optimizing compilers (Sparkplug, Maglev, TurboFan) can embed direct references to the `PropertyCell` in generated code and register `DependentCode` on the cell to deoptimize if a global constant is overwritten or deleted.

---

## 4. Elements Kinds (`ElementsKind`)

V8 stores integer-indexed array and object elements (`obj[0]`, `obj[1]`, ...) in the `elements_` backing store separately from named properties. The `Map`'s `ElementsKind` (`src/objects/elements-kind.h`) tracks the tightness of the backing store:

```
PACKED_SMI_ELEMENTS      --->  HOLEY_SMI_ELEMENTS
        |                              |
        v                              v
PACKED_DOUBLE_ELEMENTS   --->  HOLEY_DOUBLE_ELEMENTS
        |                              |
        v                              v
PACKED_ELEMENTS          --->  HOLEY_ELEMENTS          --->  DICTIONARY_ELEMENTS
```

* **`PACKED_*` vs. `HOLEY_*`**: In a `PACKED` array, every index in `[0, length)` contains a valid element. In a `HOLEY` array, unassigned indices contain the `the_hole` sentinel (or `kHoleNanInt64` in `FixedDoubleArray`), requiring prototype-chain lookups when read if the `NoElementsProtector` is invalid.
* **`*_SMI_ELEMENTS` / `*_ELEMENTS`**: Backed by a `FixedArray` of `TaggedMember<Object>`.
* **`*_DOUBLE_ELEMENTS`**: Backed by a `FixedDoubleArray` of raw 64-bit IEEE-754 doubles (avoiding `HeapNumber` allocations).
* **`DICTIONARY_ELEMENTS`**: Backed by a `NumberDictionary` hash table for sparse arrays (e.g., `const a = []; a[1000000] = 1`).
* **Elements Transition Direction**: Elements kinds only transition **downward** in the lattice (from more specific to more general, e.g., `PACKED_SMI_ELEMENTS` to `PACKED_DOUBLE_ELEMENTS` to `PACKED_ELEMENTS`). V8 never transitions a `JSArray`'s map back up to a more specific `ElementsKind`.

---

## 5. GC Body Visitation (`BodyDescriptor` in `src/objects/objects-body-descriptors.h`)

To scan or update pointers inside a `HeapObject` without virtual function calls on objects, every concrete `V8_OBJECT` class defines a `BodyDescriptor` (`ObjectTraits<T>::BodyDescriptor`):
* **`FixedBodyDescriptor<kStartOffset, kEndOffset, kSize>`**: Visits all `TaggedMember` slots in `[kStartOffset, kEndOffset)` on a fixed-size object.
* **`FlexibleBodyDescriptor<kStartOffset>`**: Visits all `TaggedMember` slots from `kStartOffset` up to `object->SizeFromMap(map)`.
* **`DataOnlyBodyDescriptor`**: No-op visitor for objects containing only raw bytes/numbers (`SeqString`, `HeapNumber`, `BigInt`, `ByteArray`).
* **Custom `BodyDescriptor`s (`src/objects/objects-body-descriptors-inl.h`)**: Used by objects that mix `TaggedMember`, `TrustedPointerMember` (`VisitTrustedPointer`), `ProtectedTaggedMember` (`VisitProtectedPointer`), `JSDispatchHandleMember` (`VisitJSDispatchEntry`), `ExternalPointerSlot` (`VisitExternalPointer`), or `CppHeapPointerSlot` (`VisitCppHeapPointer`).
