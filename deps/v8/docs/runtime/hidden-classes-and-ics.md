# Hidden Classes and Inline Caches in V8

V8 uses a combination of **Hidden Classes (Maps)** and **Inline Caches (ICs)** to achieve high performance in property access and method calls, despite JavaScript being a dynamic language.

## Hidden Classes (Maps)

As detailed in [Objects and Maps](../heap/objects-and-maps.md), V8 attaches a `Map` (hidden class) to every JavaScript object. The Map defines the shape of the object, mapping property names to their offsets in memory.

When objects share the same structure (same properties added in the same order), they share the same Map. This sharing is key to the effectiveness of Inline Caches.

### Map Transition Tree

Adding properties to an object transitions it to a new Map. V8 builds a transition tree to reuse Maps when objects follow the same transition path. Note that the order of property addition matters:

```dot
digraph G {
    rankdir=TB;
    newrank=true;
    node [shape=record];
    edge [];

    Empty [label="Map 0: {}", class="start"];
    MapA [label="Map 1: {a: @0}", class="branch"];
    MapAB [label="Map 2: {a: @0, b: @1}", class="leaf"];
    MapB [label="Map 3: {b: @0}", class="branch"];
    MapBA [label="Map 4: {b: @0, a: @1}", class="leaf"];

    Empty -> MapA [label="+a"];
    MapA -> MapAB [label="+b"];
    
    Empty -> MapB [label="+b"];
    MapB -> MapBA [label="+a"];
}
```

## Inline Caches (ICs)

Inline Caching is a technique for optimizing dynamic property lookups. Without ICs, V8 would have to perform a dictionary lookup or search the descriptor array of the map every time a property is accessed (e.g., `obj.x`).

### How ICs Work

An IC caches the result of a property lookup at the specific location in the code (the call site) where the access occurs.

1.  **Uninitialized State**: When a line of code like `return obj.x` is executed for the first time, the IC at that site is `UNINITIALIZED`.
2.  **First Access (Miss)**: V8 performs a slow lookup. It checks the object's Map, finds property `x` in the `DescriptorArray`, and gets its offset.
3.  **Caching**: V8 records the object's current Map and the calculated handler (e.g., the field offset or a getter function) in the **Feedback Vector** associated with the current function. The IC transitions to the `MONOMORPHIC` state.
4.  **Subsequent Accesses (Hit)**: The next time this code is executed:
    *   V8 compares the object's current Map with the Map stored in the IC.
    *   If they match (a Hit), V8 uses the cached handler directly to access the property. This is extremely fast.
    *   If they don't match (a Miss), V8 falls back to a slower lookup and updates the IC.

### IC States

An IC call site can be in one of several states (defined in `src/common/globals.h` as `InlineCacheState`):

*   **`NO_FEEDBACK`**: Used when no feedback should be collected (e.g., in certain debugger scenarios or for always-slow operations).
*   **`UNINITIALIZED`**: The call site has never been executed.
*   **`MONOMORPHIC`**: The IC has seen only one receiver Map. This is the most optimized state.
*   **`RECOMPUTE_HANDLER`**: Check failed due to prototype (or map deprecation).
*   **`POLYMORPHIC`**: The IC has seen a small number of different receiver Maps (typically up to 4). It stores a list of Map-handler pairs and checks them linearly.
*   **`MEGADOM`**: Many DOM receiver types have been seen for the same accessor.
*   **`HOMOMORPHIC`**: Many receiver types have been seen with the same handler.
*   **`MEGAMORPHIC`**: The IC has seen many different receiver Maps. It falls back to a global stub cache or a slower, generic lookup path to avoid excessive memory use in the feedback vector.
*   **`GENERIC`**: A generic handler is installed and no extra typefeedback is recorded.

## The Feedback Vector and Slots

As mentioned above, IC state is stored in the `FeedbackVector`.

### Structure
*   A `FeedbackVector` is associated with a `JSFunction` (instance).
*   The `SharedFunctionInfo` (shared across instances) contains `FeedbackMetadata` which describes the layout of the vector.
*   It contains an array of slots (`raw_feedback_slots`), defined in `src/objects/feedback-vector.tq`.
*   Each IC site in the bytecode has a corresponding slot index in this vector.

### FeedbackNexus
To avoid manual bit-twiddling and handle the complexity of different slot kinds, V8 uses `FeedbackNexus` (defined in `src/objects/feedback-vector.h`). This is a helper class that knows how to read and write feedback for a specific slot.

## Handler Encoding

A common misconception is that Inline Caches always cache a pointer to a generated machine code stub. In modern V8, this is often not the case to save memory. Instead, V8 uses **Handlers** which can be encoded in different ways:

### 1. Smi Handlers
For simple field accesses, the handler is often just a tagged integer (Smi). The encoding depends on the `Kind` of access. For a simple field load (`kField`), the bits of the Smi encode:
*   **Kind**: Encoded in the lowest bits (e.g., `kField`).
*   **IsWasmStruct**: Distinguishes JS objects from Wasm structs.
*   **Storage Offset**: The offset of the field in words.
*   **In-object vs. Out-of-object**: Whether the property is stored directly in the object or in the properties backing store (`IsInobjectBits`).
*   **IsDouble**: Whether the field stores a double value.
*   **Descriptor Index**: The index in the descriptor array.

The `AccessorAssembler` (CSA) decodes these Smis directly in assembly to perform the access.

### 2. DataHandler Objects
For more complex scenarios, such as accessing a property on a prototype object, V8 uses a `DataHandler` heap object (defined in `src/objects/data-handler.tq` and `src/objects/data-handler.h`). This object extends `Struct` and has a variable size to store optional data fields. It contains:
*   `smi_handler`: A Smi encoding the handler (like above) or a `Code` object.
*   `validity_cell`: A Smi or a `Cell` to check if the prototype chain is still valid.
*   `data1`, `data2`, `data3`: Optional data fields (flexible array member in C++) depending on the handler kind.

For prototype loads, `LoadHandler::LoadFromPrototype` creates a `LoadHandler` (which extends `DataHandler`) and typically stores:
*   The **holder** object in the `data1` field (as a weak reference).
*   The final access handler in `smi_handler`.

For interceptors, `LoadHandler::LoadInterceptorHolderIsLookupStartupObject` stores the `InterceptorInfo` in the `data2` field.

## The Fast Path: AccessorAssembler

The code that actually executes when an IC site is hit is generated by `AccessorAssembler` (defined in `src/ic/accessor-assembler.cc`). This class uses CodeStubAssembler (CSA) to generate highly optimized machine code for:
*   `LoadIC`, `StoreIC`, `KeyedLoadIC`, `KeyedStoreIC`.
*   Probing the `FeedbackVector`.
*   Handling monomorphic and polymorphic cases.
*   Falling back to the runtime on a miss.

## Megamorphic Stub Cache

When an IC site sees too many different Maps (becomes megamorphic), storing all of them in the `FeedbackVector` would consume too much memory.

Instead, V8 falls back to a global **Stub Cache** (defined in `src/ic/stub-cache.h`).
*   It is a fixed-size hash table.
*   It is keyed by the receiver's **Map** and the property **Name**.
*   It stores the handler directly.

If the `AccessorAssembler` misses in the `FeedbackVector` for a megamorphic site, it probes this global stub cache. This provides a fast path even for highly polymorphic call sites, without bloating the local feedback vector.

## See Also
-   [Objects and Maps](../heap/objects-and-maps.md)
-   [Descriptor and Transition Arrays](../objects/descriptor-and-transition-arrays.md)
-   [Maps (Hidden Classes) Tutorial](hidden-classes-tutorial.md)
