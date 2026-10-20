# Strings in V8

Strings are a fundamental data type in JavaScript, and V8 uses a complex hierarchy of string representations to optimize various operations like concatenation, slicing, and internalization.

## String Representation Hierarchy

All strings in V8 inherit from the `String` class (defined in `src/objects/string.h`). V8 uses different concrete classes depending on how the string was created and how it is used.

### 1. Sequential Strings (`SeqString`)
Captures sequential string values where the characters are stored directly in the object.
*   **`SeqOneByteString`**: Characters are stored as 8-bit Latin-1 code units. Used for ASCII-like strings.
*   **`SeqTwoByteString`**: Characters are stored as 16-bit UTF-16 code units. Used for strings containing non-Latin-1 characters.

### 2. Cons Strings (`ConsString`)
Describes string values built by using the addition operator (`+`) on strings.
*   Instead of copying characters immediately, a `ConsString` is a pair of pointers to the two constituent strings.
*   This creates a binary tree of strings, where the leaves are non-Cons strings.
*   **Benefit**: Fast concatenation without copying.
*   **Flattening**: When a `ConsString` is read or becomes too deep, V8 may "flatten" it by allocating a sequential string and copying the characters into it.
*   **Minimum Size**: Cons strings have a minimum size. Very short concatenations may result in a sequential string instead of a cons string to avoid the overhead of small trees.

### 3. Sliced Strings (`SlicedString`)
Describes strings that are substrings of another sequential string.
*   Instead of copying characters for `substr()` or `slice()`, a `SlicedString` contains a pointer to the parent string, an offset, and a length.
*   **Benefit**: Fast slicing without copying.
*   **Limitation**: Keeps the parent string alive in memory, even if only a small slice is needed.

### 4. Thin Strings (`ThinString`)
Describes string objects that are just references to another string object.
*   They are used for **in-place internalization** when the original string cannot actually be internalized in-place.
*   In these cases, the original string is converted to a `ThinString` pointing at its internalized version (which is allocated as a new object).
*   In terms of memory layout, they can be thought of as "one-part cons strings".
*   **Benefit**: Avoids updating all handles pointing to the original string when it is internalized.
*   **GC Behavior**: The GC *may* (but might not) patch pointers to thin strings to instead point directly to the internalized string, eventually allowing the thin string to be reclaimed.

### 5. External Strings (`ExternalString`)
Describes string values that are backed by a string resource that lies outside the V8 heap (e.g., in the embedder like Chrome or Node.js).
*   V8 must ensure that the resource is not deallocated while the `ExternalString` is live.
*   They come in one-byte and two-byte variants, similar to sequential strings. V8 accesses the characters directly from the external resource, avoiding copying the data into the V8 heap.

## String Transitions and Internalization

### Internalization
When a string is used as a property key (e.g., `obj["prop"]`), V8 **internalizes** it. This means it ensures there is only one unique instance of that string value in the **String Table** (a hash table).
*   If the string is already internalized, it returns the existing instance.
*   If not, it adds it to the table.
*   If a `SeqString` is internalized, it might be changed to an `InternalizedString` in place if possible.
*   If it cannot be changed in place (e.g., if it's a `ConsString`), V8 creates a new `InternalizedString` and converts the original string into a `ThinString` pointing to the new one.

### Flattening
As mentioned above, `ConsString` instances are tree structures. To read characters efficiently or pass them to APIs that expect flat buffers, V8 will flatten the tree into a single `SeqString`.

## String Instance Types and Bitfield

V8 uses the `InstanceType` field in the object Map to identify the specific representation and encoding of a string. For strings, the high-order bits (bits 7-15) are cleared, and the lower bits form a bitfield:

*   **Bits 0-2 (Representation)**:
    *   `000`: Sequential String
    *   `001`: Cons String
    *   `010`: External String
    *   `011`: Sliced String
    *   `101`: Thin String
*   **Bit 3 (Encoding)**:
    *   `0`: Two-Byte (UTF-16)
    *   `1`: One-Byte (Latin-1)
*   **Bit 4 (Uncached External)**: Set if the data pointer of an external string is not cached.
*   **Bit 5 (Internalization)**:
    *   `0`: Internalized String
    *   `1`: Not Internalized String
*   **Bit 6 (Shared)**: Set if the string is accessible by more than one thread.

This bitfield layout allows V8 to perform extremely fast checks (e.g., checking if a string is one-byte or internalized) using simple bitwise operations.

## The String Table

The **String Table** is a hash table that stores all internalized strings.

### How it Works
*   **Uniqueness**: Every string value in the table is unique.
*   **Lookup**: When V8 needs to internalize a string, it first computes its hash and looks it up in the String Table.
*   **Sharing**: If found, the existing string instance is returned. If not found, the new string is added to the table.
*   **Use Case**: Property names, symbol descriptions, and common identifiers are internalized to allow fast comparison by pointer equality instead of character-by-character comparison.

### Thread Safety
*   **Shared String Table**: V8 can be configured to use a single shared string table across all isolates in a process (enabled by default when the V8 Sandbox or shared isolates are used).
*   **Locking**: Access to the shared string table is protected by locks to ensure thread safety when multiple isolates are internalizing strings concurrently.

## File Structure
*   `src/objects/string.h`: Main header file defining the string hierarchy.
*   `src/objects/string.tq`: Torque definitions for strings.
*   `src/snapshot/code-serializer.cc`: Handles serialization of strings for code caching.
