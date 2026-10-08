# LookupIterator Architecture

This document explains the design and functionality of the `LookupIterator` in V8, which is the central mechanism for resolving property accesses on JavaScript objects.

## Overview

The `LookupIterator` (`src/objects/lookup.h`) is a stateful iterator used to implement the ECMAScript specification's property lookup rules. It abstracts away the differences between:
*   Named properties vs. indexed elements.
*   Fast mode vs. dictionary mode objects.
*   In-object vs. out-of-object storage.
*   Prototype chain walking.
*   Special objects like JSProxies and Interceptors.

Whenever V8 needs to get, set, or delete a property and cannot do it via a fast Inline Cache (IC) hit, it falls back to the `LookupIterator`.

## The State Machine

The iterator operates as a state machine. Calling `Next()` advances the iterator to the next state or the next object in the prototype chain. States include:

*   **`NOT_FOUND`**: The property was not found. This is a terminal state.
*   **`STRING_LOOKUP_START_OBJECT`**: Initial state for lookup starting from a primitive string. Avoids wrapper object creation.
*   **`TYPED_ARRAY_INDEX_NOT_FOUND`**: Typed arrays return undefined immediately for invalid integer indices without walking the prototype chain.
*   **`ACCESS_CHECK`**: The object requires an access check (e.g., cross-origin iframe access) before proceeding.
*   **`INTERCEPTOR`**: API-level hooks for optionally handling a lookup in embedder code.
*   **`JSPROXY`**: The current holder is a `JSProxy`. The lookup must be forwarded to the proxy's handler.
*   **`ACCESSOR`**: The property is an accessor (getter/setter) defined by an `AccessorPair` or `AccessorInfo`.
*   **`DATA`**: The property was found as a data property (field or constant) in the current `holder`.
*   **`WASM_OBJECT`**: WasmGC objects are opaque in JS and appear to have no properties.
*   **`MODULE_NAMESPACE`**: The property is accessed on a deferred module namespace.
*   **`TRANSITION`**: Intermediate state during a data property write (not observed during lookup).

## Configuration

The iterator can be configured via `LookupIterator::Configuration`, which consists of bit flags and convenience combinations:

*   **`kInterceptor`**: Consult embedder interceptors.
*   **`kPrototypeChain`**: Walk the prototype chain.

Convenience combinations:
*   **`OWN_SKIP_INTERCEPTOR`**: Only look at the receiver, skip interceptors.
*   **`OWN`**: Only look at the receiver, consult interceptors. (Equivalent to `kInterceptor`).
*   **`PROTOTYPE_CHAIN_SKIP_INTERCEPTOR`**: Full chain, skip interceptors. (Equivalent to `kPrototypeChain`).
*   **`PROTOTYPE_CHAIN`**: Full chain, consult interceptors. (Equivalent to `kPrototypeChain | kInterceptor`).
*   **`DEFAULT`**: Same as `PROTOTYPE_CHAIN`.

## Deep Dive: The Lookup Process

The core logic of `LookupIterator` is implemented in `src/objects/lookup.cc`. Let's examine the key phases:

### 1. Start()
When the iterator is created, it calls `Start()`.
*   **Fast Path**: If the lookup start object is a `JSReceiver`, it reads its `Map` and calls `LookupInHolder`.
*   **Primitive Wrappers**: If the receiver is a primitive (like a string), V8 avoids creating a wrapper object if possible. For strings, it directly checks for the `"length"` property or valid index lookups within the string's length. If not found, it falls back to the prototype of the primitive (e.g., `String.prototype`).

### 2. LookupInHolder()
This method checks the current `holder` (the object currently being inspected in the chain) for the property.
*   **Fast Objects**: It searches the **Descriptor Array** of the holder's Map using a binary search or linear scan depending on size.
*   **Dictionary Objects**: It probes the hash table (Property Dictionary) directly.
*   If found, it sets the state to `DATA` or `ACCESSOR` and fills in `PropertyDetails`.

### 3. Next() and NextInternal()
If the property is not found in the current holder, `Next()` is called to move up the prototype chain.
*   **Prototype Walking**: `NextInternal` calls `NextHolder(map)` to get the prototype of the current map.
*   **Interceptors**: If a holder has an interceptor, the state becomes `INTERCEPTOR`. V8 may need to invoke embedder callbacks.
    *   **Masking Interceptors**: Can hide properties on the prototype chain.
    *   **Non-Masking Interceptors**: Cannot hide properties. If a lookup fails on the chain, V8 may restart the lookup to check non-masking interceptors.

### 4. Property Modification and Transitions
The `LookupIterator` is not just for reading; it also prepares for writes.
*   **`PrepareForDataProperty`**: Called before writing to an existing data property. It handles:
    *   **Elements Kind Transitions**: If writing a double to a SMI array, it transitions the array to `DOUBLE_ELEMENTS`.
    *   **Representation Generalization**: If writing a double to a SMI field, it triggers a map transition to generalize the field representation.
*   **`PrepareTransitionToDataProperty`**: Called when adding a *new* property. It finds or creates a new Map with the added property and sets the iterator state to `TRANSITION`.

## Elements vs. Properties

The `LookupIterator` handles both named properties and indexed elements by templating key methods with a `bool is_element` parameter.
*   For elements, it bypasses the descriptor array and goes directly to the `ElementsAccessor` of the object to check the backing store.
*   Typed arrays have special handling where out-of-bounds accesses immediately return undefined (via `TYPED_ARRAY_INDEX_NOT_FOUND` state) without walking the prototype chain.

## ConcurrentLookupIterator

V8 also provides a `ConcurrentLookupIterator` (`src/objects/lookup.h`) for concurrent accesses from a background thread. Despite the name, it is currently an all-static class and not a stateful iterator. It implements safe lookups for specific cases like own data property lookup on fixed COW arrays, strings, and property cells in global objects, handling concurrency issues defensively.

## File Structure
*   `src/objects/lookup.h`: Main header file defining `LookupIterator` and its states.
*   `src/objects/lookup.cc`: Implementation of the lookup logic, state transitions, and property accessors.
