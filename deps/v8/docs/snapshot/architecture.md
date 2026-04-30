# V8 Snapshot and Serializer System

To achieve fast startup times, V8 uses a **Snapshot** system. Instead of parsing and compiling the standard library and initializing the heap from scratch every time a new Isolate is created, V8 captures a snapshot of a fully initialized heap and persists it.

When a new Isolate starts up, it simply restores the heap state from the snapshot, which is significantly faster.

## Key Components

### 1. `mksnapshot`

`mksnapshot` is a standalone executable tool built during the V8 build process.
*   **Purpose**: To run a minimal V8 instance, initialize the heap (including compiling builtins), and serialize the resulting heap state into a snapshot.
*   **Output**: It generates C++ files containing the snapshot data as byte arrays, which are then compiled and linked directly into the V8 binary (or written to external files).

### 2. Serializer and Deserializer

The core logic for saving and restoring heap state is in the `Serializer` and `Deserializer` classes (in `src/snapshot/`).

*   **Serialization**: The process of traversing the heap graph and writing objects to a stream (`SnapshotByteSink`).
    *   It handles pointers by converting them to references (e.g., back references to already seen objects or roots).
    *   It uses specialized serializers for different spaces.
*   **Deserialization**: The process of reading the byte stream and recreating the objects in the heap.
    *   It allocates objects and fills in their fields.
    *   It patches pointers based on the references in the stream.

## Serialization Bytecode

The snapshot data is not a raw memory dump. Instead, it is a stream of **bytecode instructions** that the `Deserializer` executes to reconstruct the heap. This design makes the snapshot format robust across different memory layouts and address spaces.

> [!IMPORTANT]
> The serialization format is trusted and strictly build- and version-dependent. Code caches and snapshots include the V8 version and critical build flags as metadata. The deserializer will bail out and reject the snapshot if a mismatch is detected.

The bytecode is defined in `src/snapshot/serializer-deserializer.h` (in `enum Bytecode`) and includes instructions for:
*   **Object Allocation**: Instructions to allocate an object of a specific size in a specific heap space (e.g., `kNewObject`).
*   **Reference Handling**:
    *   **Back References**: Pointing to an object already deserialized in the current stream.
    *   **Root References**: Pointing to a well-known object in the roots table (e.g., `kRootArrayConstants`).
    *   **Attached Objects**: References to objects provided externally to the deserializer.
*   **Data Filling**: Copying raw data into the allocated objects.
*   **Pointer Patching**: Instructions to resolve pointers between objects as they are created.
*   **Repetition**: Optimizing for repetitive data (e.g., `kFixedRepeatRoot`).

By using a bytecode interpreter for deserialization, V8 can handle complex object graphs and cyclic references efficiently during isolate startup.

## Types of Snapshots

V8 uses several distinct snapshots to organize heap data:

### 1. Read-Only Snapshot
*   **Purpose**: Contains objects that are immutable and shared across all Isolates (e.g., the `undefined` value, small integers, certain Maps).
*   **Benefit**: These objects can be placed in a read-only memory region and shared directly, saving memory and avoiding the need for locks.

### 2. Startup Snapshot
*   **Purpose**: Contains the rest of the initial heap state needed to start an Isolate, excluding custom contexts.
*   **Contents**: Includes builtins, initial symbols, and global objects.

### 3. Context Snapshot
*   **Purpose**: Contains the state of a fully initialized `NativeContext`.
*   **Benefit**: Allows embedders (like Chrome or Node.js) to create snapshots of a context with their own specific APIs already initialized.

### 4. Shared Heap Snapshot
*   **Purpose**: Contains objects that should be in the shared heap in the shared Isolate during startup (e.g., for `--shared-string-table`).
*   **Behavior**: When the shared heap is not in use, its contents are deserialized into each Isolate.

## Code Serialization (Code Cache)

Independent of the startup snapshot, V8 also uses serialization for **Code Caching**.
*   **Purpose**: To cache the generated code (Ignition bytecode or machine code) for user scripts.
*   **How it works**: When a script is run for the first time, V8 can serialize its `BytecodeArray` or optimized code and save it to disk. On subsequent loads, V8 can deserialize the code directly, bypassing parsing and compilation.
*   **Files**: Managed by `CodeSerializer` in `src/snapshot/code-serializer.cc`.

## Snapshot Compression

V8 can compress the snapshot data to reduce the size of the binary or external snapshot files.
*   **Implementation**: Managed by `SnapshotCompression` in `src/snapshot/snapshot-compression.h` & `.cc`.
*   **Configuration**: Controlled by the `V8_SNAPSHOT_COMPRESSION` build flag.

## File Structure

*   `src/snapshot/mksnapshot.cc`: Entry point for the snapshot creation tool.
*   `src/snapshot/serializer.h` & `.cc`: Base classes for serialization.
*   `src/snapshot/deserializer.h` & `.cc`: Base classes for deserialization.
*   `src/snapshot/read-only-serializer.cc` & `read-only-deserializer.cc`: Serializer and deserializer for the read-only space.
*   `src/snapshot/startup-serializer.cc` & `startup-deserializer.cc`: Serializer and deserializer for the startup heap.
*   `src/snapshot/context-serializer.cc` & `context-deserializer.cc`: Serializer and deserializer for contexts.
*   `src/snapshot/shared-heap-serializer.cc` & `shared-heap-deserializer.cc`: Serializer and deserializer for the shared heap.
*   `src/snapshot/code-serializer.cc`: Serializer for code caching.
