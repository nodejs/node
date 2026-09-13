# Field Representations and Elements Kinds

This document explains how V8 optimizes object properties and array elements by specializing their storage representation based on the types of values they contain.

## Overview

JavaScript is dynamically typed, but V8 attempts to treat objects as if they had static types to improve performance. Two key mechanisms for this are **Field Representations** (for named properties) and **Elements Kinds** (for indexed properties/array elements).

---

## Field Representations

When properties are stored in the **Descriptor Array** of a Map, V8 tracks the **Representation** of each data property. This allows V8 to avoid boxing numbers or storing full pointers when not necessary.

### Representation Types
Defined in `src/objects/property-details.h`, the main representations are:

*   **`kSmi`**: The property always holds a Small Integer (Smi). Stored directly in the object without allocation.
*   **`kDouble`**: The property holds a double-precision float. It is stored as a boxed `HeapNumber` (in-object unboxing is no longer supported). Tracking this representation allows V8 to avoid type checks. Furthermore, the boxed number is allowed to be mutated in-place on store (normally `HeapNumber`s are immutable), avoiding allocation. However, reading it requires a copy unless in optimized code that can handle raw float64 values.
    *   *Note on Size*: Fields can be double-sized (even without pointer compression), in which case they take up 8 bytes but are still referenced by a single descriptor.
*   **`kHeapObject`**: The property always holds a reference to a heap object. It can store a specific "field type" in the property descriptor (i.e., the expected `Map` of the field), or it can be a generic non-Smi `HeapObject`.
*   **`kWasmValue`**: Used for WasmObject fields. It indicates that the actual field type information must be taken from the Wasm RTT (Runtime Type) associated with the map.
*   **`kTagged`**: The most general representation. It can hold any valid JavaScript value (Smi or HeapObject).
*   **`kNone`**: Uninitialized property.

### PropertyDetails
Every property has an associated `PropertyDetails` value (a 32-bit integer) that packs:
*   **Kind**: Data or Accessor.
*   **Location**: Field (in object or property array) or Descriptor (stored in the Map itself).
*   **Constness**: Mutable or Const.
*   **Representation**: As listed above.

#### Deep Dive: PropertyDetails Bit Layout

V8 packs this information tightly into a 32-bit integer. The layout differs between **fast mode** (using descriptor arrays) and **slow mode** (dictionary properties).

**For Fast Mode Properties:**
*   **Kind** (Data vs Accessor): 1 bit
*   **Constness** (Mutable vs Const): 1 bit
*   **Attributes** (ReadOnly, DontEnum, DontDelete): 3 bits
*   **Location** (Field vs Descriptor): 1 bit
*   **Representation** (None, Smi, Double, HeapObject, Tagged): 3 bits
*   **Descriptor Pointer**: 10 bits (index in the descriptor array)
*   **Field Offset In Words**: 11 bits (offset in storage header)
*   **In-Object**: 1 bit (whether stored directly in JSObject)

**For Dictionary Mode Properties:**
*   **Kind**: 1 bit
*   **Constness**: 1 bit
*   **Attributes**: 3 bits
*   **PropertyCellType**: 3 bits (Mutable, Undefined, Constant, ConstantType, InTransition, NoCell)
*   **Dictionary Storage Index**: 23 bits (enumeration index)

This bit-packing allows V8 to pass property metadata efficiently and perform quick checks using bitmasks.

### Generalization vs. Transitions

It is important to distinguish between **Generalization** (representation changes) and **Transitions** (adding fields):
*   **Transition**: Occurs when a new property is added to an object, leading to a new Map.
*   **Generalization**: Occurs when the value assigned to an *existing* property requires a broader representation (e.g., storing a double in a field previously marked as `kSmi`).

If a property is initialized as a `Smi` and later assigned a `Double`, V8 will **generalize** the representation. This may require **Map Deprecation** (marking the old map as invalid for new objects) and creating a new map with the generalized representation. Objects with the old deprecated map are not updated immediately; instead, they are lazily migrated to the new map when they are next accessed or mutated. V8 cannot generalize "backwards" to a more specific representation.

> [!NOTE]
> A field representation change (generalization) is one of the ways to trigger a **Lazy Deoptimization** in optimized code that relied on the more specific representation. See [Deoptimization](../runtime/deoptimization.md) for details.

> Some representation changes can be done **in-place** without deprecating the map. Specifically, generalizing from `Smi`, `Double`, or `HeapObject` to `Tagged` can be done in-place. However, changing representation from `Smi` to `Double` requires deprecation because doubles might require a box allocation (e.g., `HeapNumber`).

### Slack Tracking

When V8 allocates a new object instance, it often allocates more space than currently needed for properties (in-object slack).
*   **Purpose**: To allow adding more properties without needing to resize the object or allocate an out-of-object property array.
*   **Mechanism**: V8 tracks the number of properties added to instances of a specific map. After a certain number of allocations (typically 7), V8 determines the "actual" number of properties needed and stops slack tracking.
*   **Result**: Future instances are allocated with the exact size needed, and any unused slack in existing instances is filled with a filler object (which can be reclaimed by the GC if it is at the end of the object).

---

