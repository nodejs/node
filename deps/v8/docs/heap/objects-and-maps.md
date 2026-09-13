# Objects and Maps in V8

This document describes how JavaScript objects are represented in V8's heap and how "Maps" (also known as hidden classes or shapes) are used to describe their structure.

## JSObject Layout

All standard JavaScript objects in V8 are represented by the `JSObject` class (or its subclasses). A `JSObject` typically has the following memory layout:

1.  **Map**: A pointer to the `Map` object that describes the structure of this object.
2.  **Properties**: A pointer to a backing store for named properties. This can be:
    -   `EmptyFixedArray` (if no properties).
    -   A `Smi` representing the identity hash code.
    -   A `PropertyArray` for fast properties.
    -   A `NameDictionary` (or `SwissNameDictionary`) for slow (dictionary mode) properties.
    -   A `GlobalDictionary` for properties of global objects.
3.  **Elements**: A pointer to a backing store for indexed elements (array indices). This can be:
    -   `FixedArray` for fast elements.
    -   `NumberDictionary` for slow elements.
4.  **In-Object Properties**: Properties that are stored directly within the `JSObject` instance itself, after the header, for faster access.

## The Map (Hidden Class)

Every `HeapObject` in V8 has a pointer to a `Map` as its first field. The `Map` is crucial for performance as it enables Inline Caching (IC) for property access.

A `Map` contains:
-   **Instance Size**: The size of the object in bytes.
-   **Instance Type**: What kind of object it is (e.g., `JS_OBJECT_TYPE`, `JS_ARRAY_TYPE`, `STRING_TYPE`).
-   **Visitor ID**: Used by the garbage collector to know how to iterate over the object's pointers.
-   **Property Descriptors**: A pointer to a `DescriptorArray` that describes the names and locations of the object's properties.
-   **Transitions**: Information about how the map changes when new properties are added (see below).
-   **Prototype**: A pointer to the object's prototype.

### Simplified Map Layout
(Based on `src/objects/map.h`)

-   `instance_size` (Byte)
-   `inobject_properties_start_or_constructor_function_index` (Byte)
-   `used_or_unused_instance_size_in_words` (Byte)
-   `visitor_id` (Byte)
-   `instance_type` (Short)
-   `bit_field`, `bit_field2`, `bit_field3` (Bytes/Int)
-   `prototype` (TaggedPointer)
-   `constructor_or_back_pointer_or_native_context` (TaggedPointer)
-   `instance_descriptors` (TaggedPointer)
-   `dependent_code` (TaggedPointer)
-   `prototype_validity_cell` (TaggedPointer)
-   `transitions` or `prototype_info` (TaggedPointer)

## Named Properties Storage Modes

V8 uses three different modes to store named properties, balancing access speed and modification flexibility:

### 1. In-Object Properties
*   **Description**: Properties stored directly within the `JSObject` memory layout itself.
*   **Performance**: The fastest access available, as it requires no indirection.
*   **Limit**: The number of in-object properties is predetermined by the initial size of the object when created.

### 2. Fast Properties (Out-of-Object)
*   **Description**: Properties stored in a linear `PropertyArray` pointed to by the object.
*   **Performance**: Adds one level of indirection compared to in-object properties.
*   **Mechanism**: To access a property, V8 looks up the property name in the `DescriptorArray` attached to the `Map` to find its index in the `PropertyArray`.
*   **Scalability**: The `PropertyArray` can grow independently of the object size.

### 3. Slow Properties (Dictionary Mode)
*   **Description**: Properties stored in a self-contained hash table (`NameDictionary` or `SwissNameDictionary`).
*   **Note**: `SwissNameDictionary` is an alternative representation that is currently disabled by default and controlled by the `v8_enable_swiss_name_dictionary` GN argument.
*   **Performance**: Slower to access because inline caches do not work with dictionary properties.
*   **Use Case**: Used when an object has many properties added and deleted, or when properties are deleted (which disrupts the linear transition tree).
*   **Benefits**: Metadata is stored directly in the dictionary rather than being shared via the `Map` and `DescriptorArray`. This avoids creating a massive number of maps for highly dynamic objects.

---

## ElementsAccessor and CRTP

To avoid duplicating the implementation of array methods (like `slice`, `map`, etc.) for all 20+ different elements kinds, V8 uses a C++ template pattern called **CRTP** (Curiously Recurring Template Pattern) in `ElementsAccessor` (see `src/objects/elements.cc`).
*   V8 defines the generic logic once.
*   CRTP generates specialized, highly-optimized versions of the array methods for each specific elements kind.

## Map Transitions

When a new property is added to an object in fast mode, V8 creates a new map for the object. The old map has a "transition" pointing to the new map for that specific property name. This forms a transition tree, allowing V8 to share maps among objects with the same structure.

## See Also
-   [Pointer Tagging](pointer-tagging.md)
-   [Descriptor and Transition Arrays](../objects/descriptor-and-transition-arrays.md) (for more details on descriptors and transitions).
