# V8 Write Barrier - Readme

V8 uses a write barrier to inform the GC about changes to the heap by the mutator.
A write barrier is emitted for heap stores like `host.field = value`.
The write barrier is required for multiple purposes:
* Records old-to-new references for the generational GC to work.
* During marking it prevents black-to-white references during incremental/concurrent marking.
* During marking it records old-to-old references (pointers to objects on evacuation candidates)

The generational barrier is always enabled, while the other barriers are only enabled while incremental/concurrent marking is running.

# Eliminating Write Barriers

`WriteBarrier::IsRequired()` is the source of truth of whether a barrier is required at any given point in time.
Write barriers should only be omitted via proper bottlenecks in the corresponding layers, ensuring that such operations can be verified in non-release builds.
For example, using `SKIP_WRITE_BARRIER` triggers write barrier verification in debug builds.

## Rules
A write barrier can be omitted when storing a Smi or a read-only object, since neither can create an interesting reference for the GC.

A write barrier can also be omitted when storing into the *most recent young allocation*.
This is often called an *initializing store*.
The host object must have been allocated in young generation and no potential GC point (e.g. an allocation, a stack guard check) may have occurred between the allocation and the store.
This is safe because the GC specifically supports elimination for this case to remove a large fraction of write barriers.

None of the three write barrier kinds are necessary for such stores:
* **Old-to-new (generational) barrier:** Not needed because the host object is itself young, so there is no old-to-new reference to record.
* **Old-to-old (evacuation) barrier:** Not needed because the host object is young, so there is no old-to-old slot to track. The GC can still relocate young objects during a GC; new-to-new slots are discovered by scanning the young generation during the atomic GC pause. This is cheap because the young generation is usually small relative to the full heap size. Note that old-to-new references are still recorded by the generational barrier even during incremental marking. That way we discover all references to young objects.
* **Marking barrier:** This is the trickiest case. Concurrent marking bails out of tracing objects that reside in the current new space LAB (linear allocation buffer) and pushes them into an "on hold" queue. This prevents such objects from becoming black — they remain grey. Grey objects are marked but are also contained in the worklist - so they will be traced at some point in the future. All of this means that the host object for such cases can never be black - so we also do not need a WB to prevent black-to-white references here. This is also where the *most recently allocated object* limitation comes from.

# Implementation
The barrier is split into multiple separate parts (ordered from fastest to slowest):
1. fast (inline) code path,
2. deferred (out-of-line) code path,
3. the shared slow path and
4. the C++ slow path.

The fast and deferred code paths of the write barrier have different implementations in all compilers and C++.
These code paths are also emitted for each write barrier/call site.
Only the slow path is shared across all write barrier locations.

CSA and the interpreter do not have their own barrier but reuse Turbofan's write barrier.
While implementations for compilers differ, the general idea is still pretty much the same for all of them.

## Fast path  (#1)
Checks whether the host object has the `POINTERS_FROM_HERE_ARE_INTERESTING` flag set on its page header.
If yes, the write barrier will jump into the deferred code path.
Otherwise regular execution continues as usual.
This check will be skipped if value is a Smi.