## Property Locations: Fields vs. Descriptors

In addition to representation, V8 tracks *where* a property's value is stored, defined by `PropertyLocation` in `src/objects/property-details.h`:

*   **`kField`**: The value is stored in the object instance itself.
    *   **In-Object**: Stored directly within the `JSObject` memory layout at a fixed offset.
    *   **Out-of-Object**: Stored in a separate `PropertyArray` pointed to by the object.
*   **`kDescriptor`**: The value is stored in the **Descriptor Array** attached to the `Map`.
    *   This is an optimization for values that are constant across all instances sharing the map.
    *   **Data Constants**: If a property is assigned a value that V8 determines is constant, it can store the value directly in the descriptor array, saving space in every instance.
    *   **Accessor Constants**: Getters and setters (methods or `AccessorPair`s) are typically stored here. All instances share the same accessor function references.

By combining `PropertyKind` (Data vs. Accessor) and `PropertyLocation` (Field vs. Descriptor), V8 can represent:
*   **Data Field**: Standard property stored in the instance.
*   **Data Descriptor**: Constant property stored in the map.
*   **Accessor Descriptor**: Getter/Setter stored in the map.

---

## Elements Kinds

For indexed properties (arrays), V8 uses **Elements Kinds** to specialize the backing store (`FixedArray` or `FixedDoubleArray`) and optimize operations like `map`, `reduce`, and `forEach`.

### Common Elements Kinds
Defined in `src/objects/elements-kind.h`, the most common "fast" kinds are:

*   **`PACKED_SMI_ELEMENTS`**: Array contains only Smis and has no holes. Backed by a `FixedArray`.
*   **`HOLEY_SMI_ELEMENTS`**: Contains only Smis but has missing indices (holes).
*   **`PACKED_DOUBLE_ELEMENTS`**: Contains only unboxed doubles. Backed by a `FixedDoubleArray`. Highly efficient for numerical work.
*   **`HOLEY_DOUBLE_ELEMENTS`**: Contains unboxed doubles but has holes.
*   **`PACKED_ELEMENTS`**: Contains arbitrary JS objects (tagged values). Backed by a `FixedArray`.
*   **`HOLEY_ELEMENTS`**: Contains arbitrary JS objects and has holes.

### Other Elements Kinds
For completeness, V8 also defines elements kinds for special cases:
*   **Non-extensible, Sealed, and Frozen Elements**: Used when objects are made non-extensible, sealed, or frozen (e.g., `PACKED_FROZEN_ELEMENTS`, `HOLEY_SEALED_ELEMENTS`).
*   **Sloppy Arguments Elements**: `FAST_SLOPPY_ARGUMENTS_ELEMENTS` and `SLOW_SLOPPY_ARGUMENTS_ELEMENTS` are used for `arguments` objects in sloppy mode.
*   **String Wrapper Elements**: `FAST_STRING_WRAPPER_ELEMENTS` and `SLOW_STRING_WRAPPER_ELEMENTS` are used for string wrapper objects.

### Dictionary Mode Elements

When an array becomes very sparse or has a large number of elements, V8 may switch from a flat backing store (`FixedArray` or `FixedDoubleArray`) to a dictionary-based representation (`NumberDictionary`).
*   **`DICTIONARY_ELEMENTS`**: Elements are stored in a hash table. This saves memory for sparse arrays but makes access slower.

### Packed vs. Holey
*   **Packed**: Every index from `0` to `length-1` is initialized. Accessing elements is direct and fast.
*   **Holey**: Some indices are missing (holes). Accessing a hole requires V8 to perform a costly lookup up the prototype chain to see if a value is defined there.

### Elements Kind Transitions
Elements kinds only transition from more specific to more general through a **lattice**. Once transitioned, they rarely go back:
*   `PACKED_SMI` -> `PACKED_DOUBLE` -> `PACKED_ELEMENTS`
*   Any `PACKED` kind can transition to its `HOLEY` counterpart.

> [!NOTE]
> While conceptually elements kinds form a lattice of generalization, V8's implementation in the transition tree linearizes transitions for fast elements kinds to keep the tree simple and avoid path explosion. The linear sequence used is:
> `PACKED_SMI` -> `HOLEY_SMI` -> `PACKED_DOUBLE` -> `HOLEY_DOUBLE` -> `PACKED_ELEMENTS` -> `HOLEY_ELEMENTS`
> This means V8 may create intermediate transition maps (e.g., creating a `HOLEY_SMI` map when transitioning from `PACKED_SMI` to `PACKED_DOUBLE`) even if they are not strictly needed for the final representation.

**Examples of Transitions:**
```javascript
const array = [1, 2, 3]; // PACKED_SMI_ELEMENTS
array.push(4.56);        // Transitions to PACKED_DOUBLE_ELEMENTS
array.push('x');         // Transitions to PACKED_ELEMENTS
array[9] = 1;            // Transitions to HOLEY_ELEMENTS (indices 5-8 are holes)
```


## File Structure
*   `src/objects/property-details.h`: `Representation` and `PropertyDetails` definitions.
*   `src/objects/elements-kind.h`: `ElementsKind` enum and helper predicates.