* Turbofan implements the fast path in [code-generator-x64.cc](https://source.chromium.org/chromium/chromium/src/+/main:v8/src/compiler/backend/x64/code-generator-x64.cc?q=kArchAtomicStoreWithWriteBarrier) for the `kArchAtomicStoreWithWriteBarrier` instruction opcode.
* Maglev implements the fast path in [maglev-assembler-x64.cc](https://source.chromium.org/chromium/chromium/src/+/main:v8/src/maglev/maglev-assembler.cc?q=MaglevAssembler::CheckAndEmitDeferredWriteBarrier) in the [StoreTaggedFieldWithWriteBarrier](https://source.chromium.org/chromium/chromium/src/+/main:v8/src/maglev/maglev-ir.cc?q=StoreTaggedFieldWithWriteBarrier::GenerateCode) node.
* Sparkplug implements it in [MacroAssembler::RecordWrite](https://source.chromium.org/chromium/chromium/src/+/main:v8/src/codegen/x64/macro-assembler-x64.cc?q=MacroAssembler::RecordWrite%28).
* For C++ the fast path is implemented in [CombinedWriteBarrierInternal](https://source.chromium.org/chromium/chromium/src/+/main:v8/src/heap/heap-write-barrier-inl.h?q=CombinedWriteBarrierInternal).

## Deferred code path (#2)
Checks whether the value object has the `POINTERS_TO_HERE_ARE_INTERESTING` flag set on its page header.
If yes, the write barrier will call the builtin function for the slow path.
Otherwise the deferred code jumps back to the regular instruction stream.

* Turbofan implements the deferred code path in the [OutOfLineRecordWrite](https://source.chromium.org/chromium/chromium/src/+/main:v8/src/compiler/backend/x64/code-generator-x64.cc?q=OutOfLineRecordWrite) class.
* Maglev emits this code path using [MakeDeferredCode](https://source.chromium.org/chromium/chromium/src/+/main:v8/src/maglev/maglev-assembler.cc?q=MaglevAssembler::CheckAndEmitDeferredWriteBarrier) in the same function as the fast path.
* Sparkplug is the only compiler which does not emit this check in some out of line code path but instead emits them directly inline in [MacroAssembler::RecordWrite](https://source.chromium.org/chromium/chromium/src/+/main:v8/src/codegen/x64/macro-assembler-x64.cc?q=MacroAssembler::RecordWrite%28).

## Slow path (#3)
The slow path of the barrier is shared between all compilers and call sites.
In the generational barrier the field/slot is simply recorded in the old-to-new slot set.
During incremental/concurrent marking it also marks the `value` object and may record the slot in the old-to-old remembered set for pointers to evacuation candidates.
This part of the barrier is implemented in CSA in [builtins-internal-gen.cc](https://source.chromium.org/chromium/chromium/src/+/main:v8/src/builtins/builtins-internal-gen.cc?q=WriteBarrierCodeStubAssembler) and used in the [RecordWriteXXX](https://source.chromium.org/chromium/chromium/src/+/main:v8/src/builtins/builtins-internal-gen.cc?q=RecordWriteSaveFP) builtins.

## C++ slow path (#4)
Inserting into the slot set in the [RecordWriteXXX](https://source.chromium.org/chromium/chromium/src/+/main:v8/src/builtins/builtins-internal-gen.cc?q=RecordWriteSaveFP) builtins may need to `malloc()` memory.
The builtin therefore may call into C++ for this operation.
The C++ function for this is [Heap::InsertIntoRememberedSetFromCode](https://source.chromium.org/chromium/chromium/src/+/main:v8/src/heap/heap.cc?q=Heap::InsertIntoRememberedSetFromCode).

# Indirect pointer barrier
V8 also needs a special barrier for pointers into the trusted space with the sandbox.
This barrier is only implemented for 64-bit architectures and only used in builtins written in either CSA or assembler.

This barrier is only active during incremental marking.
Therefore the fast path of this barrier only jumps into the deferred code path if marking is active.
Checking the host object flags is not necessary.

The Turbofan indirect pointer write barrier is implemented as a separate instruction opcode [kArchStoreIndirectWithWriteBarrier](https://source.chromium.org/chromium/chromium/src/+/main:v8/src/compiler/backend/x64/code-generator-x64.cc?q=kArchStoreIndirectWithWriteBarrier).
The deferred code path is implemented in the [OutOfLineRecordWrite](https://source.chromium.org/chromium/chromium/src/+/main:v8/src/compiler/backend/x64/code-generator-x64.cc?q=OutOfLineRecordWrite) class along with the regular write barrier.

Assembler builtins can emit this barrier using [MacroAssembler::RecordWrite](https://source.chromium.org/chromium/chromium/src/+/main:v8/src/codegen/x64/macro-assembler-x64.cc?q=MacroAssembler::RecordWrite%28). This method handles both the indirect pointer but also the regular write barrier.
